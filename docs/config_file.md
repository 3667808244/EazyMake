# 配置文件`ezmk.toml`

---

## `project` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `name` | string | 是 | — | 项目名称 |
| `type` | string | 否 | `"executable"` | 项目类型：`"executable"` / `"static"` / `"shared"` / `"utils"` |
| `version` | string | 是 | — | 项目版本，建议 SemVer 格式（如 `"0.1.0"`） |
| `language` | string | 否 | `"C++17"` | 语言标准，格式为 `<语言><版本>`，如 `"C++17"`、`"C11"` |

### `type` 取值说明

| 值 | 产物 | 是否要求 main.cpp |
|----|------|-------------------|
| `"executable"` | 可执行文件 | 是 |
| `"static"` | 静态库 `lib<name>.a` | 否 |
| `"shared"` | 动态库 `lib<name>.dll` / `lib<name>.so` | 否 |
| `"utils"` | 工具包（无编译产物，或 `lib<name>.a`） | 否 |

### `language` 格式

格式为 `<语言><版本>`：
- 语言：`C` 或 `C++`
- 版本：`89` / `99` / `11` / `14` / `17` / `20` / `23`

常见取值：`C++17`（默认）、`C++20`、`C11`、`C99`。

以 `C++` 开头 → 使用 `g++` 编译；以 `C` 开头（非 `C++`）→ 使用 `gcc` 编译。

---

## `compile` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 编译时添加的标志（如 `-Wall`、`-O2`）。GCC/Clang 格式，MSVC 下自动翻译 |
| `msvc_flags` | string[] | 否 | `[]` | **0.2.1+** MSVC 专用编译标志（不翻译，仅 MSVC 工具链时追加） |
| `include_dirs` | string[] | 否 | `["include"]` | 编译时 `-I` 搜索路径，相对于项目根目录 |
| `src_dirs` | string[] | 否 | `["src"]` | **0.2.2+** 源文件搜索目录，支持多个目录（如 `["src", "lib"]`）。显式设为 `[]` 会报错 |
| `ezmk_macros` | bool | 否 | `true` | **0.2.2+** 是否自动注入 `EZMK_*` 标准预处理器宏（`EZMK`/`EZMK_VERSION`/`EZMK_PROJECT_*`） |

注：旧字段 `include_dir`（单数）已废弃，解析时若遇到可自动映射到 `include_dirs`。

### `[compile.macros]` 节（0.2.2+）

独立子节，定义预处理器宏。比在 `flags` 中用 `-D` 更语义化，且 MSVC 下自动翻译为 `/D`。

| TOML 写法 | 生成的标志（GCC） | 生成的标志（MSVC） | 说明 |
|-----------|-------------------|---------------------|------|
| `DEBUG = ""` | `-DDEBUG` | `/DDEBUG` | 空值 → 仅定义符号 |
| `VERSION = "0.2.0"` | `-DVERSION="0.2.0"` | `/DVERSION="0.2.0"` | 字符串值 → key=value |
| `MAX_SIZE = 4096` | `-DMAX_SIZE=4096` | `/DMAX_SIZE=4096` | 整数值 → 不加引号 |
| `ENABLED = true` | `-DENABLED=1` | `/DENABLED=1` | 布尔 true → 1 |
| `ENABLED = false` | （不生成） | （不生成） | 布尔 false → 跳过 |

- key 必须是合法 C 标识符（`[A-Za-z_][A-Za-z0-9_]*`），非法时报错
- 宏的生效顺序：`ezmk_macros`（标准宏）→ `flags` 中的 `-D` → `[compile.macros]` → want.lib 缺失宏。后者覆盖前者同名定义

### 标准预定义宏（`ezmk_macros = true` 时）

| 宏名 | 类型 | 示例值 | 说明 |
|------|------|--------|------|
| `EZMK` | 整数 | `1` | 始终定义为 `1`，标识构建系统为 EazyMake |
| `EZMK_VERSION` | 字符串 | `"0.2.2"` | EazyMake 自身版本号 |
| `EZMK_PROJECT_NAME` | 字符串 | `"myapp"` | `[project].name` |
| `EZMK_PROJECT_VERSION` | 字符串 | `"1.0.0"` | `[project].version` |
| `EZMK_PROJECT_TYPE` | 字符串 | `"executable"` | `[project].type` |
| `EZMK_LANG` | 字符串 | `"C++17"` | `[project].language` |

设置 `ezmk_macros = false` 可完全禁用标准宏注入。

---

## `link` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 链接时添加的标志 |
| `msvc_flags` | string[] | 否 | `[]` | **0.2.1+** MSVC 专用链接标志（不翻译，仅 MSVC 工具链时追加） |
| `link_dirs` | string[] | 否 | `[]` | 链接时 `-L` 搜索路径，相对于项目根目录 |
| `system_target` | string[] | 否 | `[]` | 需要链接的系统库（如 `"pthread"`、`"m"`） |

