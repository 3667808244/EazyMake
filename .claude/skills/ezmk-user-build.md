---
name: ezmk-user-build
description: How to compile a C/C++ project managed by EazyMake — ezmk project build, profiles, and parallel compilation.
trigger:
  - glob: ezmk.toml
  - glob: src/**/*.cpp
  - glob: src/**/*.c
  - glob: include/**/*.hpp
  - glob: include/**/*.h
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
| `ezmk project compile` | Compile only (no link) |

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
```

Watches `src/`, `include/`, and `ezmk.toml`. Rebuilds automatically on changes (300ms debounce). Build failures don't exit the watch loop — fix and save to trigger a retry.

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
| `shared` | `build/<name>.dll` + `build/<name>.lib` (Windows) / `build/lib<name>.so` (Linux) |
| `utils` | `build/<tool_name>` per tool |
