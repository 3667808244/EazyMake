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

The `type` field supports the following values:

| Value | Description |
|---|---|
| `"executable"` | Executable (default) |
| `"static"` | Static library |
| `"shared"` | Shared library |
| `"utils"` | Tool package (provides `ezmk utils` subcommands, Lua-based) |

### `[depends]` Section

| Field | Type | Description |
|------|------|------|
| `lib` | string[] | Hard dependency library name list. Missing → install fails |
| `want` | string[] | **0.2.2+** Optional dependency library name list. If present during install, treated as a normal dependency; if missing, skipped. Missing at build time → warn + define `EZMK_LIB_MISS_<NAME>` macro |

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

## Install Hook Scripts (0.2.1+)

A `script/` directory may be placed at the package root, containing install lifecycle hooks:

```
<pkg_dir>/
    script/
        preinstall.sh     # Executed after extraction, before install (Linux/macOS)
        preinstall.ps1    # Executed after extraction, before install (Windows)
        preinstall.bat    # Executed after extraction, before install (Windows fallback)
        postinstall.sh    # Executed after install completes (Linux/macOS)
        postinstall.ps1   # Executed after install completes (Windows)
        postinstall.bat   # Executed after install completes (Windows fallback)
```

**Execution flow**:
1. Extract package to temporary directory
2. Detect and execute `preinstall` script (if present) → open editor for user review → ask for confirmation
3. Check existing installation → secondary confirmation if overwriting
4. Compile dependencies + copy files to install directory
5. Detect and execute `postinstall` script (if present) → same review + confirmation

- Platform selection: Windows prefers `.ps1`, falls back to `.bat`; Linux/macOS uses `.sh`
- If the user declines script execution, installation continues (skipping that phase)
- If script execution fails (exit != 0), the user may choose to continue or abort

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

EazyMake has no central repository. Package archives can be provided in the following ways:

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

See `repo.md` for details.

---

## Offline / Air-gapped Usage [0.9.4+]

When working without internet access, you have three options for installing packages:

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
