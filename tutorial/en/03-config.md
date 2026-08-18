# 3. Understanding `ezmk.toml`

`ezmk project new` generates this configuration:

```toml
[project]
name = "hello"
type = "executable"
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra"]
default_profile = "debug"
include_dirs = ["include"]

[compile.profile.debug]
flags = ["-g", "-O0"]
msvc_flags = ["/Zi", "/Od"]

[compile.profile.release]
flags = ["-O2", "-DNDEBUG"]
msvc_flags = ["/O2", "/DNDEBUG"]

[link]
flags = []
link_dirs = []
system_target = []

[depends]
lib = []

# [test]                     # 启用项目测试：取消注释后运行 `ezmk test`
# framework = "catch2"       # "catch2" | "ezmk"（内置框架）
# dirs = ["test"]
# default_profile = "debug"  # 1.2.0-dev.12+：测试默认 profile
# include_dirs = ["test/helpers"]   # 测试专属 -I（1.2.0-dev.12+）
# link_targets = ["pthread"]        # 测试专属 -l（1.2.0-dev.12+）
```

The template ships built-in `debug` / `release` profiles with
`default_profile = "debug"` — a plain `ezmk build` is debuggable out of the box,
and `ezmk build --profile release` switches to the optimized build. The `# [test]`
lines at the end are a **commented-out example section** (1.2.1+): uncomment and
fill them in to use `ezmk test`; pure comments have zero parse impact.

## `[project]`

| Key | Meaning |
|---|---|
| `name` | Project / output binary name |
| `type` | `executable` · `static` · `shared` · `utils` |
| `version` | **Required**. SemVer recommended |
| `language` | `<lang><version>`, e.g. `C++17`, `C11`, `C++20` (default `C++17`) |

- `executable` → a runnable program.
- `static` / `shared` → a library (`.a` / `.so`/`.dll`) other projects can depend on.
- `utils` → a Lua tool package (see [chapter 8](08-utils.md)).

## `[compile]`

| Key | Meaning | Default |
|---|---|---|
| `flags` | Compiler flags (GCC/Clang) | — |
| `msvc_flags` | Extra flags when using MSVC | — |
| `include_dirs` | Header search dirs | `["include"]` |
| `src_dirs` | Source dirs to scan | `["src"]` |
| `ezmk_macros` | Define built-in `EZMK_*` macros | `true` |

Semantic macros go in `[compile.macros]`:

```toml
[compile.macros]
APP_NAME = "hello"     # → -DAPP_NAME="hello"
MAX_USERS = 100        # → -DMAX_USERS=100
DEBUG = true           # → -DDEBUG
```

## `[link]`

| Key | Meaning |
|---|---|
| `flags` | Linker flags |
| `link_dirs` | Library search dirs (`-L`) |
| `system_target` | System libraries to link (e.g. `["pthread", "m"]`) |

## `[depends]`

```toml
[depends]
lib  = ["fmt"]      # hard dependency — missing → build error
want = ["spdlog"]   # optional — used if installed, skipped otherwise
```

`lib` packages must be installed (see [chapter 6](06-packages.md)); `want` packages are
optional.

For the complete specification — including profiles and hooks — see
[`docs/en/config_file.md`](../../docs/en/config_file.md).

Next: [Incremental builds & caching →](04-cache.md)
