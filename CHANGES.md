# Changelog

## 0.9.5.1 (2026-07-17) — 代码重构与质量清理

不新增用户可见功能，专注代码质量：消除重复、修复资源管理、补全测试盲区、移除死代码。

### 重构
- **`build.cpp` — 链接阶段统一**：提取 `execute_link()` 通用函数，`link_phase()` 中 6 个重复块（static/shared/exe × MSVC/GCC）简化为单行调用，消除 ~150 行重复
- **`cache.cpp` — 编译逻辑统一**：`compile_sources()` 改为委托 `compile_one_source()`，消除 ~270 行重复的编译管道代码
- **`file_watcher.cpp` — debounce 统一**：提取 `check_and_flush()` 成员函数，Windows/Linux/macOS 三处 ~20 行重复的 sleep→elapsed→flush 逻辑统一为单行调用
- **`main.cpp` — 仓库更新统一**：提取 `auto_update_repos()`，ProjectBuild/ProjectRun/ProjectWatch 三个分支中的重复 auto-update 代码块统一
- **`config.cpp` — 配置名校验统一**：提取 `is_valid_profile_name()`，`[compile.profile.*]` 和 `[link.profile.*]` 中重复的 profile 名称校验逻辑合并

### 资源管理修复
- `lua_api.cpp`：`g_cached_config` 原始指针 → `std::unique_ptr<config::EzConfig>`（异常安全）
- `file_watcher.cpp`：`OVERLAPPED*` 手动 new/delete → `std::unique_ptr<OVERLAPPED>` 池管理（消除泄漏风险）
- `lua_api.cpp`：全局变量线程安全性假设注释文档化
- `cache.cpp` + `build.cpp`：并行编译 record 只读不变量注释文档化

### 死代码移除
- 移除 `ParsedOptions::count()`（零调用方）
- 移除 `native_path()`（零调用方）

### i18n
- 新增 2 个 i18n key：`toolchain_msvc_detected`、`cache_hit_detail`，含中英双语翻译
- `toolchain.cpp` / `cache.cpp` 硬编码英文字符串 → i18n key

### 测试
- 测试套件：**497 个测试用例，2250 个断言全部通过**（+6 用例，+9 断言）
- **`compare_version()` 完整覆盖**（`test_util.cpp`）：10 个 TEST_CASE，覆盖相等、主/次/补丁差异、缺失段默认 0、预发布标签剥离、构建元数据剥离、宽数字段、长版本号、边界值
- **`extract_archive()` 基础覆盖**（`test_util.cpp`）：不支持格式抛出异常、无效 zip 抛出异常、不存在文件抛出异常
- **`compile_options_signature` 补全**（`test_cache.cpp`）：新增 `msvc_flags` 影响签名、`std_flag` 影响签名两项测试
- **`resolve_dependency_order` 增强**（`test_pkg.cpp`）：验证错误消息包含缺失包名
- **共享测试基础设施**：新建 `test/test_helpers.hpp`，提取 TempDir / CwdGuard / EnvGuard / write_minimal_config 等跨文件复用 fixtures
- `file_watcher.hpp` 平台宏复用 `util.hpp` 的 `EZMK_WIN`/`EZMK_MACOS`/`EZMK_LINUX`，删除重复检测逻辑

---

## 0.9.5 (2026-07-17) — 跨平台体验与质量保障

Windows 原生安装体验、端到端集成测试、三平台冒烟测试准备。1.0.0 之前的质量保障版本。

