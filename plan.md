# EazyMake 1.2.0-dev.3 执行计划

> **状态：待执行**（承接 dev.1/dev.2 已完成，2026-08-11）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.3.md`](plans/1.2.0/1.2.0-dev.3.md)。本计划为 1.2.0 系列第三个开发子版本：**默认模板内建 Debug/Release Profile + 默认 Profile 配置项**——`write_default_config()` 模板内置 `[compile.profile.debug/release]`，基准去 `-O2`、优化归 profile，新增 `[compile].default_profile`（模板内建 `"debug"`），无 `--profile` 时回退到该 profile。
>
> **范围边界**：只改 `write_default_config()` 模板、`CompileSection.default_profile` 字段与 `parse_compile()` 读取、以及 4 处 `--profile` 消费点（build / watch / `project cc` / `export cmake`）的回退逻辑。不新增命令、不弃用、不触碰公共 API。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯内部模板与可选字段）；③ 全量测试零回归（Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

`ezmk project new` 生成的默认 `ezmk.toml` 目前只有基准 `[compile].flags = ["-Wall", "-Wextra", "-O2"]`，无内建 profile——用户要 Debug/Release 需手写 `[compile.profile.*]`（新手不友好、易漏 `msvc_flags`）。同时基准 `-O2` 隐含"默认即优化"，与"优化级别属于 profile"的清晰语义相悖。

本计划：把 Debug/Release profile 固化进默认模板，基准收敛为警告-only；并新增 `[compile].default_profile` 配置项（模板内建 `"debug"`），让无 `--profile` 的默认构建开箱即可调试、断言开启，需优化时显式 `--profile release`。

---

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 默认模板内建 `[compile.profile.debug]` 与 `[compile.profile.release]`（含 `flags` + `msvc_flags`），跨 GCC/Clang/MSVC 一致 | P0 |
| 2 | 基准 `[compile].flags` 收敛为警告-only（`["-Wall", "-Wextra"]`），优化级别移入 profile | P0 |
| 3 | 新增 `[compile].default_profile` 配置项：声明"无显式 `--profile` 时默认使用的 profile"；模板内建 `"debug"` | P0 |
| 4 | 新项目开箱 `ezmk build` 默认即 debug 构建；`--profile debug` / `--profile release` 显式可用 | P0 |
| 5 | 仅影响**新创建**项目；既有 `ezmk.toml` 不变 | P0 |
| 6 | 单测 + 集成测试覆盖模板内容、default_profile 解析/回退与 profile 构建 | P0 |
| 7 | 文档同步（config_file.md 改写 "not auto-apply" 表述 / project new 说明） | P1 |

---

## 3 执行阶段

### 阶段一：模板与配置项

- [ ] **1.1 模板**（4.1）：`write_default_config()` 更新——base 去 `-O2`、新增 `[compile.profile.debug/release]` 段、`[compile]` 增 `default_profile = "debug"`
- [ ] **1.2 配置项**（4.2）：`CompileSection` 增 `std::string default_profile;`（`include/ezmk/config.hpp:39-49`）+ `parse_compile()`（`src/config.cpp:385-473`）读取 `[compile].default_profile`

### 阶段二：回退逻辑（4 处消费点）

- [ ] **2.1 主构建回退**（4.3）：`prepare_build_state()`（`src/build.cpp:494-533`）`opts.profile` 为空时回退 `cfg.compile.default_profile`，复用 profile 合并 / unknown 报错路径（含 did-you-mean）
- [ ] **2.2 其他消费点**（4.3）：`project watch`（`src/main.cpp:240-249`）、`project cc`（`src/main.cpp:201`）、`export cmake`（`src/export.cpp:133-134`）无 `--profile` 时同款回退——保证 compile_commands.json / CMake 导出与默认构建一致

### 阶段三：测试与文档

- [ ] **3.1 单测**（4.4）：`test_config.cpp`——`write_default_config()` 输出含两个 profile 段 + `default_profile = "debug"`、base 不含 `-O2`；parse `default_profile`（合法名/缺省为空）；`parse_config()` round-trip；既有 `write_default_config: no profile or hooks sections`（:1459）断言更新
- [ ] **3.2 集成**（4.5）：`test_integration.cpp`——`ezmk project new` → 校验模板含 profile 段 + `default_profile = "debug"`；无 `--profile` `ezmk build` 默认 debug flags（`-g -O0`）；`--profile debug` / `--profile release` 构建成功
- [ ] **3.3 文档**（4.6）：`docs/en|zh/config_file.md`（`[compile]` 段补 `default_profile`；改写 "Profiles do not auto-apply" → default_profile 例外 + 优先级）、`docs/en|zh/cli.md`（`project new` 提及新模板）、CHANGES.md
- [ ] **3.4 回归**（4.7）：全量测试零回归（基线 546 用例 / 2617 断言）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **default_profile 解析顺序** | 显式 `--profile <name>` > `[compile].default_profile`（非空时）> 基准-only；旧配置无此字段 → 行为不变 |
| **模板内建 `"debug"`** | 开箱 `ezmk build` 默认 = debug（`-Wall -Wextra -g -O0`，可调试、断言开启）；需优化时显式 `--profile release` |
| **4 处消费点统一回退** | build / watch / `project cc` / `export cmake` 无 `--profile` 均回退 default_profile——compile_commands.json 与 CMake 导出和默认构建一致，避免"构建用 debug、索引/导出用基准"分叉 |
| **unknown profile 复用现有报错** | default_profile 指向不存在的 profile → 走既有 `prepare_build_state()` unknown-profile fatal（含 did-you-mean），不新增错误路径 |
| **不内建 `[link.profile.*]` / profile macros** | 模板保持最小；用户按需自行追加 |
| **无新 i18n key** | 模板内容非用户消息，无需 i18n |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 profile 段（模板） | 纯新增 | 新项目开箱可用；旧项目不变 |
| 新增 `[compile].default_profile`（可选字段） | 纯新增 | 旧配置无此字段 → 无 `--profile` 时仍基准-only，行为不变；模板内建 `"debug"` 仅影响新建项目 |
| base 移除 `-O2`（模板） | 新建项目默认构建 = debug（`-g -O0`，无优化、断言开启） | 需要优化时显式 `--profile release`；CHANGES.md 明确 |
| 无新 i18n key | 无 | 模板内容非用户消息 |
| 无新命令 / 无既有字段语义变化 | 无 | 仅新增可选字段；`--profile` 显式优先级不变 |

---

## 6 延后项

- **profile macros 内建**（如 debug 注入 `DEBUG=1`）：可选增强，本版不做——用户按需在 `[compile.profile.<name>.macros]` 追加，模板保持最小。
- **`[link.profile.*]` 模板内建**：链接 profile 为空 `flags = []` 时与缺省等价，模板不加无意义空段；用户有链接专属 profile 标志（如 `-flto`）时自行追加。
- **dev.4 CMake 导入 / dev.5 catch2 v3 / dev.6 构建耗时统计**：1.2.0 系列后续子版本，独立并行。
