---
name: ezmk-repo
description: How to manage packages in the official EazyMake repository — package creation workflow, index.toml format, SHA-256 verification.
---

# EazyMake Repo

## Official repository structure

```
eazymake-repo/
├── index.toml          # Package index — all available packages
├── packages/           # Package archives (.tar.gz)
│   ├── zlib-1.3.tar.gz
│   ├── fmt-10.0.0.tar.gz
│   └── ...
└── README.md
```

A repo is a **git repository**. `ezmk repo add` clones it to a local cache; `ezmk repo update` does `git pull`.

## `index.toml` format

```toml
[platform]
# Platform-specific URL prefixes (os_arch_toolchain triples)
windows_x86_64_msvc = "https://example.com/pkgs/win64-msvc/"
windows_x86_64_gcc   = "https://example.com/pkgs/win64-gcc/"
linux_x86_64_gcc     = "https://example.com/pkgs/linux64-gcc/"

[[packages]]
name = "zlib"
version = "1.3"
type = "static"
description = "Compression library"
platform = "win-x64"        # Optional: restrict to specific platform (os-arch format)
dependencies = []
sha256 = "abc123..."
url = "packages/zlib-1.3.tar.gz"

[[packages]]
name = "zlib"
version = "1.2.13"
type = "static"
description = "Compression library (older version)"
# platform omitted = available on all platforms
dependencies = []
sha256 = "def456..."
url = "packages/zlib-1.2.13.tar.gz"
```

### Platform fields

| Field | Format | Example | Scope |
|-------|--------|---------|-------|
| `[platform]` keys | `os_arch_toolchain` (triple) | `windows_x86_64_msvc` | URL prefix mapping |
| `[[packages]].platform` | `os-arch` (double) | `win-x64` | Package-level filter |

`resolve_platform_prefix()` in `repo.cpp` tries triple → double → empty for fallback.

### Package search precedence

1. `repo add` order (first-added repo searched first)
2. Within a repo: highest version matching the current platform filter
3. `platform` field empty = available on all platforms (backward compatible)

## Package creation workflow

### 1. Prepare the package structure

```
mylib-1.0/
├── ezmk.toml           # Package metadata (required — validated by pkg.cpp)
├── include/
│   └── mylib/
│       └── mylib.h
├── lib/
│   ├── libmylib.win-x64.a
│   ├── libmylib.linux-x64.a
│   └── libmylib.mac-arm64.a   # Multi-platform precompiled
└── src/                 # Or source files for source packages
    └── mylib.cpp
```

### 2. Package metadata (`ezmk.toml`)

```toml
[project]
name = "mylib"
version = "1.0"
type = "static"
language = "C++17"

[compile]
include_dirs = ["include"]

# For precompiled packages:
# precompiled = true

# For header-only packages:
# header_only = true
```

### 3. Archive

```bash
tar -czf mylib-1.0.tar.gz mylib-1.0/
```

### 4. Compute SHA-256

```bash
sha256sum mylib-1.0.tar.gz
# Or: certutil -hashfile mylib-1.0.tar.gz SHA256   (Windows)
```

### 5. Update `index.toml`

Add a `[[packages]]` entry with the computed sha256.

### 6. Commit and push

```bash
cp mylib-1.0.tar.gz packages/
git add packages/mylib-1.0.tar.gz index.toml
git commit -m "Add mylib 1.0"
git push
```

## Precompiled vs source packages

| Type | `ezmk.toml` field | Contents | Install |
|------|-----------------|----------|---------|
| Source | (none) | `src/` + `include/` | Compile from source |
| Precompiled | `precompiled = true` | `lib/` + `include/` | Copy `.a`/`.lib` directly |
| Header-only | `header_only = true` | `include/` only | Copy headers only |

### Platform-specific precompiled naming

Files in `lib/` follow the convention:

```
lib<name>.<os>-<arch>.a     e.g., libmylib.win-x64.a
lib<name>.<os>-<arch>.lib   e.g., libmylib.win-x64.lib
lib<name>.a                 Fallback (any platform, backward compatible)
```

`compile_package()` in `pkg.cpp` auto-selects the correct file for the current platform.

## `ezmk project pack` (alternative)

For EazyMake-managed static library projects, use `ezmk project pack` to automate steps 1-4:

```bash
ezmk project pack --output ./dist
ezmk project pack --output ./dist --format tar.gz   # or: zip / tgz (tgz = tar.gz alias, 1.4.0-dev.5+)
# Output: ./dist/mylib-1.0.tar.gz
# Sidecar: ./dist/mylib-1.0.tar.gz.sha256  ("<hash>  <filename>")
# Prints SHA-256 and index.toml entry template
```

- `--format` accepts `tar.gz` (default), `zip`, or `tgz` (alias for `tar.gz` since 1.4.0-dev.5); the archive name follows the chosen format.
- `pack` always writes a `<archive>.sha256` sidecar file next to the archive, in addition to printing the SHA-256 and an `index.toml` entry template.

## Repository registries

Each scope has its own `list.toml` tracking registered repos:

| Scope | Path |
|-------|------|
| Global | `<ezmk_install_dir>/repo/list.toml` |
| User | `~/.local/ezmk/repo/list.toml` (Unix) / `%LOCALAPPDATA%\ezmk\repo\list.toml` (Windows) |
| Project | `<project>/.ezmk/repo/list.toml` |

See `docs/en/repo.md` for full details.
