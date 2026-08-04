# Technical Details

This document covers technical internals — dependencies, build from source instructions, compiler support matrix, and project structure. For user-facing documentation, see the [README](../../README.md).

## Dependencies

All dependencies below, except for the compiler and MSYS2, are embedded and do not require additional installation.

| Dependency                             | Version          | Required            | Notes                                             |
| -------------------------------------- | ---------------- | ------------------- | ------------------------------------------------- |
| GCC (g++/gcc) or Clang (clang++/clang) | ≥ 8.0            | **Build & runtime** | C++17 support required                            |
| MSVC (Visual Studio)                   | ≥ 2019           | **Optional**        | `cl.exe` + `link.exe`; auto-detected via `vcvars64.bat` |
| Lua                                    | 5.4.7            | **Embedded**        | Statically linked into `ezmk`                     |
| nlohmann/json                          | header-only      | **Embedded**        | JSON support (`include/vendor/nlohmann_json.hpp`) |
| toml++                                 | header-only      | **Embedded**        | TOML parsing (`include/vendor/toml.hpp`)          |
| Catch2                                 | v3               | **Test only**       | Unit test framework                               |
| miniz                                  | v3.0.2           | **Embedded**        | ZIP extraction (`src/vendor/miniz/*`)             |
| Python                                 | ≥ 3.6            | **Build only**      | Locale data embedding (`scripts/embed_locale.py`) |
| MSYS2 (Windows)                        | —                | **Build & runtime** | Provides g++ and bash environment                 |

## Building EazyMake

```bash
# Via helper script (generates locale data + version header + compiles)
bash build.sh

# Or manually — MSYS2 / Windows
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -lwinhttp -static

# Linux
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -static

# macOS
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk
```

### Running tests

```bash
# Build and run unit tests (skips integration tests)
bash build.sh test

# Build and run all tests (unit + integration)
bash build.sh test-all

# Run integration tests only
bash build.sh integration

# Verbose output
bash build.sh test -v
```

- **Unit tests** (`test/test_*.cpp`): 546 test cases covering all modules
- **Integration tests** (`test/test_integration.cpp`): 8 end-to-end scenarios tagged `[integration]` (`test-all` = 556 cases / 2666 assertions)
- Tests use [Catch2](https://github.com/catchorg/Catch2) v3
- Set `EZMK_TEST_BIN` to override the ezmk binary path for integration tests

## Compiler Support

EazyMake auto-detects your compiler at build time (priority: `$CXX` / `$CC` → platform defaults). The same `ezmk.toml` works across compilers.

| Compiler | Platform | Detection |
|---|---|---|
| **GCC** (g++/gcc) | Linux, macOS, MSYS2 | Default on all platforms |
| **Clang** (clang++/clang) | Linux, macOS | `$CXX=clang++` or auto-fallback |
| **MSVC** (`cl.exe`) | Windows | Auto-detected via `vcvars64.bat` (Visual Studio 2019+) |

### Using MSVC

On Windows with Visual Studio installed, EazyMake automatically detects MSVC by loading the `vcvars64.bat` environment. No extra configuration needed — just run `ezmk build`.

**MSVC-only flags** in `ezmk.toml`:

```toml
[compile]
flags = ["-Wall", "-O2"]          # GCC/Clang flags (ignored by MSVC)
msvc_flags = ["/W4", "/O2"]       # MSVC-only flags (ignored by GCC/Clang)

[link]
msvc_flags = ["/SUBSYSTEM:CONSOLE"]
```

EazyMake translates common GCC flags to MSVC equivalents automatically (e.g. `-Wall` → `/W4`, `-O2` → `/O2`, `-g` → `/Zi`). Use `msvc_flags` for flags that need explicit MSVC naming or have no translation rule.

> **Note:** MSVC support is for building *user projects*, not EazyMake itself. To build `ezmk` from source, use GCC via MSYS2 or Linux/macOS.

### Cross-compiler builds

The same project builds with GCC and MSVC without changes — cache records are isolated by compiler, so switching compilers does not cause cache conflicts.

## Project Structure

```
my_project/
  .ezmk/
    pkg/            # installed packages
    temp/           # temp files (auto-cleaned)
    cache/          # build cache (record.json + obj/)
    repo/           # repo registry + cloned repos
      list.toml
      .cache/
  include/          # project headers (*.h, *.hpp)
  src/              # project sources (*.c, *.cpp, *.cxx)
  build/            # build output
  ezmk.toml         # project configuration
```

## Shell Completion (zsh)

EazyMake ships a static zsh completion script at `res/ezmk.zsh`. Install it as
`_ezmk` in a directory on your `fpath`:

```bash
# Install system-wide
cp res/ezmk.zsh /usr/share/zsh/site-functions/_ezmk

# Or install for current user
mkdir -p ~/.zsh/completions
cp res/ezmk.zsh ~/.zsh/completions/_ezmk
# Then add to ~/.zshrc: fpath=(~/.zsh/completions $fpath)
```

After installing, restart your shell or run `autoload -Uz compinit && compinit`.
