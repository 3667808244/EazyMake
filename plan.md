# EazyMake 1.1.0 执行计划

> 详细设计：[`plans/release/1.1.0-dev.1.md`](plans/release/1.1.0-dev.1.md)

---

## 1 背景

1.0.0 完成了文档整理与翻译补全，EazyMake 作为首个正式版发布。但经过代码审查与使用场景分析，识别出三个影响实际使用的工程缺陷：

1. **MSVC 包编译问题（高）**：`compile_package()` 硬编码 GCC 工具链（永远输出 `.a`、永远调用 `ar`），MSVC 用户无法从源码编译安装包；`index.toml` 平台映射缺少 MSVC 条目
2. **编译确定性问题（中）**：`__DATE__`/`__TIME__` 嵌入、调试信息绝对路径、编译器版本未追踪，导致构建产物不可复现
3. **构建产物不易安装（中）**：无 `install` 命令、无 `[install]` 配置节，用户需手动复制文件

此外，0.9.8 计划中提出的 `header_only = true` 支持尚未实现，加重了 MSVC 包的编译压力。

---

## 2 目标

1. **MSVC 包编译** — `compile_package()` 支持 MSVC 工具链（`lib.exe` → `.lib`），`index.toml` 平台映射支持 MSVC 预编译二进制
2. **Header-Only 包支持** — 实现 0.9.8 遗留的 `header_only = true` 字段，跳过编译步骤
3. **确定性构建** — 支持 `SOURCE_DATE_EPOCH` + GCC `-ffile-prefix-map` + MSVC `/Brepro`，`record.json` 记录编译器版本，`ezmk.lock` 锁定依赖版本与内容哈希
4. **`ezmk project install`** — 新增安装命令，支持 `[install]` 配置节，构建产物一键安装到指定前缀

---

## 3 执行阶段

### 阶段一：Header-Only 包支持（前置条件，成本最低）

> 此设计取自 0.9.8 的 §3.3.0，header-only 包跳过编译可减少 MSVC 包编译压力。

- [x] **`config.hpp`**：`ProjectSection` 或独立 `PackageConfig` 新增 `header_only` 字段
- [x] **`config.cpp`**：解析 `pkg.toml` 中的 `header_only = true`（默认 `false`）
- [x] **`pkg.cpp` `install()`**：`header_only = true` → 跳过编译+归档，仅复制 `include/` 头文件到安装目录
- [x] **`pkg.cpp` `info()`**：header-only 包标注 `Type: header-only`
- [x] **i18n**：`i18n_keys.def` 新增 key（`pkg_header_only`、`pkg_installing_header_only` 等），`en.json` / `zh.json` 添加翻译
- [x] `build.sh` 编译通过 + 全量测试通过

### 阶段二：MSVC 包编译（`compile_package()` 工具链感知）

**2.1 归档阶段 MSVC 分支**：

- [x] **`toolchain.hpp`**：`Toolchain` 新增 `version` 字段（`std::string`）
- [x] **`toolchain.cpp`**：`detect_toolchain()` 填充 `version`（运行 `g++ --version` / `cl` 捕获首行输出）
- [x] **`pkg.cpp` `compile_package()`**：新增 `Toolchain` 参数，内部根据 `tc.family` 选择：
  - MSVC：`lib.exe /OUT:"<lib_tmp>" <obj_files...>` → `.lib`
  - GCC/Clang：`ar rcs "<lib_tmp>" <obj_files...>` → `.a`（现有逻辑）
  - 原子写入：沿用 temp → rename 模式
- [x] **`pkg.cpp` `install()`**：传递当前工具链到 `compile_package()`

**2.2 `index.toml` 平台映射扩展**：

