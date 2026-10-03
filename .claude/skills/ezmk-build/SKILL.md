---
name: ezmk-build
description: How to compile EazyMake from source — build.sh, manual g++ commands, and platform differences.
---

# EazyMake Build

## Quick build

```bash
bash build.sh
```

This generates locale data (`scripts/embed_locale.py` → `src/locale_data.cpp`) + ASCII logo header (`scripts/embed_logo.py` → `include/ezmk/logo.gen.h`) + embedded examples (`scripts/embed_examples.py` → `src/example_data.cpp`, 1.2.3+) + version header (`include/ezmk/version.hpp`), then compiles both binaries: `build/ezmk` and the standalone Lua hook runtime `build/ezmk-lua`.

Use `bash build.sh -v` for verbose output (full compile command and flags).

## Source composition (build.sh)

`build.sh` builds two binaries from one shared source set (build.sh:81-91):

- `COMMON_SRC` = every `src/*.cpp` + `src/vendor/*.c` + `src/vendor/lua/*.c`, **except** the two entry points `src/main.cpp` and `src/ezmk_lua_main.cpp` (build.sh:83-87).
- `SRC = COMMON_SRC + src/main.cpp` → `build/ezmk` — `src/main.cpp` owns ezmk's `main()`.
- `LUA_RUNTIME_SRC = COMMON_SRC + src/ezmk_lua_main.cpp` → `build/ezmk-lua` — `src/ezmk_lua_main.cpp` owns ezmk-lua's `main()`.

> **Never write `src/*.cpp` in a manual g++ command** — it would compile both `main.cpp` and `ezmk_lua_main.cpp`, producing two `main()` symbols and a link failure.

## Manual build

The source list below is exactly `SRC` from `build.sh` (COMMON_SRC + `src/main.cpp`); `src/ezmk_lua_main.cpp` is deliberately excluded.

### Windows (MSYS2)

```bash
g++ -std=c++17 \
    src/build.cpp src/cache.cpp src/cli.cpp src/argparse.cpp src/compile_db.cpp \
    src/config.cpp src/crypto.cpp src/example.cpp src/example_data.cpp src/export.cpp \
    src/file_watcher.cpp src/i18n.cpp src/import.cpp src/locale_data.cpp src/lockfile.cpp \
    src/lua_api.cpp src/pkg.cpp src/project.cpp src/repo.cpp src/toolchain.cpp src/util.cpp \
    src/version.cpp src/workspace.cpp src/workspace_build.cpp src/main.cpp \
    src/vendor/*.c src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/ezmk -lwinhttp -static
```

### Linux

```bash
g++ -std=c++17 \
    src/build.cpp src/cache.cpp src/cli.cpp src/argparse.cpp src/compile_db.cpp \
    src/config.cpp src/crypto.cpp src/example.cpp src/example_data.cpp src/export.cpp \
    src/file_watcher.cpp src/i18n.cpp src/import.cpp src/locale_data.cpp src/lockfile.cpp \
    src/lua_api.cpp src/pkg.cpp src/project.cpp src/repo.cpp src/toolchain.cpp src/util.cpp \
    src/version.cpp src/workspace.cpp src/workspace_build.cpp src/main.cpp \
    src/vendor/*.c src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/ezmk -static
```

### macOS

```bash
g++ -std=c++17 \
    src/build.cpp src/cache.cpp src/cli.cpp src/argparse.cpp src/compile_db.cpp \
    src/config.cpp src/crypto.cpp src/example.cpp src/example_data.cpp src/export.cpp \
    src/file_watcher.cpp src/i18n.cpp src/import.cpp src/locale_data.cpp src/lockfile.cpp \
    src/lua_api.cpp src/pkg.cpp src/project.cpp src/repo.cpp src/toolchain.cpp src/util.cpp \
    src/version.cpp src/workspace.cpp src/workspace_build.cpp src/main.cpp \
    src/vendor/*.c src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/ezmk
```

