# 1.2.x — 正式版 + 补丁发布系列

> 1.2.x 系列 = **1.2.0 正式版**（dev.1 ~ dev.12 + pre.1/pre.2，2026-08-17 发布 ✅）+ **1.2.1 补丁**（按项目类型差异化模板生成，✅ 2026-08-18 发布，tag `v1.2.1`）+ **1.2.2 补丁**（教程分类重组，✅ 2026-08-18 完成）+ **1.2.3 补丁**（`ezmk example` 命令组，✅ 2026-08-18 完成）。
>
> 本目录为 1.2.x 全部文档的**平铺结构**（无子文件夹）：`1.2.0*.md` 为正式版系列（dev/pre/正式版），`1.2.1.md` 为补丁。

## 1.2.0 — 正式版发布系列

> 1.2.0 为 1.1.0（经 1.1.1 补丁）之后的次版本，主题：**工具链互操作 + 开箱工程化**。**已于 2026-08-17 正式发布 ✅**（tag `v1.2.0`，GitHub Release + Homebrew tap + winget PR + pacman PKGBUILD）。

> **前置补丁**：dev.1 的**核心重构**（`build_compile_args()` + `compile_db` 模块 + 拦截 `ezmk utils cc` + `[compile].compile_commands` 自动生成）已移入 **[1.1.1](../1.1.x/1.1.1.md)**（1.1.x 稳定线补丁）。dev.1 本身保留，内容调整为仅覆盖 `ezmk project cc` 命令交付 + `ezmk utils cc` 弃用。

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.2.0-dev.1](1.2.0-dev.1.md) | `ezmk project cc` 命令 | 新增内置命令（基于 1.1.1 的 `build_compile_args()`/`compile_db`）；`ezmk utils cc` 弃用 | **已完成**（2026-08-11） |
| [1.2.0-dev.2](1.2.0-dev.2.md) | CMakeLists.txt 导出 | `ezmk project export cmake` 命令；project/compile/link/deps 全映射；默认拒绝覆盖；`--resolve` 具体路径模式 | **已完成**（2026-08-11） |
| [1.2.0-dev.3](1.2.0-dev.3.md) | 默认模板内建 Debug/Release Profile | `ezmk project new` 模板内置 `[compile.profile.debug/release]`；基准收敛为警告-only | **已完成**（2026-08-12） |
| [1.2.0-dev.4](1.2.0-dev.4.md) | CMake 项目导入（实验性） | `ezmk project import --from cmake`；标准命令映射 + `find_package` best-effort + 非标准写法拒绝 | **已完成**（2026-08-13） |
| [1.2.0-dev.5](1.2.0-dev.5.md) | catch2 v3 测试主程序兼容 | `ezmk test` test_main 生成改 v3 兼容（显式 `main` + `Catch::Session().run()`）；v2 vendor 路径不回归 | **已完成**（2026-08-14） |
| [1.2.0-dev.6](1.2.0-dev.6.md) | 各源文件构建耗时统计 | `ezmk build` 并行编译路径 per-file 耗时明细（`-v` 全量 / 慢构建自动 top-N）；零配置、不新增 flag | **已完成**（2026-08-14） |
| [1.2.0-dev.7](1.2.0-dev.7.md) | 本地包源 + 项目定位 | `pkg install <dir>` 从文件夹安装；`ezmk.toml` 向上查找（5 层） | **已完成**（2026-08-15） |
| [1.2.0-dev.8](1.2.0-dev.8.md) | CMake 导出钩子运行时（dev.2 延伸） | 独立无黑白名单运行时 `ezmk-lua`；export 对 `[hooks]` 生成 `add_custom_command`（pre/post） | **已完成**（2026-08-15） |
| [1.2.0-dev.9](1.2.0-dev.9.md) | 包构建配置收敛（dev.7 延伸） | 包 `[compile].src_dirs` / `include_dirs` 生效：`collect_sources` 复用 + include 去重 + utils 门控对齐 + `pkg info` 增显 | **已完成**（2026-08-15） |
| [1.2.0-dev.10](1.2.0-dev.10.md) | 平台标识符扩展（工具链/ABI） | `lib<name>.<os>-<arch>[-<compiler>][-<abi>]` 命名 + 4 级匹配 + 降级 ABI 警告 + 可选 `precompiled_strict` | **已完成**（2026-08-15） |
| [1.2.0-dev.11](1.2.0-dev.11.md) | 代码质量审查与改进 | 基于 dev.10 后代码的全库审查（6 模块并行，68 条问题）：run_tests 收口/钩子安全/编码修复/校验前移/死代码清理 | **已完成**（2026-08-15） |
| [1.2.0-dev.12](1.2.0-dev.12.md) | 测试配置收口（dev.3 延伸） | `[test].default_profile` + `ezmk test --profile`（复用 compile/link profile 表）+ 测试专属 `include_dirs`/`link_targets`；`[test].flags` 弃用（warn，2.0.0 移除） | **已完成**（2026-08-15） |
| [1.2.0-pre.2](1.2.0-pre.2.md) | README 整理与高级特性触达 | README 重组 + 高级特性教程（semver/lockfile/第三方仓库/预编译共包）+ 预编译包 ABI 警告加强 | **已完成**（2026-08-17） |
| [1.2.0-pre.1](1.2.0-pre.1.md) | pacman 分发（发布流水线） | `publish/` 重组 + `publish/arch/PKGBUILD`；本机 MSYS2 + 远程 Arch Linux 验证 makepkg | **已完成**（2026-08-17） |
| [1.2.0](1.2.0.md) | 正式发布 | 聚合 dev.1 ~ dev.12 + pre.1/pre.2；前置 1.1.1；发布门槛 + 全量回归；三渠道分发（winget/Homebrew/pacman） | **已发布**（2026-08-17，tag `v1.2.0`） |

