# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

EazyMake is a simple C/C++ build tool (CLI named `ezmk`), based on GCC/g++ (MSYS2 on Windows). Design philosophy: ease of use over feature richness. **See `README.md` for user-facing documentation** (quick start, CLI reference, project structure, config examples).

Design specifications live in `docs/en/` (English) / `docs/zh/` (Chinese). Version milestones are in `plans/dev/` (0.1.6~0.2.6) and `plans/release/` (0.9.0~1.0.0), indexed by `plans/README.md`. Current execution plan: `plan.md`.

## Build & test commands

- Build: `bash build.sh` (generates locale data + version header, then compiles)
- Manual build (MSYS2): `g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c -I include/ -I include/vendor/ -I include/vendor/lua/ -DLUA_COMPAT_5_3 -o build/ezmk -lwinhttp -static`
- Manual build (Linux): `g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c -I include/ -I include/vendor/ -I include/vendor/lua/ -DLUA_COMPAT_5_3 -o build/ezmk -static`
- Test: `g++ -std=c++17 test/test_*.cpp src/vendor/catch2_impl.cpp src/build.cpp src/cache.cpp src/cli.cpp src/argparse.cpp src/config.cpp src/crypto.cpp src/file_watcher.cpp src/i18n.cpp src/lua_api.cpp src/pkg.cpp src/project.cpp src/repo.cpp src/toolchain.cpp src/util.cpp src/version.cpp src/vendor/*.c src/vendor/lua/*.c -I include/ -I include/vendor/ -I include/vendor/lua/ -DLUA_COMPAT_5_3 -o build/test_ezmk -lwinhttp -static && ./build/test_ezmk`
- Test framework: Catch2 v3 (header-only: `include/vendor/catch2.hpp` + `src/vendor/catch2_impl.cpp`)

## Architecture

### CLI flags not in README

These implementation-relevant flags are not documented in the README command table:

| Flag | Command(s) | Purpose |
|---|---|---|
| `--disable-git-init` | `project new` | Skip `git init` |
| `--disable-gitignore` | `project new` | Skip `.gitignore` generation |
| `--sha256 <hash>` | `pkg install` | Verify package integrity |
| `-y` | `pkg install` | Skip confirmation prompts |
| `-j` / `--jobs <N>` | `project build/run/watch` | Parallel compile jobs (0=auto, default) |
| `--profile <name>` | `project build/run/watch` | Apply a build profile (e.g. debug/release) |
| `--no-build-on-start` | `project watch` | Skip initial build in watch mode |

Additional commands not yet in README:
- `ezmk pkg list [-p|-u|-g]` — list installed packages (0.2.3+)
- `ezmk pkg update [-p|-u|-g] <pkg>` — update a package from repos (0.2.3+)

Scope flags (`-p`/`-u`/`-g`): `install` and `repo add` accept only one; others accept combined flags like `-pug`.

### Command shorthands & global `--color` (0.2.6+)

- **Shorthands**: `cli::parse()` expands a top-level alias in `argv[1]` before any other parsing (so downstream logic and error messages see the canonical command). Aliases: `pn/pb/pr/pc/pw` (project), `ki/kr/ks/kn/kl/ku` (pkg), `ra/rr/rl/ru/ri` (repo), `u`/`h`/`v` (utils/help/version). Only apply at the command position; `ezmk project pn` is still an unknown subcommand. Deliberately **not** added to `completions/_ezmk`.
- **`--color=<mode>`**: global option consumed by `strip_color_option()` at the top of `cli::parse()` (before per-command parsing, which would reject it). Values (case-insensitive): `always`/`enable`, `auto`/`default`, `never`/`disable`. Tokens after `--` are left for pass-through. Sets `util::set_color_mode()`; explicit `always`/`never` override `NO_COLOR` (only `auto` honors it), matching git/ls. `always` also runs `init_console()` for Windows VT100.

### Internationalization (i18n) — single source of truth (0.2.6+)

