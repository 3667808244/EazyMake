# 3. 认识 `ezmk.toml`

`ezmk project new` 会生成以下配置：

```toml
[project]
name = "hello"
type = "executable"
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]

[link]
flags = []
link_dirs = []
system_target = []

[depends]
lib = []
```

## `[project]`

| 键 | 含义 |
|---|---|
| `name` | 项目 / 输出二进制名称 |
| `type` | `executable` · `static` · `shared` · `utils` |
| `version` | **必填**。推荐使用语义化版本 |
| `language` | `<语言><版本>`，例如 `C++17`、`C11`、`C++20`（默认 `C++17`） |

- `executable` → 可运行的程序。
- `static` / `shared` → 可供其他项目依赖的库（`.a` / `.so`/`.dll`）。
- `utils` → Lua 工具包（参见[第 8 章](08-utils.md)）。

## `[compile]`

| 键 | 含义 | 默认值 |
|---|---|---|
| `flags` | 编译器标志（GCC/Clang） | — |
| `msvc_flags` | 使用 MSVC 时的额外标志 | — |
| `include_dirs` | 头文件搜索目录 | `["include"]` |
| `src_dirs` | 要扫描的源文件目录 | `["src"]` |
| `ezmk_macros` | 是否定义内置 `EZMK_*` 宏 | `true` |

语义化宏放在 `[compile.macros]` 中：

```toml
[compile.macros]
APP_NAME = "hello"     # → -DAPP_NAME="hello"
MAX_USERS = 100        # → -DMAX_USERS=100
DEBUG = true           # → -DDEBUG
```

## `[link]`

| 键 | 含义 |
|---|---|
| `flags` | 链接器标志 |
| `link_dirs` | 库搜索目录（`-L`） |
| `system_target` | 要链接的系统库（例如 `["pthread", "m"]`） |

## `[depends]`

```toml
[depends]
lib  = ["fmt"]      # 硬性依赖 — 缺失则构建报错
want = ["spdlog"]   # 可选依赖 — 已安装则使用，否则跳过
```

`lib` 中的包必须已安装（参见[第 6 章](06-packages.md)）；`want` 中的包是
可选的。

完整的配置规范（包括构建配置与钩子）请参阅
[`docs/config_file.md`](../../docs/zh/config_file.md)。

下一章：[增量构建与缓存 →](04-cache.md)