### 依赖关系

```
1.1.0 ──→ 1.1.1 (拦截 ezmk utils cc) ──┐
                                        ├──→ 1.2.0 (正式发布) ──→ 1.2.1 (补丁)
1.2.0-dev.1 (`ezmk project cc` 命令) ───┤
1.2.0-dev.2 (CMakeLists.txt 导出) ──────┤
1.2.0-dev.3 (默认模板内建 Profile) ──────┤
1.2.0-dev.4 (CMake 项目导入, 实验性) ────┤
1.2.0-dev.5 (catch2 v3 测试主程序兼容) ──┐
1.2.0-dev.6 (各源文件构建耗时统计) ────────┤
1.2.0-dev.7 (文件夹安装/向上查找) ──────────┤
1.2.0-dev.8 (CMake 导出钩子运行时) ──────────┤
1.2.0-dev.9 (包构建配置收敛) ────────────────┤
1.2.0-dev.10 (平台标识符: 工具链/ABI) ────────┤
1.2.0-dev.11 (代码质量审查, 依赖 dev.10) ──────┤
1.2.0-dev.12 (测试配置收口, 依赖 dev.3+dev.5) ──┤
1.2.0-pre.1 (pacman 分发) ─────────────────┘
1.2.0-pre.2 (README 整理, 依赖 dev.10) ───────┘
```

- **1.1.1 先行**：dev.1 的 `ezmk project cc` 命令依赖 1.1.1 的 `build_compile_args()`/`compile_db`/拦截/自动生成基础。
- dev.1（命令层）/ dev.2 / dev.3 / dev.4 与 1.1.1、以及彼此**相互独立**，可并行。
- dev.2 与 dev.4 互为**反向互补**（导出/导入，单向、非可逆往返），共享包名别名表。
- **dev.8 依赖 dev.2**：`ezmk-lua` + 导出钩子映射是 dev.2 `export cmake` 的收口延伸；与 dev.5/dev.6/dev.7 相互独立。
- **dev.9 依赖 dev.7**：包构建配置收敛（`src_dirs`/`include_dirs`）作用于 dev.7 引入的本地目录安装共用路径（`validate_pkg` + `compile_package`）；与 dev.8 相互独立。
- **dev.10 与 dev.7/dev.8/dev.9 相互独立**：作用于 `precompiled` 包的 `lib/` 选择路径（`select_precompiled_archive`），不碰 `src_dirs`/`include_dirs`。
- **dev.11 依赖 dev.10**：代码质量审查需基于 dev.10 完成后的代码。
- **dev.12 依赖 dev.3 + dev.5**：测试 profile 支持镜像 dev.3 的 `[compile].default_profile` 与 `merge_compile_profile` 机制（复用 compile/link profile 表），作用于 dev.5 改造过的 `run_tests`（catch2 v3 兼容路径）；与 dev.10/dev.11 相互独立。
- **pre.2 依赖 dev.10**：README/教程/package_authoring 声明的 `os-arch[-compiler][-abi]` 最佳实践与 ABI 警告，前提是 dev.10 已实现。
- dev.3 与 dev.1、dev.2 存在**协同**：默认模板内建 profile 后，`ezmk project cc --profile release` 与 `ezmk project export cmake --profile release` 开箱即可对照验证。
- `1.2.0` 正式版依赖 dev.1 ~ dev.12 + pre.1 + pre.2 + 前置 1.1.1 全部完成 + 发布门槛（实现完整 / API 兼容 / 全量测试零回归）。
- **1.2.1 独立补丁**：基于 1.2.0 发布后的代码，只改模板生成（见下节）。

### 跨子版本共享的关注点

- **命令构造单一事实源**（1.1.1 起）：重构后编译命令拼装收敛到 `cache.cpp` 一处，任何新增编译标志自动进入 compile_commands.json。
- **纯增量兼容**：均为新增命令与可选配置字段，不破坏 1.1.0 稳定 API；`ezmk utils cc` 仅弃用、不删除（2.0.0 移除）。
- **i18n**：新增 key 遵循 X-macro（`i18n_keys.def` + 两份 JSON），`check_i18n.py` 三向一致。
- **回归基线**：当前全量测试 775 用例 / 3554 断言（1.2.0 发布后基线），重构与新增命令均不得引入回归。

