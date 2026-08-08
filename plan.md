# EazyMake 1.2.0 执行计划

> 正式发布计划：**1.2.0**（工具链互操作与开箱工程化）。详细设计：[`1.2.0.md`](plans/1.2.0/1.2.0.md)；功能子计划：[dev.1](plans/1.2.0/1.2.0-dev.1.md)（`ezmk project cc` 命令）、[dev.2](plans/1.2.0/1.2.0-dev.2.md)（CMakeLists.txt 导出）、[dev.3](plans/1.2.0/1.2.0-dev.3.md)（默认模板内建 Profile）、[dev.4](plans/1.2.0/1.2.0-dev.4.md)（CMake 项目导入，实验性）；dev.1 的核心重构/拦截由前置补丁 [1.1.1](plans/1.1.1/1.1.1.md) 先行交付。
>
> **⛔ 发布门槛**：实现完整 + API 兼容 + 全量测试零回归，三项同时满足才可发布（Gate 定义见 [1.1.0-pre.3](plans/1.1.0/1.1.0-pre.3.md#-发布门槛release-gate)）。前置 **1.1.1** 已发布（2026-08-08）。

---

## 1 背景

1.1.0 发布后，三个开发体验缺口未闭合：

1. `compile_commands.json` 由外置 Lua 工具（`ezmk utils cc`）生成，命令与 `ezmk build` 真实命令 drift。**重构（`build_compile_args()`）、对 `ezmk utils cc` 的拦截、`[compile].compile_commands` 自动生成已在 1.1.1 落地**；1.2.0 补齐正式命令 `ezmk project cc`。
2. 缺少从 `ezmk.toml` 导出 CMakeLists.txt 的通道，无法与 CMake 生态互操作。
3. `ezmk project new` 默认模板缺少 Debug/Release Profile。

本计划以 **dev.2**、**dev.3** + 直接交付 `ezmk project cc` 命令实现，聚合为 1.2.0 正式发布。均为纯增量，不触碰 1.1.0 起的 API 稳定性承诺。

---

## 2 目标

| # | 目标 | 类别 | 优先级 | 状态 |
|---|------|------|--------|------|
| 1 | dev.1：`ezmk project cc` 内置命令（基于 1.1.1 的 `build_compile_args()` + `compile_db`），`arguments` 数组输出，零外部包依赖 | 功能 | P0 | 待实现 |
| 2 | dev.1：`ezmk utils cc` 保留可用 + 弃用提示（2.0.0 移除） | 兼容 | P1 | 待实现 |
| 3 | dev.2：`ezmk project export cmake` 命令，project/compile/link/deps 全映射，默认拒绝覆盖 | 功能 | P0 | 待实现 |
| 4 | dev.2：依赖 best-effort 映射 + `--resolve` 具体路径模式 | 功能 | P1 | 待实现 |
| 5 | dev.2：进阶映射（profiles / test / install / deterministic） | 功能 | P2 | 延后/可选 |
| 6 | dev.3：`ezmk project new` 默认模板内建 Debug/Release profile（base 去 `-O2`、优化归 profile） | 功能 | P0 | 待实现 |
| 7 | dev.4：`ezmk project import --from cmake`（实验性）——标准命令映射 + `find_package` best-effort + 非标准写法拒绝 | 功能 | P0 | 待实现 |
| 8 | i18n key + en/zh 翻译，`check_i18n.py` 三向一致 | 质量 | P0 | 待实现 |
| 9 | 单测 + 集成测试覆盖新命令与模板；全量测试零回归 | 质量 | P0 | 待实现 |
| 10 | 文档与计划收口（cli.md / utils.md / config_file.md / 迁移文档 / README / CHANGES / plans） | 文档 | P1 | 待实现 |

---

## 3 执行阶段

### 阶段一（dev.1）：`ezmk project cc` 命令

**设计**：[`1.2.0-dev.1.md`](plans/1.2.0/1.2.0-dev.1.md)；前置基础（`build_compile_args()` / `compile_db` / 拦截）由 [`plans/1.1.1/1.1.1.md`](plans/1.1.1/1.1.1.md) 交付

- [x] 前置确认：1.1.1 已发布（`build_compile_args()` / `compile_db` / 拦截 / 自动生成）
- [ ] 命令：`Command::ProjectCc` + `project cc` 子命令 + `-o/--output`、`--profile`；`main.cpp` 分发（拦截逻辑迁移为新命令入口）
- [ ] i18n：`compile_db_*`/`help_*`/弃用提示 key（en/zh）
- [ ] 测试：`test_compile_db.cpp` 单测 + 集成场景（`ezmk project cc` 输出）
- [ ] 兼容：`ezmk utils cc` 转弃用提示（`use ezmk project cc instead`）+ `ezmk-official-utils` cc.lua/README 标注

### 阶段二（dev.2）：CMakeLists.txt 导出

**设计**：[`1.2.0-dev.2.md`](plans/1.2.0/1.2.0-dev.2.md)

- [ ] 命令骨架：`Command::ProjectExport` + `export <target>` 子命令 + `-o`/`--overwrite`/`--profile`/`--resolve`/`--glob`/`--no-glob`；`main.cpp` 分发
- [ ] 项目级映射：`project()` / `add_executable`/`add_library`（utils 跳过 / header_only / precompiled）
- [ ] 编译映射：src glob / include（含 `@link:`）/ 宏（`ezmk_macros` 一致）/ `-std` 拆分 / flags / `msvc_flags` / stdlib
- [ ] 链接映射：link_dirs / system_targets / link flags 拆分
- [ ] 覆盖安全：已有文件拒绝 + `--overwrite`；文件头生成标注
- [ ] P1 依赖映射：常见包别名映射表 + `find_package` 模板 + `--resolve` 路径解析
- [ ] i18n：`export_*`/`help_*` key（en/zh）
- [ ] 测试：`test_export.cpp` 单测 + 集成场景
- [ ] P2 进阶（按需）：profiles / test / install / deterministic 映射

### 阶段三（dev.3）：默认模板内建 Debug/Release Profile

**设计**：[`1.2.0-dev.3.md`](plans/1.2.0/1.2.0-dev.3.md)

- [ ] `write_default_config()` 模板更新：base 去 `-O2`（仅 `["-Wall", "-Wextra"]`）+ 新增 `[compile.profile.debug/release]`（含 `msvc_flags`）
- [ ] 单测：模板内容断言 + `parse_config()` round-trip
- [ ] 集成：`ezmk project new` → 校验模板；`--profile debug/release` 构建成功
- [ ] 文档：`docs/en|zh/config_file.md` + `docs/en|zh/cli.md` + `CHANGES.md`（明确基准行为变更）

### 阶段四（dev.4）：CMake 项目导入

**设计**：[`1.2.0-dev.4.md`](plans/1.2.0/1.2.0-dev.4.md)

- [ ] 命令骨架：`Command::ProjectImport` + `--from`（默认 cmake、大小写不敏感）+ `--overwrite`；`main.cpp` 分发
- [ ] 解析器：轻量 CMake 函数调用解析（引号/括号嵌套/注释/`[[...]]`）
- [ ] 核心映射：project / executable / library / sources / includes / definitions / options / link
- [ ] 依赖 best-effort：`find_package` 包名映射（共享别名表）→ `[depends]` 注释条目 + i18n TODO
- [ ] 条件编译 best-effort：平台条件求值 / 跳过未求值块 + TODO 注释
- [ ] 拒绝逻辑：非声明式写法检测 + 事务性中止 + i18n 报错 + 迁移文档指引
- [ ] 生成物头部注释 + 覆盖拒绝；i18n key；单测 + 集成；迁移文档/教程

### 阶段五：i18n / 文档 / 收尾

- [ ] `check_i18n.py` 三向一致通过
- [ ] `docs/en|zh/cli.md` 命令表新增 `project cc`、`project export cmake`、`project import`
- [ ] `docs/en|zh/utils.md` 标注 cc 工具内化 + 弃用
- [ ] 迁移文档 `docs/en|zh/migrate-from-cmake.md` + 教程（dev.4）
- [ ] `README.md` / `README_ZH.md` 命令速览补充
- [ ] `CHANGES.md` 新增 `1.2.0-dev.1`（`ezmk project cc` + `utils cc` 弃用）、`1.2.0-dev.2`、`1.2.0-dev.3`、`1.2.0-dev.4`、`1.2.0` 条目（`1.1.1` 条目在 1.1.1 发布时记录）

### 阶段六：发布准备

- [ ] 版本号预置：`include/ezmk/version.hpp` / `build.sh` 默认版本为 `1.2.0`
- [ ] 本地全量回归：`bash build.sh test-all` 零回归（基线 565/2727）
- [ ] **发布门槛预检**：① 计划清单全部完成或明确收口；② API 兼容（无破坏性变更）；③ 全量测试零回归
- [ ] 打 `v1.2.0` tag 触发 `release.yml`；产物核验 + 分发渠道（沿用 1.1.0 流程）
- [ ] `plans/README.md` / `plan.md` 状态收口（1.2.0 → 已完成）

> 门槛未满足即停止，禁止带着未收口项打 tag。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **命令面** | `ezmk project cc`；`ezmk project export <target>`（`export` 为 `project` 子命令，预留 make/meson 子目标） |
| **`arguments` 数组输出** | compile_commands.json 用 `arguments` 而非 `command`，避免 shell 双重转义歧义，clangd 推荐格式 |
| **命令构造单一事实源（1.1.1 交付）** | 编译命令拼装收敛到 `build_compile_args()`（1.1.1 重构），构建与 compile-db 共用，从结构上消除 drift；1.2.0 只在其上加新命令 |
| **`ezmk utils cc` 只弃用不删** | 按 API 稳定性承诺，破坏性变更仅 2.0.0；1.2.0 保留工具 + 弃用提示 |
| **导出为单向快照** | `ezmk.toml` 为唯一事实源，`CMakeLists.txt` 重新生成、勿手改；默认拒绝覆盖手写文件 |
| **依赖 best-effort** | 默认便携 `find_package`（内置常见包别名表），`--resolve` 输出具体安装路径（本机可用、不可移植） |
| **默认模板收敛优化** | 新项目模板 base 仅 `["-Wall", "-Wextra"]`，优化级别由 profile 显式决定（debug `-O0` / release `-O2`）；旧项目不受影响 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `ezmk project cc` | 纯新增命令 | 不影响既有命令 |
| 新增 `ezmk project export cmake` | 纯新增命令 | 不影响既有命令 |
| 前置 1.1.1（`build_compile_args()` 重构 + 拦截 + `compile_commands` 自动生成） | 1.2.0 基础 | 1.1.1 先发布；行为已由 1.1.1 回归验证 |
| `ezmk utils cc` 弃用 | 工具仍可用 | 仅弃用提示；2.0.0 移除 |
| `compile_commands.json` 格式 `command`→`arguments` | clangd 用户 | 两者 clangd 均支持，`arguments` 更优 |

---

## 6 延后项

- **dev.2 P2 进阶映射**（profiles / `[test]` / `[install]` / deterministic → CMake）：不作为 1.2.0 阻塞项，按需实现
- **`ezmk utils cc` 移除**：推迟到 2.0.0 破坏性窗口
- **`project export make` / `project export meson`**：`Command::ProjectExport` 预留扩展点，后续版本实现

---

## 7 版本路线图

```
1.1.0 (正式版) ──→ 1.1.1 (拦截 ezmk utils cc) ✅ 前置
                 → 1.2.0-dev.1 (`ezmk project cc` 命令)
                 → 1.2.0-dev.2 (CMakeLists.txt 导出)
                 → 1.2.0-dev.3 (默认模板内建 Profile)
                 → 1.2.0-dev.4 (CMake 项目导入, 实验性)
                 → 1.2.0 (正式发布) 🔄 本计划
                 → 2.0.0 (未来) —— 破坏性变更窗口
```