### 新增
- **PowerShell 安装脚本** (`install.ps1`)：Windows 原生一键安装，对标 `install.sh`。从 GitHub Release 下载预编译 `ezmk.exe`，SHA-256 校验，原子化安装到 `%LOCALAPPDATA%\ezmk\bin`，自动配置用户 PATH + 预注册官方仓库。支持 `-Version` / `-InstallDir` / `-NoPath` / `-DryRun` 参数
- **端到端集成测试** (`test/test_integration.cpp`)：7 个场景、41 个断言，覆盖完整 build pipeline —— 从零创建项目 → 编译 → 运行、增量构建缓存命中、Watch 模式文件变更检测、`ezmk utils cc` 生成 compile_commands.json、项目目录布局验证、CLI 基本命令（version/help）。全部标记 `[integration]` tag，支持按需运行或跳过
- **`build.sh` 测试模式扩展**：新增 `test-all`（单元 + 集成）、`integration`（仅集成测试）目标；test 模式默认跳过 `[integration]` 用例（`~"[integration]"`）；集成测试前自动编译 ezmk 二进制；通过 `EZMK_TEST_BIN` 环境变量传递给测试

### 变更
- **Windows 安装文档**：`README.md` / `README_ZH.md` / `docs/en/cli.md` / `docs/zh/cli.md` 新增 Windows 原生安装章节（`install.ps1` 使用说明 + 参数表）
- **环境变量表扩展** (`docs/en/cli.md` / `docs/zh/cli.md`)：新增 `EZMK_TEST_BIN` 条目

### 测试
- 测试套件：**491 个测试用例，2250 个断言全部通过**（+9 用例，+41 断言）
- 单元测试 482 用例 2209 断言；集成测试 9 用例 41 断言
- Watch 模式测试为时序敏感型（Windows 上可能假阴性），使用 WARN 而非 FAIL

---

## 0.9.4 (2026-07-15) — 文档与质量完善

文档补全与代码质量打磨，不新增核心功能。

### 新增
- **FAQ / 故障排除文档**：`docs/en/faq.md` + `docs/zh/faq.md`，覆盖安装/构建/包管理/配置/跨平台五大类 25+ 条常见问题及排错流程
- **离线场景文档**：FAQ 新增离线使用章节，涵盖本地仓库镜像、手动下载归档、USB/内网共享镜像三种方案
- **Lua API 版本化基础设施**：新增 `EZMK_LUA_API_VERSION` 常量（当前为 1）+ `ezmk.api_version` Lua 字段，脚本可通过 `if ezmk.api_version >= 2 then ... end` 做兼容性判断
- **`util::closest_match()` 模糊匹配函数**：基于 Levenshtein 编辑距离，为未知命令/profile 提供 "did you mean" 建议
- **API 版本化策略文档**：`docs/en/utils.md` + `docs/zh/utils.md` 新增"API 版本化"章节，定义向后兼容策略（仅不兼容变更触发版本号递增；废弃函数保留 ≥2 个 minor 版本后移除）

### 变更
- **错误信息打磨**：修复 `cli.cpp` 空异常消息（`throw std::invalid_argument("")` → i18n 化错误）；未知 profile 和未知命令增加 "did you mean" 模糊匹配建议
- **`std::runtime_error` 审计**：排查 `src/` 中所有裸 `throw std::runtime_error(...)` 位置，面向用户的错误信息改为 i18n 化
- **CHANGES.md 补全**：补全 0.9.0 ~ 0.9.4 版本条目

---

## 0.9.3 (2026-07-14) — 捆绑包迁移

将 7 个捆绑预编译库包迁移至官方仓库，清理主项目冗余文件。

### 变更
- **7 个捆绑包迁移至官方仓库** (`ezmk-repo`)：catch2 (3.6.0), fmt (10.2.1), lua (5.4.7), nlohmann_json (3.11.3), spdlog (1.14.1), sqlite3 (3.46.0), tinyxml2 (11.0.0)
- **逐包标准化**：补全 `version` 字段、TOML 格式更新 (`include_dir` → `include_dirs`)、补 `language` 字段、清理硬编码 `-Wall -O2`
- **仓库侧**：`sources/` 新增 7 个源工程，`packages/` 新增 7 个归档，`index.toml` 含 9 个包条目，`validate.sh` 全部通过
- **主项目清理**：删除 `pkg/` 下 7 个 `.tar.gz` 捆绑归档，`ezmk-cc/` 目录保留（内置工具源码参考）
- **`install.sh` 清理**：移除捆绑包拷贝逻辑（已预注册官方仓库，`pkg install` 自动从仓库拉取）

