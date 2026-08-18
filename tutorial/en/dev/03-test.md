# 9. Testing your project

`ezmk test` builds your project if needed, then compiles and runs your tests.
You get one command, a pass/fail summary, and a non-zero exit code when anything
fails — so tests can gate CI and pre-push hooks.

## Configuring tests

Tests live in a source directory (default `test/`) and are configured in `[test]`:

```toml
[test]
dirs = ["test"]        # test source directories
framework = "catch2"   # "catch2" (default) or "ezmk"
flags = []             # extra flags for the test build
```

There are two frameworks. Both follow the same "zero-config" philosophy — you
point `ezmk test` at your files and it figures out the rest.

## Framework 1: Catch2 (default)

The default framework uses [Catch2](https://github.com/catchorg/Catch2). Get it as
a package (`ezmk pkg install catch2` in `[depends]`) or vendor the single header as
`include/vendor/catch2.hpp`.

```cpp
// test/math_test.cpp
#include <catch2/catch_all.hpp>   // or "catch2.hpp" when vendored

int add(int a, int b);

TEST_CASE("add works", "[math]") {
    REQUIRE(add(2, 3) == 5);
}
```

```bash
$ ezmk test
Running tests (Catch2)...
===============================================================================
All tests passed (1 assertion in 1 test case)
```

Filter by test name with `--filter`, run everything (even passing cases) verbosely
with `-V`:

```bash
$ ezmk test --filter "add works"
$ ezmk test -V
```

## Framework 2: ezmk built-in

For minimal dependencies — no framework header at all — use `framework = "ezmk"`.
Each test file is compiled into its **own standalone executable** (linked against
your project's object files, minus `main`). The exit code is the verdict:
`0` = pass, anything else = fail. A test that hangs is killed after 30 seconds and
reported as a timeout.

```cpp
// test/basic_test.cpp
#include <cstdlib>

int add(int a, int b);

int main() {
    if (add(2, 3) != 5) return 1;   // non-zero → FAIL
    if (add(0, 0) != 0) return 1;
    return 0;                        // zero → PASS
}
```

```bash
$ ezmk test -f ezmk
Running tests (ezmk)...
  [PASS] basic_test  (12ms)
```

`--filter` matches a substring of the file name here (`ezmk test --filter basic`).

## Run it

| Command | Meaning |
|---|---|
| `ezmk test` | Build + run tests (Catch2 by default) |
| `ezmk test -f ezmk` | Run with the built-in framework |
| `ezmk test --filter <name>` | Only tests matching `<name>` |
| `ezmk test -V` | Verbose — show every test, even passing ones |

If the project isn't built yet, `ezmk test` builds it first. A failing test suite
exits non-zero, ready to wire into your CI.

> 💡 Want a complete runnable example? Run `ezmk example with-tests` to scaffold a
> Catch2 test project (run `ezmk pkg install catch2 -y` first; see
> [`examples/README.md`](../../../examples/README.md) for the list).
