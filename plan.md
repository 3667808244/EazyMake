# EazyMake 1.2.0-dev.1 执行计划

> **状态：待执行**（前置 1.1.1 已发布；1.1.3 已于 2026-08-10 发布）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)，dev.2~dev.4 独立并行。
>
> 详细设计：[`1.2.0-dev.1.md`](plans/1.2.0/1.2.0-dev.1.md)。本计划为 1.2.0 系列首个开发子版本：**`ezmk project cc` 命令 + `ezmk utils cc` 弃用**，基于 1.1.1 已交付的 `build_compile_args()` + `compile_db` 单一事实源。
>
> **范围边界**：仅新增正式命令 `ezmk project cc`（零外部包依赖）并过渡 `ezmk utils cc` 为弃用提示（工具保留可用，2.0.0 移除）。核心重构 / `utils cc` 拦截 / `[compile].compile_commands` 自动生成已由 1.1.1 交付，本版生成器核心**零改动**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯新增 + 弃用不删）；③ 全量测试零回归（Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

1.1.1 已把编译命令构造收敛为**单一事实源**（`build_compile_args()` / `join_shell_args()` 重构 + `compile_db` 模块），并**拦截** `ezmk utils cc` 改由内置 C++ 生成器服务，修复了原有 Lua 工具的 compile_commands.json 命令 drift。

1.2.0 在其上补齐最后一步：新增**正式命令** `ezmk project cc`——内置 CLI 入口、零外部包依赖，不再需要 `ezmk-official-utils`。同时把 `ezmk utils cc` 从「拦截」过渡到「弃用提示」（2.0.0 移除）。

> 自动生成（`[compile].compile_commands`）已在 **1.1.1** 交付，本版不重复。

---

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 新增内置命令 `ezmk project cc`（基于 1.1.1 的 `build_compile_args()` + `compile_db`），`arguments` 数组输出，零外部包依赖 | P0 |
| 2 | `ezmk utils cc` 从 1.1.1 的拦截过渡为弃用提示（`use ezmk project cc instead`），工具保留可用，2.0.0 移除 | P1 |
| 3 | 全量测试零回归 + 新增单测/集成测试覆盖新命令与弃用 | P0 |

---

## 3 执行阶段

### 阶段一：前置确认 + 命令交付

- [x] **1.1 前置确认**（4.0）：1.1.1 已发布——`build_compile_args()` / `compile_db` / `utils cc` 拦截 / `[compile].compile_commands` 自动生成均已落地
- [x] **1.2 命令实现**（4.1）：`Command::ProjectCc` 枚举 + `project cc` 子命令（与 `build`/`run`/`clean` 同级）+ `-o/--output`（默认 `<proj_root>/compile_commands.json`）+ `--profile <name>` 解析（`cli.hpp` / `cli.cpp`）
- [x] **1.3 命令分发**（4.1）：`main.cpp` 新增 `Command::ProjectCc` 分支，转调 `compile_db::generate_compile_db()`（与 1.1.1 拦截共用同一实现）
- [x] 阶段一自测：`bash build.sh` 编译通过 + 冒烟（`project cc` 输出 JSON / `-o` 自定义路径 / `--profile` 应用 `-g -DDEBUG=1` / help 新增行）

### 阶段二：弃用过渡 + i18n

- [x] **2.1 弃用提示**（4.2）：`Command::Utils` 对 `cc` 的分支改为输出弃用提示并转调 `project cc` 逻辑：

      [ezmk warn] `ezmk utils cc` is deprecated since 1.2.0 — use `ezmk project cc` instead.

- [x] **2.2 包侧标注**（4.2）：`ezmk-official-utils/utils/cc.lua` 的 `help()` 标注 `@deprecated since 1.2.0`；包 README 同步；包版本随声明提升（1.1.0 → 1.2.0）
- [x] **2.3 i18n**（4.3）：`i18n_keys.def` + `locale/en.json` + `locale/zh.json` 新增 `compile_db_*` / `help_*` / 弃用提示 key；`check_i18n.py` 三向一致通过（280 keys）

### 阶段三：测试

