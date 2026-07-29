---
name: ezmk-codebase
description: EazyMake project architecture — source layout, module responsibilities, data flow, key design patterns, and implementation notes.
---

# EazyMake Codebase

## Directory overview

```
EazyMake/
├── src/                  # Core implementation (.cpp)
│   ├── main.cpp          # Entry point + CLI dispatch
│   ├── cli.cpp           # Command-line parsing (GNU-style)
│   ├── build.cpp         # Build orchestration (compile → link → cache)
│   ├── cache.cpp         # Incremental build cache (hash + record.json)
│   ├── config.cpp        # ezmk.toml parsing
│   ├── pkg.cpp           # Package management (install/update/list)
│   ├── repo.cpp          # Repository management (add/update/clone)
│   ├── toolchain.cpp     # Compiler detection (GCC/Clang/MSVC)
│   ├── project.cpp       # Project creation / structure
│   ├── i18n.cpp          # Internationalization (X-macro driven)
│   ├── lua_api.cpp       # Lua C++ API (23 functions + sandbox)
│   ├── file_watcher.cpp  # File watcher (Watch mode)
│   ├── crypto.cpp        # SHA-256 hashing
│   ├── util.cpp          # Utilities (path/process/color/platform)
│   ├── version.cpp       # Version number
│   ├── argparse.cpp      # GNU-style argument parsing
│   ├── lockfile.cpp      # ezmk.lock generation & verification
│   ├── pack.cpp          # ezmk project pack
│   └── vendor/           # Third-party (miniz, Catch2 impl, Lua 5.4.7)
├── include/ezmk/         # Public headers (mirrors src/ layout)
│   └── i18n_keys.def     # X-macro: single source of truth for all i18n keys
├── test/                 # Catch2 unit tests (test_<module>.cpp)
├── docs/
│   ├── en/               # English user documentation
│   └── zh/               # Chinese user documentation
├── plans/
│   ├── dev/              # Early development version plans (0.1.6~0.2.6)
│   ├── release/          # Release version plans (0.9.0~1.1.0 + dev sub-versions)
│   └── README.md         # Version index + roadmap
├── locale/
│   ├── en.json           # English translations
│   └── zh.json           # Chinese translations
├── scripts/              # Build helper scripts (embed_locale.py, embed_logo.py, etc.)
├── res/                  # Resources (logo.txt)
├── build.sh              # One-shot build + test script
├── ezmk.toml             # EazyMake's own project config (dogfooding)
├── CLAUDE.md             # AI agent entry point → skill index
├── CHANGES.md            # Version changelog
└── README.md             # User-facing documentation
```

## Module responsibilities

### Core pipeline

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| **main** | `src/main.cpp` | Entry point. Parses CLI via `cli::parse()`, dispatches to command handlers. |
| **cli** | `src/cli.cpp` | GNU-style argument parsing. Defines `CliArgs` struct with all command options. Command shorthands (`pb`→`project build`, `ki`→`pkg install`, etc.) expanded here. Global `--color=<mode>` consumed here. |
| **argparse** | `src/argparse.cpp` | Low-level argument parser — handles `--flag`, `--key=value`, positional args, `--` pass-through. |
| **config** | `src/config.cpp` | Parses `ezmk.toml`. Defines all config structs (`ProjectSection`, `CompileSection`, `LinkSection`, `DependsSection`, `InstallSection`, etc.). Also parses `ezmk.lock` (via `lockfile::load()`). |
| **build** | `src/build.cpp` | Build orchestration: source collection, compilation scheduling (via `ThreadPool`), linking, `ezmk project install`. Calls into `cache.cpp` and `toolchain.cpp`. |
| **cache** | `src/cache.cpp` | Content-hash-based incremental compilation. Reads/writes `record.json` (v2: includes `compiler`, `compiler_version`, `deterministic` fields). Atomic writes via temp → rename. |
| **toolchain** | `src/toolchain.cpp` | Compiler auto-detection (GCC/Clang/MSVC). `Toolchain` struct captures family, path, flags, and version. GCC→MSVC flag translation layer. |

### Package & repository

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| **pkg** | `src/pkg.cpp` | Package install/update/list/remove. Handles precompiled packages, header-only packages, platform-aware `lib/` selection. `detect_install_script()` finds install hooks. |
| **repo** | `src/repo.cpp` | Repository add/update/remove/list. Clones git repos, parses `index.toml`, resolves platform-specific package entries (`os_arch_toolchain` triple keys). |

