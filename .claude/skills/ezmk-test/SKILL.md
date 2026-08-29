---
name: ezmk-test
description: How to run EazyMake tests — Catch2 framework, test organization, and writing new tests.
---

# EazyMake Test

## Quick test

```bash
bash build.sh test       # Build + run unit tests (skips [integration])
bash build.sh test-all   # Build + run ALL tests (unit + integration)
bash build.sh integration # Build + run integration tests only
bash build.sh test -v    # Verbose: shows full compile command
```

## Manual test build

```bash
g++ -std=c++17 test/test_*.cpp \
    src/build.cpp src/cache.cpp src/compile_db.cpp src/export.cpp src/import.cpp \
    src/cli.cpp src/argparse.cpp src/config.cpp src/crypto.cpp src/example_data.cpp \
    src/file_watcher.cpp src/i18n.cpp src/locale_data.cpp src/lockfile.cpp src/lua_api.cpp \
    src/pkg.cpp src/project.cpp src/repo.cpp src/toolchain.cpp src/util.cpp \
    src/version.cpp src/workspace.cpp src/workspace_build.cpp \
    src/vendor/*.c src/vendor/catch2_impl.cpp src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/test_ezmk -lwinhttp -static && ./build/test_ezmk
```

> **Note:** the source list above is exactly `TEST_SRC` from `build.sh` (build.sh:93) — keep it in sync if `build.sh` changes; a partial list (e.g. missing `export`/`import`/`compile_db`/`example_data`/`lockfile`/`workspace`/`workspace_build`) fails at link time. Both entry points are excluded: `src/main.cpp` (ezmk's own `main()`) and `src/ezmk_lua_main.cpp` (ezmk-lua's `main()`); `src/vendor/catch2_impl.cpp` provides the test `main()`. `test/test_*.cpp` expands to every unit + integration test file (helpers are `.hpp`, so not matched).

## Test framework

**[Catch2 v3](https://github.com/catchorg/Catch2)** — header-only, vendored in the repo:

| File | Role |
|------|------|
| `include/vendor/catch2.hpp` | Catch2 header (single-header distribution) |
| `src/vendor/catch2_impl.cpp` | Catch2 implementation (provides `main()`) |

## Test file organization

```
test/
├── test_main.cpp                  # Catch2 test suite entry point (uses catch2_impl.cpp)
├── test_helpers.hpp               # Shared test helpers
├── test_cli.cpp                   # CLI parsing tests
├── test_argparse.cpp              # Argument parsing tests
├── test_config.cpp                # ezmk.toml config tests
├── test_build.cpp                 # Build orchestration tests
├── test_cache.cpp                 # Incremental cache tests
├── test_compile_db.cpp            # compile_commands.json generation tests
├── test_export.cpp                # `project export` (cmake/vscode) tests
├── test_import.cpp                # CMake importer tests
├── test_workspace.cpp             # Workspace config / command group tests
├── test_workspace_build.cpp       # Workspace build/test/watch orchestration tests
├── test_example.cpp               # Embedded example table tests
├── test_pkg.cpp                   # Package management tests
├── test_repo.cpp                  # Repository management tests
├── test_toolchain.cpp             # Compiler detection tests
├── test_project.cpp               # Project creation tests
├── test_i18n.cpp                  # Internationalization tests
├── test_lua.cpp                   # Lua C++ API tests
├── test_hooks.cpp                 # Build/install hook tests
├── test_util.cpp                  # Utility function tests
├── test_utils_perms.cpp           # `utils` permission model tests
├── test_version.cpp               # Version comparison tests
├── test_crypto.cpp                # SHA-256 tests
├── test_lockfile.cpp              # ezmk.lock tests
├── test_file_watcher.cpp          # File watcher tests
├── test_thread_pool.cpp           # ThreadPool tests
├── test_integration.cpp           # End-to-end integration tests
├── test_integration_workspace.cpp # Workspace end-to-end tests
├── test_integration_report.cpp    # Report/watch/pack end-to-end tests
└── test_integration_helpers.hpp   # Integration-test helpers
```

**Naming convention:** `test/test_<module>.cpp` — one file per source module. `test_integration*.cpp` hold end-to-end scenarios (split from `test_integration.cpp` in 1.3.6).

## Current baseline

- **988 test cases** / **5770 assertions** — full suite via `bash build.sh test-all` (measured after 1.4.0-dev.7, zero regressions)
- Covers: CLI, argparse, config, build, cache, compile_db, export/import, workspace, workspace_build, example, pkg, repo, toolchain, project, i18n, Lua, hooks, util, utils perms, version, crypto, lockfile, file watcher, thread pool
- Integration tests: `test_integration*.cpp` scenarios tagged `[integration]` (skipped by `bash build.sh test`, included by `test-all`)

## Writing new tests

### Basic structure

```cpp
#include "ezmk/ezmk.hpp"  // or specific headers
#include <catch2/catch.hpp>  // actually: include/vendor/catch2.hpp

TEST_CASE("Module: brief description", "[module]") {
    // Arrange
    // Act
    // Assert
    REQUIRE(result == expected);
}
```

### Using fixtures

```cpp
// Shared fixtures are in test/test_helpers.hpp (if applicable)
// Each test file can define its own helpers in an anonymous namespace
```

### Integration tests

Tag integration tests with `[integration]` so they can be skipped during quick test runs:

```cpp
TEST_CASE("Integration: full build pipeline", "[integration]") {
    // These tests use the real ezmk binary via EZMK_TEST_BIN env var
}
```

### Running a specific test

```bash
./build/test_ezmk "Module: brief description"
./build/test_ezmk "[module]"            # all tests with tag [module]
./build/test_ezmk "~[integration]"      # exclude integration tests
```

## Test CI pattern

Integration tests need a working `ezmk` binary. `build.sh test` builds `ezmk` first (if sources are newer than binary), then builds + runs tests. The `EZMK_TEST_BIN` environment variable points integration tests to the built binary.