---

## 0.9.2 (2026-07-13) — 文档多语言

`docs/` 和 `tutorial/` 拆分为 `en/` + `zh/` 双语目录，补齐英文翻译。

### 变更
- **目录重组**：`docs/` → `docs/en/` + `docs/zh/`，`tutorial/` → `tutorial/en/` + `tutorial/zh/`
- **英文翻译补齐**：`cli.md`, `pkg.md`, `repo.md`, `utils.md`, `config_file.md`, `@cache.md`, `@safety.md` 全部提供英文版
- **术语表** (`glossary.md`)：中英双语对照，随新功能扩展更新
- **CI 文件对应检查**：确保 `docs/en/` ↔ `docs/zh/` 一一对应

---

## 0.9.1 (2026-07-12) — 默认仓库创建

创建官方默认仓库，建立包生态基础设施。

### 新增
- **官方默认仓库** (`ezmk-repo`)：GitHub 托管，Gitee 镜像，符合 `docs/repo.md` 结构
- **预注册策略**：`install.sh` 安装时自动将官方仓库注册到用户作用域，用户装完即可按名装包
- **初始示例包**：`hello-lib` (static) + `example-utils` (utils)，含完整源工程
- **打包流程**：`pack.sh` (打包 + SHA-256) + `validate.sh` (校验)，CI 可复现
- **贡献流程文档**：`CONTRIBUTING.md` + `CONTRIBUTING_ZH.md`

---

## 0.9.0 (2026-07-10) — 准备发布正式版

首个面向用户的正式版准备，聚焦"能装上、能看懂、能上手"。

### 新增
- **一键安装脚本** (`install.sh`)：`curl -fsSL <url> | bash` 一键安装，支持 Linux/macOS/MSYS2，幂等可重入，失败即清理
- **文档整理**：`docs/cli.md` 完整 CLI 参考，`docs/@safety.md` 安全性集中化文档
- **README 双语互链**：`README.md` (EN) ↔ `README_ZH.md` (ZH)
- **上手教程** (`tutorial/`)：从零创建项目 → 添加依赖 → 构建运行的分步教程

---

## 0.2.6 (2026-07-11) — 翻译补全与命令行改进

可用性收尾版本，无新增构建/包管理能力，聚焦 i18n 系统性修复与命令行打磨。

### Bug 修复
- **根除 `{???}` 显示 bug**：`ezmk help` / `pkg list` / `repo list` 等命令输出的 `{???}` 占位符消失。根因是 `src/i18n.cpp` 手写的 `key_name()` switch 漏登记了 0.2.3~0.2.5 新增的约 50 个 `I18nKey` 枚举值（枚举 / switch / JSON 三处数据源手动同步时漏了中间一处），而非 JSON 缺翻译
- **POSIX `run_command()` stderr 捕获修复**：用花括号组 `{ cmd ; } 1>out 2>err` 包裹重定向，确保被调命令自身的 fd 重定向（如 `>&2`）不污染 stdout/stderr 捕获（修复 Linux 上暴露的 2 个既有 `test_util` 失败）

### 新增
- **i18n 单一数据源（X-macro）**：新增 `include/ezmk/i18n_keys.def`，`I18nKey` 枚举与 `key_name()` 映射均由它生成，从结构上杜绝三处失配。新增键 = `.def` 加一行 + 两份 JSON 各加一条
- **开发期缺失键审计**：`i18n::init()` 末尾的 `audit_missing_keys()`（仅 `NDEBUG` 未定义时启用），对枚举有键但 JSON 缺翻译的情况逐一告警一次
- **命令简写**：顶层别名在 `cli::parse()` 分发前展开 —— `pn/pb/pr/pc/pw`（project）、`ki/kr/ks/kn/kl/ku`（pkg）、`ra/rr/rl/ru/ri`（repo）、`u/h/v`（utils/help/version）。仅在命令位生效，不进 zsh 补全，仅在帮助页展示
- **全局 `--color=<mode>`**：`always`/`enable`、`auto`/`default`、`never`/`disable`（大小写不敏感）。显式 `always`/`never` 覆盖 `NO_COLOR`，仅 `auto` 尊重之（对齐 git/ls）；`always` 亦尝试开启 Windows VT100