### Cross-cutting

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| **util** | `src/util.cpp` | Path normalization, process execution (`run_command()`), color output, SHA-256, `detect_platform_tag()`, `create_targz()`, string/vector helpers. |
| **i18n** | `src/i18n.cpp` | Locale loading, `key_name()` mapping (from `i18n_keys.def`), `audit_missing_keys()` debug check. |
| **lua_api** | `src/lua_api.cpp` | Lua C++ API bridge — 23 sandboxed functions. Shared `run_lua_script_with_ctx()` used by build hooks and install hooks. |
| **crypto** | `src/crypto.cpp` | SHA-256 hashing (used for cache hashes, package verification, lockfile). |
| **file_watcher** | `src/file_watcher.cpp` | Cross-platform file monitoring. Windows: `ReadDirectoryChangesW` + IOCP; Linux: `inotify`; macOS: `kqueue`. 300ms debounce. |
| **project** | `src/project.cpp` | `ezmk project new` — scaffolds project from templates. |
| **version** | `src/version.cpp` | Version comparison utilities. |
| **lockfile** | `src/lockfile.cpp` | `ezmk.lock` generation, loading, verification. |
| **pack** | `src/pack.cpp` | `ezmk project pack` — packages static library projects into `.tar.gz`. |

## Data flow

```
CLI (cli.cpp) → config (config.cpp) → build (build.cpp) → cache (cache.cpp) → toolchain (toolchain.cpp)
                   ↑                        ↓
              ezmk.toml              pkg/repo (pkg.cpp, repo.cpp)
                                         ↓
                                   Lua hooks (lua_api.cpp)
```

1. **CLI** parses user command → fills `CliArgs`
2. **Config** loads `ezmk.toml` → fills `ProjectConfig`
3. **Build** orchestrates: collect sources → for each source: check cache → if miss, compile via toolchain → link
4. **Cache** checks `record.json`: source hash + header hashes + compile flags → hit (reuse `.o`) or miss (recompile)
5. **Toolchain** provides compiler abstraction: GCC/Clang/MSVC flag translation, platform detection
6. **Pkg/Repo** manage external dependencies — resolved before build

## Key design patterns

### X-macro (i18n)
`include/ezmk/i18n_keys.def` is the single source of truth for all translatable string keys. It generates both the `I18nKey` enum and the `key_name()` mapping at compile time — they can never drift. See `ezmk-i18n` skill for details.

### RAII
Resources managed via RAII throughout: `std::ifstream`/`std::ofstream`, `ThreadPool` (destructor joins), file handles in `FileWatcher`.

### Atomic write (temp → rename)
Cache files (`.o`, `record.json`) are written to temp files first, then renamed — prevents corruption on mid-build failure.

### Thread safety
`ThreadPool` (fixed-size) for parallel compilation. Console output uses a global `std::mutex` for clean interleaved messages. Cache loaded once before compilation, saved once after all tasks complete.

## Files you should NOT modify

- **`src/vendor/**`** — third-party code (miniz, Catch2 impl, Lua 5.4.7)
- **`include/vendor/**`** — third-party headers (Catch2, Lua)
- **`include/ezmk/version.hpp`** — auto-generated by `build.sh`
- **`include/ezmk/logo.gen.h`** — auto-generated by `scripts/embed_logo.py`
- **`src/locale_data.cpp`** — auto-generated by `scripts/embed_locale.py`

## CLI flags not in README

These implementation-relevant flags are not documented in the README command table:

| Flag | Command(s) | Purpose |
|---|---|---|
| `--disable-git-init` | `project new` | Skip `git init` |
| `--disable-gitignore` | `project new` | Skip `.gitignore` generation |
| `--sha256 <hash>` | `pkg install` | Verify package integrity |
| `-y` | `pkg install` | Skip confirmation prompts |
| `--locked` | `pkg install` | Install from lockfile only, error on mismatch |
| `--no-lock` | `pkg install` | Skip lockfile generation |
| `--prefix <path>` | `project install` | Override `[install].prefix` |
| `--dry-run` | `project install` | Show what would be installed, don't write |
| `--no-headers` | `project install` | Skip header installation |
| `--no-data` | `project install` | Skip data file installation |
| `-j` / `--jobs <N>` | `project build/run/watch` | Parallel compile jobs (0=auto, default) |
| `--profile <name>` | `project build/run/watch` | Apply a build profile (e.g. debug/release) |
| `--no-build-on-start` | `project watch` | Skip initial build in watch mode |

Additional commands not yet in README:
- `ezmk project install [-v] [--prefix <path>] [--dry-run] [--no-headers] [--no-data]` — install build artifacts
- `ezmk pkg list [-p|-u|-g]` — list installed packages
- `ezmk pkg update [-p|-u|-g] <pkg>` — update a package from repos
- `ezmk pkg install --locked / --no-lock` — lockfile-aware install modes

Scope flags (`-p`/`-u`/`-g`): `install` and `repo add` accept only one; others accept combined flags like `-pug`.

### Command shorthands

`cli::parse()` expands top-level aliases in `argv[1]` before any other parsing:
- Project: `pn/pb/pr/pc/pi/pw` → `project new/build/run/compile/install/watch`
- Pkg: `ki/kr/ks/kn/kl/ku` → `pkg install/remove/search/info/list/update`
- Repo: `ra/rr/rl/ru/ri` → `repo add/remove/list/update/info`
- Utils: `u`/`h`/`v` → `utils`/`help`/`version`

Only apply at the command position; `ezmk project pn` is still an unknown subcommand. Deliberately **not** added to `completions/_ezmk`.

### Global `--color=<mode>`

