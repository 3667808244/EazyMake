# Package Management

---

## Package Structure

Standard library packages:

```
<pkg_dir>/
    include/
        *.h
        *.hpp
    src/
        *.c
        *.cpp
        *.cxx
    ezmk.toml
```

Utils tool packages (`type = "utils"`, see `utils.md` for details):

```
<utils_pkg>/
    ezmk.toml         # type = "utils"
    utils/            # Lua scripts (required)
        <name>.lua
    include/          # optional
    src/              # optional
```

---

## Package Configuration (`ezmk.toml`)

### `[project]` Section

| Field | Type | Required | Default | Description |
|------|------|------|--------|------|
| `name` | string | **Yes** | — | Package name (lowercase, hyphens allowed, e.g. `"my-lib"`) |
| `version` | string | **Yes** | — | Semantic version, e.g. `"1.2.3"` |
| `type` | string | No | `"executable"` | Package type (see value table below) |
| `language` | string | No | `"C++17"` | Format: `<lang><version>`, e.g. `"C11"`, `"C++17"`, `"C++20"`. **1.3.1+** range syntax (`">=C++11"` / `"C++11..C++17"`) declares a minimum standard; install-time compatibility check warns when the package's minimum exceeds the consuming project's standard |
| `header_only` | bool | No | `false` | **0.9.7+** Header-only library (no `src/`, skip compilation) |
| `precompiled` | bool | No | `false` | **0.9.7+** Precompiled package (`lib/` provides pre-built `.a`, no `src/`). See [Package Authoring Guide](package_authoring.md#33-precompiled-package-precompiled--true-097) |
| `precompiled_strict` | bool | No | `false` | **1.2.0-dev.10+** Precompiled-package strict mode: toolchain fallback (L2/L1, possibly ABI-incompatible) becomes a fail-fast error |

The `type` field supports the following values:

| Value | Description |
|---|---|
| `"executable"` | Executable (default) |
| `"static"` | Static library |
| `"shared"` | Shared library |
| `"utils"` | Tool package (provides `ezmk utils` subcommands, Lua-based) |

> **Why header-only and precompiled packages?** Both skip a local compile:
> `header_only = true` ships headers only, so a pure-header library installs
> instantly; `precompiled = true` ships a ready-built `.a` for heavy libraries.

### `[depends]` Section

| Field | Type | Description |
|------|------|------|
| `lib` | string[] | Hard dependency library name list. Missing → install fails |
| `want` | string[] | **0.2.2+** Optional dependency library name list. If present during install, treated as a normal dependency; if missing, skipped. Missing at build time → warn + define `EZMK_LIB_MISS_<NAME>` macro |

> **Why both `lib` and `want`?** A hard dependency (`lib`) must be installed or the
> build fails. An optional one (`want`) degrades gracefully — if missing, ezmk warns
> and defines `EZMK_LIB_MISS_<NAME>` so your code can fall back instead of failing.

---

## Package Install Paths and Cache Directory

| Install Mode | Path                       |
| ------------ | -------------------------- |
| Global       | `<ezmk_install_dir>/pkg/`  |
| User         | `~/.local/ezmk/pkg/`       |
| Project      | `<project_dir>/.ezmk/pkg/` |

Cache is always stored in `<project_dir>/.ezmk/cache/`, keyed by compile flags and file content.

---

## Package Compilation

Each standard library package is compiled into a `*.a` file, following the dependency chain.

For `type = "utils"` tool packages:
- If any `[compile].src_dirs` directory (default `src/`) contains source files: compile → `build/*.a`, and register Lua tools under `utils/`
- If all `src_dirs` directories are missing or empty: skip compilation, only extract and register Lua tools

**`[compile]` settings take effect for packages (1.2.0-dev.9+)**:

- **`src_dirs`** (default `["src"]`): package sources are collected from the configured directories — multiple directories supported (e.g. `["src", "generated"]`), missing directories warn + skip, duplicate filenames deduplicated — exactly like the project build's `collect_sources`. Packages are **always** compiled as static libraries; `[project].type` never triggers the `main.cpp` check (the docs' default `type = "executable"` works fine).
- **`include_dirs`** (default `["include"]`): resolved relative to the package root as `-I` for the package's own compile, order-preserving dedup against the default `include/`, missing directories skipped; the consumer side is wired the same way — every `<pkg>/<include_dir>` is added to the consumer's compile `-I` paths.
- **Empty sources are fatal**: a degenerate package that is not header_only / precompiled / utils yet has no source files fails the install (`no source files` / `src/ directory not found`) instead of silently producing an empty library.