### 变更
- **帮助正文全本地化**：`print_usage()` 每条命令/flag 的说明文字改走 i18n（约 30 个 `help_*` 键），命令用法串保持字面
- **参数校验报错本地化**：`src/cli.cpp` 各 `parse_*_args()` 的硬编码英文 `util::fatal` 替换为 i18n 键（`cli_arg_required` / `cli_too_many_args` / `cli_unknown_subcommand` 等 + `arg_*` 名词键）
- **`repo list` 专属键**：新增 `repo_list_title` / `repo_list_none`，不再复用语义为"已安装包"的 `pkg_list_*`
- **代码卫生**：确认 `src/pkg.cpp` 全局安装确认处注释为正常 `// Safety:`

### 测试
- 测试套件：**476 个测试用例，2180 个断言全部通过**（Windows UCRT64 g++ + Linux Arch g++）
- 新增回归防线：遍历全部 `I18nKey` 枚举，断言 `get(key)` 不以 `{` 开头（直接卡住 `{???}` 类 bug）
- 新增 `[alias]`（26 断言）与 `[color]`（16 断言）用例组

---

## 0.2.5 (2026-07-09) — 生态与安全

### 新增
- **zsh 命令补全**：静态补全脚本 `completions/_ezmk`，覆盖全部命令、子命令与 flag
- **Utils 细粒度权限管理**：`[utils.permissions]` read/write/run 白名单，脚本越权访问被拒；未声明权限的旧包行为不变 + deprecation warning（向后兼容）
- **`ezmk repo info`**：显示仓库名称、作用域、URL、类型、分支、更新时间、缓存路径与包版本列表
- **跨仓库版本选择**：同名包在多个仓库中出现时，按语义化版本比较选取最新
- **仓库本地校验增强**：`index.toml` 中 file 存在性检查与 sha256 格式校验
- **`--auto-update`**：`pkg install` / `search` 前自动 `git pull` 已注册仓库

### 测试
- 测试套件：**测试全部通过**（Windows + Linux）

---

## 0.2.4 (2026-07-08) — 健壮性与完善

### Bug 修复
- **版本比较逻辑统一**：新增 `util::compare_version()`（`src/version.cpp`），替换 `pkg.cpp` 的字符串比较和 `repo.cpp` 的内联数值比较，正确处理 `1.10.0` vs `9.0.0` 等边界
- **Shell 注入风险修复**：`build.cpp` 全部 4 个链接命令构建器 + `cache.cpp` 全部 2 个编译命令构建器，所有路径和标志统一使用 `util::escape_shell_arg()`
- **`/tmp` 硬编码修复**：`run_command()` 中的临时文件改用 `$TMPDIR` 环境变量 + 动态路径拼接，移除硬编码魔数偏移

### 代码质量
- **`build_project()` 重构**：530 行函数拆分为 `BuildState` 结构体 + `prepare_build_state()` + `compile_phase()` + `link_phase()` + `run_hook()` 五个模块，主函数降至 ~20 行编排逻辑
- **CLI 标志解析去重**：`parse_build_flag()` lambda 统一 `build`/`run`/`watch` 三个命令的 `--disable-cache`/`--verbose`/`-j`/`--profile` 解析，减少 ~70 行重复代码
- **帮助文本 i18n**：章节标题全部使用 I18nKey 枚举，支持中英文切换

### 功能补全
- **C23 语言标准支持**：`config.cpp` 语言版本映射已包含 C23（`-std=c23`）
- **`pkg update --all`**：批量更新全部已安装包，自动遍历 → 版本比较 → 安装，输出 `N updated, M up-to-date, K failed` 摘要
- **扩展 GCC→MSVC 标志映射**：新增 17 个标志（`-fno-rtti`→`/GR-`、`-fno-exceptions`→`/EHs-c-`、`-ffast-math`→`/fp:fast`、`-fstack-protector`→`/GS` 等），总计 58 个映射