## 1.2.1 — 补丁（按项目类型差异化模板生成）

1.2.1 为 **1.2.x 稳定线的补丁发布**：修复 `ezmk project new` 对所有类型生成相同 `main.cpp` 的问题——`static`/`shared` 库项目改为生成 `include/<name>.hpp` + `src/<name>.cpp` 库骨架（不再生成无意义的 `main.cpp`），`executable`/`utils` 保持现状。**不新增命令、不弃用、不触碰 1.2.0 稳定 API**。

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.2.1](1.2.1.md) | 按项目类型差异化模板生成 + 默认配置补全 | `project new` 按类型生成：executable → main.cpp；static/shared → `include/<name>.hpp` + `src/<name>.cpp` 库骨架；utils → 不变；namespace 净化（`-`/`.`/空格 → `_`）；默认模板追加注释 `[test]` 示例节 | **已发布**（2026-08-18，tag `v1.2.1`；全量 785/3629 零回归；Homebrew/winget/pacman 已同步） |

## 1.2.2 — 补丁（教程分类重组）

1.2.2 为 **1.2.x 稳定线的文档补丁**：教程 14 章平铺无分类，改为移入分类子目录（`basic/` 入门 · `packages/` 包管理 · `dev/` 开发体验 · `interop/` 工具链互操作），**组内重新编号（每组从 01 起）**，README 索引分组展示；全部既有链接按新旧映射表同步更新（README 高级特性表 / migrate-from-cmake / 教程内部交叉引用），grep 零死链 + 零旧编号残留双验收。**纯文档变更，无代码/API 影响**。

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.2.2](1.2.2.md) | 教程分类重组（子目录迁移 + 组内重新编号） | `tutorial/zh\|en/` 四分类子目录（basic/packages/dev/interop）+ 组内重编号（每组 01 起）+ README 分组索引 + 全仓链接/编号修复（grep 零死链 + 零旧编号残留） | **已完成**（2026-08-18，纯文档；全量 785/3629 零回归） |

## 1.2.3 — 补丁（`ezmk example` 命令组 + 内置示例）

1.2.3 为 **1.2.x 稳定线补丁**：新增顶层命令组 `ezmk example`（`list` / `<name>` / `-o`），内置 6 个示例（hello / greeter / with-packages / with-tests / with-hooks / cmake-interop）。示例内容以**方案B（构建期嵌入资源）**存储：`examples/` 源目录为单一事实源，`scripts/embed_examples.py` 构建期生成 `src/example_data.cpp` 嵌入二进制（对齐 embed_locale/embed_logo 机制）——装好即用、离线可用、与版本同源。生成到 `./<name>/`（同 `project new`）；with-packages 在 CI 正常联网测试（GitHub runner 有完整出站网络）。**纯新增命令组，公共 API 无破坏性变更**。

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.2.3](1.2.3.md) | `ezmk example` 命令组 + 内置示例 | 顶层命令组（list/<name>/-o）+ 6 内置示例 + 构建期嵌入管线（examples/ → example_data.cpp）+ 集成测试/CI 自举验证 + 文档 | **已完成**（2026-08-18，全量 791/3745 零回归） |

## 1.2.4 — 补丁（仓库文件夹包支持）

1.2.4 为 **1.2.x 稳定线补丁**：官方仓库目前只能托管归档包（`file` → `archive_path` → `extract_archive`），而 `pkg install <dir>` 的文件夹安装（dev.7）只作用于用户手传目录、不经过仓库解析。本版打通「仓库托管目录包」：按名安装解析出的路径为目录时复用 `install_from_directory`；`index.toml` 增可选 `type = "dir"` 标注（sha256 语义区分——目录包无归档 hash，跳过校验）；归档包零影响。header-only/源码包可免打包、以 git 目录形式托管。**纯增量，公共 API 无破坏性变更**。

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.2.4](1.2.4.md) | 仓库文件夹包支持 | 按名安装目录分支（复用 dev.7 install_from_directory）+ index `type = "dir"` 标注 + sha256 语义区分 + 集成测试（local 仓库目录包端到端）/归档回归 + 文档 | 待实现 |

## 发布门槛

⛔ 发布前必须同时满足：**实现完整 + API 兼容 + 全量测试零回归**。详细 Gate 定义见 [1.1.0-pre.3](../1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)。

## 与 1.1.x 的关系

- **1.1.1 先行**：为 1.2.0 的 `ezmk project cc` 命令铺路（共享 `build_compile_args()` + `compile_db` + 自动生成基础）。
- 1.2.0 承接：`ezmk project cc` 新命令 + `ezmk utils cc` 弃用（见本目录）；自动生成已在 1.1.1 交付，1.2.0 不重复。
- 后续 1.2.x 补丁（如有）平铺追加到本目录。
