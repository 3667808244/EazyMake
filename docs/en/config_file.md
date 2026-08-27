# Configuration File `ezmk.toml`

---

## `project` Section

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `name` | string | Yes | — | Project name |
| `type` | string | No | `"executable"` | Project type: `"executable"` / `"static"` / `"shared"` / `"utils"` |
| `version` | string | Yes | — | Project version; SemVer format recommended (e.g. `"0.1.0"`) |
| `language` | string | No | `"C++17"` | Language standard, e.g. `"C++17"`, `"C11"`, `"GNUCPP17"`. Case-insensitive; `C++`/`CXX`/`CPP` unified. **1.3.1+** range syntax: `">=C++11"` (minimum) / `"C++11..C++17"` (min..max) |
| `stdlib` | string | No | `"libstdc++"` | **1.1.0-dev.4+** Standard library: `"libstdc++"` (default) or `"libc++"`. Aliases: `"glibcxx"` / `"gnu"` → `libstdc++`; `"llvm"` → `libc++`. Case-insensitive |
| `precompiled_strict` | bool | No | `false` | **1.2.0-dev.10+** Precompiled-package strict mode: L2/L1 toolchain fallback (possibly ABI-incompatible) becomes a fail-fast error. Only affects packages (with `precompiled = true`); see [Package Authoring Guide](package_authoring.md#33-precompiled-package-precompiled--true-097) |

> **Why is `type` a string instead of an enum?** So adding a project type later
> (e.g. `"header-only"`) only introduces a new string value — existing `ezmk.toml`
> files keep parsing unchanged. Strings also read naturally in a declarative config.

### `type` Values

| Value | Output | Requires main.cpp? |
|-------|--------|--------------------|
| `"executable"` | Executable file | Yes |
| `"static"` | Static library `lib<name>.a` | No |
| `"shared"` | Dynamic library `lib<name>.dll` / `lib<name>.so` | No |
| `"utils"` | Utils package (no compile output, or `lib<name>.a`) | No |

### `language` Format

Format is `<language><version>`. The parser normalizes the value case-insensitively and accepts multiple variant spellings:

| User input | Normalized | `-std=` flag | Compiler |
|------------|-----------|-------------|----------|
| `C++17` / `c++17` / `cpp17` / `cxx17` | `CPP17` | `-std=c++17` | `g++` |
| `C11` / `c11` | `C11` | `-std=c11` | `gcc` |
| `C++` (no version) | `CPP17` | `-std=c++17` | `g++` |
| `C` (no version) | `C11` | `-std=c11` | `gcc` |

- Language: `C++` / `CXX` / `CPP` (all map to C++), or `C`
- Version: `89` / `98` / `99` / `03` / `11` / `14` / `17` / `20` / `23` / `26`
- Default version: C++ → `17`, C → `11`

> **Why accept so many spellings?** Users type `c++17` / `cpp17` / `cxx17`
> interchangeably. Normalizing to one canonical value (e.g. `CPP17`) makes the value
> comparable internally and lets it be baked into the `EZMK_LANG` macro with a
> stable form.

#### GNU Extensions (1.1.0-dev.4+)

Prefix `GNU` before the language to enable GNU compiler extensions:

| User input | Normalized | `-std=` flag | Warning |
|------------|-----------|-------------|---------|
| `GNUCPP17` / `gnuc++17` | `GNUCPP17` | `-std=gnu++17` | non-ISO warning |
| `GNU11` / `gnu11` | `GNU11` | `-std=gnu11` | non-ISO warning |

> A warning will be emitted when GNU extensions are used, suggesting standard alternatives (e.g. `language = "CPP17"`).

> **Why warn on GNU extensions?** GNU extensions are non-portable — code that relies
> on them may not compile under a strict or different compiler. The warning nudges
> users toward the standard spelling rather than forbidding the feature.

#### Range Syntax (1.3.1+)

Declare a **minimum compatibility standard** — useful for library authors whose
code supports a baseline standard (optionally up to a documented upper bound):

| User input | min | max | Effective `-std=` | Meaning |
|------------|-----|-----|-------------------|---------|
| `C++11` | 11 | — | `-std=c++11` | exact standard (unchanged behavior) |
| `>=C++11` | 11 | — | `-std=c++11` | at least C++11 |
| `C++11..C++17` | 11 | 17 | `-std=c++11` | C++11 minimum, C++17 documented upper bound |
| `>=GNUCPP11` | 11 | — | `-std=gnu++11` | at least GNU C++11 |
| `>=C` | 11 | — | `-std=c11` | at least C11 (missing-version defaults apply) |

- The **minimum** is the *effective* standard: compilation always uses `min`, for
  maximum predictability and artifact compatibility.
- The **maximum** (range form only) is **metadata**: it documents how far the
  project is known to work, but never changes the compile flags or the cache
  signature (a max-only edit does not trigger a rebuild).
- `EZMK_LANG` keeps the min canonical form (`">=C++11"` → `"CPP11"`) — identical to
  the exact spelling, so existing `#ifdef` code is unaffected.
- Invalid forms are rejected: `C++17..C++11` (max < min), `>C++11` (only `>=` is
  supported), `C++11+` (no `+` suffix), empty range ends (`C++11..` / `..C++17`),
  `>=C++11..C++17`, and mixed-family ranges (`C11..C++17`).

> **When to use a range?** Package authors should declare the *lowest* standard
> their code compiles at (e.g. `">=C++11"` when C++11 is the baseline and newer
> standards are optional). Since 1.3.1, the install-time standard compatibility
> check warns when a package requires a higher minimum than the consuming project
> compiles at — see [Package Authoring Guide](package_authoring.md) and
> [Packages](pkg.md).
>
> **Compile negotiation (1.4.0-dev.3+):** a *source* package's effective compile
> standard is `min( max(pkg_min, consumer_min), max_supported_std, pkg_max )` —
> the consumer's higher standard is used when the compiler supports it and the
> package declares it (its range **max** is a commitment boundary: consumers
> above it never push the package past it). See
> [Package Authoring Guide](package_authoring.md) for the full semantics.

---

## `compile` Section

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `flags` | string[] | No | `[]` | Compile flags (e.g. `-Wall`, `-O2`). GCC/Clang format, auto-translated under MSVC |
| `msvc_flags` | string[] | No | `[]` | **0.2.1+** MSVC-specific compile flags (not translated, only appended when using MSVC toolchain) |
| `include_dirs` | string[] | No | `["include"]` | `-I` search paths during compilation, relative to project root; also honored for packages (1.2.0-dev.9+, resolved relative to the package root, order-preserving dedup against the default `include/`) |
| `src_dirs` | string[] | No | `["src"]` | **0.2.2+** Source file search directories; supports multiple directories (e.g. `["src", "lib"]`). Explicitly setting to `[]` causes an error; also honored for packages (1.2.0-dev.9+, package sources are collected from these directories, missing dirs warn + skip) |
| `ezmk_macros` | bool | No | `true` | **0.2.2+** Whether to auto-inject `EZMK_*` standard preprocessor macros (`EZMK`/`EZMK_VERSION`/`EZMK_PROJECT_*`) |
| `compile_commands` | bool | No | `false` | **1.1.1+** Auto-generate `compile_commands.json` (clangd index) after a successful build |
| `default_profile` | string | No | `""` | **1.2.0+** Profile applied when no `--profile` is passed. When set, a plain `ezmk build` merges that profile (same lookup/merge/error path as an explicit `--profile`); when empty, no profile applies |

Note: Legacy field `include_dir` (singular) is deprecated; if encountered during parsing, it is automatically mapped to `include_dirs`.

> **`compile_commands` (1.1.1+):** When `true`, `ezmk build` writes `compile_commands.json`
> after a successful link. The index is produced from the same command construction as
> the build (single source of truth), so it can never drift from the real compile flags —
> `-D` macros, include dirs, `@link:` resolutions, and the active profile are all reflected.
> Equivalent to CMake's `CMAKE_EXPORT_COMPILE_COMMANDS`. `ezmk utils cc` generates it on
> demand, and the `--compile-commands` build flag enables it for a single invocation
> without changing the config.

> **Why a separate, untranslated `msvc_flags`?** The GCC→MSVC auto-translation can't
> cover every difference (e.g. `/Zi`, `/Od` have no GCC equivalent). `msvc_flags` is
> passed verbatim so MSVC users get precise control without fighting the translator.

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

> **Why this resolution order?** Later definitions win, so the most specific source
> overrides the most general: your `[compile.macros]` overrides `-D` in `flags`,
> which overrides the standard `EZMK_*` macros. This gives a predictable precedence
> chain.

### Standard Predefined Macros (when `ezmk_macros = true`)

| Macro name | Type | Example value | Description |
|------------|------|---------------|-------------|
| `EZMK` | integer | `1` | Always defined as `1`; identifies the build system as EazyMake |
| `EZMK_VERSION` | string | `"0.2.2"` | EazyMake's own version number |
| `EZMK_PROJECT_NAME` | string | `"myapp"` | `[project].name` |
| `EZMK_PROJECT_VERSION` | string | `"1.0.0"` | `[project].version` |
| `EZMK_PROJECT_TYPE` | string | `"executable"` | `[project].type` |
| `EZMK_LANG` | string | `"CPP17"` | **1.1.0-dev.4+** Normalized `[project].language` (e.g. `c++17` → `CPP17`) |
| `EZMK_STDLIB` | string | `"libstdcxx"` | **1.1.0-dev.4+** `[project].stdlib` with `++` replaced by `xx` (`libstdc++` → `libstdcxx`) |

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
| `workspace` | string[] | No | `[]` | **1.3.0+** Workspace sibling-member dependencies (develop-and-use, source-level) |

> **Why warn instead of failing?** `want` expresses an optional dependency. Instead
> of blocking the build, a missing lib defines `EZMK_LIB_MISS_<NAME>` so the code
> can `#ifdef` around the optional feature and degrade gracefully.

### Workspace sibling dependencies (`workspace`, 1.3.0+)

`workspace` declares a dependency on another member (sibling project) of the **same workspace** — develop-and-use against source, with the sibling's static-library artifact injected automatically at build time:

```toml
[depends]
workspace = ["strutil"]          # sibling member (basename or full relative path, e.g. "libs/strutil")
```

- **Prerequisite**: the project must be a member of some workspace (`ezmk-workspace.toml`); workspace declaration and discovery are in the [`cli.md`](cli.md) `workspace` section.
- **Constraints**: the dependency graph must be **one-way acyclic** (cycles / self-loops rejected at config time); the depended-on member must be `type = "static"` (its `build/lib<name>.a` is reused); no versions (version/snapshot semantics belong to packages — see `lib` above).
- **Injection**: building the member injects `-I <ws>/<m>/include` + `-L <ws>/<m>/build -l<m>` (existence-gated, zero environment variables); changing a library `.cpp` → dependents relink only, changing a library `.h` → dependents recompile automatically.
- **Non-member project** declaring `workspace` with no workspace found upward → build warns and skips injection (does not block).
- Full commands (`workspace build / test / clean`, `-w` redirect, `--member`) are in [`cli.md`](cli.md).

> **Config generated by `ezmk project import` (1.2.0+)** — the CMake importer
> writes `find_package` hits as **commented** `[depends]` entries (e.g.
> `# lib = ["boost@1.82"]`) with a `# TODO:` line. Uncomment and adjust them,
> then run `ezmk pkg install <name>`. The generated file also carries a header
> comment block noting it is auto-generated and experimental. See
> [migrate-from-cmake.md](migrate-from-cmake.md).

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
- **Lockfile (`ezmk.lock`, 1.1.0+)**: version resolution happens at install time, then `ezmk.lock` pins the exact versions that were actually installed. See the Lockfile subsection below.
- **Constraint unsatisfied**: if no available version satisfies the constraint, installation fails with an error listing all available versions.

> **Why backward compatible?** Bare entries (`"fmt"`) keep their old
> "latest wins" meaning so existing configs don't change behavior. The lockfile
> (1.1.0+) sits on top of that: install resolution still honors `[depends]`
> constraints, but once written, `ezmk.lock` records exactly what was installed
> for reproducible builds.

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

### Lockfile (`ezmk.lock`) (1.1.0+)

`ezmk pkg install` writes `ezmk.lock` (TOML) in the project root, pinning each installed package's **exact version**, `sha256`, platform, and dependency graph for reproducible builds.

- **Written**: automatically on every `ezmk pkg install`.
- **Verified**: `ezmk build` checks it at startup:
  - `[compile] deterministic = true` → a missing or failing lockfile is an **error**; the lockfile hash is part of the compile-cache signature.
  - otherwise → changed dependencies / sha256 mismatch are just **warnings**.
- **Flags**: `ezmk pkg install --locked` (install only against the lockfile, error on mismatch); `--no-lock` (skip lockfile generation).
- **Do not hand-edit**: `ezmk.lock` is auto-generated — edit `[depends]` in `ezmk.toml` and reinstall instead.

```toml
[metadata]
version = 1
generated_by = "ezmk 1.1.0"
toolchain = "gcc"
direct_deps = ["fmt", "spdlog@^1.14.0"]

[[packages]]
name = "spdlog"
version = "1.14.1"
sha256 = "..."
type = "static"
scope = "user"
platform = "windows_x86_64_msvc"
dependencies = []
```

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

Profiles do **not** auto-apply by default — without a `default_profile`, the user must explicitly pass `--profile <name>`.

**`default_profile` (1.2.0+) is the one exception.** If `[compile].default_profile` is set (e.g. `default_profile = "debug"`), a plain `ezmk build` **does** apply that profile automatically. Resolution priority:

1. explicit `--profile <name>` — always wins
2. `[compile].default_profile` — applied when non-empty and no `--profile` is given
3. base-only — neither set

> **Why the exception?** `default_profile` is *declared state*, not hidden state — the
> project author chooses the default build shape explicitly, so users get a sensible
> out-of-the-box build (e.g. debuggable) while still being able to override with
> `--profile release`. When the field is absent, the old explicit-only rule still holds:
> profiles never auto-apply and each invocation stays deterministic.

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

> **Why sandboxed and non-fatal?** Hooks run arbitrary code inside your build, so the
> sandbox (and package-side exclusion) limits the damage a third-party hook can do.
> A missing script is a configuration slip, not a build failure — warn and continue.

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

> **CMake export (1.2.0-dev.8+):** `ezmk project export cmake` maps `pre_build` /
> `post_build` to `add_custom_command(TARGET ... PRE_BUILD/POST_BUILD)` that invokes
> the **standalone `ezmk-lua` runtime** (`find_program(EZMK_LUA ezmk-lua)`) with
> `--project-root`, `--output $<TARGET_FILE:...>` and the export profile. `ezmk-lua`
> runs the hook in an *unrestricted* Lua environment (a strict superset of the build
> sandbox) and ships alongside `ezmk` in every distribution channel. If `ezmk-lua` is
> not on `PATH`, the generated CMake falls back to a `message(WARNING)` (best-effort —
> hook post-processing is skipped, never fatal). `on_failure` has no CMake equivalent
> and is not exported. To keep behavior identical under both `ezmk build` (sandboxed)
> and the exported CMake build, write hooks against the `ezmk.*` API subset only.

---

## `install` Section (1.1.0+)

Controls where `ezmk install` copies build artifacts. Per-invocation override: `--prefix <path>`.

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `prefix` | string | No | `~/.local` (Unix) · `%LOCALAPPDATA%\ezmk` (Windows) | Install root directory; supports `~` expansion |
| `bindir` | string | No | `"bin"` | Subdirectory for executables (relative to `prefix`) |
| `libdir` | string | No | `"lib"` | Subdirectory for static/shared libraries |
| `includedir` | string | No | `"include"` | Subdirectory for headers |
| `sharedir` | string | No | `"share"` | Subdirectory for data files |

Install layout:
- `executable` → `<bindir>/`
- `static` → `<libdir>/`
- `shared` → `<bindir>/` (DLL) + `<libdir>/` (import library)
- Headers → `<includedir>/<name>/`

> **Why DLL to `bindir` but import lib to `libdir`?** On Windows the DLL must be
> reachable at load time, i.e. on the PATH (`bin`), while the import library is a
> link-time artifact that belongs in `lib` alongside other libraries.

Example:

```toml
[install]
prefix = "~/.local"
bindir = "bin"
libdir = "lib"
includedir = "include"
sharedir = "share"
```

Corresponding CLI command: `ezmk install` (alias of `ezmk project install`).

---

## `test` Section (1.1.0+)

Configuration for `ezmk test` (build and run project tests).

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `dirs` | string[] | No | `["test"]` | Test source file directories |
| `framework` | string | No | `"catch2"` | Test framework: `"catch2"` or `"ezmk"` (case-insensitive) |
| `default_profile` | string | No | `""` | **1.2.0-dev.12+** Profile applied when no `--profile` is given (reuses `[compile.profile.<name>]` / `[link.profile.<name>]`, symmetric with `[compile].default_profile`) |
| `include_dirs` | string[] | No | `[]` | **1.2.0-dev.12+** Test-only `-I` directories (resolved relative to the project root, missing dirs skipped); does not pollute the main build |
| `link_targets` | string[] | No | `[]` | **1.2.0-dev.12+** Test-only `-l` link targets; does not pollute the main build |
| `flags` | string[] | No | `[]` | ⚠️ **Deprecated (1.2.0-dev.12+, removed in 2.0.0)** — still honored but warns; use `default_profile` + `[compile.profile.<name>]`, or `include_dirs` / `link_targets` |

Example:

```toml
[test]
dirs = ["test"]
framework = "catch2"
default_profile = "release"        # 1.2.0-dev.12+: run tests with the release profile by default
include_dirs = ["test/helpers"]    # 1.2.0-dev.12+: test-only header directories
link_targets = ["pthread"]         # 1.2.0-dev.12+: test-only link libraries
```

Corresponding CLI command: `ezmk test` (alias of `ezmk project test`), with `--framework <name>`, `--filter <pattern>`, `--profile <name>` (1.2.0-dev.12+, overrides `default_profile`), and `-V` (verbose output).

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

## `pkg` Section (1.4.0-dev.2+)

Package-management configuration. Optional — all fields default to the
informative (non-failing) behavior.

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `strict_std_check` | bool | No | `false` | **1.4.0-dev.2+** Escalate the install-time standard compatibility check (1.3.1) from a warning to a **fatal error** when a package's minimum standard exceeds the project's |

> **When to enable?** With `strict_std_check = true`, installing a package whose
> minimum standard is higher than your `[project].language` compiles at **fails**
> (instead of warning and continuing). Use it in CI or requirement-tight projects
> where an ABI/source mismatch must block the install. To resolve a failure: raise
> `[project].language` — or set `strict_std_check = false` to downgrade back to the
> 1.3.1 warning. The default stays a warning because a strict error would break the
> existing package ecosystem (opt-in only).

```toml
[pkg]
strict_std_check = true   # warn → fatal (default: false)
```

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
