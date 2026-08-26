# EazyMake 1.4.0-dev.1 执行计划

> **状态：执行中**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.0-dev.1.md**](plans/1.4.x/1.4.0-dev.1.md)。本计划为 1.4.0 第一个开发子版本，主题为**调试配置生成**——`ezmk project export vscode` 一键生成 `.vscode/` 三件套（launch/tasks/settings），**per-platform 调试器**（Windows/MSVC → `cppvsdbg`、Linux/GCC → `cppdbg`(gdb)、macOS/Clang → `lldb`），与 `[compile.profile.*]` 联动。
>
> **范围边界**：**明确不做**——launch 参数透传（`--`）、多配置（test 目标调试）延后到 1.4.0 后续或 1.5.x 评估；`project cc`/`export cmake` 本身不改（settings 仅复用 compile_commands.json 产物）；`--resolve`/`--glob` 语义不引入 vscode 目标（vscode 目标是纯新增子形态，cmake 目标全部现有 flag 不变）。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 911 用例 / 5302 断言，1.3.6 后实测）。

---

## 1 背景

- `ezmk` 生成构建/运行环境（`ezmk build`/`run` 及 `project export cmake`），但**不生成调试器配置**——用户用 VS Code 调试需手写 `launch.json`/`tasks.json`/`settings.json`，且必须自行拼出 ezmk 的编译参数（include 路径、宏、`-std`、依赖包注入），极易出错。
- 本版提供 `ezmk project export vscode` 一键生成三件套，per-platform 调试器，与 `[compile.profile.*]` 联动；settings 优先走 `compile_commands.json`（单一数据源），无则回退 `C_Cpp.default.*`。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `ezmk project export vscode`：生成 `.vscode/launch.json` + `tasks.json` + `settings.json`（存在则询问/`--overwrite`） | P0 |
| 2 | **launch.json**：per-platform 调试器（Windows/MSVC → `cppvsdbg`；Linux/GCC → `cppdbg`(gdb)；macOS/Clang → `lldb`）；program 指向 `build/<name>`；`preLaunchTask` 触发构建 | P0 |
| 3 | **tasks.json**：`ezmk build [--profile <name>]` 任务 + 默认组；依赖注入的 include 路径/宏随构建自动生效（不硬编码） | P0 |
| 4 | **settings.json**：clangd/c_cpp 的 include 路径 + 宏（优先 `compile_commands.json`，回退 `C_Cpp.default.includePath`/`defines`） | P0 |
| 5 | profile 联动：`--profile <name>` 时 launch/tasks 指向该 profile 的产物与编译参数 | P1 |
| 6 | 测试：三件套生成 + JSON 合法性（nlohmann 反序列化断言）+ 关键字段（调试器/program/preLaunchTask/include 回退）+ 覆盖保护 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：CLI（4.1）

- [ ] **1.1 CLI 子形态**：`project export` 接受 `vscode` 目标（`cli.cpp`，与 `cmake` 并列校验）；`--overwrite`/`--profile` 复用现有解析（`--resolve`/`--glob`/`-o` 对 vscode 目标报错或忽略——按设计文档落点对齐）
- [ ] **1.2 参数结构**：`ExportOptions` 不变（target 已在 `ProjectExportOptions`），vscode 目标校验 + `main.cpp` 分发到 `export_vscode()`；help 文本补 vscode 形态
- [ ] **1.3 单测**：CLI 解析（`export vscode` 合法 / 未知 target 仍 fatal / `--overwrite`/`--profile` 透传）

### 阶段二：生成器（4.2）

- [ ] **2.1 `export_vscode()`**（`export.cpp` + `export.hpp` 声明）：`.vscode/` 三文件生成，统一 nlohmann `dump(2)` 序列化（禁止手拼 JSON）
- [ ] **2.2 launch.json**：per-platform 调试器表（`toolchain::detect_toolchain` 检测 family → `cppvsdbg`/`cppdbg`+`miDebuggerPath`/`lldb`）；`program` = `build/<name>`（Windows 加 `.exe`）；`preLaunchTask` = `ezmk-build`；`cwd` = `${workspaceFolder}`；`externalConsole` = false
- [ ] **2.3 tasks.json**：`version 2.0.0`；`ezmk-build` shell 任务（`ezmk build`，`--profile <name>` 仅 `--profile` 时）+ `group.build.isDefault`
- [ ] **2.4 settings.json**：compile_commands.json 存在（项目根或 `[compile].compile_commands`）→ `clangd.arguments: ["--compile-commands-dir=${workspaceFolder}"]`；否则回退 `C_Cpp.default.includePath`（`<proj>/include` + `[compile].include_dirs` 解析 + 依赖包 include）+ `C_Cpp.default.defines`（复用 `generate_ezmk_macros` + `[compile].macros`）
- [ ] **2.5 profile 联动**：`--profile` 时 launch 的 `preLaunchTask` 仍是 `ezmk-build`，tasks 带 `--profile`；profile 为空时回退 `[compile].default_profile`（与 export cmake 语义对齐）
- [ ] **2.6 覆盖保护**：三文件逐个检查存在 → 拒绝（`export_exists_refuse` 语义），`--overwrite` 覆盖；原子写（temp → rename）

