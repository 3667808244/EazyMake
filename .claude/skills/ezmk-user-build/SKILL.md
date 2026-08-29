---
name: ezmk-user-build
description: How to compile a C/C++ project managed by EazyMake — ezmk project build, profiles, and parallel compilation.
---

# EazyMake Project Build (User)

## Quick build

```bash
ezmk project build
# or shorthand:
ezmk pb
```

This compiles all sources defined in `ezmk.toml` → links → produces the output binary/library.

## Build commands

| Command | Description |
|---------|-------------|
| `ezmk project build` | Full build (compile + link) |
| `ezmk project run` | Build and run the executable |
| `ezmk project watch` | Watch mode: auto-rebuild on file changes |
| `ezmk project cc` | Generate `compile_commands.json` for IDEs (no build) |

## Parallel compilation (`-j`)

```bash
ezmk project build -j 4    # 4 parallel compile jobs
ezmk project build -j 0    # Auto-detect (uses all CPU cores)
```

## Build profiles (`--profile`)

Activate a named build configuration from `ezmk.toml`:

```bash
ezmk project build --profile debug
ezmk project build --profile release
```

Profiles define overrides for compile flags, link flags, and macros (see `ezmk-user-config` skill).

## Watch mode

```bash
ezmk project watch
ezmk project watch --no-build-on-start   # Skip initial build
ezmk project watch --profile debug
ezmk project watch --run                 # Run the executable after each successful rebuild (1.3.4+)
ezmk project watch --run -- <args>       # ... passing <args> to it (1.4.0-dev.5+)
```

Watches `src/`, `include/`, and `ezmk.toml`. Rebuilds automatically on changes (300ms debounce). Build failures don't exit the watch loop — fix and save to trigger a retry.

`--run` / `-r` (1.3.4+) runs the freshly built executable after each successful rebuild; with `--run -- <args>`, the arguments after `--` are passed through to it (1.4.0-dev.5+). Only `executable` projects support `--run` — it is rejected for `static`/`shared`/`utils` projects.

## Incremental build cache

EazyMake caches compilation results in `.ezmk/cache/`. Only modified source files (and files that include modified headers) are recompiled.

```bash
ezmk project build --disable-cache   # Force full rebuild
```

Cache is invalidated automatically when:
- Source file content changes
- Included headers change
- Compile flags change
- Compiler version changes

## Build output

| Project type | Output |
|-------------|--------|
| `executable` | `build/<name>` (Linux/macOS) / `build/<name>.exe` (Windows) |
| `static` | `build/lib<name>.a` (GCC/Clang) / `build/<name>.lib` (MSVC) |
| `shared` | MSVC: `build/<name>.dll` + `build/<name>_implib.lib`; MinGW: `build/lib<name>.dll`; Linux/macOS: `build/lib<name>.so` |
| `utils` | `build/<tool_name>` per tool |