### 工程规范
- `License` 重命名为 `LICENSE`
- 历史计划文件 checkbox 全部标记为 `[x]`
- 补打 6 个缺失的 git tag（v0.1.6 ~ v0.2.3）

### 测试
- 测试套件：**411 个测试用例，409 通过**（2 个预存 i18n 失败）

---

## 0.2.3 (2026-07-04) — 开发者体验提升

### 新增
- **并行编译 `-j` / `--jobs`**：基于 `ThreadPool`（`include/ezmk/thread_pool.hpp`）的多线程编译，默认自动检测 CPU 核数
- **构建 Profile `--profile`**：`[compile.profile.<name>]` / `[link.profile.<name>]` 预定义配置段，Profile 标志追加到基础标志后
- **Build Hooks `[hooks]`**：`pre_build` / `post_build` / `on_failure` Lua 脚本，在构建生命周期各阶段自动执行
- **Watch 模式 `ezmk project watch`**：跨平台 `FileWatcher`（Windows IOCP / Linux inotify / macOS kqueue），300ms 防抖，配置变更触发全量重建
- **`ezmk pkg list`**：列出全部已安装包（含版本、类型、工具列表）
- **`ezmk pkg update`**：从注册仓库更新指定包到最新版本

### 修复
- `list_sources()` 不再仅扫描 `src/`，跟随 `src_dirs` 配置
- `ezmk-cc` 的 `cc.lua` 不再硬编码 `g++`，改为使用检测到的编译器
- 移除多处裸 `catch(...)`，改为具体异常处理

### 测试
- 测试套件：**333+ 用例，973+ 断言**

---

## 0.2.2 (2026-07-02) — 精细化编译控制

### 新增
- **可选依赖 `[depends].want`**：缺失时 warn + 自动定义 `EZMK_LIB_MISS_<NAME>` 宏，不阻断构建。`lib` 中的硬性依赖仍然缺失即 fatal
- **语义化宏定义 `[compile.macros]`**：独立 TOML 子节管理预处理器宏。支持字符串/整数/布尔值类型，布尔 `false` 自动跳过
- **标准预定义宏 `compile.ezmk_macros`**：默认自动注入 `EZMK` / `EZMK_VERSION` / `EZMK_PROJECT_NAME` / `EZMK_PROJECT_VERSION` / `EZMK_PROJECT_TYPE` / `EZMK_LANG` 六个标准宏，用户可在 `[compile.macros]` 中覆盖
- **多源目录 `compile.src_dirs`**：支持 `["src", "lib", "vendor"]` 等多目录源文件扫描。文件名去重（同文件跨目录 warn），`main.cpp` 跨目录查找，默认 `["src"]` 向后兼容
- **4 个新 API** (`build.hpp`)：`macros_to_flags()`, `generate_ezmk_macros()`, `want_to_macro_name()`, `collect_sources()`

### 变更
- **`include/ezmk/config.hpp`**：`CompileSection` 新增 `src_dirs` / `macros` / `ezmk_macros`；`DependsSection` 新增 `want`
- **`src/config.cpp`**：解析四个新字段；宏名合法性校验；布尔/整数/字符串值类型处理
- **`src/build.cpp`**：有效标志合并（ezmk_macros → flags → macros → want）；多目录源文件收集；可选依赖包扫描
- **`src/pkg.cpp`**：`install()` / `resolve_dependency_order()` / `compile_package()` / `info()` 中 want 依赖处理
- **`src/cache.cpp`**：缓存签名包含 `msvc_flags` + `std_flag` + `extra_includes`；补全 `check_cache` 重载链

### 修复
- GCC 编译命令优先使用运行时检测的编译器（`detected_compiler`），而非硬编码 `"g++"`
- 缓存签名修复：全局签名与逐文件签名一致（修复有依赖包时缓存永久全量重编译）
- `collect_sources` 去重改用 `filename()`（含扩展名），避免 `util.cpp` 和 `util.c` 误判为重复
- 合并 `.ezmk/pkg/` 双重扫描为单次遍历

