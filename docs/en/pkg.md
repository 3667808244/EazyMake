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
| `language` | string | No | `"C++17"` | Format: `<lang><version>`, e.g. `"C11"`, `"C++17"`, `"C++20"` |
| `header_only` | bool | No | `false` | **0.9.7+** Header-only library (no `src/`, skip compilation) |
| `precompiled` | bool | No | `false` | **0.9.7+** Precompiled package (`lib/` provides pre-built `.a`, no `src/`). See [Package Authoring Guide](package_authoring.md#33-precompiled-package-precompiled--true-097) |

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
- If `src/` is present: compile `src/` → `build/*.a`, and register Lua tools under `utils/`
- If `src/` is not present: skip compilation, only extract and register Lua tools

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

**Safety**: Lua hooks run in a sandboxed environment (no `os.execute`, no `io.open`, and no file-loading/introspection functions such as `dofile`/`loadfile`/`load`/`require`/`debug`/`package`). All system access must go through `ezmk.*` API functions. Unlike shell scripts, Lua hooks do not require editor review — the sandbox boundary limits what the script can do.

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
1. Local file path / explicit URL (same as before)
2. Search by name in local cache of registered repos (project → user → global)
3. Still not found → error

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
