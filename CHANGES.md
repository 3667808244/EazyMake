# Changelog

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
