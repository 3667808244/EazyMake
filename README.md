# EazyMake

[中文](README_ZH.md) · English

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

### Install (Linux / macOS / MSYS2)

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

Builds from source and installs `ezmk` to `$HOME/.local/bin`. Customize with environment variables:

```bash
# Install system-wide
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | PREFIX=/usr/local bash

# Review before running (recommended)
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh -o install.sh
less install.sh
bash install.sh
```

| Variable | Purpose | Default |
|---|---|---|
| `PREFIX` | Install prefix (binary → `$PREFIX/bin`) | `$HOME/.local` |
| `EZMK_REF` | git tag/branch/commit to build | default branch |
| `EZMK_NO_COMPLETIONS` | Set to `1` to skip zsh completions | (installs if zsh found) |
| `CXX` / `CC` / `CXXFLAGS` | Compiler override (passed to `build.sh`) | auto-detected |

> **Bare Windows (non-MSYS2):** download the prebuilt `ezmk.exe` from the [GitHub Release](https://github.com/3667808244/EazyMake/releases) instead.

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
| `ezmk project build [--disable-cache] [--verbose] [-j <N>] [--profile <name>] [--auto-update]` | Incremental build |
| `ezmk project run [--disable-cache] [--verbose] [-j <N>] [--profile <name>] [--auto-update] [-- <program args>]` | Build and execute |
| `ezmk project clean` | Remove cache and temp files |
| `ezmk project watch [--profile <name>] [--no-build-on-start] [-j <N>] [--auto-update]` | Watch for changes and auto-rebuild |

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
| `ezmk repo info [-p\|-u\|-g] <name>` | Show repository details (packages, versions) |

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

### Option syntax (GNU conventions)

EazyMake follows GNU argument syntax conventions:

- **Long options**: `--flag=value` or `--flag value` are equivalent
- **Short option grouping**: `-pug` equals `-p -u -g`
- **Attached values**: `-j4` equals `-j 4`
- **Option/value interleaving**: Options and positional arguments can be freely mixed
- **`--` terminator**: Everything after `--` is treated as a positional argument, enabling pass-through for `utils` and `project run`

### Command shorthands

Every command has a short alias (shown in `ezmk help`). Aliases only apply at the command position:

| Alias | Expands to | | Alias | Expands to | | Alias | Expands to |
|---|---|---|---|---|---|---|---|
| `pn` | `project new` | | `ki` | `pkg install` | | `ra` | `repo add` |
| `pb` | `project build` | | `kr` | `pkg remove` | | `rr` | `repo remove` |
| `pr` | `project run` | | `ks` | `pkg search` | | `rl` | `repo list` |
| `pc` | `project clean` | | `kn` | `pkg info` | | `ru` | `repo update` |
| `pw` | `project watch` | | `kl` | `pkg list` | | `ri` | `repo info` |
| `u` | `utils` | | `ku` | `pkg update` | | `h` / `v` | `help` / `version` |

For example, `ezmk pb -j4` is `ezmk project build -j4`. Shorthands are display/typing sugar only and are not part of the zsh completion.

### Color output (`--color`)

The global `--color=<mode>` option controls ANSI color (may appear on any command):

| Mode | Aliases | Behavior |
|---|---|---|
| `always` | `enable` | Force color (also enables VT100 on legacy Windows terminals) |
| `auto` | `default` | Color only on an interactive terminal (**default**) |
| `never` | `disable` | Disable color |

An explicit `--color=always` / `--color=never` overrides the `NO_COLOR` environment variable; only `--color=auto` honors `NO_COLOR`.

### Shell completion (zsh)

EazyMake ships a static zsh completion script at `completions/_ezmk`:

```bash
# Install system-wide
cp completions/_ezmk /usr/share/zsh/site-functions/

# Or install for current user
mkdir -p ~/.zsh/completions
cp completions/_ezmk ~/.zsh/completions/
# Then add to ~/.zshrc: fpath=(~/.zsh/completions $fpath)
```

After installing, restart your shell or run `autoload -Uz compinit && compinit`. Then `ezmk <TAB>` will offer:
- Top-level command completion (`project`, `pkg`, `repo`, `utils`)
- Subcommand flags (GNU style: `--flag=value`, `-j4`, `-pug`)
- Dynamic completions: build profiles, installed packages, registered repos, utils tools

### Build flags

`--auto-update` runs `ezmk repo update --pug` before building, so packages installed from registered repos resolve the latest index. Default is off — updates are explicit to avoid network latency on every build.

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
| `tutorial/` | Hands-on, zero-to-productive guide |
| `docs/cli.md` | Authoritative CLI & environment variable reference |
| `docs/config_file.md` | Full `ezmk.toml` specification |
| `docs/pkg.md` | Package format and lifecycle |
| `docs/repo.md` | Git-based repository system |
| `docs/utils.md` | Lua-based plugin tool system and API reference |
| `docs/@cache.md` | Incremental build cache |
| `docs/@safety.md` | Safety invariants |
| `CHANGES.md` | Version changelog |
| `plans/` | Version milestone plans |
| `plan.md` | Current execution plan |
