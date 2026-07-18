# Configuration File `ezmk.toml`

---

## `project` Section

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `name` | string | Yes | — | Project name |
| `type` | string | No | `"executable"` | Project type: `"executable"` / `"static"` / `"shared"` / `"utils"` |
| `version` | string | Yes | — | Project version; SemVer format recommended (e.g. `"0.1.0"`) |
| `language` | string | No | `"C++17"` | Language standard, format is `<language><version>`, e.g. `"C++17"`, `"C11"` |

### `type` Values

| Value | Output | Requires main.cpp? |
|-------|--------|--------------------|
| `"executable"` | Executable file | Yes |
| `"static"` | Static library `lib<name>.a` | No |
| `"shared"` | Dynamic library `lib<name>.dll` / `lib<name>.so` | No |
| `"utils"` | Utils package (no compile output, or `lib<name>.a`) | No |

### `language` Format

Format is `<language><version>`:
- Language: `C` or `C++`
- Version: `89` / `99` / `11` / `14` / `17` / `20` / `23`

Common values: `C++17` (default), `C++20`, `C11`, `C99`.

Starts with `C++` → compile with `g++`; starts with `C` (not `C++`) → compile with `gcc`.

---

## `compile` Section

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `flags` | string[] | No | `[]` | Compile flags (e.g. `-Wall`, `-O2`). GCC/Clang format, auto-translated under MSVC |
| `msvc_flags` | string[] | No | `[]` | **0.2.1+** MSVC-specific compile flags (not translated, only appended when using MSVC toolchain) |
| `include_dirs` | string[] | No | `["include"]` | `-I` search paths during compilation, relative to project root |
| `src_dirs` | string[] | No | `["src"]` | **0.2.2+** Source file search directories; supports multiple directories (e.g. `["src", "lib"]`). Explicitly setting to `[]` causes an error |
| `ezmk_macros` | bool | No | `true` | **0.2.2+** Whether to auto-inject `EZMK_*` standard preprocessor macros (`EZMK`/`EZMK_VERSION`/`EZMK_PROJECT_*`) |

Note: Legacy field `include_dir` (singular) is deprecated; if encountered during parsing, it is automatically mapped to `include_dirs`.

### `[compile.macros]` Subsection (0.2.2+)

A standalone subsection that defines preprocessor macros. More semantic than using `-D` in `flags`, and auto-translated to `/D` under MSVC.

| TOML syntax | Generated flag(s) (GCC) | Generated flag(s) (MSVC) | Description |
|-------------|-------------------------|---------------------------|-------------|
| `DEBUG = ""` | `-DDEBUG` | `/DDEBUG` | Empty value → define symbol only |
| `VERSION = "0.2.0"` | `-DVERSION="0.2.0"` | `/DVERSION="0.2.0"` | String value → key=value |
| `MAX_SIZE = 4096` | `-DMAX_SIZE=4096` | `/DMAX_SIZE=4096` | Integer value → no quotes |
| `ENABLED = true` | `-DENABLED=1` | `/DENABLED=1` | Boolean true → 1 |
| `ENABLED = false` | (not generated) | (not generated) | Boolean false → skip |

- Key must be a valid C identifier (`[A-Za-z_][A-Za-z0-9_]*`); error on invalid
- Macro resolution order: `ezmk_macros` (standard macros) → `-D` in `flags` → `[compile.macros]` → want.lib missing macros. Later definitions override earlier ones with the same name

### Standard Predefined Macros (when `ezmk_macros = true`)

| Macro name | Type | Example value | Description |
|------------|------|---------------|-------------|
| `EZMK` | integer | `1` | Always defined as `1`; identifies the build system as EazyMake |
| `EZMK_VERSION` | string | `"0.2.2"` | EazyMake's own version number |
| `EZMK_PROJECT_NAME` | string | `"myapp"` | `[project].name` |
| `EZMK_PROJECT_VERSION` | string | `"1.0.0"` | `[project].version` |
| `EZMK_PROJECT_TYPE` | string | `"executable"` | `[project].type` |
| `EZMK_LANG` | string | `"C++17"` | `[project].language` |

Setting `ezmk_macros = false` fully disables standard macro injection.

---

## `link` Section

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `flags` | string[] | No | `[]` | Link flags |
| `msvc_flags` | string[] | No | `[]` | **0.2.1+** MSVC-specific link flags (not translated, only appended when using MSVC toolchain) |
| `link_dirs` | string[] | No | `[]` | `-L` search paths during linking, relative to project root |
| `system_target` | string[] | No | `[]` | System libraries to link (e.g. `"pthread"`, `"m"`) |

---

## `depends` Section

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `lib` | string[] | No | `[]` | List of hard dependency library names. Missing → build fails |
| `want` | string[] | No | `[]` | **0.2.2+** List of optional dependency library names. Missing → warn + define `EZMK_LIB_MISS_<NAME>` macro, does not block the build |

