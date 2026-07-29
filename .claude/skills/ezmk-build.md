---
name: ezmk-build
description: How to compile EazyMake from source — build.sh, manual g++ commands, and platform differences.
trigger:
  - glob: build.sh
  - glob: src/**/*.cpp
  - glob: include/**/*.hpp
  - glob: scripts/embed_locale.py
  - glob: scripts/embed_logo.py
---

# EazyMake Build

## Quick build

```bash
bash build.sh
```

This generates locale data (`scripts/embed_locale.py` → `src/locale_data.cpp`) + ASCII logo header (`scripts/embed_logo.py` → `include/ezmk/logo.gen.h`) + version header (`include/ezmk/version.hpp`), then compiles all sources.

Use `bash build.sh -v` for verbose output (full compile command and flags).

## Manual build

### Windows (MSYS2)

```bash
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/ezmk -lwinhttp -static
```

### Linux

```bash
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/ezmk -static
```

### macOS

```bash
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/ezmk
```

> **Note:** macOS does not fully support static linking — `-static` is omitted.

## Build output

| Output | Path |
|--------|------|
| Binary | `build/ezmk` (Linux/macOS) / `build/ezmk.exe` (Windows) |
| Version header | `include/ezmk/version.hpp` (auto-generated, DO NOT EDIT) |
| Logo header | `include/ezmk/logo.gen.h` (auto-generated) |
| Locale data | `src/locale_data.cpp` (auto-generated from `locale/en.json` + `locale/zh.json`) |

## Key compile flags

| Flag | Purpose |
|------|---------|
| `-std=c++17` | C++17 standard |
| `-DLUA_COMPAT_5_3` | Lua 5.3 API compatibility for embedded Lua 5.4 |
| `-lwinhttp` | Windows HTTP client (package downloads) — MSYS2 only |
| `-static` | Static linking (Linux/MSYS2) |

## Platform differences

| Aspect | Windows (MSYS2) | Linux | macOS |
|--------|-----------------|-------|-------|
| Compiler | `g++` (MSYS2) | `g++` | `g++` (Apple Clang) |
| Extra libs | `-lwinhttp` | none | none |
| Static link | `-static` | `-static` | omitted |
| Binary ext | `.exe` | (none) | (none) |

### Overriding the compiler

Set `CXX` and `CC` environment variables before running `build.sh`:

```bash
CXX=clang++ CC=clang bash build.sh
```

## Common issues

1. **"Python not found" warning** — locale data and logo will be empty stubs. Install Python 3 and rebuild.
2. **Lua link errors** — ensure `-DLUA_COMPAT_5_3` is set (Lua 5.4 compiled in 5.3 compatibility mode).
3. **Missing `-lwinhttp`** on MSYS2 — install `mingw-w64-x86_64-libwinhttp` or use the full MSYS2 toolchain.
4. **`build/` directory missing** — `build.sh` creates it automatically; for manual builds, run `mkdir -p build` first.
