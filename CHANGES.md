# Changelog

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