### 测试
- 测试套件：**333 个测试用例, 973 个断言全部通过**（+71 用例, +175 断言）

---

## 0.2.1 (2026-06-30) — MSVC 支持

### 新增
- **`Toolchain` 抽象层**（`include/ezmk/toolchain.hpp` + `src/toolchain.cpp`）：`CompilerFamily::Gcc/Clang/Msvc` 枚举，自动检测可用工具链
- **GCC→MSVC 标志翻译层**：`translate_compile_flags()` / `translate_link_flags()`，覆盖常用标志（`-Wall`→`/W4`、`-O2`→`/O2`、`-g`→`/Zi` 等）
- **MSVC 依赖解析**：`parse_show_includes()` 解析 `/showIncludes` 输出替代 `-MMD`
- **`cl.exe` / `link.exe` / `lib.exe`** 完整编译/链接/归档流程
- **`vcvars64.bat` 环境自动加载**：捕获环境变量 map，一次加载全流程复用
- **`ezmk.toml` 扩展 `msvc_flags`**：MSVC 专用编译/链接标志，GCC 模式下被忽略

### 变更
- **`src/cache.cpp`**：MSVC 编译命令生成（`/utf-8`、`/MD` 默认标志）
- **`src/build.cpp`**：MSVC 链接命令构建器（EXE/DLL/LIB）；产物路径适配（`.obj` / `.lib` / `.exe`）
- **`include/ezmk/config.hpp`**：`CompileSection` / `LinkSection` 添加 `msvc_flags`

### 测试
- 测试套件：**262 个测试用例, 886 个断言全部通过**（+88 断言）

---

## 0.2.0 (2026-06-28) — Lua 工具链

### 新增
- **嵌入式 Lua 5.4.7**：静态链接进 `ezmk` 二进制（`include/vendor/lua/` + `src/vendor/lua/`，32 源文件）
- **`include/ezmk/lua_api.hpp`** + **`src/lua_api.cpp`**：22 个 C++ → Lua 绑定函数
- **ezmk Lua API**：
  - 项目信息（5）：`project_root`, `project_name`, `project_type`, `project_config`, `build_dir`
  - 编译选项（4）：`compile_flags`, `include_dirs`, `link_flags`, `link_dirs`
  - 文件系统（4）：`list_sources`, `file_exists`, `file_read`, `file_write`
  - 进程执行（2）：`run` → `{exit_code,stdout,stderr}`, `run_capture`
  - 日志输出（3）：`info`, `warn`, `error`
  - 路径工具（3）：`pkg_dir`, `temp_dir`, `cache_dir`
  - JSON（2）：`json_encode`, `json_decode`（基于 nlohmann/json，含 Lua table ↔ JSON 双向转换）
- **Sandbox 安全模型**：每次调用独立环境表（脚本间零污染）、`io`/`os` 库编译期移除、`file_write` 拒绝项目根目录外写入
- **`find_utils_script()`**（`util.cpp`）：按项目 → 用户 → 全局 → 开发作用域查找 Lua 工具脚本
- **内置工具包 `pkg/ezmk-cc/`**：`ezmk utils cc` 生成 clangd 兼容的 `compile_commands.json`
- **`test/test_lua.cpp`**：61 个测试用例、212 个断言

### 变更
- **`main.cpp`**：`Command::Utils` 从占位实现改为完整 Lua 脚本执行；进程启动/退出时 `lua::init()`/`lua::shutdown()`
- **`src/vendor/lua/linit.c`**：移除 `io` 和 `os` 库注册（安全沙箱）
- **`build.sh`**：Lua 源文件加入编译；新增 `-DLUA_COMPAT_5_3` 和 `-I include/vendor/lua/`
- **`src/config.cpp`**：`type` 字段校验（`executable`/`static`/`shared`/`utils`）
- **`src/pkg.cpp`**：`validate_pkg()` 对 `type = "utils"` 包放宽 `include/`/`src/` 检查；无 `src/` 的 utils 包跳过编译
- **i18n**：新增 5 个 Lua 相关 I18nKey（`lua_init_failed`, `lua_error`, `lua_api_type_error`, `lua_api_arg_count`, `utils_not_found`）

