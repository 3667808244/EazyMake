# 11. Importing a CMake project

Have an existing CMake project you want to build with EazyMake? The experimental
`ezmk project import` command converts a standard `CMakeLists.txt` into a fresh
`ezmk.toml` in one shot.

## Import a sample project

Start with a typical small CMake project:

```bash
$ mkdir /tmp/hello-cmake && cd /tmp/hello-cmake
$ cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(hello VERSION 1.0.0 LANGUAGES CXX)

set(SRCS src/main.cpp)
add_executable(hello ${SRCS})
target_include_directories(hello PRIVATE include)
target_compile_definitions(hello PRIVATE GREETING="hi")
EOF
$ mkdir -p src include
$ cat > src/main.cpp <<'EOF'
#include <cstdio>
int main() { std::printf("%s\n", GREETING); return 0; }
EOF
```

Now import it:

```bash
$ ezmk project import --from cmake
```

This writes `ezmk.toml` (and refuses to touch an existing one unless you pass
`--overwrite`). Let's look at what it produced:

```bash
$ cat ezmk.toml
```

You'll see the `[project]` header (name/version/language), a `[compile]` section
with `src_dirs = ["src"]` and `include_dirs = ["include"]` (from the
`${SRCS}` / `target_include_directories` expansion), and `[compile.macros]`
containing `GREETING`.

## Build and run

```bash
$ ezmk build
$ ezmk run
hi
```

Everything — source dirs, include paths, compile definitions — came from the
CMakeLists mapping. After import, **`ezmk.toml` is the source of truth**: edit it
directly, don't go back to editing `CMakeLists.txt`.

## What maps, what's skipped, what's rejected

| Situation | Behavior |
|---|---|
| `project`, `add_executable`, `add_library`, `target_sources`, `target_include_directories`, `target_compile_definitions`, `target_compile_options`, `target_link_libraries` | Mapped to `ezmk.toml` |
| `set(...)` + `${VAR}` (top-level, constant) | Expanded once; leftovers become `# TODO: 未解析的参数` |
| `find_package(Boost 1.82)` | Written as a **commented** `# lib = ["boost@1.82"]` under `[depends]` |
| `if(WIN32)` / `if(UNIX)` … | Branch for the **current platform** is taken |
| `add_custom_command`, `function()`, `$<...>` generator expressions, `pkg_check_modules` | **Import aborts** (nothing written) |

For rejected projects, see
[`docs/en/migrate-from-cmake.md`](../../../docs/en/migrate-from-cmake.md) for manual
migration using Lua `[hooks]`.

> 💡 Want a complete runnable example? Run `ezmk example cmake-interop` to scaffold a
> project ready for `export cmake` (see
> [`examples/README.md`](../../../examples/README.md) for the list).

## Next steps

- Regenerate `compile_commands.json` for clangd/LSP: `ezmk project cc`.
- Read the generated `# TODO:` comments and uncomment/adjust `[depends]`.
- Check `docs/en/migrate-from-cmake.md` for the full supported/rejected list.