Circular dependencies or missing packages cause an error.

---

## Install Hook Scripts (0.2.1+, Lua support in 0.9.9+)

A `script/` directory may be placed at the package root, containing install lifecycle hooks.

**Directory structure**:

```
<pkg_dir>/
    script/
        preinstall.lua    # Executed after extraction, before install (cross-platform, 0.9.9+, **recommended**)
        preinstall.sh     # Executed after extraction, before install (Linux/macOS, legacy fallback)
        preinstall.ps1    # Executed after extraction, before install (Windows, legacy fallback)
        preinstall.bat    # Executed after extraction, before install (Windows legacy fallback)
        postinstall.lua   # Executed after install completes (cross-platform, 0.9.9+, **recommended**)
        postinstall.sh    # Executed after install completes (Linux/macOS, legacy fallback)
        postinstall.ps1   # Executed after install completes (Windows, legacy fallback)
        postinstall.bat   # Executed after install completes (Windows legacy fallback)
```

**Detection priority (0.9.9+)**:
1. `.lua` — cross-platform, highest priority (if present, used directly)
2. Platform-specific fallback: `.ps1` → `.bat` (Windows) or `.sh` (Linux/macOS)

> **Why prefer `.lua` hooks?** One Lua script runs on every platform and is
> sandbox-safe, replacing the separate `.sh` / `.ps1` / `.bat` fallbacks. The shell
> variants stay for legacy packages that have not migrated.

**Execution flow**:
1. Extract package to temporary directory
2. Detect and execute `preinstall` script (if present):
   - **Lua scripts** (0.9.9+): ask for confirmation → execute in sandbox (no editor review needed, API is sandbox-limited)
   - **Shell scripts** (legacy): open editor for user review → ask for confirmation → execute
3. Check existing installation → secondary confirmation if overwriting
4. Compile dependencies + copy files to install directory
5. Detect and execute `postinstall` script (if present) → same flow as step 2

- If the user declines script execution, installation continues (skipping that phase)
- If script execution fails (exit != 0), the user may choose to continue or abort

### Lua Hook Scripts (0.9.9+)

Lua install hooks provide a cross-platform, sandbox-safe alternative to shell scripts. They share the same `ezmk.*` API as utils and build hooks (see [Utils documentation](utils.md)).

**Entry point**: Each Lua hook must define a `run(ctx)` function that returns an integer exit code (0 = success, non-zero = failure).

```lua
-- script/preinstall.lua
function run(ctx)
    ezmk.info("preinstall hook for " .. ctx.pkg_name)

    -- Example: backup existing config file
    if ezmk.file_exists(ctx.install_path .. "/config.ini") then
        ezmk.warn("found existing config.ini, backing up...")
        local backup = ctx.install_path .. "/config.ini.bak"
        local ok = ezmk.file_write(backup,
            ezmk.file_read(ctx.install_path .. "/config.ini"))
        if not ok then
            ezmk.error("failed to backup config.ini")
            return 1
        end
    end

    return 0
end
```

**Context table `ctx`**:

