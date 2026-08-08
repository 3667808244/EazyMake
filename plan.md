# EazyMake 1.1.1 执行计划

> 正式发布计划：**1.1.1**（拦截 `ezmk utils cc` + 构建后自动生成 compile_commands）。详细设计：[`plans/1.1.1/1.1.1.md`](plans/1.1.1/1.1.1.md)。
>
> **1.1.x 稳定线补丁**：不新增命令、不弃用、不触碰 1.1.0 稳定 API；`ezmk project cc` 正式命令与 `ezmk utils cc` 弃用推迟至 [1.2.0](plans/1.2.0/1.2.0.md)。
>
> **⛔ 发布门槛**：实现完整 + API 兼容 + 全量测试零回归，三项同时满足才可发布（Gate 定义见 [1.1.0-pre.3](plans/1.1.0/1.1.0-pre.3.md#-发布门槛release-gate)）。本补丁发布后，`plan.md` 转回 [1.2.0](plans/1.2.0/) 执行计划。

---

## 1 背景

`compile_commands.json` 由外置 Lua 工具（`ezmk utils cc`，`ezmk-official-utils/utils/cc.lua`）生成，与 `ezmk build` 真实命令存在**系统性 drift**：缺 `@link:` 解析目录、依赖包 `extra_includes`、`-stdlib`、`-fPIC`、确定性标志、宏 `-D`、`--profile`、MSVC 翻译等。clangd 等索引看到的是"假设命令"，而非真实构建命令。

1.1.1 做两件事——**不改命令名、不新增命令、不弃用**：

1. **拦截 `ezmk utils cc`**：修复既有命令的输出正确性。在 CLI 分发处拦截 `utils cc`，改由内置 C++ 逻辑（复用真实命令构造）服务。
2. **`[compile].compile_commands` 自动生成**：新增配置项，`ezmk build` 成功后自动生成 compile_commands.json（对标 CMake `CMAKE_EXPORT_COMPILE_COMMANDS`）。

同时把编译命令构造重构为**单一事实源**（`build_compile_args()`），为 1.2.0 的 `ezmk project cc` 正式命令铺路（命令与弃用均在 1.2.0 交付）。

---

## 2 目标

| # | 目标 | 类别 | 优先级 | 状态 |
|---|------|------|--------|------|
| 1 | 重构 `compile_one_source()`，提取 `build_compile_args()`（参数向量）+ `join_shell_args()`（shell 转义），构建路径行为不变 | 功能 | P0 | 待实现 |
| 2 | 新建 `compile_db` 模块 `generate_compile_db()`：复用真实命令构造，`arguments` 数组输出，相对项目根、顺序稳定、原子写 | 功能 | P0 | 待实现 |
| 3 | `Command::Utils` 分发处拦截 `utils_name == "cc"`，改由内置 C++ 生成器服务，`-o`/`--help`/`--profile` 兼容 | 功能 | P0 | 待实现 |
| 4 | 新增 `[compile].compile_commands`（bool，默认 false）：`ezmk build` 成功后自动生成，输出与本次构建完全一致 | 功能 | P1 | 待实现 |
| 5 | `ezmk utils cc` 输出与 `ezmk build` 完全一致（drift-free），零外部包依赖 | 质量 | P0 | 待实现 |
| 6 | 单测 + 集成覆盖拦截 / 自动生成 / 配置解析；全量测试零回归（基线 546/2617） | 质量 | P0 | 待实现 |
| 7 | 文档与发布收口：`docs/config_file.md` / `CHANGES.md`（§3.6 口径）+ 版本号 1.1.1 | 文档 | P1 | 待实现 |

---

## 3 执行阶段

### 阶段一：命令构造重构（核心）

**设计**：[`plans/1.1.1/1.1.1.md`](plans/1.1.1/1.1.1.md) §3.1

- [ ] 提取 `build_compile_args()` / `join_shell_args()` 到 `include/ezmk/cache.hpp`；`compile_one_source()` 改为调用
- [ ] `build_compile_args()` 完整包含：真实编译器（`detected_compiler` 优先）/ `-std=` / stdlib 标志 / 确定性标志 / `-fPIC` / `[compile].flags`（MSVC 翻译 + `msvc_flags`）/ `-I`（`@link:` + 依赖包 `extra_includes`）/ 宏 `-D` / `-MMD -MF` / `-c <src> -o <obj>`
- [ ] 验收：重构前后命令逐字节一致，`bash build.sh test` 零回归

### 阶段二：compile_db 模块

**设计**：[`plans/1.1.1/1.1.1.md`](plans/1.1.1/1.1.1.md) §3.2

- [ ] 新建 `src/compile_db.cpp` + `include/ezmk/compile_db.hpp`：`generate_compile_db(project_root, config, profile, output_path)`
- [ ] 复用 `build::prepare_build_state()` → `build_compile_args()`；**最小规范化**（剔除 `-MMD`/`-MF <path>`/`-frandom-seed=<x>`/`/showIncludes`）
- [ ] `file` 相对项目根 `rel_src`、`directory` 项目根绝对路径、按 `rel_src` 字典序、temp → rename 原子写；无源文件 → warning + 成功

### 阶段三：拦截 `ezmk utils cc`

**设计**：[`plans/1.1.1/1.1.1.md`](plans/1.1.1/1.1.1.md) §3.3

- [ ] `main.cpp` `Command::Utils` 分发处（`find_utils_script()` **之前**）拦截 `cc`，改由内置 C++ 生成器服务
- [ ] 兼容行为：默认输出 `<proj_root>/compile_commands.json`；`-o <path>` 相对项目根解析；`-h/--help` 复用原工具文案；`--profile <name>` 透传
- [ ] 不调用 `find_utils_script("cc")`；`cc.lua` 不再执行（包内文件保留，1.2.0 弃用）

### 阶段四：`[compile].compile_commands` 自动生成

**设计**：[`plans/1.1.1/1.1.1.md`](plans/1.1.1/1.1.1.md) §3.4

- [ ] `CompileSection::compile_commands` 字段 + `src/config.cpp` 解析（默认 false）
- [ ] `build.cpp` 链接成功后 hook：用**构建期 `CompileInput`**（含 profile 应用后标志、依赖包 include、`@link:` 结果），输出与本次构建逐条一致
- [ ] 失败 warning 不阻塞构建；`--compile-commands` flag 允许临时开启而不改配置

### 阶段五：i18n / 测试 / 文档 / 发布

- [ ] i18n：`compile_db_*`/`help_*`/自动生成 warning key（en/zh），`check_i18n.py` 三向一致通过
- [ ] 测试：`test_compile_db.cpp`（新建）+ `test_config.cpp`（字段解析）+ `test_integration.cpp`（拦截 / 自动生成）；全量零回归
- [ ] `docs/en|zh/config_file.md` 补 `[compile].compile_commands` 字段说明
- [ ] `CHANGES.md` 新增 `1.1.1` 条目——**口径：优化 compile_commands.json 生成算法 + 新增配置项；不宣布拦截 / 弃用**（`cc` 工具弃用声明在 1.2.0）
- [ ] 版本号预置：`include/ezmk/version.hpp` / `build.sh` 默认版本为 `1.1.1`
- [ ] **发布门槛预检**：① 计划清单全部完成或明确收口；② API 兼容（无破坏性变更）；③ 全量测试零回归
- [ ] 打 `v1.1.1` tag 触发 `release.yml`（沿用 1.1.0 流程）；发布后 `plan.md` 转回 [1.2.0](plans/1.2.0/) 执行计划

> 门槛未满足即停止，禁止带着未收口项打 tag。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **拦截而非改名** | 修复既有 `ezmk utils cc` 输出正确性，不改命令名/参数，不破坏现有脚本；1.1.1 是纯修复补丁 |
| **命令构造单一事实源** | 编译命令拼装收敛到 `build_compile_args()`，构建与 compile-db 共用，从结构上消除 drift |
| **拦截改由 C++ 服务** | `cc.lua` 不再执行；输出由 C++ 生成，等价且更准；脚本保留待 1.2.0 弃用 |
| **自动生成用构建期状态** | 直接复用本次构建 `CompileInput`，保证零 drift 且无额外探测开销 |
| **最小规范化** | 仅剔除 clangd 不需要的 `-MMD`/`-MF`/`-frandom-seed`/`/showIncludes`，其余保留 |
| **`arguments` 数组输出** | 而非 `command` 字符串，避免 shell 双重转义歧义，clangd 推荐格式 |
| **发布口径** | 1.1.1 对外仅表述"优化生成算法 + 新配置项"；拦截是内部实现细节，`cc` 工具不声明废弃（弃用在 1.2.0） |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `ezmk utils cc` 输出修正 | 现有命令行为 | **行为修复**：输出从"假设命令"变为真实命令，clangd 索引更准；命令名/参数不变 |
| `compile_one_source()` 重构 | 内部 | 行为不变；逐字节一致 + 全量测试零回归验收 |
| 拦截跳过 `cc.lua` | 工具脚本不再执行 | 输出由 C++ 生成，等价且更准；脚本保留（1.2.0 弃用） |
| 新增 `[compile].compile_commands` | 可选字段 | 默认 false，旧配置行为不变 |
| 无新命令 / 无弃用 | 无 | 纯修复补丁，不触碰 1.1.0 稳定 API |

---

## 6 延后项

- **`ezmk project cc` 正式命令**：1.2.0 交付（`Command::ProjectCc`，复用 `build_compile_args()` + `compile_db`）
- **`ezmk utils cc` 弃用**：1.2.0 声明（`use ezmk project cc instead`），2.0.0 移除
- **`ezmk-official-utils` `cc` 工具标注**：1.2.0 加 `@deprecated` 标注 + 包版本提升（1.1.0 → 1.2.0）

---

## 7 版本路线图

```
1.1.0 (正式版) ──→ 1.1.1 (拦截 ezmk utils cc) 🔄 本计划
                 → 1.2.0-dev.1 (`ezmk project cc` 命令)
                 → 1.2.0-dev.2 (CMakeLists.txt 导出)
                 → 1.2.0-dev.3 (默认模板内建 Profile)
                 → 1.2.0-dev.4 (CMake 项目导入, 实验性)
                 → 1.2.0 (正式发布)
                 → 2.0.0 (未来) —— 破坏性变更窗口
```
