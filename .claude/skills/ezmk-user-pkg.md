---
name: ezmk-user-pkg
description: How to manage third-party packages in an EazyMake project — install, update, list, and remove dependencies.
---

# EazyMake Package Management (User)

## Quick reference

```bash
ezmk pkg install <name>         # Install a package
ezmk pkg install                # Install all dependencies from ezmk.toml
ezmk pkg remove <name>          # Remove a package
ezmk pkg list                   # List installed packages
ezmk pkg update <name>          # Update a package to latest
ezmk pkg search <keyword>       # Search available packages
ezmk pkg info <name>            # Show package details
```

Shorthands: `ki` (install), `kr` (remove), `kl` (list), `ku` (update), `ks` (search), `kn` (info).

## Scope flags

| Flag | Scope | Install path |
|------|-------|-------------|
| `-p` | Project (default) | `<project>/.ezmk/pkg/` |
| `-u` | User | `~/.local/ezmk/pkg/` (Unix) / `%LOCALAPPDATA%\ezmk\pkg\` (Windows) |
| `-g` | Global | `<ezmk_install_dir>/pkg/` |

```bash
ezmk pkg install -u fmt        # Install for current user (all projects)
ezmk pkg install -g zlib       # Install globally (system-wide)
ezmk pkg list -pug             # List packages in all scopes
```

> **Note:** `install` and `repo add` accept only one scope flag at a time.

## Installing dependencies

### Install all project dependencies

```bash
ezmk pkg install
```

Reads `[depends]` from `ezmk.toml` and installs all `lib` + `want` entries.

### Install a specific package

```bash
ezmk pkg install fmt           # Latest version
ezmk pkg install fmt@10.0.0    # Specific version
ezmk pkg install fmt@^9.0      # Version constraint
```

### Install with verification

```bash
ezmk pkg install --sha256 <hash>    # Verify package integrity
ezmk pkg install --locked           # Only install from lockfile (CI mode)
ezmk pkg install --no-lock          # Skip lockfile generation
ezmk pkg install -y                 # Skip confirmation prompts
```

## Lockfile workflow (`ezmk.lock`)

After installing dependencies, EazyMake generates `ezmk.lock`:

```toml
# Auto-generated — DO NOT EDIT manually
[packages.fmt]
name = "fmt"
version = "10.0.0"
type = "static"
scope = "project"
platform = "win-x64"
sha256 = "abc123..."
dependencies = []
```

**CI pattern:**

```bash
ezmk pkg install --locked     # Fails if lockfile is missing or mismatched
ezmk project build
```

**Update workflow:**

```bash
ezmk pkg update fmt            # Update to latest compatible version
# ezmk.lock is regenerated automatically
git add ezmk.lock
git commit -m "Update fmt to 10.1.0"
```

## Listing and inspecting packages

```bash
ezmk pkg list                  # Project scope
ezmk pkg list -u               # User scope
ezmk pkg list -g               # Global scope

ezmk pkg info fmt              # Show version, type, dependencies, platform
ezmk pkg search "compression"  # Search available packages
```

## Adding package repositories

```bash
ezmk repo add <url>            # Add a repository
ezmk repo add <path> --local   # Add a local directory as repo
ezmk repo list                 # List registered repos
ezmk repo update               # Pull latest index from all repos
ezmk repo remove <name>        # Remove a repository
```

Repositories are searched in the order they were added. The highest version matching the current platform is selected.

## Common scenarios

### New project setup

```bash
ezmk project new myapp
cd myapp
# Edit ezmk.toml → add [depends] entries
ezmk pkg install               # Install all dependencies
ezmk project build
```

### Adding a new dependency

```bash
ezmk pkg install yaml-cpp
# Manually add to ezmk.toml [depends].lib
git add ezmk.toml ezmk.lock
```

### Updating all dependencies

```bash
ezmk pkg update --all          # Update all installed packages
ezmk project build             # Verify everything still builds
```
