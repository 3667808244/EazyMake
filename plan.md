# EazyMake 1.3.0 执行计划

> **状态：执行中**（2026-08-19 启动）。1.3.x 系列路线图见 [`plans/1.3.x/README.md`](plans/1.3.x/README.md)。
>
> 详细设计：[**1.3.0.md**](plans/1.3.x/1.3.0.md)。本计划为 1.2.x 收官后的**首个功能 minor**：新增 **Workspace（工作区）**——一个目录下若干**编译期互不依赖的独立项目集合**，统一、可并行的批量管理。正式版按 **dev（功能）→ pre（收口）** 推进：dev.1 配置与定位 → dev.2 命令与并行执行 → dev.3 测试与 CI → pre.1 文档与发布收口 → 1.3.0 聚合发布。
>
> **范围边界**：纯增量——新命令组 `ezmk workspace` + 新配置文件 `ezmk-workspace.toml`；**公共 API 无破坏性变更**（单项目路径 `project`/`pkg`/`repo`/`example` 零改动，破坏性变更仍仅归 2.0.0）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 793 用例 / 3755 断言）。

---

## 1 背景

- EazyMake 只有单项目模型（`ezmk build` 向上找 `ezmk.toml`），多独立项目只能逐个 cd 或外部脚本循环。
- 需要多项目组织层：workspace = 编译期互不依赖的独立项目集合，统一批量管理（build/test/clean），可跨成员并行、缓存/产物独立。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 独立配置文件 `ezmk-workspace.toml` + `locate_workspace_root()` 定位 | P0 |
| 2 | 顶层 `ezmk workspace` 命令组（list/build/test/clean/--member/--stop-on-error） | P0 |
| 3 | 跨成员并行调度 + 结果汇总 | P0 |
| 4 | 安全边界：路径逃逸拒绝 + 成员间依赖互引拒绝 | P0 |
| 5 | 测试（单元 + 集成）+ 全量零回归 | P0 |
| 6 | 文档：cli.md / README / 教程 / CHANGES.md 1.3.0 条目 | P1 |
| 7 | 公共 API 无破坏性变更 | P0 |

## 3 执行阶段（每阶段一个 commit）

### dev.1 — 配置与定位（1.3.0-dev.1）

- [ ] **1.1 数据结构与解析**：`include/ezmk/workspace.hpp` + `src/workspace.cpp`（toml11 解析 `[workspace]`/`[workspace.options]` + 缺省 + 非法格式报错）
- [ ] **1.2 定位**：`locate_workspace_root()`（5 层上溯，与 `locate_project_root` 对称互不干扰）
- [ ] **1.3 成员校验**：路径逃逸（`../`/绝对/盘符/符号链接出根）、存在性 + `ezmk.toml`、无嵌套、成员间 `[depends]` 互引拒绝
- [ ] **1.4 单测**（`test/test_workspace.cpp`）+ 全量回归（基线 793/3755）

### dev.2 — 命令与执行（1.3.0-dev.2）

- [ ] **2.1 `Command::Workspace` + cli 解析**（list/build/test/clean/--member/--stop-on-error）+ main 分发
- [ ] **2.2 `src/workspace.cpp` 执行器**：跨成员并行调度（线程池思路，`-j` 限并发）+ 成员 cwd 独立管线 + 输出成员前缀 + 结果汇总（成功/失败/耗时）+ 退出码
- [ ] **2.3 i18n**：`workspace_*` key（X-macro 三向一致，`check_i18n.py` 通过）

### dev.3 — 测试与 CI（1.3.0-dev.3）

- [ ] **3.1 集成测试**：3 成员 workspace → list 校验 / build -j 并行全成功 / 改一成员增量隔离 / 成员缺失 ezmk.toml 标记 invalid 不阻断 / `../` 逃逸与依赖互引报错 / --member 子集 / 失败汇总 + --stop-on-error
- [ ] **3.2 CI**：ubuntu job 追加 workspace 冒烟步骤（如有）
- [ ] **3.3 全量回归**：`bash build.sh test-all` 零失败（基线 793/3755）

### pre.1 — 文档与发布收口（1.3.0-pre.1）

- [ ] **4.1 文档**：cli.md（workspace 节，zh/en 中文基准先行）/ README 命令速览 / 教程（新章或并入 06）/ CHANGES.md 1.3.0 条目
- [ ] **4.2 发布门槛复核**：API 无破坏性变更 + 全量零回归；`plans/1.3.x/README.md` 与 plan.md 收口

### 1.3.0 — 正式发布

- [ ] **5.1 聚合发布**：版本置 1.3.0 + tag + Release + 三渠道分发（winget/Homebrew/pacman）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 独立 `ezmk-workspace.toml` | 与 `ezmk.toml` 解耦，独立解析；根可同时是项目与 workspace |
| 无编译依赖 = 可并行 | 成员间 `[depends]` 互引在配置期拒绝（并行保证） |
| 批量命令 + 并行调度 | 非构建图（区别于 CMake add_subdirectory）；成员各自独立管线 |
| 复用单项目管线 | `build_compile_args` / 增量缓存 / 测试缓存按成员天然隔离 |
| 顶层 build 不隐式降级 | workspace 纯容器根提示用 `ezmk workspace build` |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `ezmk workspace` 命令组 | 纯新增 | 与既有命令不冲突；无别名 |
| 新增 `ezmk-workspace.toml` | 纯新增 | 独立解析；定位互不干扰 |
| 顶层 `ezmk build` 在纯容器根 | 提示 | 有 `ezmk.toml` 行为不变 |
| 公共 API | 无破坏性变更 | 新命令/新文件纯增量 |

## 6 延后项（明确收口）

- **成员级过滤 / `cc` 批量生成 / 成员间真实依赖（构建图）**：归 2.0.0 或后续评估；本版为最小可用（批量 + 并行）。
- **2.0.0**：保持破坏性变更窗口（`[test].flags` / `ezmk utils cc` 移除等），与 workspace 解耦。
