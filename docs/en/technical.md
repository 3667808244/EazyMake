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
| miniz                                  | embedded (vendor version string 2.2.0 / `MZ_VERSION` 10.2.0) | **Embedded**        | ZIP extraction (`src/vendor/miniz*.c` + `include/vendor/miniz*.h`) |
| Python                                 | ≥ 3.6            | **Build only**      | Locale data embedding (`scripts/embed_locale.py`) |
| MSYS2 (Windows)                        | —                | **Build & runtime** | Provides g++ and bash environment                 |

> **Why embed every dependency?** So `ezmk` is one self-contained binary that runs
> with nothing more than a compiler. Third-party code is vendored in-tree
> (`src/vendor/` + `include/vendor/`) and kept untouched, so builds are reproducible
> offline, version-pinned, and easy to upgrade.

## Building EazyMake

```bash
# Via helper script (generates locale data + version header + compiles)
bash build.sh

# Or manually — MSYS2 / Windows
# `src/*.cpp` also matches both entry points (`main.cpp` + `ezmk_lua_main.cpp`),
# and linking two `main()` definitions fails — exclude the Lua one.
g++ -std=c++17 $(ls src/*.cpp | grep -v ezmk_lua_main.cpp) src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -lwinhttp -static

# Linux
g++ -std=c++17 $(ls src/*.cpp | grep -v ezmk_lua_main.cpp) src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -static

# macOS
g++ -std=c++17 $(ls src/*.cpp | grep -v ezmk_lua_main.cpp) src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk
```

> **Run the generators first.** These recipes assume the generated sources
> already exist; a fresh clone does not have three of them (they are
> `.gitignore`d). Run the scripts to produce them: `scripts/embed_locale.py` →
> `src/locale_data.cpp`, `scripts/embed_examples.py` → `src/example_data.cpp`,
> `scripts/embed_logo.py` → `include/ezmk/logo.gen.h`, and `build.sh` →
> `include/ezmk/version.hpp`.

> **Why a bash script for building?** All first-class build environments — Linux,
> macOS, and MSYS2 — ship a POSIX shell, so a `build.sh` that orchestrates locale
> embedding, version-header generation, and compilation needs no extra build
> system and behaves identically everywhere.

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

- **Unit tests** (`test/test_*.cpp`): ~987 test cases covering all modules
- **Integration tests** (`test/test_integration*.cpp`): ~112 end-to-end scenarios tagged `[integration]`, spread over `test_integration.cpp`, `test_integration_workspace.cpp`, `test_integration_report.cpp`, and `test_integration_git.cpp` (`test-all` = 1099 cases / 6342 assertions; 1094 passed, 5 skipped)
- Tests use [Catch2](https://github.com/catchorg/Catch2) v3
- Set `EZMK_TEST_BIN` to override the ezmk binary path for integration tests

## Compiler Support

EazyMake auto-detects your compiler at build time (priority: `$CXX` / `$CC` → platform defaults). The same `ezmk.toml` works across compilers.

> **Why auto-detect instead of asking?** Detecting the toolchain means a project
> builds on whatever compiler the platform already has — one `ezmk.toml` stays
> portable across Linux, macOS, and Windows instead of being rewritten per setup.

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

> **Why translate common flags?** So the same `ezmk.toml` expresses intent once, in
> familiar GCC-style flags, instead of being duplicated per compiler. Translation
> can't cover every flag, so `msvc_flags` remains as the explicit escape hatch.

> **Note:** MSVC support is for building *user projects*, not EazyMake itself. To build `ezmk` from source, use GCC via MSYS2 or Linux/macOS.

### Cross-compiler builds

The same project builds with GCC and MSVC without changes — switching compilers does not reuse stale objects: the record stores the detected compiler's **version string** (`compiler_version`), and a change in it invalidates the cache as a whole. The `compiler` name field in the record is written for diagnostics and is not compared.

> **Why key the cache on the compiler version?** Objects compiled by different
> toolchains are binary-incompatible; if a compiler switch (or an upgrade) did not
> invalidate the cache, a stale GCC object would be silently reused in an MSVC
> build (and vice versa). The version string is the field that actually triggers
> that invalidation.

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

## Man Pages

Four hand-written roff pages form a concise offline reference: `ezmk.1`,
`ezmk-lua.1`, `ezmk.toml.5` and `ezmk-workspace.toml.5`. The full specification
stays in `docs/`; each page ends with a SEE ALSO pointing back here, and
`ezmk help` prints the same pointer as its last line.

| Channel | Where the pages land |
|---|---|
| `install.sh` (Linux / macOS / MSYS2) | `$PREFIX/share/man/man1/{ezmk.1,ezmk-lua.1}` and `$PREFIX/share/man/man5/{ezmk.toml.5,ezmk-workspace.toml.5}` (skip with `EZMK_NO_MAN=1`) |
| Arch / MSYS2 package (`publish/arch/PKGBUILD`) | `/usr/share/man/man1/{ezmk.1,ezmk-lua.1}` and `/usr/share/man/man5/{ezmk.toml.5,ezmk-workspace.toml.5}` |
| Release assets + Homebrew | the macOS / Linux tarballs carry `man/`; the formula installs it via `man1.install` / `man5.install` |

With a non-standard prefix (`$HOME/.local`) `man` may not search there; the installer
therefore prints the line to add to your shell profile:

```bash
export MANPATH="$HOME/.local/share/man:$MANPATH"
```

Read a page straight from a checkout without installing anything:

```bash
man -l man/ezmk.1
man -l man/ezmk-lua.1
man -l man/ezmk.toml.5
man -l man/ezmk-workspace.toml.5
```

Windows (native, without MSYS2) has no `man` command and ships no pages: the PowerShell
installer and the Windows zip are intentionally untouched; MSYS2 users get the pages
through `install.sh`.

**Keeping them in sync.** `scripts/check_man_sync.py` compares the pages against the CLI
option specs (`src/cli.cpp`), the configuration parser (`src/config.cpp`) and the
environment variables in both directions; CI additionally renders both pages with
`groff -man -Tutf8 -z -ww`. See [CONTRIBUTING](../../CONTRIBUTING.md#man-pages) before
editing a page.
