# CLI Reference

Authoritative reference for the `ezmk` command line and environment variables.
This document is the single source of truth; the README command tables are a
quick-start subset. For behavior details see the per-topic docs
(`pkg.md`, `repo.md`, `utils.md`, `config_file.md`, `cache.md`, `safety.md`).

## Synopsis

```
ezmk <command> [subcommand] [options] [arguments]
ezmk <shorthand> [options] [arguments]
```

Global options may appear on any command (see [Global options](#global-options)).

---

## Installation

### Linux / macOS / MSYS2

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

Builds from source and installs `ezmk` to `$HOME/.local/bin`. See [README](../../README.md#quick-start) for customization options and environment variables.

### Windows (native, no MSYS2)

```powershell
# Download and run the PowerShell installer:
.\install.ps1

# Or one-line remote execution:
irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex
```

Downloads the prebuilt `ezmk.exe` from GitHub Releases, verifies SHA-256, installs to `%LOCALAPPDATA%\ezmk\bin`, and configures user PATH. Supports `-Version`, `-InstallDir`, `-NoPath`, `-DryRun` parameters. See [README](../../README.md#quick-start) for details.

---

## Top-level aliases (1.1.0+)

For convenience, the most-used `project` subcommands are also available as
top-level commands:

| Alias | Full form |
|-------|-----------|
| `ezmk build` | `ezmk project build` |
| `ezmk run` | `ezmk project run` |
| `ezmk clean` | `ezmk project clean` |
| `ezmk watch` | `ezmk project watch` |
| `ezmk install` | `ezmk project install` |
| `ezmk test` | `ezmk project test` |
| `ezmk pack` | `ezmk project pack` |

Both forms are equivalent — all flags and arguments work the same way. The short
forms are recommended for daily use; the full `project <action>` forms are kept for
scripting and muscle memory.

> **Why two forms?** The aliases were introduced (1.1.0-pre.1) to lower the barrier
> for newcomers — `ezmk build` beats `ezmk project build` for daily use. The full
> form is kept so scripts and habits stay unambiguous and unbroken.

---

## `project` — build your code

| Command | Description |
|---|---|
| `ezmk project new <name> [--type <t>]` | Scaffold a new project |
| `ezmk build [build-opts]` | Incremental build (full: `ezmk project build`) |
| `ezmk run [build-opts] [-- <program args>]` | Build and execute (full: `ezmk project run`) |
| `ezmk clean` | Remove cache and temp files (full: `ezmk project clean`) |
| `ezmk install [install-opts]` | Install build artifacts to prefix, 1.1.0+ (full: `ezmk project install`) |
| `ezmk pack [--output <dir>] [--precompiled] [--format <tar.gz\|zip>]` | Package as `.tar.gz` (default: **source package**, platform-independent; `--precompiled` produces a prebuilt archive, `static` only; `--format zip` picks the zip archiver, 1.3.5+), 1.1.0+ (full: `ezmk project pack`) |
| `ezmk watch [build-opts] [--no-build-on-start]` | Watch sources and auto-rebuild (full: `ezmk project watch`) |
| `ezmk test [test-opts]` | Build and run project tests, 1.1.0+ (full: `ezmk project test`) |
| `ezmk project cc [-o <path>] [--profile <p>]` | Generate `compile_commands.json` for clangd/LSP, 1.2.0+ |
| `ezmk project export cmake [flags]` | Generate `CMakeLists.txt` from `ezmk.toml` (single-direction snapshot), 1.2.0+ |
| `ezmk project import [--from <fmt>] [--overwrite]` | Import a CMake project into `ezmk.toml` (experimental, single-direction snapshot), 1.2.0+ |

**`--type <t>`** (for `new`): `executable` (default) · `static` · `shared` · `utils`.

**`ezmk.toml` upward search (1.2.0-dev.7+):** every command that needs a project
config (`build` / `run` / `clean` / `install` / `pack` / `watch` / `test` /
`project cc` / `project export`, …) looks for `ezmk.toml` by walking **up at most
5 parent directories** from the current directory — so `ezmk build` / `ezmk test`
work straight from a subdirectory, just like `git`. The found directory is the
project root; artifacts, caches and `.ezmk/*` (cache / pkg / repo) all live under
it. If none is found within 5 levels, commands that require a config fail with a
clear message (`clean` falls back to the current directory). Creation commands
like `project new` / `project import` do not depend on an existing config.

**`project import`** converts the current directory's `CMakeLists.txt` into a
fresh `ezmk.toml` (single-direction snapshot — after import, `ezmk.toml` is the
source of truth). `--from` defaults to `cmake` and is case-insensitive. It
refuses to overwrite an existing `ezmk.toml` unless `--overwrite` is given, and
aborts (without writing anything) on unsupported non-standard CMake constructs
(custom commands, generator expressions, `function()`/`macro()`,
`pkg_check_modules`). **Experimental** — verify library links and platform
macros after import. See [migrate-from-cmake.md](migrate-from-cmake.md) for
supported/unsupported constructs and manual migration steps.

**Companion runtime: `ezmk-lua` (1.2.0-dev.8+).** `ezmk project export cmake`
maps `[hooks]` `pre_build` / `post_build` to `add_custom_command` calls that
invoke the standalone `ezmk-lua` binary, which ships alongside `ezmk` in every
install channel:

```
ezmk-lua <hook.lua> [--project-root <dir>] [--profile <name>] [--output <path>]
```

It runs the hook in an **unrestricted** Lua environment (a strict superset of the
build sandbox) and builds the `ctx` table (`output` / `project_root` / `profile`)
from the CLI flags. The generated CMake locates it via
`find_program(EZMK_LUA ezmk-lua)` and falls back to a `message(WARNING)` when it
is not installed (hook post-processing is skipped, never fatal). `on_failure`
has no CMake equivalent and is not exported. See the `hooks` section in
[config_file.md](config_file.md) for the full mapping.

**`build-opts`** (shared by `build` / `run` / `watch`):

| Flag | Purpose |
|---|---|
| `--disable-cache` | Force recompilation (cache is still updated afterward) |
| `--verbose` / `-v` | Show full compile commands and cache hits |
| `-j <N>` / `--jobs <N>` | Parallel compile jobs; `0` = auto (`hardware_concurrency`), the default |
| `--profile <name>` | Apply a build profile from `[compile.profile.<name>]` / `[link.profile.<name>]` |
| `--auto-update` | Run `ezmk repo update --pug` before building (default off) |

> **Why `-j 0` is the default?** Auto-parallelism (`hardware_concurrency`) gives a
> good speedup with zero configuration. Note also that `--disable-cache` still
> *updates* the cache afterward — it forces one clean recompile, not a permanently
> cold cache, so the next build is fast again.

> **Build timing detail (1.2.0+):** `ezmk build -v` always prints a per-file
> compile-time breakdown, slowest first. Without `-v`, a build that takes over
> 5s automatically prints the 10 slowest units. No config, no extra flags — only
> actually-compiled (non-cached) files are listed, and the single-threaded path
> shows just the total time.

**`new`-only flags:**

| Flag | Purpose |
|---|---|
| `--disable-git-init` | Skip `git init` |
| `--disable-gitignore` | Skip `.gitignore` generation |

**Generated template (1.2.0+):** `project new` scaffolds a template with built-in
`[compile.profile.debug]` (`-g -O0` / `/Zi /Od`) and `[compile.profile.release]`
(`-O2 -DNDEBUG` / `/O2 /DNDEBUG`) profiles and sets `default_profile = "debug"` — so a
plain `ezmk build` is debuggable out of the box, and `ezmk build --profile release`
switches to the optimized build. Base `[compile].flags` are warnings-only (`-Wall -Wextra`).

**Per-type source templates (1.2.1+):** `project new` now generates sources
differently depending on `--type`:

| `--type` | Generated files |
|---|---|
| `executable` (default) | `src/main.cpp` (Hello world entry, unchanged) |
| `static` / `shared` | `include/<name>.hpp` (sample public API) + `src/<name>.cpp` (implementation), **no `main.cpp`** |
| `utils` | No C++ code — only the `utils/` directory (for Lua scripts) |

Library templates **keep the original project name in file names** (`my-lib` →
`include/my-lib.hpp` + `src/my-lib.cpp`), and the C++ **namespace replaces `-` / `.` /
spaces with `_`** (`my-lib` → `namespace my_lib`); headers are guarded with
`#pragma once`. The generated `ezmk.toml` also ends with a **commented-out `[test]`
example section** (`# [test]` / `# framework` / `# dirs` / `# default_profile` /
`# include_dirs` / `# link_targets`) — uncomment and fill it in to use `ezmk test`;
pure comments have zero parse impact, and the fields match the `[test]` config
exactly (including the 1.2.0-dev.12 additions; the deprecated `flags` is
deliberately not shown).

**`watch`-only flags:**

| Flag | Purpose |
|---|---|
| `--no-build-on-start` | Skip the initial build; wait for the first change |
| `--run` / `-r` | **1.3.4+** Run the executable after each **successful** rebuild (blocking); watch resumes when the program exits |

**`watch --run` (1.3.4+):** after every successful rebuild the freshly built executable runs **blockingly** on the watcher thread — change detection is naturally paused while the program runs and resumes when it exits (zero process management). Non-zero exits only **warn** (watch keeps looping). Only valid for `executable` projects (`static`/`shared`/`utils` + `--run` → startup error). The **initial** build does not run — the first run happens after the first change. Behavior without `--run` is unchanged. Ctrl+C terminates the child and watch together (same foreground process group). A long-running program (server/GUI) pauses watching until it exits — press Ctrl+C to stop.

**`install`-only flags:**

| Flag | Purpose |
|---|---|
| `--prefix <path>` | Override `[install].prefix` |
| `--dry-run` | Show what would be installed without copying |
| `--no-headers` | Skip header installation |
| `--no-data` | Skip data file installation |

**`pack`-only flags:**

| Flag | Purpose |
|---|---|
| `--output <dir>` | Output directory (default `.`). Only valid for `type = "static"` projects |
| `--format <tar.gz\|zip>` | **1.3.5+** Archive format (default `tar.gz`, unchanged; `zip` uses the vendored miniz writer). Both formats contain the same files (identical stage contents) and ship a `<archive>.sha256` sidecar |

> **`pack` writes a `.sha256` sidecar (1.3.5+):** every successful pack writes `<archive>.sha256` (`<hash>  <filename>`) — same format for `tar.gz` and `zip`; a pure add-on that does not affect existing consumers. `.deb` / `.rpm` are deliberately out of scope (use `fpm` + `ezmk project install --prefix <staging>`).

**`test`-only flags:**

| Flag | Purpose |
|---|---|
| `--framework` / `-f <catch2\|ezmk>` | Temporarily override `test.framework` |
| `--filter <pattern>` | Filter test names (Catch2: test name; ezmk: filename glob) |
| `--profile <name>` | **1.2.0-dev.12+** Temporarily override `test.default_profile` (symmetric with `ezmk build --profile`) |
| `--verbose` / `-V` | Show detailed output for every test (even passing ones) |
| `--report <fmt>[:<path>]` | **1.3.2+** Write a machine-readable test report. Format `junit` (default path `<proj_root>/.ezmk/test-results/junit.xml`); a custom relative `<path>` resolves against the project root. **Catch2** additionally accepts any Catch2 reporter name (`json`, `xml`, `sonarqube`…) — forwarded as `-r <fmt>::out=<file>`, with the console summary untouched. The **EZMK** built-in framework supports only `junit` (other formats error with a hint to use Catch2). The report is an add-on: it never changes the test exit code. `--filter` and `--report` compose (the report covers only the filtered cases). `ezmk workspace test --report ...` forwards the flag to every member, each writing its own report file |

`ezmk run` (and its full form `ezmk project run`) passes everything after `--` to the built program.

> **`test` always builds first (1.3.0+):** `ezmk test` always runs an **incremental build** before collecting/compiling/running tests — near-zero cost when artifacts are fresh (one info line + cache hit); this removes the stale-artifact trap where tests ran against old objects after sources changed. `utils` projects have no compiled artifact and skip the build.

> **`pack --precompiled` always builds first (1.3.0+):** `--precompiled` runs an **incremental build** before packing, so the archive is always packed from fresh artifacts; the post-build artifact check stays — a build that fails to produce the archive does not silently pack a stale one. The default source package (platform-independent, compiled on the consumer side) does not trigger a build.

---

## `workspace` — manage a set of projects (1.3.0+)

A workspace is a **collection of independent projects (members) under one directory**: `ezmk-workspace.toml` declares the members and `ezmk workspace build / test / clean` manages them in batch. Members may declare **one-way acyclic dependencies**; builds are topologically ordered with intra-layer parallelism and sibling artifacts injected automatically — covering the most common monorepo shape: a shared base library plus several executables.

| Command | Description |
|---|---|
| `ezmk workspace list` | List members (name / type / workspace deps); invalid members show their reason |
| `ezmk workspace build [-j N] [--stop-on-error] [--member <name>...]` | Build all members topologically (dependency layers first, parallel within a layer) |
| `ezmk workspace test [-j N] [--stop-on-error] [--member <name>...] [--report <fmt>[:<path>]]` | Run member tests; members without tests are skipped (not an error). **1.3.2+** `--report` is forwarded to every member — each writes its own report file |
| `ezmk workspace clean [--member <name>...]` | Clean members in reverse dependency order (same semantics as single-project `ezmk clean`: caches/temp only, `build/` artifacts kept) |

The config file is **`ezmk-workspace.toml`** (independent of `ezmk.toml`; a root may be both a project and a workspace):

```toml
[workspace]
name = "my-ws"                    # optional
members = ["apps/tool-a", "apps/tool-b", "libs/strutil"]   # required, non-empty

[workspace.options]
default_jobs = 4                  # optional, default 0 = auto (hardware concurrency)
stop_on_error = false             # optional, default false
```

Each member keeps its own `ezmk.toml`; declaring a sibling dependency is one line (nothing else changes in the member's config):

```toml
[depends]
workspace = ["strutil"]           # sibling member (basename or full relative path)
```

Dependency constraints: **one-way acyclic** (cycles / self-loops rejected at config time) + the depended-on member must be `type = "static"` (its `build/lib<name>.a` is reused); no versions (develop-and-use; version/snapshot semantics belong to packages). See the `[depends]` section of [`config_file.md`](config_file.md).

**`-w` / `--workspace` redirect (on `build` / `test` / `clean`):** `ezmk build -w` ≡ `ezmk workspace build` (the workspace root is located upward from any subdirectory). It is **not** "build both the project and the workspace"; without `-w`, a member-internal `ezmk build` keeps single-project semantics (builds only the current member, injecting **already-existing** sibling artifacts).

**`--member <name>` = target member + dependency closure:** builds only the named member and its **dependencies** (dependencies build first in topological order so artifacts are fresh); `--member apps/tool-a` and `--member tool-a` (basename) are equivalent. To build **a single member without the closure** → `cd <member> && ezmk build`. An unknown member is an error.

**`--stop-on-error` (`build` / `test`):** after the first failure the scheduler **stops dispatching new tasks** — not-yet-started members of the current layer and all later layers are marked `skipped`; members already running **finish naturally, never killed**. The summary reports succeeded / failed / skipped; any failure gives a non-zero exit. Without the flag, all members run and failures are summarized. `clean` does **not** support the flag (no dependency semantics).

**`-j N` / `--jobs N`:** intra-layer parallelism; precedence `-j` > `[workspace.options].default_jobs` > hardware concurrency.

**Sibling artifact injection (member self-discovery, zero environment variables):** the member's build process locates the workspace, resolves its own `[depends] workspace`, and injects `-I <ws>/<m>/include` (only if it exists) + `-L <ws>/<m>/build -l<m>` (only if `lib<m>.a` exists) — **no `EZK_WS_*` environment variable is read**, so injection size does not grow with workspace size. The injected `-I` enters the compile signature: injected parameters change → dependents recompile automatically. When a sibling artifact is missing (standalone build), a hint to run `ezmk workspace build` first is printed and the build continues — a missing link will fail naturally.

**Incremental semantics:** changing a library `.cpp` → the library rebuilds + dependents **relink only**; changing a library `.h` → dependents **recompile automatically** (their depfile tracks the injected headers). No manual `clean` needed.

**Pure container root:** a directory with only `ezmk-workspace.toml` (no `ezmk.toml`) makes `ezmk build` print a hint to use `ezmk workspace build` (or `ezmk build -w`); when the root is also a project, behavior is unchanged.

---

## `pkg` — manage packages

| Command | Description |
|---|---|
| `ezmk pkg install [scope] [pkg-opts] <file\|url\|name>` | Install a package |
| `ezmk pkg remove [scope] <name>` | Remove a package |
| `ezmk pkg search [scope] <name>` | Search registered repos |
| `ezmk pkg info [scope] <name>` | Show package details |
| `ezmk pkg list [scope]` | List installed packages (0.2.3+) |
| `ezmk pkg update [scope] <name>` | Update a package from repos (0.2.3+) |
| `ezmk pkg update [scope] --all` | Update all installed packages (0.2.4+) |

**`install`-only options:**

| Flag | Purpose |
|---|---|
| `--sha256 <hash>` | Verify archive integrity before installing |
| `-y` / `--yes` | Skip confirmation prompts (non-interactive) |
| `--locked` | Install only against the existing `ezmk.lock` — error on mismatch (1.1.0+) |
| `--no-lock` | Skip `ezmk.lock` generation (1.1.0+) |

See [`pkg.md`](pkg.md) for the package format and dependency resolution.

---

## `repo` — manage repositories

| Command | Description |
|---|---|
| `ezmk repo add [scope] <git_url\|path> [--name <n>] [--branch <b>]` | Register and clone |
| `ezmk repo remove [scope] <name>` | Unregister and delete cache |
| `ezmk repo update [scope] [<name>]` | `git pull` to refresh (all if `<name>` omitted) |
| `ezmk repo list [scope]` | List registered repos |
| `ezmk repo info [scope] <name>` | Show repo details (packages, versions) |

Local directories are supported via `type = "local"`. See [`repo.md`](repo.md).

**Official default repository:** `install.sh` automatically pre-registers the official
repo (user scope, `--name official`) so `ezmk pkg install` works by name out of the box.
Set `EZMK_NO_DEFAULT_REPO=1` to skip this during install.

> **Why pre-register a default repo?** So `ezmk pkg install <name>` works right
> after install, before the user has set up any repository. The opt-out
> (`EZMK_NO_DEFAULT_REPO=1`) keeps offline/self-hosted installs clean.

| URL | Target |
|-----|--------|
| `https://github.com/3667808244/ezmk-repo.git` | GitHub (global) |
| `https://gitee.com/egglzh/ezmk-repo.git` | Gitee mirror (China) |

Manual registration (if skipped during install, or to add the mirror as a fallback):

```bash
ezmk repo add -u https://github.com/3667808244/ezmk-repo.git --name official
ezmk repo update -u official
```

The registration is user-scoped (`-u`) so it can be removed with `ezmk repo remove -u official`.

---

## `utils` — Lua-based tools (0.2.0+)

| Command | Description |
|---|---|
| `ezmk utils <name> [args...]` | Run a Lua tool from an installed `type = "utils"` package |

Everything after `<name>` is passed through to the tool. Tools are looked up in
project → user → global scope.

## `example` — built-in examples (1.2.3+)

| Command | Description |
|---|---|
| `ezmk example` / `ezmk example list` | List all built-in examples (name + one-liner) |
| `ezmk example <name> [-o <dir>]` | Scaffold an example into `./<name>/` (or `<dir>/<name>/`) |

Six examples ship inside the binary (hello / greeter / with-packages / with-tests /
with-hooks / cmake-interop), one-to-one with the tutorial chapters, embedded at
build time from the repo's `examples/` source dir — **offline-ready** and versioned
with the binary. Each scaffold is a complete buildable project:
`cd <name> && ezmk build` (with-packages installs its dependency on first build —
network required; with-tests uses the built-in framework, zero deps). An existing
target directory or an unknown name is an error that lists the available examples.
Index: `examples/README.md` in the repo.

### Official tools (`ezmk-official-utils` package, 1.1.0+)

The installer automatically pre-installs `ezmk-official-utils` (global scope), which
provides the following tools:

| Command | Description |
|---|---|
| `ezmk utils cc [--output <path>]` | Generate `compile_commands.json` (clangd-compatible) — **deprecated since 1.2.0**, use `ezmk project cc` (removed in 2.0.0) |
| `ezmk utils link add <name> <path>` | Add a `.ezmk/links.json` link |
| `ezmk utils link remove <name>` | Remove a link |
| `ezmk utils link list` | List all links |
| `ezmk utils link show <name>` | Show link details |
| `ezmk utils gen-build-package [--output <dir>] [--name <name>]` | Generate a self-contained `.tar.gz` build package |

Manual install: `ezmk pkg install -g ezmk-official-utils -y`

### `.ezmk/links.json` and `@link:` syntax (1.1.0+)

The `.ezmk/links.json` file in the project root defines cross-directory link mappings
(name → relative path) for sharing source files across projects. Reference them in
`ezmk.toml` via the `@link:<name>` syntax:

```toml
[compile]
src_dirs = ["src", "@link:shared/src"]
include_dirs = ["include", "@link:shared/include"]
```

Chain resolution (A→B→C, depth limit 10) and cycle detection are supported. Link
values must be relative paths (no absolute paths) to keep projects portable.

> **Why relative paths only?** Links may point outside the project root; absolute
> paths would embed machine-specific locations and break portability when the
> project is shared or moved. The depth limit and cycle detection guard against
> misconfigured link chains.

See [`utils.md`](utils.md) for the plugin API.

---

## `version` · `help`

| Command | Description |
|---|---|
| `ezmk version` / `-V` / `--version` / `v` | Print version |
| `ezmk help` / `-h` / `--help` / `h` | Print usage |

---

## Scope flags

| Flag | Scope | Install path |
|---|---|---|
| `-p` | Project | `<project>/.ezmk/pkg/` |
| `-u` | User | `~/.local/ezmk/pkg/` (Unix) · `%LOCALAPPDATA%\ezmk\pkg\` (Windows) |
| `-g` | Global | `<ezmk_install_dir>/pkg/` |

`pkg install` and `repo add` accept **only one** scope flag. Other commands accept
combined flags like `-pug` (equivalent to `-p -u -g`).

> **Why only one scope for `install`/`add`?** These write to a concrete location
> (project / user / global) — the target must be unambiguous. Query commands
> (`list` / `info` / `search`) are read-only, so they can aggregate across scopes
> with combined flags like `-pug`.

---

## Command shorthands (0.2.6+)

Aliases apply only at the command position (`argv[1]`); `ezmk project pn` is still an
unknown subcommand. Shorthands are typing sugar and are **not** part of zsh completion.

> **Why `argv[1]`-only and not in completion?** Shorthands are typing sugar for
> interactive use; restricting them to the command position keeps the subcommand
> namespace unambiguous. Completion shows the canonical names so the list stays
> discoverable and does not double in size.

| Alias | Expands to | Alias | Expands to | Alias | Expands to |
|---|---|---|---|---|---|
| `pn` | `project new` | `ki` | `pkg install` | `ra` | `repo add` |
| `pb` | `project build` | `kr` | `pkg remove` | `rr` | `repo remove` |
| `pr` | `project run` | `ks` | `pkg search` | `rl` | `repo list` |
| `pc` | `project clean` | `kn` | `pkg info` | `ru` | `repo update` |
| `pi` | `project install` | `kl` | `pkg list` | `ri` | `repo info` |
| `pw` | `project watch` | `ku` | `pkg update` | | |
| `pp` | `project pack` | | | | |
| `pt` | `project test` | | | | |
| `wl` | `workspace list` (1.3.3+) | `wc` | `workspace clean` (1.3.3+) | | |
| `wb` | `workspace build` (1.3.3+) | | | | |
| `wt` | `workspace test` (1.3.3+) | | | | |
| `u` | `utils` | | | `h` / `v` | `help` / `version` |

> **Workspace shorthands (1.3.3+):** `wl`/`wb`/`wt`/`wc` expand at the command
> position exactly like the p/k/r shorthands (`ezmk wb` ≡ `ezmk workspace build`;
> `ezmk workspace wb` is still an unknown subcommand). They are orthogonal to the
> `-w` redirect flag (a build/test/clean *option*). `w` alone and the `example`
> group deliberately have **no** shorthand — see the 1.3.3 plan.

---

## Option syntax (GNU conventions)

- **Long options:** `--flag=value` and `--flag value` are equivalent.
- **Short grouping:** `-pug` equals `-p -u -g`.
- **Attached values:** `-j4` equals `-j 4`.
- **Interleaving:** options and positional arguments can be freely mixed.
- **`--` terminator:** everything after `--` is a positional argument (pass-through
  for `utils` and `project run`).

---

## Global options

These may appear on any command and are consumed before per-command parsing.

### `--color=<mode>` (0.2.6+)

| Mode | Aliases | Behavior |
|---|---|---|
| `always` | `enable` | Force color (also enables VT100 on legacy Windows terminals) |
| `auto` | `default` | Color only on an interactive terminal (**default**) |
| `never` | `disable` | Disable color |

Values are case-insensitive. Both `--color=always` and `--color always` are accepted.
An explicit `always` / `never` overrides `NO_COLOR`; only `auto` honors it (matching
git/ls). Tokens after `--` are left untouched for pass-through.

> **Why does only `auto` honor `NO_COLOR`?** An explicit `--color=always|never` is a
> stronger user intent than the ambient environment variable, so it wins (matching
> git/ls). `auto` is where the environment variable takes effect.

---

## Environment variables

| Variable | Scope | Purpose |
|---|---|---|
| `EZMK_LANG` | runtime | UI language (`en` / `zh` / variant tags like `zh-TW`, 1.3.0+), overrides system detection (`src/i18n.cpp`) |
| `NO_COLOR` | runtime | Disable colored output (honored only by `--color=auto`) (`src/util.cpp`) |
| `CXX` / `CC` | runtime + build | Override compiler detection (0.1.8+) |
| `CXXFLAGS` | build | Extra compiler flags, passed through by `build.sh` |
| `EZMK_VERSION` | build | Version string baked into the binary (`build.sh`) |
| `PREFIX` | install | Install prefix; binary goes to `$PREFIX/bin` (default `$HOME/.local`) (`install.sh`) |
| `EZMK_REF` | install | git tag/branch/commit to build (`install.sh`) |
| `EZMK_NO_COMPLETIONS` | install | Set to `1` to skip zsh completion install (`install.sh`) |
| `EZMK_NO_DEFAULT_REPO` | install | Set to `1` to skip official repo pre-registration (`install.sh`) |
| `EZMK_TEST_BIN` | test | Path to the `ezmk` binary for integration tests (default `build/ezmk[.exe]`) |

**UI language variants (1.3.0+):** `EZMK_LANG` accepts BCP-47-style tags and normalizes them — `zh_CN` / `zh-CN` / `zh_CN.UTF-8` / mixed case all map to a canonical form (e.g. `zh-CN`). A variant tag (e.g. `zh-TW`) selects the corresponding variant text (`locale/zh-TW.json`); keys it does not translate **inherit** the base language. The fallback chain is **variant → base language → English**, silently degrading at any missing step — `{???}` never appears. When unset, system detection (Windows locale / POSIX `$LANG`) goes through the same normalization — a `zh-TW` system automatically gets Traditional Chinese.

---

## Related documents

- [`config_file.md`](config_file.md) — full `ezmk.toml` spec
- [`pkg.md`](pkg.md) — package format and management
- [`repo.md`](repo.md) — repository system
- [`utils.md`](utils.md) — Lua plugin API
- [`cache.md`](cache.md) — build cache algorithm
- [`safety.md`](safety.md) — security model (confirmations, sha256, sandbox)