| Field | Type | Description |
|---|---|---|
| `ctx.pkg_name` | string | Package name (from `ezmk.toml` `[project].name`) |
| `ctx.pkg_root` | string | Extracted package root directory (absolute path) |
| `ctx.install_path` | string | Target installation directory (absolute path) |
| `ctx.scope` | string | Installation scope: `"project"` / `"user"` / `"global"` |
| `ctx.pkg_version` | string | Package version (from `ezmk.toml` `[project].version`) |
| `ctx.pkg_type` | string | Package type: `"executable"` / `"static"` / `"shared"` / `"utils"` |

**Safety**: Lua hooks run in a sandboxed environment (no `os.execute`, no `io.open`, and no file-loading/introspection functions such as `dofile`/`loadfile`/`load`/`require`/`debug`/`package`). All system access must go through `ezmk.*` API functions. Unlike shell scripts, Lua hooks do not require editor review — the sandbox boundary limits what the script can do. **Install hooks are package code and, since 1.2.0-dev.11, are gated by `[utils.permissions]` on the same footing as utils scripts**: the three controlled-access categories (`file_read`/`file_write`/`run`) are evaluated as deny > allow > ask (see [utils.md Permission Management](utils.md#permission-management-version--025)); packages that declare permissions converge their capability accordingly, while legacy packages without `[utils.permissions]` keep unrestricted behavior (a deprecation warning is printed on the first call to a controlled API).

**Note**: If a `run()` function is not defined, ezmk prints a warning and skips the hook (continues installation).

---

## Scope Parameters

`-p` : project scope
`-u` : user scope
`-g` : global scope

`-p`, `-u`, `-g` flags can be combined, e.g. `-pug`.

Operations search in the order of the specified flags.

Note: `ezmk pkg install` does not support multiple scopes.

---

## Package Sources

An official default repository is pre-registered during installation so packages can be installed by name (`ezmk pkg install fmt -u`). Packages can also be installed from the following sources:

> **Why accept a file, URL, or name?** One `install` command covers every way a
> package can be obtained — a local archive, a remote download, or a bare package
> name looked up in the registered repositories. Name-based install is the daily
> path once a repo is registered; the file/URL forms handle everything else.

### Local Files

```bash
ezmk pkg install -p ./foo-0.1.0.zip
ezmk pkg install -u ~/downloads/bar-1.2.0.tar.gz
```

### Install from a Directory (1.2.0-dev.7+)

If the argument is an **existing directory**, the package is installed straight
from that source directory — the structure must follow the package spec
(`include/` + source directories + `ezmk.toml`, or precompiled `lib/` /
header-only). The source directories default to `src/` and can be customized
via `[compile].src_dirs` (1.2.0-dev.9+, both validation and compilation honor
`src_dirs`). Great
for developing/debugging a local package without first packing an archive:

```bash
ezmk pkg install -p ./mylib          # ./mylib is the package source directory
ezmk pkg install -u ~/dev/bar        # absolute paths work too
```

- Directory installs skip extraction and SHA-256 verification (there is no
  archive to check); passing `--sha256` explicitly prints a notice that it is
  skipped.
- Install path, scope (`-p/-u/-g`), install hooks, and dependency resolution are
  identical to archive installs.
- Unpacked package directories under `packages/` in a local `ezmk-repo` checkout
  can be installed this way too.

### Git Repository URL (1.4.1+)

A git repository URL is cloned and installed as a package — the repository root
must be a valid package (an `ezmk.toml` with `include/` + source dirs, or a
precompiled/header-only layout), i.e. the repo root is the package root:

```bash
# SSH (scp-style)
ezmk pkg install -p git@github.com:user/mylib.git
# https / explicit protocol
ezmk pkg install -p https://github.com/user/mylib.git
ezmk pkg install -p file:///tmp/mylib.git          # local repo
```

- **Ref selection**: `#<ref>` fragment or `--branch <ref>` (branch, tag, or full
  commit SHA). Priority: `--branch` > URL fragment > default branch.
  - Branch/tag/default: shallow clone (`--depth 1`).
  - Commit SHA: full clone + checkout of that exact commit (a shallow clone may
    not reach it).
  - Bare URLs without a scheme (`github.com/user/mylib.git`) get `https://`
    prepended automatically.
- **Detection**: the argument is treated as a git source when it starts with
  `git@`, uses the `git://` / `file://` scheme, or ends with `.git` (after any
  `#ref` fragment). Archive URLs (`.zip` / `.tar.gz`) never match.
- **`git://` is plaintext**: a warning + confirmation is shown (skipped with
  `-y`) before cloning, mirroring `http://` downloads.
- **Integrity**: git sources are pinned by the **commit SHA**, not a sha256
  archive hash. An explicit `--sha256` prints a skip notice.
- **Lockfile**: `ezmk.lock` records `source = "git"`, `source_url`, and the
  pinned `commit`; `ezmk pkg install <url> --locked` re-clones the recorded
  commit and refuses to install if the upstream ref was force-pushed or moved
  (`lock_commit_mismatch`).

### URL Download

```bash
ezmk pkg install -p https://example.com/packages/foo-0.1.0.zip
ezmk pkg install -g example.com/packages/bar-1.2.0.tar.gz   # protocol omitted, defaults to https://
```

URL format notes:
- Full URL: `https://<host>/<path>/<pkg>.zip` or `.tar.gz`
- Omitted protocol: `<host>/<path>/<pkg>.zip` → auto-prepended with `https://`
- Supported protocols: `https://`, `http://`
- URL auto-detection: if the argument contains `://`, or contains both `.` and `/` and is not a locally existing file, it is treated as a URL
- Downloaded to `.ezmk/temp/`, extracted and installed; temp files deleted after install

> **Why auto-detect URLs?** The heuristic (contains `://`, or has both `.` and `/`
> and is not a local file) lets `pkg install` tell a URL apart from a file path, so
> the protocol can be omitted — `example.com/path/pkg.zip` defaults to `https://` —
> without a real local archive ever being mistaken for a URL.

### Repository Search (0.1.3+)

If repositories have been registered via `ezmk repo add`, packages can be installed by name without providing a full URL or file path:

```bash
ezmk repo add -p git@github.com:user/ezmk-repo.git --name my-repo
ezmk repo update
ezmk pkg install -p foo          # automatically searches for "foo" in registered repos
```

Search order:
0. Directory (if the argument is an existing directory) → install directly (1.2.0-dev.7+)
1. Git repository URL (1.4.1+) → clone → install from the cloned directory
2. Local file path / explicit URL (same as before)
3. Search by name in local cache of registered repos (project → user → global)
4. Still not found → error

> **Why search every registered repo?** So a bare package name resolves no matter
> which repo (or scope) hosts it — you never need to remember which repository
> provides a package, and a mirror works transparently as a fallback.

See `repo.md` for details.

---

## Offline / Air-gapped Usage [0.9.4+]

When working without internet access, you have three options for installing packages:

> **Why dedicated offline options?** Bundled packages moved into the official repo
> (0.9.3), so installs now normally need the network. These options restore offline
> use — a local mirror, a manual archive, or a pre-staged image.

### Option 1: Local repository mirror

Clone the repository on a connected machine and register it as a local repo on the offline machine:

```bash
# On a connected machine
git clone https://github.com/3667808244/ezmk-repo.git /path/to/ezmk-repo

# Copy to the offline machine, then:
ezmk repo add /path/to/ezmk-repo --type local
ezmk pkg install <name>
```

### Option 2: Manual archive download and install

Download the `.tar.gz` or `.zip` archive from GitHub Releases (or any source), transfer to the offline machine, then install from the file:

```bash
ezmk pkg install ./<pkg>-<version>.tar.gz --type file
```

### Option 3: Pre-staged mirror on USB / network share

Prepare a full repo mirror on portable media or a network share:

```bash
# Prepare on a connected machine
git clone https://github.com/3667808244/ezmk-repo.git /mnt/usb/ezmk-repo

# On each offline machine
ezmk repo add /mnt/usb/ezmk-repo --type local
```

> For more offline scenarios, see the [FAQ](faq.md).
