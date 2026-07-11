# 3. Understanding `ezmk.toml`

`ezmk project new` generates this configuration:

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
[`docs/config_file.md`](../docs/config_file.md).

Next: [Incremental builds & caching →](04-cache.md)