### 阶段三：测试（4.3）

- [ ] **3.1 单测**（`test/test_export.cpp` 追加）：`build_vscode_files()` 纯函数——三件套 JSON 反序列化（nlohmann parse 不抛）+ 关键字段断言（`type`/`program`/`preLaunchTask`/`group`/include 回退）
- [ ] **3.2 per-platform 表**：模拟 `Toolchain` family → 调试器类型断言（cppvsdbg/cppdbg+miDebuggerPath/lldb）；`build/<name>`/`build/<name>.exe` 平台后缀
- [ ] **3.3 覆盖保护**：`export_vscode()` 写盘——存在文件拒绝（`--overwrite` 后替换）、三文件全量写出
- [ ] **3.4 集成**：真实临时项目 `ezmk project export vscode` → `.vscode/` 三文件存在且 JSON 合法

### 阶段四：文档 + 收口（4.4）

- [ ] **4.1 cli.md（en/zh）**：`project export vscode` 节（命令/flag/三件套说明/调试器表/坑位：cppvsdbg 依赖 VS 扩展）
- [ ] **4.2 CHANGES.md**：1.4.0-dev.1 条目（新增 / 行为变更 / 文档 / 已知限制）
- [ ] **4.3 全量零回归**：`bash build.sh test-all`（基线 911/5302）
- [ ] **4.4 文档收口**：plan.md 勾选；`plans/1.4.x/README.md` 状态更新；发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| vscode 作为 export 新子形态 | `export` 现有子形态（cmake）不变；`--overwrite`/`--profile` 语义完全复用 |
| 统一 nlohmann 序列化 | 三文件全部经 `nlohmann::json` 构造 + `dump(2)`，杜绝手拼 JSON 转义坑（项目名/路径含引号反斜杠） |
| settings 单一数据源 | 优先 `compile_commands.json`（`[compile].compile_commands` 或 `project cc` 产物）；回退路径仅作无 DB 时的兜底 |
| tasks 不硬编码编译参数 | 依赖包 include/宏由构建侧注入，tasks 只跑 `ezmk build`——与 1.3.0-dev.5「总是先构建」语义对齐 |
| per-platform 调试器表 | `toolchain::detect_toolchain` 检测 family 定调试器；`cppdbg` 时 `miDebuggerPath` 按检测工具链（gdb/lldb）注入 |
| `preLaunchTask` 键名常量 | launch/tasks 由同一生成器产出，键名 `ezmk-build` 常量对齐，测试断言交叉引用 |
| 覆盖保护对齐 cmake | 三文件逐个 `export_exists_refuse`（无 `--overwrite` 时 fatal），原子写 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `export vscode` 新子形态 | 纯新增 | `export cmake` 全部现有 flag/行为不变 |
| `.vscode/` 输出 | 纯新增目录 | 询问覆盖保护；不触碰既有产物 |
| 新 i18n key（若有） | 纯新增 | `i18n_keys.def` + en/zh JSON 三处同步 |
| 公共 API | **无破坏性变更** | 纯增量（新命令 + 新生成器） |

## 6 延后项（明确收口）

- **launch 参数透传（`--`）**、**多配置（test 目标调试）**：1.4.0 后续或 1.5.x 评估。
- **`project cc`/`export cmake` 改造**：settings 仅复用 compile_commands 产物，不改 `cc` 本身。
- **`--resolve`/`--glob`/`-o` 对 vscode 目标**：不引入（设计文档未定义）；vscode 输出固定 `.vscode/`。
- **2.0.0**：保持破坏性变更窗口，与本版解耦。