### 测试
- 测试套件：**262 个测试用例, 798 个断言全部通过**（+61 用例, +212 断言）

---

## 0.1.8 (2026-06-24) — 跨平台支持与编译器探测

### 新增
- **`detect_compiler()`**（`src/build.cpp`）：多级编译器自动探测（`$CXX`/`$CC` → 平台候选列表 → 安装指引）
- **平台宏完善**（`util.hpp`）：`EZMK_MACOS` / `EZMK_LINUX` / `EZMK_WIN` 三平台互斥宏
- **`EZMK_OBJ_SUFFIX` 修正**：MinGW 上从 `.obj` 改为 `.o`（MinGW g++ 实际产出）
- Apple Clang 检测提示（macOS 上 `g++` 可能是 clang 别名）

### 变更
- **`build.cpp`**：`find_compiler()` 重构为调用 `detect_compiler()`；编译器验证逻辑简化
- **`config.hpp`**：`LanguageInfo` 新增 `detected_compiler` 字段
- **`build.sh`**：macOS 平台 `-static` 处理、`$CXX`/`$CC` 环境变量支持
- **`test/test_build.cpp`**：21 个编译器探测测试用例

---

## 0.1.7 (2026-06-22) — 基本国际化（i18n）

### 新增
- **i18n 模块** (`include/ezmk/i18n.hpp`, `src/i18n.cpp`)：编译期 JSON 嵌入 + I18nKey 枚举方案
- **85 个 I18nKey**，覆盖 build / cache / pkg / repo / project / run / editor / general 全部模块
- **locale/en.json** + **locale/zh.json**：英文和中文资源文件，占位符使用 `{key}` 格式
- **`scripts/embed_locale.py`**：将 `locale/*.json` 编译期嵌入二进制（零外部文件依赖）
- **`include/ezmk/version.hpp`**：由 `build.sh` 自动生成版本号头文件
- **`test/test_i18n.cpp`**：19 个测试用例覆盖 key 一致性 / fmt 替换 / 语言检测 / fallback 行为

### 变更
- **日志系统** (`util.hpp` / `util.cpp`)：新增 `info/warn/error/fatal(I18nKey, args)` 重载，翻译后再着色输出
- **main.cpp**：启动时调用 `i18n::init()` 自动检测语言；`version` 输出使用 i18n
- **build.cpp**：15 处字符串迁移（building / compiling / linking / build_success 等）
- **cache.cpp**：11 处字符串迁移（cache_hit / cache_miss / compilation_failed 等）
- **pkg.cpp**：31 处字符串迁移（installing / downloading / verifying / sha256 等）
- **repo.cpp**：10 处字符串迁移（cloning / pulling / repo_added 等）
- **project.cpp**：6 处字符串迁移（creating_project / init_git 等）
- **util.cpp**：7 处迁移（I18nKey 日志重载 + no_editor / opening_editor / editor_error）
- **build.sh**：编译前自动运行 `embed_locale.py`，生成 `version.hpp`，支持 `EZMK_VERSION` 环境变量
- **test_main.cpp**：全局 `i18n::init("en")` 初始化，测试输出使用英文

### 语言检测
- 优先级：`EZMK_LANG` 环境变量 > 系统语言 (Windows `GetUserDefaultLocaleName` / Linux `$LANG`) > 默认 `en`
- 支持 `zh-CN` → `zh`、`en-US` → `en` 自动规范化
- 运行时 `locale/<lang>.json` 文件 > 嵌入式数据 > 硬编码英文 fallback

### 测试
- 测试套件：**192 个测试用例, 573 个断言全部通过**
- i18n 专项测试：19 个 TEST_CASE, 118 个断言
