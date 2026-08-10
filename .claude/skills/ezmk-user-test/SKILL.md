---
name: ezmk-user-test
description: How to run tests in an EazyMake-managed C/C++ project — test configuration, Catch2 integration, and custom test commands.
---

# EazyMake Project Test (User)

> **Note:** `ezmk project test` is a 1.1.0-dev.6 feature. If your version doesn't support it yet, use `ezmk project run` with a test target or a custom build hook.

## Quick test

```bash
ezmk project test
```

Runs the project's test suite using the framework configured in `ezmk.toml`.

## Test configuration (`ezmk.toml`)

```toml
[test]
framework = "catch2"          # Test framework: "catch2" or "builtin"
test_dirs = ["test"]          # Directories containing test source files
test_pattern = "test_*.cpp"   # Glob pattern for test files
flags = ["-g", "-O0"]         # Extra compile flags for test builds
```

### Catch2 integration

If your project uses Catch2:

```toml
[depends]
lib = ["catch2"]              # Declare Catch2 as a dependency
```

Then `ezmk project test` automatically:
1. Compiles test sources + project sources (excluding `main.cpp`)
2. Links with Catch2
3. Runs the test binary with `--verbosity high`

### Builtin framework

For projects without a dedicated test framework, `framework = "builtin"` compiles and runs each test file as a separate executable (each with its own `main()`).

## Manual test execution

You can always run tests manually via build hooks:

```toml
[hooks]
post_build = "scripts/run_tests.lua"
```

The Lua script can call `ezmk.run()` to execute test binaries:

```lua
-- scripts/run_tests.lua
local result = ezmk.run("./build/test_suite", {})
if result.exit_code ~= 0 then
    ezmk.error("Tests failed: " .. result.stderr)
    os.exit(1)
end
```

## Test file organization (convention)

```
project/
├── test/
│   ├── test_main.cpp       # Test entry point (if using Catch2)
│   ├── test_module_a.cpp   # Tests for module A
│   ├── test_module_b.cpp   # Tests for module B
│   └── ...
└── src/
    └── ...
```

## Common patterns

### Running specific tests

```bash
# With Catch2:
./build/test_suite "Module A: specific test"
./build/test_suite "[tag]"
./build/test_suite "~[integration]"   # Exclude integration tests
```

### Debugging test failures

```bash
ezmk project build --profile debug    # Build with debug symbols
gdb ./build/test_suite                 # Run under debugger
```

### CI pipeline

```bash
ezmk pkg install --locked             # Install locked dependencies
ezmk project test                     # Run tests
```