- [x] **`repo.cpp`**：平台映射解析适配 `os_arch_toolchain` 三元组格式（工具链标签：`gcc` / `msvc` / `clang`）
- [x] **Fallback 逻辑**：优先匹配三元组 `{os}_{arch}_{toolchain}` → 旧格式 `{os}_{arch}`（映射到 GCC）→ 都不存在则报错
- [x] **向后兼容**：旧格式 `"windows_x86_64"` 映射到 `"windows_x86_64_gcc"`

**2.3 i18n**：

- [x] `i18n_keys.def` 新增 key（`pkg_lib_creating`、`pkg_lib_creating_msvc` 等），`en.json` / `zh.json` 添加翻译
- [x] `build.sh` 编译通过 + 全量测试通过

### 阶段三：确定性构建 + Lockfile

**3.1 确定性编译 flags**：

- [x] **`config.hpp`**：`CompileSection` 新增 `deterministic`（默认 `false`）+ `source_date_epoch`（可选 `uint64_t`）
- [x] **`config.cpp`**：解析 `compile.deterministic` + `compile.source_date_epoch`
- [x] **`cache.cpp` `compile_one_source()`**：`deterministic = true` 时注入：
  - GCC/Clang：`-ffile-prefix-map=<root>=.` + `-frandom-seed=<src_filename>` + 设置 `SOURCE_DATE_EPOCH` 环境变量
  - MSVC：`/Brepro` + 设置 `SOURCE_DATE_EPOCH` 环境变量
- [x] **`build.cpp` `prepare_build_state()`**：`SOURCE_DATE_EPOCH` 取值优先级：环境变量 → `ezmk.toml` 配置 → git HEAD commit 时间戳 → `ezmk.toml` 修改时间（fallback）

**3.2 `record.json` v1 → v2**：

- [x] **`cache.hpp`**：`CacheRecord` 新增 `compiler_version` + `deterministic` 字段
- [x] **`cache.cpp`**：加载 `record.json` 后检查 `compiler_version` 变化 → 全量清空 record 条目
- [x] **`cache.cpp` `compile_options_signature()`**：纳入 `deterministic` 标志（开关确定性构建 → 全量重编译）

**3.3 Lockfile（`ezmk.lock`）**：

- [x] **新建 `include/ezmk/lockfile.hpp` + `src/lockfile.cpp`**：
  - `LockedPackage` / `Lockfile` 数据结构（含 `sha256`、`platform`、`toolchain` 等字段）
  - `load()` / `save()` / `verify()` / `depends_changed()` API（TOML 格式）
- [x] **`config.hpp`**：新增 `LockedPackage` / `Lockfile` 结构体
- [x] **`pkg.cpp` `install()`**：成功后调用 `lockfile::save()` 生成/更新 `ezmk.lock`（整个传递闭包）
- [x] **`pkg.cpp` `update()`**：成功后更新 `ezmk.lock` 中对应条目
- [x] **`cli.cpp`**：`InstallOptions` 新增 `locked` / `no_lock` 字段 → `pkg.cpp` 实现：
  - `--locked`：仅使用 lockfile 安装，不重新解析，不一致则报错退出
  - `--no-lock`：不生成/更新 lockfile（一次性安装）
- [x] **`build.cpp` `prepare_build_state()`**：调用 `lockfile::verify()` 做完整性校验
  - `deterministic = true` 时 lockfile 缺失或校验失败 → **error**（非零 exit code）
  - `deterministic = false` 时 lockfile 缺失仅跳过校验，不影响构建
- [x] **`cache.cpp` `compile_options_signature()`**：`deterministic = true` 时纳入 lockfile 内容 SHA-256

**3.4 i18n**：

- [x] `i18n_keys.def` 新增 key（`build_deterministic`、`cache_compiler_changed`、`lock_*` 系列 ~8 个），`en.json` / `zh.json` 添加翻译
- [x] `build.sh` 编译通过 + 全量测试通过

### 阶段四：`ezmk project install` 命令

