# Glossary

Standardized English terminology for EazyMake documentation. Maintainers and translators should reference this glossary to ensure consistency across all documents.

## Core Concepts

| Term | Definition |
|------|------------|
| package | A redistributable unit of code (static library or utils tool), installed via `ezmk pkg install`. Packaged as `.zip` or `.tar.gz` archives. |
| repository (repo) | A git repository containing `index.toml` + `packages/` that hosts packages for installation by name. |
| scope | The installation/registration level: project (`-p`), user (`-u`), or global (`-g`). |
| project | An EazyMake workspace defined by an `ezmk.toml` file at its root. |
| build | The full process: compile sources to link objects to produce final artifact (executable or library). |
| compile / compilation | Translating a single source file (`.cpp`, `.c`) into an object file (`.o`, `.obj`). |
| link / linking | Combining object files and libraries into a final executable or library. |
| cache | Content-hash-based incremental build cache stored in `.ezmk/cache/`. |
| hook | A Lua script executed at build lifecycle points: `pre_build`, `post_build`, `on_failure`. |
| toolchain | Compiler/linker abstraction layer supporting GCC, Clang, and MSVC. |
| profile | A named set of compile/link flag overrides in `ezmk.toml`, activated via `--profile <name>`. |

## Package & Repository Terms

| Term | Definition |
|------|------------|
| archive | A compressed package file (`.zip` or `.tar.gz`). |
| dependency | A required library declared in `[depends].lib`. Missing to build error. |
| optional dependency | A library declared in `[depends].want`. Missing to warning + `EZMK_LIB_MISS_*` macro, build continues. |
| index | The `index.toml` file in a repository listing all available packages with versions and SHA-256 hashes. |
| registry | The `list.toml` file tracking registered repositories per scope. |
| upstream | The original third-party project that a package wraps (e.g. fmt, spdlog). |

## Configuration Terms

| Term | Definition |
|------|------------|
| macro | A preprocessor definition, either via `-D` flags, `[compile.macros]`, or auto-injected `EZMK_*` macros. |
| flag | A compiler or linker command-line option (e.g. `-Wall`, `-O2`). |
| include directory | A search path for `#include` headers, configured in `[compile].include_dirs`. |
| source directory | A directory scanned for source files, configured in `[compile].src_dirs`. |
| link directory | A search path for libraries at link time, configured in `[link].link_dirs`. |
| system target | A system library to link against (e.g. `pthread`, `m`), declared in `[link].system_target`. |

## Lua & Utils Terms

| Term | Definition |
|------|------------|
| utils package | A package with `type = "utils"` that provides Lua-based tools via `ezmk utils <name>`. |
| sandbox | The restricted Lua environment: `os` and `io` removed at compile time; file writes restricted to project root. |
| permission | Fine-grained allowlist/denylist for `file_read`/`file_write`/`run` in `[utils.permissions]`. |
| entry script | The `utils/<name>.lua` file that implements a tool's `run(args)` and optional `help()` functions. |
| built-in tool | A tool compiled directly into the ezmk binary (currently only `ezmk-cc`). |

## Security Terms

| Term | Definition |
|------|------------|
| SHA-256 | Cryptographic hash used to verify package archive integrity. |
| atomic write | Write to a temporary file first, then `rename` to the target path to prevent corruption on failure. |
| secondary confirmation | An extra interactive prompt required for sensitive operations (global install, file overwrite). |
| validation | Checking that data meets expected constraints (e.g. `index.toml` parseable, `sha256` matches, files exist). |

## Build & Compilation Terms

| Term | Definition |
|------|------------|
| object file | A compiled translation unit (`.o` on Unix, `.obj` on Windows). |
| amalgamation | A single-file distribution of a C/C++ library (e.g. SQLite's `sqlite3.c`). |
| header-only | A library that requires no compilation -- all code is in header files (e.g. Catch2, nlohmann_json). |
| incremental build | Only recompiling source files whose content or dependencies have changed since the last build. |
| parallel compilation | Compiling multiple source files concurrently using `-j <N>`. |
| watch mode | Auto-rebuilding when source files change, via `ezmk project watch`. |
