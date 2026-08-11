---
name: ezmk-user-config
description: How to configure an EazyMake project — ezmk.toml structure, sections, and common configuration patterns.
---

# EazyMake Project Configuration (User)

## `ezmk.toml` — full structure

```toml
[project]
name = "myapp"
type = "executable"         # executable | static | shared | utils
version = "1.0.0"
language = "C++17"          # C++17 | C++20 | C23 | C11 | etc.

[compile]
flags = ["-Wall", "-Wextra"]
msvc_flags = ["/W4"]        # MSVC-only flags (ignored on GCC/Clang)
include_dirs = ["include"]
src_dirs = ["src"]
ezmk_macros = true          # Enable semantic macros (default: true)
deterministic = false       # Reproducible builds (1.1.0+)

[compile.macros]
VERSION = "1.0.0"           # string macro
ENABLE_FEATURE = 1          # int macro → -DENABLE_FEATURE=1
DEBUG_MODE = false          # bool false → skipped

[link]
flags = ["-lpthread"]
msvc_flags = []
link_dirs = []
system_target = []          # System libraries to link (e.g. "pthread" → -lpthread)

[depends]
lib = ["fmt", "zlib"]       # Hard dependencies (missing → error)
want = ["optional-lib"]     # Optional dependencies (missing → warning)

[compile.profile.debug]
flags = ["-g", "-O0"]
macros = { DEBUG = 1 }

[compile.profile.release]
flags = ["-O3", "-DNDEBUG"]
macros = { DEBUG = 0 }

[link.profile.release]
flags = ["-flto"]

[hooks]
pre_build = "scripts/pre_build.lua"
post_build = "scripts/post_build.lua"
on_failure = "scripts/on_failure.lua"

[install]
prefix = "~/.local"         # Install prefix (~ expanded)
bindir = "bin"              # Relative to prefix
libdir = "lib"
includedir = "include"
sharedir = "share"

[test]
framework = "catch2"
test_dirs = ["test"]
```

## Project types

| Type | Output | Use case |
|------|--------|----------|
| `executable` | Binary (`.exe` on Windows) | CLI tools, GUI apps |
| `static` | Static library (`.a` / `.lib`) | Reusable libraries |
| `shared` | Shared library (`.so` / `.dll`) | Dynamic plugins |
| `utils` | Multiple tools | Tool collections |

## Build profiles

Profiles append flags and override macros:

```toml
[compile]
flags = ["-Wall"]              # Base flags

[compile.profile.release]
flags = ["-O3"]                # Appended: -Wall -O3

[compile.profile.debug]
flags = ["-g", "-O0"]          # Appended: -Wall -g -O0
```

**Macro override rule:** profile macros override base macros on key conflict (not merged).

Profiles are **not auto-applied** — always use `--profile <name>` explicitly.

## Dependencies

```toml
[depends]
lib = ["fmt@^10.0", "zlib@>=1.2"]  # With version constraints
want = ["optional-feature"]
```

| Field | Behavior |
|-------|----------|
| `lib` | Hard dependency. Missing → error, build stops. |
| `want` | Optional dependency. Missing → warning, build continues. |

**Version syntax:** `@1.2.3` (exact), `@^1.0` (compatible), `@~1.2` (patch), `@>=1.0` (minimum).

## Deterministic builds (1.1.0+)

```toml
[compile]
deterministic = true
source_date_epoch = 1700000000   # Optional: fixed Unix timestamp
```

When enabled:
- Debug paths use relative paths (`-ffile-prefix-map`)
- `__DATE__` / `__TIME__` use `SOURCE_DATE_EPOCH`
- Lockfile (`ezmk.lock`) is required and strictly verified

## Install layout

```toml
[install]
prefix = "/usr/local"
bindir = "bin"
libdir = "lib"
includedir = "include/myapp"
```

`ezmk project install` copies:
- `executable` → `<prefix>/<bindir>/`
- `static` → `<prefix>/<libdir>/lib<name>.a`
- `shared` → `<prefix>/<bindir>/<name>.dll` + `<prefix>/<libdir>/<name>.lib`
- Headers → `<prefix>/<includedir>/`

## Common patterns

### Header-only library

```toml
[project]
name = "mylib"
type = "static"
version = "1.0.0"
header_only = true
```

### Cross-platform flags

```toml
[compile]
flags = ["-Wall", "-std=c++17"]      # GCC/Clang
msvc_flags = ["/W4", "/std:c++17"]   # MSVC

[link]
flags = ["-lpthread"]                 # GCC/Clang: -l prefix
msvc_flags = []                       # MSVC: pthreads not needed
```
