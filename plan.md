# EazyMake 1.2.0-dev.4 执行计划

> **状态：待实现**。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.4.md`](plans/1.2.0/1.2.0-dev.4.md)。本计划为 1.2.0 系列第四个开发子版本：**CMake 项目导入（实验性）**——新增 `ezmk project import --from cmake`，把标准 CMake 项目**单向转换**为 `ezmk.toml`，与 dev.2 的 `export cmake` 构成反向互补。
>
> **范围边界**：新增 `ezmk project import` 命令（`--from` 仅 cmake、大小写不敏感、`--overwrite`）。轻量 CMake 解析器 + 核心命令映射 + `find_package`/条件编译 best-effort + 非声明式写法事务性拒绝。不触碰既有命令、不弃用、不破坏公共 API。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯新增命令 + flag）；③ 全量测试零回归（Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

使用 CMake 的项目无法直接导入 EazyMake，可能影响 CMake 用户转化。dev.2 的 `ezmk project export cmake` 提供 EazyMake → CMake 单向导出；本计划补齐反向通道：**CMake → EazyMake** 的 `ezmk project import`，让标准 CMake 项目无缝转化为 EazyMake 项目。

定位：**实验性**（转换 best-effort，非标准写法明确拒绝并指引手动迁移）；**单向快照**（转换后 `ezmk.toml` 成为唯一事实源）；面向「最最标准」的 CMake 项目，覆盖大多数小型/单 target 项目。

---

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 新增 `ezmk project import` 命令（`--from` 目前仅支持 `cmake`，大小写不敏感） | P0 |
| 2 | 支持标准 CMake 命令的转换（§3.2 命令列表），核心目标场景零手工迁移 | P0 |
| 3 | `find_package` 依赖 best-effort：生成注释掉的 `[depends]` 条目 + i18n TODO 提示 | P1 |
| 4 | 非声明式写法（自定义命令/生成器表达式/函数宏/外部依赖查找）**明确拒绝**：事务性中止 + i18n 报错 + 迁移文档指引 | P0 |
| 5 | 生成物 `ezmk.toml` 头部注释（来源/版本/时间戳/实验性警告） | P1 |
| 6 | i18n key + en/zh 翻译，`check_i18n.py` 三向一致 | P0 |
| 7 | 单测 + 集成测试覆盖解析/映射/拒绝/条件编译；全量测试零回归 | P0 |
| 8 | 迁移文档与教程编写 | P1 |

---

## 3 执行阶段

### 阶段一：命令骨架 + 解析器

- [x] **1.1 命令骨架**（4.1）：`Command::ProjectImport` + `--from`（默认 cmake、大小写不敏感）+ `--overwrite`；`main.cpp` 分发
- [x] **1.2 解析器**（4.2）：轻量 CMake 函数调用解析（命令名 + 参数，处理引号/括号嵌套/`#` 注释/`[[...]]`）+ 有限 `set()` 变量表 + 单层 `${VAR}` 展开（§3.2「变量展开策略」）

### 阶段二：核心映射 + best-effort

- [x] **2.1 核心映射**（4.3）：§3.2 表格实现（project / executable / library / sources / includes / definitions / options / link）
- [x] **2.2 依赖 best-effort**（4.4）：`find_package` 包名映射（共享别名表）→ `[depends]` 注释条目 + i18n TODO
- [x] **2.3 条件编译 best-effort**（4.5）：平台条件求值 / 跳过未求值块 + TODO 注释

### 阶段三：拒绝逻辑 + 生成物

- [x] **3.1 拒绝逻辑**（4.6）：非声明式写法检测 + **事务性中止** + i18n 报错 + 迁移文档指引
- [x] **3.2 生成物**（4.7）：头部注释（来源/版本/时间戳/实验性警告）+ 覆盖拒绝

### 阶段四：i18n + 测试

- [x] **4.1 i18n**（4.8）：`import_*`/`help_*` key（en/zh），`check_i18n.py` 通过
- [x] **4.2 测试**（4.9）：单测（解析器引号/嵌套/注释、核心映射、`find_package` 注释、拒绝中止、条件编译、覆盖拒绝）+ 集成（样例 CMake 项目导入校验）；全量零回归

### 阶段五：文档/教程

- [ ] **5.1 文档**（4.10）：迁移文档 `docs/en|zh/migrate-from-cmake.md`（新建）+ `cli.md`/`config_file.md` 补充 + 教程 11-import-cmake + README/CHANGES.md

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **有限单层变量展开** | 仅捕获顶层、条件块外的常量 `set()`（值不含 `${}`/`$<...>`/`$ENV{}`）；变量被任何命令修改即剔除；只在映射命令参数上单层 `${VAR}` 展开；结果仍含 `${}`/`$<...>` → 该参数不可解析 |
| **best-effort vs 明确拒绝** | `${VAR}`/`find_package`/无法求值的条件块 → best-effort 跳过 + `# TODO` 注释，**不中止**；生成器表达式/自定义命令/函数宏/`pkg_check_modules`/`execute_process` → **事务性中止** + i18n 报错 |
| **共享包名别名表** | dev.2 导出（CMake target ← ezmk 包）与 dev.4 导入（find_package 名 → ezmk 包）共用同一张常见包别名表（`src/pkg_alias.hpp`），避免两处漂移 |
| **`--overwrite` 默认拒绝** | 项目根已存在 `ezmk.toml` 时默认拒绝（保护手写配置），显式传 flag 才覆盖 |
| **事务性中止** | 拒绝时绝不产出半成品 `ezmk.toml`，报错 + 指向迁移文档（用 Lua `[hooks]` 复刻自定义步骤） |
| **实验性定位** | `--from cmake` 稳定前语义可调整；破坏性调整集中于 2.0.0 窗口，不纳入 API 稳定性承诺 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `ezmk project import` | 纯新增命令 | 不影响既有命令 |
| `--from` 仅支持 cmake | 新 flag | 未来扩展其他格式，向后兼容 |
| 已存在 `ezmk.toml` | 默认拒绝 | `--overwrite` 显式覆盖 |
| 拒绝非标准写法 | 转换中止 | 不产出半成品；报错 + 迁移文档指引 |
| 生成物为快照 | 与 CMakeLists.txt 单向 | 转换后以 `ezmk.toml` 为准，头部注释说明 |
| 实验性功能 | 输出提示 | 稳定前语义可调整，不纳入 API 稳定性承诺 |

---

## 6 延后项

- **完整 CMake 变量求值**：作用域（`function()`/`macro()` 局部变量）、`CACHE` 变量、`if` 条件求值、`$ENV{}`、嵌套递归求值——超出轻量解析器范围，本版不求值；不可解析处 best-effort + TODO。
- **多 target 项目**：仅导入第一个/主 target，其余 warning + 注释说明；多 target 建议分项目。
- **非标准写法支持**：生成器表达式、自定义命令、函数/宏、`pkg_check_modules`/`execute_process` 等本版明确拒绝，用 Lua `[hooks]` 手动迁移。
- **dev.5 catch2 v3 / dev.6 构建耗时统计**：1.2.0 系列后续子版本，独立并行。