- [x] **`config.hpp`**：新增 `InstallSection`（`prefix`、`bindir`、`libdir`、`includedir`、`sharedir`）
- [x] **`config.cpp`**：解析 `[install]` 配置节（`prefix` 默认：Unix `$HOME/.local`，Windows `%LOCALAPPDATA%\ezmk`）
- [x] **`cli.hpp`**：新增 `InstallOptions` 结构体（`prefix`、`dry_run`、`no_headers`、`no_data`）
- [x] **`cli.cpp`**：新增 `parse_install()` 解析器 + `cmd_project_install()` 枚举值 + 简写 `pi`
- [x] **新建 `src/install.cpp`**（或 `build.cpp` 中新增 `install_project()`）：
  - 流程：构建（若未构建）→ 解析 `[install]` → 创建目标目录 → 复制产物 → 输出摘要
  - 布局：`executable` → `<bindir>/`；`static` → `<libdir>/`；`shared` → `<bindir>/`（DLL）+ `<libdir>/`（导入库）；头文件 → `<includedir>/<name>/`
- [x] **`main.cpp`**：分发 `ProjectInstall` → 调用安装函数
- [x] **`--dry-run`**：仅显示将安装什么，不实际写入；**`--no-headers`** / **`--no-data`**：跳过对应步骤
- [x] **i18n**：`i18n_keys.def` 新增 key（`install_*` 系列 ~12 个），`en.json` / `zh.json` 添加翻译
- [x] `build.sh` 编译通过 + 全量测试通过

### 阶段五：集成测试与校验

- [x] 创建 MSVC 包编译测试（Windows + MSVC 环境下 `pkg install` 从源码编译）
- [x] 创建 header-only 包安装测试（编译跳过 + 头文件正确复制）
- [x] 创建确定性构建验证测试（连续两次构建产生字节级相同的产物）
- [x] 创建 lockfile 生成/校验/`--locked` 模式测试
- [x] 创建 `ezmk.lock` 与 `deterministic = true` 联动测试（缺失 lockfile → error）
- [x] 创建 lockfile 依赖变更检测测试（`ezmk.toml` 变了但 lockfile 未更新 → warn/error）
- [x] 创建 `project install` 端到端测试（static / executable / shared 三种类型）
- [x] `build.sh` 编译通过 + 全量测试通过
- [x] 检查编译无新增警告
- [x] 更新 `CHANGES.md`

---

## 4 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `compile_package()` 新增 `Toolchain` 参数 | 所有调用方需更新 | 调用方仅限于 `pkg.cpp` 内部（`install()` / `update()`），影响面小 |
| `index.toml` 平台键扩展为三元组 | 旧格式 `"windows_x86_64"` 继续工作 | 解析时 fallback：三元组 → 二元组（映射到 GCC） |
| `record.json` 格式 v1 → v2 | 旧缓存失效 | `version` 字段控制：v1 记录全量清空后重建 |
| `compile.deterministic = true` 改变输出 | 旧缓存失效（签名变化） | 预期行为，仅新配置触发 |
| 新增 `ezmk.lock` | 没有 lockfile 的项目行为不变 | `deterministic = false` 时 lockfile 缺失仅跳过校验 |
| `ezmk pkg install --locked` | 新 flag，默认不启用 | 向后兼容；旧脚本不加 `--locked` 行为不变 |
| 新增 `[install]` 配置节 | 不声明 `[install]` 的项目行为不变 | 纯增量 |
| `header_only = true` 跳过编译 | 旧包（未声明）行为不变 | 纯增量 |

---

## 5 延后项（1.2.0+）

- MSVC 预编译包的上传到默认仓库（本版本实现工具链能力，上传是运维操作）
- 共享库（DLL/SO）的安装后运行时搜索路径（RPATH / `PATH` 配置）
- `ezmk project uninstall` 卸载命令
- Lockfile 跨平台合并策略（`ezmk.lock.d/` 按平台拆分）
- `ezmk pkg verify` 独立校验命令
