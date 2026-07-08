# Changelog

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