- [x] **3.1 单测**（4.4）：`test/test_cli.cpp` 解析用例（`project cc` 命令 / `-o` / `--output[=]` / `--profile[=]` / 拒绝 positional / 缺值报错）；`test/test_compile_db.cpp` 新增 `--profile` 生效（profile flags 追加）+ 未知 profile 拒绝（`fatal_error`）
- [x] **3.2 集成测试**（4.4）：`test/test_integration.cpp` 新增——建项目 → `project cc` → 校验 JSON / `-o custom.json` / `--profile` 与 `--profile=` 应用 / `utils cc` 弃用提示且仍生成 JSON
- [x] **3.3 全量回归**（4.4）：`bash build.sh test-all` 零回归（642 用例 / 2970 断言，基线 630 用例 / 2918 断言，净增 12 用例）

### 阶段四：文档与收口

- [ ] **4.1 文档**（4.5）：`docs/en|zh/cli.md` 命令表（`project cc` + `-o`/`--profile`）、`docs/en|zh/utils.md` cc 工具弃用说明、README、CHANGES.md（弃用口径：仅弃用不删除）
- [ ] **4.2 发布门槛预检**：① 清单全部完成或明确收口；② 公共 API 无破坏性变更（新增命令纯增量、`utils cc` 仅弃用提示仍可用）；③ 全量测试零回归
- [ ] **4.3 交接推进**：dev.1 收口 → `plans/1.2.0/README.md` 状态更新（dev.1 完成）；接续 dev.2（CMakeLists.txt 导出）或与 dev.3 协同验证 `--profile`

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **命令形态** | `Command::ProjectCc`，`project` 子命令与 `build`/`run`/`clean` 同级；不新增顶层别名/简写（避免 `ezmk cc` 与 C 编译器歧义，保持与用户指定命名一致） |
| **flags** | `-o/--output <path>`（相对项目根解析，默认 `<proj_root>/compile_commands.json`）、`--profile <name>` |
| **输出格式** | 沿用 1.1.1 `compile_db` 输出：`arguments` 数组（clangd 推荐，免 shell 双重转义）；`file` 相对项目根、`directory` 项目根绝对路径；按 `rel_src` 字典序；temp → rename 原子写。生成器核心**零改动** |
| **三个入口同一实现** | 拦截（1.1.1）/ 自动生成（1.1.1）/ `project cc` 命令（本版）共用 `generate_compile_db()`，输出永远一致 |
| **弃用声明时机** | 1.1.1 未声明废弃（拦截是内部实现）；**1.2.0 才正式声明**：输出弃用提示 + 转调，工具保留可用，2.0.0 移除 |
| **`--profile` 一致性** | `project cc --profile` 应用逻辑须与 `build.cpp` 一致，避免「构建用 profile、索引不用」 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `ezmk project cc` 命令 | 纯新增 | 不影响既有命令 |
| `ezmk utils cc` 弃用提示 | 工具仍可用 | 仅输出弃用提示 + 转调；2.0.0 移除 |
| 前置 1.1.1（重构/拦截/自动生成） | 本版基础 | 1.1.1 已发布；`arguments` 格式 clangd 兼容 |
| i18n 新增 key | 纯增量 | X-macro（`i18n_keys.def` + 两份 JSON）三向一致 |

---

## 6 延后项

- **`ezmk utils cc` 移除**：2.0.0 破坏性窗口执行，本版只弃用不删。
- **dev.2 CMakeLists.txt 导出 / dev.3 默认模板 Profile / dev.4 CMake 项目导入**：1.2.0 系列后续子版本，与本版独立并行；dev.2/dev.4 互为反向互补（导出/导入），dev.3 完成后 `project cc --profile release` 可开箱对照验证。
- **承接 1.1.3 延后项**（归 1.2.0）：安装钩子权限门控（S1b）、watcher 静默死亡 / kqueue 缺口、宽窄字符路径 / i18n 二次替换、CLI 参数嵌入 NUL 完整防御、OVERLAPPED 池实例化重构——均不在 dev.1 范围，归后续子版本或正式版。