All string keys live in **`include/ezmk/i18n_keys.def`** (X-macro list). Both the `I18nKey` enum (`i18n.hpp`) and the enum→JSON-name mapping `key_name()` (`i18n.cpp`) are generated from it, so they can never drift (this eliminated the historical `{???}` bug where new enum values were missing from a hand-written `key_name()` switch). Adding a key = one line in `i18n_keys.def` + a string in `locale/en.json` **and** `locale/zh.json`, then rebuild (`build.sh` re-runs `scripts/embed_locale.py`). Debug builds run `audit_missing_keys()` in `i18n::init()` to warn once per key that exists in the enum but is missing from the loaded locale.

### Configuration (`ezmk.toml`) — implementation notes

See `README.md` for the TOML example and `docs/en/config_file.md` for the full spec. Key sections for implementation:

- `[project]` — `name`, `type` (`"executable"` / `"static"` / `"shared"` / `"utils"`), `version` (required), `language` (default `"C++17"`, format `<语言><版本>`)
- `[compile]` — `flags`, `msvc_flags` (0.2.1+), `include_dirs` (default `["include"]`), `src_dirs` (default `["src"]`, 0.2.2+), `ezmk_macros` (bool, default `true`, 0.2.2+). Sub-table `[compile.macros]` (0.2.2+) for semantic macro definitions (key-value, supports string/int/bool)
- `[link]` — `flags`, `msvc_flags` (0.2.1+), `link_dirs`, `system_target`
- `[depends]` — `lib` (hard deps, missing → error), `want` (optional deps, 0.2.2+)
- `[utils]` — `tools` array (only for `type = "utils"`)
- `[compile.profile.<name>]` (0.2.3+) — profile-specific `flags`, `msvc_flags`, `macros` (sub-table). Flags append to base; macros override base on key conflict.
- `[link.profile.<name>]` (0.2.3+) — profile-specific link `flags`, `msvc_flags`
- `[hooks]` (0.2.3+) — `pre_build`, `post_build`, `on_failure`: paths to Lua hook scripts (relative to project root)

### Package management

Packages are `.zip` or `.tar.gz` archives compiled to `*.a` static libraries following dependency chain. Circular dependencies or missing packages are errors. `type = "utils"` packages additionally provide Lua-based tools via `ezmk utils`.