### Version Constraints (0.9.6+)

Each dependency entry can optionally include a version constraint using one of the following operators:

| Syntax | Meaning | Example |
|--------|---------|---------|
| `pkg@1.2.3` | Exact version | `fmt@10.2.1` |
| `pkg^1.2.3` | Compatible (same major) | `spdlog^1.14.0` → `>=1.14.0, <2.0.0` |
| `pkg~1.2.3` | Approximate (same minor) | `nlohmann_json~3.11.0` → `>=3.11.0, <3.12.0` |
| `pkg>=1.2.3` | Greater-than-or-equal | `zlib>=1.2.0` |
| `pkg>1.2.3` | Strictly greater-than | `boost>1.80.0` |
| `pkg` | No constraint (latest) | `fmt` — takes the highest available version |

**Design notes:**
- **Backward compatible**: entries without operators (`"fmt"`) behave exactly as in previous versions (take latest).
- **No lockfile**: version resolution is performed at install time; a lockfile (`ezmk.lock`) is deferred to a future version.
- **Constraint unsatisfied**: if no available version satisfies the constraint, installation fails with an error listing all available versions.

**Example:**
```toml
[depends]
lib = [
    "fmt",              # no constraint — latest version
    "spdlog@1.14.1",    # exact version
    "catch2^3.6.0",     # compatible: >=3.6.0, <4.0.0
    "nlohmann_json~3.11" # approximate: >=3.11.0, <3.12.0
]
want = [
    "sqlite3",          # optional, no constraint
    "yaml-cpp>=0.8.0"   # optional with GTE constraint
]
```

When the same package name appears in both `lib` and `want`, `lib` takes priority (as a hard dependency) and a warning is issued about redundant configuration.

Conversion rules from `want` package name to macro name:
- Uppercase conversion
- `-` / `.` / space → `_`
- Remove other special characters
- Examples: `sqlite3` → `EZMK_LIB_MISS_SQLITE3`, `boost-filesystem` → `EZMK_LIB_MISS_BOOST_FILESYSTEM`

---

## `compile.profile.<name>` Section (0.2.3+)

Build configuration activated via `--profile <name>`. Profile name must be alphanumeric (supports `-` and `_`); spaces are not allowed.

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `flags` | string[] | No | `[]` | Compile flags appended after `[compile].flags` |
| `msvc_flags` | string[] | No | `[]` | MSVC-specific flags appended after `[compile].msvc_flags` |
| `macros` | table | No | `{}` | Macro definitions merged into `[compile.macros]`; same-name keys are overridden |

Merging rules:
- `flags` / `msvc_flags`: profile flags are **appended** after base flags (GCC/Clang behavior: later overrides earlier)
- `macros`: merged into the base macro table; **profile keys override base keys with the same name**

Example:

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

Profiles do **not** auto-apply — the user must explicitly pass `--profile <name>`.

---

## `link.profile.<name>` Section (0.2.3+)

Link-phase configuration corresponding to `compile.profile`, activated by the same `--profile <name>`.

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `flags` | string[] | No | `[]` | Link flags appended after `[link].flags` |
| `msvc_flags` | string[] | No | `[]` | MSVC-specific link flags appended after `[link].msvc_flags` |

Merging rules are the same as compile profile: profile flags are appended after base flags.

Example:

```toml
[link.profile.debug]
flags = []

[link.profile.release]
flags = ["-flto"]
```

---

## `hooks` Section (0.2.3+)

Build lifecycle hooks — execute Lua scripts at key points of compilation/linking. Hook scripts receive a `ctx` table (`ctx.output`, `ctx.project_root`, `ctx.profile`) and run in a sandboxed Lua environment. Script not found → warn + skip (non-fatal). Only effective for user projects; not executed during package compilation.

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `pre_build` | string | No | `""` | Path to Lua script executed before compilation begins (relative to project root) |
| `post_build` | string | No | `""` | Path to Lua script executed after successful linking |
| `on_failure` | string | No | `""` | Path to Lua script executed on compile or link failure |

Example:

```toml
[hooks]
pre_build = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
```

See `utils.md` (Lua API reference) and CLAUDE.md (build hook implementation details).

---

## `utils` Section [version >= 0.2.0]

Only valid when `[project].type = "utils"`.

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `tools` | string[] | Yes | — | List of tool names provided by this package; each corresponds to `utils/<name>.lua` |

Example:

```toml
[utils]
tools = ["cc", "compile-commands"]
```

See `utils.md`.

---

## Full Examples

### Normal Project (0.2.3)

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

### Utils Package

```toml
[project]
name = "ezmk-cc"
version = "0.1.0"
type = "utils"

[utils]
tools = ["cc", "compile-commands"]
```
