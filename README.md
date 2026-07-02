# EazyMake

A simple C/C++ build tool — `ezmk`. Based on GCC/g++ (MSYS2 on Windows, or native on Linux/macOS).

**Design philosophy:** ease of use over feature richness. For complex builds, use CMake.

## Dependencies

| Dependency | Version | Required | Notes |
|---|---|---|---|
| GCC (g++/gcc) or Clang (clang++/clang) | ≥ 8.0 | **Build & runtime** | C++17 support required |
| Lua | 5.4.7 | **Embedded** | Statically linked into `ezmk` |
| nlohmann/json | header-only | **Embedded** | JSON support (`include/vendor/nlohmann_json.hpp`) |
| toml++ | header-only | **Embedded** | TOML parsing (`include/vendor/toml.hpp`) |
| Catch2 | v3 (header-only) | **Test only** | Unit test framework |
| miniz | v3.0.2 | **Embedded** | ZIP extraction (`src/vendor/miniz/*`) |
| Python | ≥ 3.6 | **Build only** | Locale data embedding (`scripts/embed_locale.py`) |
| MSYS2 (Windows) | — | **Build & runtime** | Provides g++ and bash environment |

## Quick start

### Build EazyMake itself

```bash
# Via helper script (generates locale data + version header + compiles)
bash build.sh

# Or manually — MSYS2 / Windows
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -lwinhttp -static

# Linux
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -static

# macOS
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk
```

### Create and build a project

```bash
ezmk project new hello          # scaffold hello/
cd hello
ezmk project build               # compile + link
ezmk project run                 # build + run
```

### Install packages

```bash
# From a local file
ezmk pkg install -p ./foo-0.1.0.zip

# From a URL
ezmk pkg install -p https://example.com/packages/bar-1.2.0.tar.gz

# By name (requires registered repo)
ezmk repo add -p git@github.com:user/ezmk-repo.git --name my-repo
ezmk repo update
ezmk pkg install -p foo
```

### Run utility tools

```bash
# Generate compile_commands.json for clangd/LSP
ezmk utils cc

# Custom output path
ezmk utils cc -o build/compile_commands.json
```

## CLI reference

### `project` — build your code

| Command | Description |
|---|---|
| `ezmk project new <name> [--type executable\|static\|shared\|utils]` | Scaffold a new project |
| `ezmk project build [--disable-cache] [--verbose]` | Incremental build |
| `ezmk project run [--disable-cache] [--verbose]` | Build and execute |
| `ezmk project clean` | Remove cache and temp files |

### `pkg` — manage packages

| Command | Description |
|---|---|
| `ezmk pkg install [-p\|-u\|-g] <file_or_url_or_name>` | Install a package |
| `ezmk pkg remove [-p\|-u\|-g] <name>` | Remove a package |
| `ezmk pkg search [-p\|-u\|-g] <name>` | Search for a package |
| `ezmk pkg info [-p\|-u\|-g] <name>` | Show package details |

### `repo` — manage repositories

| Command | Description |
|---|---|
| `ezmk repo add [-p\|-u\|-g] <git_url_or_path> [--name <n>] [--branch <b>]` | Register and clone |
| `ezmk repo remove [-p\|-u\|-g] <name>` | Unregister and delete cache |
| `ezmk repo update [-p\|-u\|-g] [<name>]` | `git pull` to refresh |
| `ezmk repo list [-p\|-u\|-g]` | List registered repos |

### `utils` — Lua-based tools (0.2.0+)

| Command | Description |
|---|---|
| `ezmk utils <name> [args...]` | Run a Lua-based tool from an installed utils package |

Utils tools are packages (`type = "utils"`) installed via `ezmk pkg install`. They expose Lua scripts under `utils/<name>.lua`. See `docs/utils.md` for the full plugin API.

**Built-in tools:**

| Tool | Description |
|---|---|
| `ezmk utils cc` | Generate `compile_commands.json` (clangd-compatible) |
| `ezmk utils cc -o <path>` | Output to custom path |

### Scope flags

| Flag | Scope | Install path |
|---|---|---|
| `-p` | Project | `<project>/.ezmk/pkg/` |
| `-u` | User | `~/.local/ezmk/pkg/` (Linux/macOS) or `%LOCALAPPDATA%/ezmk/pkg/` (Windows) |
| `-g` | Global | `<ezmk_install_dir>/pkg/` |

`install` and `repo add` accept only one scope flag; others accept combinations like `-pug`.

## Project structure

```
my_project/
  .ezmk/
    pkg/            # installed packages
    temp/           # temp files (auto-cleaned)
    cache/          # build cache (record.json + obj/)
    repo/           # repo registry + cloned repos
      list.toml
      .cache/
  include/          # project headers (*.h, *.hpp)
  src/              # project sources (*.c, *.cpp, *.cxx)
  build/            # build output
  ezmk.toml         # project configuration
```

## Configuration (`ezmk.toml`)

```toml
[project]
name = "myapp"
type = "executable"     # executable | static | shared | utils
version = "0.1.0"
language = "C++17"      # C++17 | C++20 | C11 | ...

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]
src_dirs = ["src"]                    # 0.2.2+ multi-directory source scanning
ezmk_macros = true                    # 0.2.2+ inject EZMK_* standard macros
msvc_flags = []                       # 0.2.1+ MSVC-only flags

[compile.macros]                      # 0.2.2+ semantic macro definitions
DEBUG = ""
VERSION = "0.1.0"

[link]
flags = []
link_dirs = []
system_target = ["pthread"]
msvc_flags = []                       # 0.2.1+ MSVC-only link flags

[depends]
lib = ["foo", "bar"]                  # hard dependencies (missing → error)
want = ["sqlite3", "zlib"]            # 0.2.2+ optional dependencies
```

### Utils package config

```toml
[project]
name = "ezmk-cc"
type = "utils"
version = "0.1.0"

[utils]
tools = ["cc"]
```

## Repository

A repo is a **git repository** containing `index.toml` + `packages/` directory. `ezmk repo add` clones to a local cache; `ezmk repo update` does `git pull`. Local directories also supported. See `docs/repo.md`.

## Design docs

| Document | Topic |
|---|---|
| `docs/config_file.md` | Full `ezmk.toml` specification |
| `docs/pkg.md` | Package format and lifecycle |
| `docs/repo.md` | Git-based repository system |
| `docs/utils.md` | Lua-based plugin tool system and API reference |
| `docs/@cache.md` | Incremental build cache |
| `docs/@safety.md` | Safety invariants |
| `CHANGES.md` | Version changelog |
| `plans/` | Version milestone plans |
| `plan.md` | Current execution plan |
