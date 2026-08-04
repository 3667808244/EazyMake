---
name: ezmk-test
description: How to run EazyMake tests — Catch2 framework, test organization, and writing new tests.
trigger:
  - glob: test/**/*.cpp
  - glob: test/**/*.hpp
  - glob: src/vendor/catch2_impl.cpp
  - glob: include/vendor/catch2.hpp
  - glob: build.sh
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
    src/build.cpp src/cache.cpp src/cli.cpp src/argparse.cpp src/config.cpp \
    src/crypto.cpp src/file_watcher.cpp src/i18n.cpp src/locale_data.cpp \
    src/lua_api.cpp src/pkg.cpp src/project.cpp src/repo.cpp src/toolchain.cpp \
    src/util.cpp src/version.cpp \
    src/vendor/*.c src/vendor/catch2_impl.cpp src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/test_ezmk -lwinhttp -static && ./build/test_ezmk
```

> **Note:** `src/main.cpp` is excluded from tests — it has its own `main()`; `catch2_impl.cpp` provides the test `main()`.

## Test framework

**[Catch2 v3](https://github.com/catchorg/Catch2)** — header-only, vendored in the repo:

| File | Role |
|------|------|
| `include/vendor/catch2.hpp` | Catch2 header (single-header distribution) |
| `src/vendor/catch2_impl.cpp` | Catch2 implementation (provides `main()`) |

## Test file organization

```
test/
├── test_main.cpp          # Catch2 main (uses catch2_impl.cpp)
├── test_cli.cpp           # CLI parsing tests
├── test_config.cpp        # ezmk.toml config tests
├── test_build.cpp         # Build orchestration tests
├── test_cache.cpp         # Incremental cache tests
├── test_pkg.cpp           # Package management tests
├── test_repo.cpp          # Repository management tests
├── test_toolchain.cpp     # Compiler detection tests
├── test_i18n.cpp          # Internationalization tests
├── test_lua_api.cpp       # Lua C++ API tests
├── test_util.cpp          # Utility function tests
├── test_version.cpp       # Version comparison tests
├── test_crypto.cpp        # SHA-256 tests
├── test_file_watcher.cpp  # File watcher tests
├── test_project.cpp       # Project creation tests
├── test_argparse.cpp      # Argument parsing tests
└── test_integration.cpp   # End-to-end integration tests
```

**Naming convention:** `test/test_<module>.cpp` — one file per source module.

## Current baseline

- **546 test cases** / **2617 assertions** (unit)
- Covers: CLI, config, build, cache, pkg, repo, toolchain, i18n, Lua API, util, version, crypto, file watcher, project, argparse
- Integration tests: 8 end-to-end scenarios tagged `[integration]` (`test-all` = 556 cases / 2666 assertions)

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
