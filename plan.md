# EazyMake 1.3.0 执行计划

> **状态：执行中**（2026-08-19 启动）。1.3.x 系列路线图见 [`plans/1.3.x/README.md`](plans/1.3.x/README.md)。
>
> 详细设计：[**1.3.0.md**](plans/1.3.x/1.3.0.md)。本计划为 1.2.x 收官后的**首个功能 minor**：新增 **Workspace（工作区）**——一个目录下若干独立项目（成员）的集合，统一、可并行的批量管理；成员间可声明**单向非循环依赖**（最常见 monorepo 形态：共享基础库 + 多个可执行文件），构建时**拓扑排序 + 产物复用**。正式版按 **dev（功能）→ pre（收口）** 推进：dev.1 配置/定位/依赖校验 → dev.2 命令/拓扑/注入 → dev.3 测试与 CI → pre.1 文档与发布收口 → 1.3.0 聚合发布。
>
> **范围边界**：纯增量——新命令组 `ezmk workspace` + 新配置文件 `ezmk-workspace.toml` + 成员 `[depends] workspace` 新字段；**公共 API 无破坏性变更**（单项目路径 `project`/`pkg`/`repo`/`example` 零改动，破坏性变更仍仅归 2.0.0）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 794 用例 / 3769 断言，1.2.5 后）。

---

## 1 背景

- EazyMake 只有单项目模型（`ezmk build` 向上找 `ezmk.toml`），多独立项目只能逐个 cd 或外部脚本循环。
- 需要多项目组织层：workspace = 若干独立项目的集合，统一批量管理（build/test/clean），可跨成员并行、缓存/产物独立。
- 实际 monorepo 最常见的形态是**共享基础库 + 多个可执行文件**：`@link:` 源码级共享每次改动会让所有消费者各自重编；workspace 的成员依赖（静态库产物复用）可消灭这种重复编译。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 独立配置文件 `ezmk-workspace.toml` + `locate_workspace_root()` 定位 | P0 |
| 2 | 成员依赖声明：成员 `ezmk.toml` 的 `[depends] workspace = ["<成员名>"]`（命名空间区别于外部包） | P0 |
| 3 | 依赖约束：**单向非循环**（环在配置校验期拒绝）+ 被依赖成员为 `static`（产物 `build/lib<name>.a`）；无版本（开发中即改即用） | P0 |
| 4 | 顶层 `ezmk workspace` 命令组（list/build/test/clean/--member（含依赖闭包）/--stop-on-error）+ `build/test/clean` 附 `-w` 重定向 | P0 |
| 5 | 构建语义：Kahn 拓扑分层（依赖层先构建）+ 层内并行 + 兄弟产物自动注入（**成员自发现，零环境变量**） | P0 |
| 6 | 增量：改库 `.cpp` → 库重编 + 消费者仅重链接；改库 `.h` → 消费者经 depfile 自动重编 | P0 |
| 7 | 安全边界：路径逃逸拒绝 + 环拒绝 + 依赖成员类型校验 | P0 |
| 8 | **命令行长度兜底**：编译/链接命令超阈值 → GCC 响应文件（大型项目不炸） | P1 |
| 9 | 测试（单元 + 集成，含坑位锁定用例）+ 全量零回归 | P0 |
| 10 | 文档：cli.md / README / 教程 / CHANGES.md 1.3.0 条目 + non-goals「多项目工作区」条款更新 | P1 |
| 11 | 公共 API 无破坏性变更 | P0 |

## 3 执行阶段（每阶段一个 commit）

### dev.1 — 配置、定位与依赖校验（1.3.0-dev.1）✅

- [x] **1.1 数据结构与解析**：`include/ezmk/workspace.hpp` + `src/workspace.cpp`（toml11 解析 `[workspace]`/`[workspace.options]` + 缺省 + 非法格式报错）
- [x] **1.2 定位**：`locate_workspace_root()`（5 层上溯，与 `locate_project_root` 对称互不干扰）
- [x] **1.3 成员校验**：路径逃逸（`../`/绝对/盘符/符号链接出根）、存在性 + `ezmk.toml`、无嵌套
- [x] **1.4 成员依赖校验**：`config.hpp` `DependsSection::workspace` + `validate_ws_deps`（引用解析 / DFS 环检测含自环 / 被依赖类型须 static）
- [x] **1.5 单测**（`test/test_workspace.cpp`：解析 / 定位 / 校验各拒绝分支 / 依赖、环、类型）+ 全量回归（基线 794/3769 → 822/3922，零回归）

### dev.2 — 命令、拓扑与注入（1.3.0-dev.2）

- [ ] **2.1 `Command::Workspace*` + cli 解析**（list/build/test/clean/--member/--stop-on-error/-j）+ main 分发 + `-w` 重定向 + help 文本
- [ ] **2.2 Kahn 拓扑分层**：`topo_layers(ws)`（依赖层先构建，同层并行）
- [ ] **2.3 子进程执行器**（`src/workspace_build.cpp`）：每成员子进程 + 层内 ThreadPool + 输出成员前缀 + 结果汇总（成功/失败/耗时/skipped）+ 退出码；`--stop-on-error` = 失败后停派发（本层未启动 + 后续层 skipped）、不 kill 在跑成员
- [ ] **2.4 兄弟产物注入 = 成员自发现**：build.cpp 读 `[depends] workspace` → locate/load（复用 dev.1）→ 拼 `-I/-L/-l`（存在性门控）；**零环境变量**（坑 1）；签名含注入参数
- [ ] **2.5 命令行长度兜底**：编译/链接命令 >16K → GCC 响应文件 `@<rsp>`（坑 1 兜底）
- [ ] **2.6 跨成员增量**：库 `.cpp` → 依赖者重链；库 `.h` → 依赖者经 depfile 重编
- [ ] **2.7 i18n**：`workspace_*` key（X-macro 三向一致，`check_i18n.py` 通过）

