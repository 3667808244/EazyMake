# Migrating a CMake project to EazyMake

`ezmk project import --from cmake` converts the current directory's
`CMakeLists.txt` into a fresh `ezmk.toml`. It targets the **most standard CMake
projects** (typically small, single-target ones) and is **experimental**:

- **Single-direction snapshot** — after import, `ezmk.toml` is the source of
  truth. Edit `ezmk.toml`, do *not* go back to editing `CMakeLists.txt`.
- **Best-effort** — some constructs are mapped approximately and left as
  `# TODO:` comments for you to review; others are rejected outright.
- **Transactional** — if the importer hits an unsupported construct, it stops
  and writes **nothing** (no half-generated `ezmk.toml`).

## Usage

```bash
cd /path/to/cmake-project
ezmk project import                 # --from cmake (default)
ezmk project import --from CMAKE    # case-insensitive
ezmk project import --overwrite     # replace an existing ezmk.toml
```

Preconditions: `CMakeLists.txt` must exist in the current directory. If
`ezmk.toml` already exists, import refuses unless `--overwrite` is given.

## Supported constructs

These standard commands are mapped to `ezmk.toml`:

| CMake command | Mapped to |
|---|---|
| `project(name VERSION x.y.z LANGUAGES CXX/C)` | `[project]` `name` / `version` / `language` |
| `add_executable(t ...)` | `[project]` `type = "executable"`, `name = t` |
| `add_library(t STATIC/SHARED ...)` | `type = "static"` / `"shared"` |
| `target_sources(t PRIVATE <src...>)` | `[compile].src_dirs` (dirs of the source files) |
| `target_include_directories(t PRIVATE <dir...>)` | `[compile].include_dirs` |
| `target_compile_definitions(t PRIVATE <NAME=VAL...>)` | `[compile.macros]` |
| `target_compile_options(t PRIVATE <flag...>)` | `[compile].flags` |
| `target_link_libraries(t PRIVATE <lib...>)` | `[link].system_targets` (unrecognized libs) |

**Multiple targets**: only the first / main target is imported; others are left
out. Split multi-target projects into separate EazyMake projects.

## Best-effort (mapped approximately, with `# TODO:`)

- **`set(...)` variables + `${VAR}`** — a *finite, single-level* expansion is
  applied: only top-level, constant `set()` calls outside conditional blocks are
  captured; a variable that is later modified is dropped from the table. If an
  argument still contains `${...}` after expansion, it is left as a
  `# TODO: 未解析的参数` comment.
- **`find_package(...)`** — the package name is mapped to an EazyMake package
  (common aliases such as `Boost`→`boost`, `OpenSSL`→`openssl` are built in) and
  written as a **commented** `[depends]` entry, e.g.:

  ```toml
  [depends]
  # TODO: 原 CMake 引用了 boost，请手动执行 `ezmk pkg install boost` 后取消注释
  # lib = ["boost@1.82"]
  ```

- **Conditional blocks** (`if(WIN32)`, `if(UNIX)`, `if(APPLE)`, `if(MSVC)`, …) —
  the branch matching the **current platform** is taken. Conditions that cannot
  be evaluated (custom variables, `$ENV{...}`, complex expressions) are skipped
  and marked `# TODO: 未求值的条件块`.

## Rejected constructs (import stops, nothing written)

These non-declarative constructs are **not** supported and cause a transactional
abort with a pointer to this document:

- Custom build steps: `add_custom_command`, `add_custom_target`
- Custom functions / macros: `function()`, `macro()`
- External dependency probing: `pkg_check_modules`, `execute_process`
- Generator expressions: `$<...>` (e.g. `$<TARGET_FILE:...>`, `$<JOIN:...>`)

## Manual migration for rejected constructs

For projects that need custom build steps, replicate them with EazyMake **Lua
hooks** instead:

```toml
[hooks]
pre_build  = "scripts/gen_headers.lua"   # runs before compiling
post_build = "scripts/strip_symbols.lua" # runs after linking
```

See `docs/en/config_file.md` (§ `[hooks]`) and `tutorial/en/dev/01-watch-hooks.md`
for hook examples. `execute_process`-style logic maps to
`ezmk.run_command()` / `ezmk.file_write()` in the Lua API; `pkg_check_modules`
maps to `ezmk pkg install <name>` (or the commented `[depends]` entries above).

## After-import checklist

1. Review every `# TODO:` comment in the generated `ezmk.toml`.
2. Uncomment and fix the `[depends]` entries; run `ezmk pkg install <name>`.
3. Verify `[compile.macros]` (especially platform macros) and `[link].system_targets`.
4. Run `ezmk build` then `ezmk run`. Use `ezmk project cc` to regenerate
   `compile_commands.json` for clangd/LSP.
