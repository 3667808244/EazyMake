# EazyMake

A simple C/C++ build tool — `ezmk`. Based on GCC/g++ (MSYS2 on Windows, or native on Linux).

**Design philosophy:** ease of use over feature richness. For complex builds, use CMake.

## Quick start

### Build EazyMake itself

```bash
# Via helper script
bash build.sh

# Or manually — MSYS2 / Windows
g++ -std=c++17 src/*.cpp src/vendor/*.c -I include/ -I include/vendor/ -o ezmk -lwinhttp -static

# Linux
g++ -std=c++17 src/*.cpp src/vendor/*.c -I include/ -I include/vendor/ -o ezmk -static
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

## CLI reference

### `project` — build your code

| Command | Description |
|---|---|
| `ezmk project new <name> [--type executable\|static\|shared]` | Scaffold a new project |
| `ezmk project build [--disable-cache]` | Incremental build |
| `ezmk project run [--disable-cache]` | Build and execute |
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

### Scope flags

| Flag | Scope | Install path |
|---|---|---|
| `-p` | Project | `<project>/.ezmk/pkg/` |
| `-u` | User | `~/.local/ezmk/pkg/` |
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
type = "executable"     # executable | static | shared
version = "0.1.0"
language = "C++17"      # C++17 | C++20 | C11 | ...

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]

[link]
flags = []
link_dirs = []
system_target = ["pthread"]

[depends]
lib = ["foo", "bar"]
```

## Repository

A repo is a **git repository** containing `index.toml` + `packages/`. `ezmk repo add` clones; `ezmk repo update` does `git pull`. Local directories also supported. See `docs/repo.md`.

## Design docs

| Document | Topic |
|---|---|
| `docs/config_file.md` | Full `ezmk.toml` specification |
| `docs/pkg.md` | Package format and lifecycle |
| `docs/repo.md` | Git-based repository system |
| `docs/@cache.md` | Incremental build cache |
| `docs/@safety.md` | Safety invariants |
| `plan.md` | Current milestone plan + progress |