> **Note:** to build `ezmk-lua` instead, replace `src/main.cpp` with `src/ezmk_lua_main.cpp` and use `-o build/ezmk-lua` — that is `LUA_RUNTIME_SRC` (build.sh:91).

> **Note:** macOS does not fully support static linking — `-static` is omitted.

## Build output

| Output | Path |
|--------|------|
| ezmk binary | `build/ezmk` (Linux/macOS) / `build/ezmk.exe` (Windows) |
| ezmk-lua binary | `build/ezmk-lua` (Linux/macOS) / `build/ezmk-lua.exe` (Windows) — standalone Lua hook runtime |
| Version header | `include/ezmk/version.hpp` (auto-generated, DO NOT EDIT) |
| Logo header | `include/ezmk/logo.gen.h` (auto-generated) |
| Locale data | `src/locale_data.cpp` (auto-generated from `locale/en.json` + `locale/zh.json`) |
| Example data | `src/example_data.cpp` (auto-generated from `examples/`, 1.2.3+) |

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

## Toolchain detection (Windows / MSVC)

`ezmk` detects the toolchain at build time, in this order:

1. `CXX` / `CC` environment override.
2. System `g++` / `clang++` on `PATH` (MinGW on Windows).
3. **MSVC as a fallback** — locates `vcvars64.bat` (VS 2019+ **or Build Tools**), loads that environment itself, and invokes `cl.exe` / `link.exe` / `lib.exe` by **absolute path**.

Force a toolchain with `EZMK_TOOLCHAIN=gcc|clang|msvc` (unset = auto; an invalid or unavailable value fails loudly rather than silently falling back). MSVC does **not** need a "Developer Command Prompt".

Three Windows/CRT traps that are easy to re-introduce:

- **`CreateProcessW` resolves the program name with the PARENT's `PATH`**, not the child `RunOptions.env` block — so MSVC tools must be invoked by an absolute path (resolved from the vcvars `PATH`); injecting the environment alone is not enough.
- **`cmd /c ... set` output is CRLF** — `std::getline` keeps the `\r`, so every env value gets a trailing CR and `TMP` becomes an invalid path (`cl` fails with `D8037`). Strip the CR.
- **`vswhere -latest -property installationPath` excludes VS Build Tools** — add `-products *`, or a Build Tools-only machine is never detected.

Full root-cause chain: `plans/1.4.x/1.4.7.md` §3.7.

## Common issues

1. **"Python not found" warning** — locale data, logo, and example data will be empty stubs. Install Python 3 and rebuild.
2. **Lua link errors** — ensure `-DLUA_COMPAT_5_3` is set (Lua 5.4 compiled in 5.3 compatibility mode).
3. **Missing `-lwinhttp`** on MSYS2 — install `mingw-w64-x86_64-libwinhttp` or use the full MSYS2 toolchain.
4. **Duplicate `main()` at link time** — the manual command listed `src/*.cpp`, which pulls in both `src/main.cpp` and `src/ezmk_lua_main.cpp`. Use the explicit `SRC` list above (or run `bash build.sh`).
5. **`build/` directory missing** — `build.sh` creates it automatically; for manual builds, run `mkdir -p build` first.
6. **`cl : error D8037` (cannot create temporary il file)** — the child environment's `TMP`/`TEMP` is invalid (a POSIX `/tmp` from MSYS2, or a value with a trailing `\r`). `ezmk` normalizes both; if it persists, check `TMP`/`TEMP` in the invoking shell.
7. **`'"C:\...\vcvars64.bat"' is not recognized`** — the vcvars probe used escaped quotes; it must be `cmd /c call "..."` (fixed in 1.4.7).
8. **MSVC selected when you wanted MinGW** — GCC/Clang on `PATH` win in auto mode; set `EZMK_TOOLCHAIN=gcc` to force MinGW.
