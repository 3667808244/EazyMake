---
name: ezmk-user-test
description: How to run tests in an EazyMake-managed C/C++ project — test configuration, Catch2 integration, and custom test commands.
---

# EazyMake Project Test (User)

## Quick test

```bash
ezmk project test
```

Runs the project's test suite using the framework configured in `ezmk.toml`.

## Test configuration (`ezmk.toml`)

```toml
[test]
framework = "catch2"          # Test framework: "catch2" or "ezmk" (anything else is an error)
dirs = ["test"]               # Directories containing test source files
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
3. Runs the compiled `build/test_runner` binary and gates on its exit code (no `--verbosity high` is passed; `-v` only prints the underlying commands)

### Ezmk framework

For projects without a dedicated test framework, `framework = "ezmk"` compiles and runs each test file as a separate executable (each with its own `main()`), with a 30s per-test timeout.

## Manual test execution

You can always run tests manually via build hooks:

```toml
[hooks]
post_build = "scripts/run_tests.lua"
```

The Lua script can call `ezmk.run()` to execute test binaries:

```lua
-- scripts/run_tests.lua
local result = ezmk.run("./build/test_runner", {})
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

Select tests with `--filter` (there is no `test_pattern` config key):

```bash
ezmk project test --filter "specific test"   # Catch2: test spec (name or [tag], "~" excludes)
ezmk project test --filter "test_module_a"   # ezmk framework: substring match on file name
```

Or run the compiled runner directly:

```bash
./build/test_runner "[tag]"
./build/test_runner "~[integration]"   # Exclude integration tests
```

### Debugging test failures

```bash
ezmk project build --profile debug    # Build with debug symbols
gdb ./build/test_runner                # Run under debugger
```

### CI pipeline

```bash
ezmk pkg install --locked             # Install locked dependencies
ezmk project test                     # Run tests
```