### dev.3 — 测试与 CI（1.3.0-dev.3）

- [ ] **3.1 集成测试**：3 成员 workspace（1 库 + 2 可执行，带 `[depends] workspace`）→ list / build -j / test / 依赖构建顺序 / 跨成员增量（库 .cpp→重链、库 .h→重编）/ clean / 失败汇总 + --stop-on-error（停派发 + skipped 计数）/ --member 含闭包 / **成员内独立构建（注入已存在产物、不触发闭包）** / **注入零环境变量** / 校验拒绝（路径逃逸、环、非 static 被依赖、嵌套、成员缺失）
- [ ] **3.2 单测补全**：自发现注入解析（存在性门控、无 `EZK_WS_*`）+ 响应文件阈值触发（含空格路径）
- [ ] **3.3 CI**：ubuntu job 追加 workspace 冒烟步骤（含依赖成员 + 二次构建增量断言）
- [ ] **3.4 全量回归**：`bash build.sh test-all` 零失败（基线 793/3755 + workspace 用例）

### pre.1 — 文档与发布收口（1.3.0-pre.1）

- [ ] **4.1 文档**：cli.md（workspace 节，zh/en 中文基准先行）/ README 命令速览 / 教程（新章或并入 06）/ CHANGES.md 1.3.0 条目
- [ ] **4.2 non-goals 条款更新**：`docs/zh/non-goals.md`（中文基准先行）→ `docs/en/non-goals.md`：允许**单向非循环成员依赖**；完整构建图（环 / 版本 / 平台矩阵 / 可编程图）仍拒绝
- [ ] **4.3 发布门槛复核**：API 无破坏性变更 + 全量零回归；`plans/1.3.x/README.md` 与 plan.md 收口

### 1.3.0 — 正式发布

- [ ] **5.1 聚合发布**：版本置 1.3.0 + tag + Release + 三渠道分发（winget/Homebrew/pacman）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 独立 `ezmk-workspace.toml` | 与 `ezmk.toml` 解耦，独立解析；根可同时是项目与 workspace（members 不含根，除非显式列出） |
| 依赖声明跟随成员 | `[depends] workspace` 写在成员自己的 `ezmk.toml`；搬移/独立出去声明不变 |
| 依赖图 = 最小形态 | **单向非循环** + 被依赖须 `static` + 无版本；环/类型在配置期 fail-fast |
| 拓扑分层 + 层内并行 | Kahn 算法；无依赖成员 = 第 0 层（纯批量退化为特例） |
| 子进程执行模型 | 每成员独立 `<ezmk> build/test/clean` 子进程（cwd/缓存/Lua 状态天然隔离） |
| 产物注入 = 成员自发现 | 成员进程自行 locate/load + 解析自身 `[depends] workspace` 拼 `-I/-L/-l`（存在性门控）；**零环境变量**——长度与规模无关，大型项目不炸（坑 1） |
| 命令行长度兜底 | 编译/链接命令 >16K → GCC 响应文件 `@<rsp>`（Windows 32767 上限的通用加固，坑 1 兜底） |
| `--member` = 含依赖闭包 | 目标成员 + 依赖按拓扑先构建（坑 2）；单成员不构建闭包 → `cd <member> && ezmk build` |
| `--stop-on-error` 精确语义 | 失败后停派发（本层未启动 + 后续层 skipped）、在跑成员自然结束不 kill、摘要含 skipped（坑 3）；`clean` 不支持 |
| `-w` = 重定向 | `ezmk build -w` ≡ `ezmk workspace build`；**非**「项目 + workspace 都构建」（坑 4） |
| 复用单项目管线 | `build_compile_args` / 增量缓存 / 测试缓存按成员天然隔离 |
| 顶层 build 不隐式降级 | workspace 纯容器根提示用 `ezmk workspace build` |
| 与包模型互补 | 成员依赖 = 开发中源码即改即用；包（目录包/归档包）= 分发与快照隔离 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `ezmk workspace` 命令组 | 纯新增 | 与既有命令不冲突；无别名 |
| 新增 `ezmk-workspace.toml` | 纯新增 | 独立解析；定位互不干扰 |
| `[depends] workspace` 新字段 | 纯新增 | 未声明时零影响；单项目（非成员）声明了但找不到 workspace → 报错提示 |
| 成员内 `ezmk build` 自动解析兄弟产物 | 仅成员生效 | 非成员项目行为不变 |
| 顶层 `ezmk build` 在纯容器根 | 提示 | 有 `ezmk.toml` 行为不变 |
| 公共 API | 无破坏性变更 | 新命令/新文件/新字段纯增量 |

## 6 延后项（明确收口）

- **shared 运行时依赖 / 成员级过滤（按标签）/ `cc` 批量生成**：归 2.0.0 或后续评估；本版为最小可用（批量 + 并行 + 单向非循环依赖）。
- **完整构建图**（环 / 版本约束 / 平台矩阵 / 可编程图）：**non-goals**，本版明确拒绝（见 `docs/zh/non-goals.md` 与 `docs/en/non-goals.md`「多项目工作区」条款）。
- **2.0.0**：保持破坏性变更窗口（`[test].flags` / `ezmk utils cc` 移除等），与 workspace 解耦。