Consumed by `strip_color_option()` at the top of `cli::parse()`. Values (case-insensitive): `always`/`enable`, `auto`/`default`, `never`/`disable`. Tokens after `--` are left for pass-through. Sets `util::set_color_mode()`; explicit `always`/`never` override `NO_COLOR` (only `auto` honors it). `always` also runs `init_console()` for Windows VT100.

## Configuration system (`ezmk.toml`)

See `README.md` for the TOML example and `docs/en/config_file.md` for the full spec. Key sections:

- `[project]` — `name`, `type` (`"executable"` / `"static"` / `"shared"` / `"utils"`), `version` (required), `language` (default `"C++17"`)
- `[compile]` — `flags`, `msvc_flags`, `include_dirs` (default `["include"]`), `src_dirs` (default `["src"]`), `ezmk_macros` (bool, default `true`), `deterministic` (bool, default `false`), `source_date_epoch` (uint64, optional). Sub-table `[compile.macros]` for semantic macro definitions (key-value, supports string/int/bool).
- `[link]` — `flags`, `msvc_flags`, `link_dirs`, `system_target`
- `[depends]` — `lib` (hard deps, missing → error), `want` (optional deps)
- `[utils]` — `tools` array (only for `type = "utils"`)
- `[compile.profile.<name>]` — profile-specific `flags`, `msvc_flags`, `macros` (sub-table). Flags append to base; macros override base on key conflict.
- `[link.profile.<name>]` — profile-specific link `flags`, `msvc_flags`
- `[hooks]` — `pre_build`, `post_build`, `on_failure`: paths to Lua hook scripts
- `[install]` — `prefix`, `bindir`, `libdir`, `includedir`, `sharedir`: install layout

## Package management

Packages are `.zip` or `.tar.gz` archives compiled to static libraries. Install paths by scope:
- Global: `<ezmk_install_dir>/pkg/`
- User: `~/.local/ezmk/pkg/` (Unix) / `%LOCALAPPDATA%\ezmk\pkg\` (Windows)
- Project: `<project_dir>/.ezmk/pkg/`

**MSVC-aware:** `compile_package()` selects archiver based on `tc.family`: MSVC → `lib.exe`, GCC/Clang → `ar rcs`.

**Header-only packages:** `pkg.toml` field `header_only = true` skips compilation — only `include/` is copied.

**Platform mapping:** `index.toml`'s `[platform]` section supports `os_arch_toolchain` triple keys with fallback to legacy `os_arch` double keys. `resolve_platform_prefix()` in `repo.cpp` tries triple → double → empty.

See `docs/en/pkg.md` for full details.

## Repository management

A repo is a git repository containing `index.toml` + `packages/` directory. `ezmk repo add` clones to local cache; `ezmk repo update` does `git pull`. Local directories supported (`type = "local"`).

Repo registries (`list.toml`) per scope:
- Global: `<ezmk_install_dir>/repo/list.toml`
- User: `~/.local/ezmk/repo/list.toml` (Unix) / `%LOCALAPPDATA%\ezmk\repo\list.toml` (Windows)
- Project: `.ezmk/repo/list.toml`

See `docs/en/repo.md` for full details.

## Deterministic builds

`[compile]` section supports `deterministic = true` and optional `source_date_epoch` (uint64 Unix timestamp). When enabled:
- **GCC/Clang**: injects `-ffile-prefix-map=<proj_root>=.` + `-frandom-seed=<src_filename>` + sets `SOURCE_DATE_EPOCH`
- **MSVC**: injects `/Brepro` + sets `SOURCE_DATE_EPOCH`
- **Resolution priority**: environment variable → `ezmk.toml` config → git HEAD commit timestamp → `ezmk.toml` mtime

## Lockfile (`ezmk.lock`)

TOML file pinning exact dependency versions and content hashes.
- **API**: `lockfile::load()` / `lockfile::save()` / `lockfile::verify()` / `lockfile::depends_changed()`
- **Strict mode**: when `deterministic = true` — missing lockfile or sha256 mismatch → fatal error
- **`--locked` mode**: requires lockfile to exist and match `ezmk.toml`; used in CI to prevent accidental dependency drift

## Safety requirements

See `docs/en/@safety.md`:
- Global package installs require secondary confirmation
- Installations that would overwrite existing files require secondary confirmation
- Install hook scripts (`.lua`/`.sh`/`.ps1`/`.bat`) are detected via `detect_install_script()` (`include/ezmk/pkg.hpp`)

## Internationalization (i18n)

String keys live in `include/ezmk/i18n_keys.def` (X-macro). Both `I18nKey` enum and `key_name()` mapping are generated from it. Adding a key = one line in `i18n_keys.def` + a string in `locale/en.json` **and** `locale/zh.json`, then rebuild. Debug builds run `audit_missing_keys()`. See `ezmk-i18n` skill for the full workflow.

## Workflow rules (for contributors)

1. **Commit per phase** — after completing each execution phase, make a Git commit. Do NOT squash unrelated phases.
2. **All phases done** → update `CLAUDE.md`, `CHANGES.md`, `plan.md`, `plans/README.md`
3. **Push** after documentation is current.