---

## `depends` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `lib` | string[] | 否 | `[]` | 硬性依赖库名列表。缺失 → 构建失败 |
| `want` | string[] | 否 | `[]` | **0.2.2+** 可选依赖库名列表。缺失 → warn + 定义 `EZMK_LIB_MISS_<NAME>` 宏，不阻断构建 |

同一包名同时出现在 `lib` 和 `want` 中时，`lib` 优先（作为硬性依赖）并 warn 冗余配置。

`want` 包名到宏名的转换规则：
- 大写转换
- `-` / `.` / 空格 → `_`
- 去除其他特殊字符
- 示例：`sqlite3` → `EZMK_LIB_MISS_SQLITE3`，`boost-filesystem` → `EZMK_LIB_MISS_BOOST_FILESYSTEM`

---

## `compile.profile.<name>` 节（0.2.3+）

通过 `--profile <name>` 激活的构建配置。profile 名称必须是字母数字（支持 `-` 和 `_`），不允许空格。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 追加到 `[compile].flags` 之后的编译标志 |
| `msvc_flags` | string[] | 否 | `[]` | 追加到 `[compile].msvc_flags` 之后的 MSVC 专用标志 |
| `macros` | table | 否 | `{}` | 合并到 `[compile.macros]` 的宏定义，同名 key 覆盖 |

合并规则：
- `flags` / `msvc_flags`：profile 标志**追加**到基础标志之后（GCC/Clang 行为：后面的覆盖前面的）
- `macros`：合并到基础宏表，**profile 的 key 覆盖同名基础 key**

示例：

```toml
[compile.profile.debug]
flags = ["-g", "-O0"]
msvc_flags = ["/Zi", "/Od"]

[compile.profile.debug.macros]
DEBUG = "1"

[compile.profile.release]
flags = ["-O3", "-DNDEBUG"]
msvc_flags = ["/O2", "/DNDEBUG"]
```

Profile **不会**自动应用——用户必须显式传 `--profile <name>`。

---

## `link.profile.<name>` 节（0.2.3+）

与 `compile.profile` 对应的链接阶段配置，通过同一个 `--profile <name>` 激活。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 追加到 `[link].flags` 之后的链接标志 |
| `msvc_flags` | string[] | 否 | `[]` | 追加到 `[link].msvc_flags` 之后的 MSVC 专用链接标志 |

合并规则与 compile profile 相同：profile 标志追加到基础标志之后。

示例：

```toml
[link.profile.debug]
flags = []

[link.profile.release]
flags = ["-flto"]
```

---

## `hooks` 节（0.2.3+）

构建生命周期钩子——在编译/链接的关键节点执行 Lua 脚本。钩子脚本接收 `ctx` 表（`ctx.output`、`ctx.project_root`、`ctx.profile`），运行在沙箱 Lua 环境中。脚本不存在 → warn + 跳过（非致命）。仅对用户项目生效，包编译时不执行。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `pre_build` | string | 否 | `""` | 编译开始前执行的 Lua 脚本路径（相对于项目根目录） |
| `post_build` | string | 否 | `""` | 链接成功后执行的 Lua 脚本路径 |
| `on_failure` | string | 否 | `""` | 编译或链接失败时执行的 Lua 脚本路径 |

示例：

```toml
[hooks]
pre_build = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
```

详见 `docs/utils.md`（Lua API 参考）和 CLAUDE.md（构建钩子实现细节）。

---

## `utils` 节 [version >= 0.2.0]

仅当 `[project].type = "utils"` 时有效。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `tools` | string[] | 是 | — | 本包提供的工具名列表，每个对应 `utils/<name>.lua` |

示例：

```toml
[utils]
tools = ["cc", "compile-commands"]
```

详见 `docs/utils.md`。

---

## 完整示例

### 普通项目（0.2.3）

```toml
[project]
name = "myapp"
type = "executable"
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
msvc_flags = []
include_dirs = ["include"]
src_dirs = ["src", "lib"]
ezmk_macros = true

[compile.macros]
DEBUG = ""
VERSION = "0.1.0"
MAX_CONNECTIONS = 64

[compile.profile.debug]
flags = ["-g", "-O0"]

[compile.profile.debug.macros]
DEBUG = "1"

[compile.profile.release]
flags = ["-O3", "-DNDEBUG"]

[link]
flags = []
msvc_flags = []
link_dirs = []
system_target = ["pthread"]

[link.profile.release]
flags = ["-flto"]

[depends]
lib = ["foo", "bar"]
want = ["sqlite3", "zlib"]

[hooks]
pre_build = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
```

### utils 工具包

```toml
[project]
name = "ezmk-cc"
version = "0.1.0"
type = "utils"

[utils]
tools = ["cc", "compile-commands"]
```