Install paths by scope:
- Global: `<ezmk_install_dir>/pkg/`
- User: `~/.local/ezmk/pkg/` (Unix) / `%LOCALAPPDATA%\ezmk\pkg\` (Windows)
- Project: `<project_dir>/.ezmk/pkg/`

See `docs/en/pkg.md` for full details.

### Repository management

A repo is a git repository containing `index.toml` + `packages/` directory. `ezmk repo add` clones to local cache; `ezmk repo update` does `git pull`. Local directories supported (`type = "local"`).

Repo registries (`list.toml`) per scope:
- Global: `<ezmk_install_dir>/repo/list.toml`
- User: `~/.local/ezmk/repo/list.toml` (Unix) / `%LOCALAPPDATA%\ezmk\repo\list.toml` (Windows)
- Project: `.ezmk/repo/list.toml`

See `docs/en/repo.md` for full details.

### Parallel compilation (0.2.3+)

`ThreadPool` (fixed-size thread pool in `include/ezmk/thread_pool.hpp`) for parallel source compilation. Cache records (`record.json`) loaded once before compilation, saved once after all tasks complete. Console output uses a global mutex for clean interleaved messages. Default `-j 0` auto-detects via `std::thread::hardware_concurrency()`.

### Build profiles (0.2.3+)

Predefined compile/link configs in `[compile.profile.<name>]` / `[link.profile.<name>]`. Activated via `--profile <name>`. Profiles do NOT auto-apply. Profile flags append after base flags (later overrides earlier, matching GCC/Clang behavior). Profile macros merge into base macros (profile wins on key conflict).

### Build hooks (0.2.3+) + Install hooks (0.9.9+)

Build hooks (`pre_build`/`post_build`/`on_failure`) and install hooks (`preinstall`/`postinstall`) share the same Lua sandbox execution framework. The internal function `run_lua_script_with_ctx()` (`src/lua_api.cpp`, 0.9.10+) provides a unified pipeline: sandbox creation → script loading → chunk execution → `run(ctx)` invocation → exit code extraction. Both `run_hook_script()` and `run_install_hook_script()` are thin wrappers.

**Install hook detection** (0.9.10+): `detect_install_script()` is a public API in `include/ezmk/pkg.hpp` — priority: `.lua` → platform script (`.ps1`/`.bat` on Windows, `.sh` on POSIX). Returns `std::optional<fs::path>`.

Build hooks receive `ctx` table (`ctx.output`, `ctx.project_root`, `ctx.profile`); install hooks receive (`ctx.pkg_name`, `ctx.pkg_root`, `ctx.install_path`, `ctx.scope`, `ctx.pkg_version`, `ctx.pkg_type`). Hooks run in sandboxed Lua environments. Script not found → warn + skip (non-fatal). Hooks only apply to user projects/packages, not during package compilation.

### File watcher (0.2.3+)

Cross-platform `FileWatcher` class (`include/ezmk/file_watcher.hpp`): Windows uses `ReadDirectoryChangesW` + IOCP; Linux uses `inotify`; macOS uses `kqueue`. Watch mode (`ezmk project watch`) monitors `src_dirs`, `include_dirs`, and `ezmk.toml`. 300ms debounce coalesces rapid edits. `ezmk.toml` changes trigger cache clear + full rebuild. Build failures don't exit the watch loop.

### Build caching

Content-hash-based incremental compilation. See `docs/en/@cache.md`. Algorithm:
1. Hash the source file content
2. Compare against `record.json` entry (source hash + compile flags)
3. Recursively check all `#include`d header hashes
4. All match → cache hit, reuse `.o`; otherwise → recompile and update record

Atomic writes: `.o` and `record.json` written to temp files first, then `rename` to avoid corruption on mid-build failure. Cache stored in `.ezmk/cache/obj/` and `.ezmk/cache/record.json`. `--disable-cache` forces recompilation but still updates the cache afterward.

### Safety requirements

See `docs/en/@safety.md`:
- Global package installs require secondary confirmation
- Installations that would overwrite existing files require secondary confirmation
- Install hook scripts (`.lua`/`.sh`/`.ps1`/`.bat`) are detected via `detect_install_script()` (`include/ezmk/pkg.hpp`, public API since 0.9.10)

### Lua scripting & utils (0.2.0+)

Embedded Lua 5.4.7 (static-linked). `ezmk utils <name>` runs Lua-based tools from `type = "utils"` packages. See `docs/en/utils.md` for the full plugin API.

**Sandbox:** `io` and `os` removed at compile time (`linit.c`). Scripts use `ezmk.*` API. `ezmk.file_write()` denies writes outside project root. Each invocation gets a fresh sandbox environment table.

**ezmk Lua API (23 functions):**

| Category | Functions |
|---|---|
| Project info | `project_root()`, `project_name()`, `project_type()`, `project_config()`, `build_dir()` |
| Compile options | `compile_flags()`, `include_dirs()`, `link_flags()`, `link_dirs()` |
| Filesystem | `list_sources()`, `file_exists()`, `file_read()`, `file_write()` |
| Process | `run()` → `{exit_code,stdout,stderr}`, `run_capture()` |
| Logging | `info()`, `warn()`, `error()` |
| Path tools | `pkg_dir()`, `temp_dir()`, `cache_dir()` |
| JSON | `json_encode()`, `json_decode()` |
| Version | `api_version` (integer field, not a function; introduced 0.9.4) |

**Built-in tool:** `ezmk-cc` — generates `compile_commands.json` (clangd-compatible) via `ezmk utils cc`.

**Script discovery order:** project scope (`.ezmk/pkg/*/utils/`) → user scope (`~/.local/ezmk/pkg/*/utils/`) → global scope (`<install_dir>/pkg/*/utils/`).
