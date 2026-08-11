# EazyMake

[中文](README_ZH.md) · English

**写 C++，但不需要写 CMake。**

A simple C/C++ build tool — `ezmk`. Supports GCC, Clang, and MSVC.

**Design philosophy:** ease of use over feature richness. For complex builds, use CMake.

> **Stable public API as of v1.1.0.** Breaking changes are introduced only in `2.0.0`, preceded by deprecation warnings in at least one minor version. See [CHANGES.md](CHANGES.md#api-stability).

## Why EazyMake

- **Zero config** — `ezmk project new my_app && cd my_app && ezmk build` and you're running
- **Built-in package manager** — `ezmk pkg install fmt` downloads, compiles, and links automatically
- **Cross-compiler** — same `ezmk.toml` works with GCC, Clang, and MSVC
- **Extensible** — Lua scripting for custom build logic and utility tools

## Quick start

### Install

**Linux / macOS / MSYS2:**

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

**Windows (native, no MSYS2):**

```powershell
irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex
```

Customize with `PREFIX`, `EZMK_REF`, `EZMK_NO_DEFAULT_REPO`. See [install options](#install-options).

### Your first project

```bash
ezmk project new hello
cd hello
ezmk build                # compile + link
ezmk run                  # build + run
```

### Install a package

```bash
ezmk pkg install fmt      # by name — official repo pre-registered
```

## vs CMake

| EazyMake | CMake |
|----------|-------|
| `ezmk project new app && cd app && ezmk build` | `mkdir build && cd build && cmake .. && make` |
| `ezmk pkg install fmt` | `find_package(fmt)` + manual install |
| 1 TOML file | 1+ `CMakeLists.txt` files |
| Auto-detects compiler | `-DCMAKE_CXX_COMPILER=...` |

## Configuration

```toml
[project]
name = "myapp"
type = "executable"     # executable | static | shared | utils
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]

[link]
system_target = ["pthread"]

[depends]
lib = ["fmt", "zlib"]           # hard dependencies
want = ["sqlite3"]              # optional dependencies
```

Full reference: [`docs/en/config_file.md`](docs/en/config_file.md)

## Commands at a glance

```bash
# Daily
ezmk build [flags]              # incremental build
ezmk run [flags] [-- args]      # build and run
ezmk clean                      # clear cache
ezmk watch [flags]              # auto-rebuild on change
ezmk install [flags]            # install artifacts to prefix
ezmk test [flags]               # run project tests
ezmk pack [flags]               # create distributable .tar.gz (full: ezmk project pack)

# Project
ezmk project new <name>         # scaffold new project
ezmk project cc [flags]         # generate compile_commands.json for clangd
ezmk project export cmake [flags]  # generate CMakeLists.txt from ezmk.toml

# Packages
ezmk pkg install <pkg>          # install a package
ezmk pkg search <pkg>           # search registered repos
ezmk pkg list                   # list installed packages
ezmk pkg update [<pkg>]         # update to latest

# Repos
ezmk repo add <url>             # register a repo
ezmk repo update                # refresh repo indices

# Utilities
ezmk utils cc                   # generate compile_commands.json (deprecated since 1.2.0 → ezmk project cc)
```

Full reference: [`docs/en/cli.md`](docs/en/cli.md)

## Install options

| Variable / Flag | Purpose | Default |
|-----------------|---------|---------|
| `PREFIX` | Install prefix (binary → `$PREFIX/bin`) | `$HOME/.local` |
| `EZMK_REF` | git tag/branch/commit to build | default branch |
| `EZMK_NO_DEFAULT_REPO` | Set to `1` to skip official repo registration | (registers) |
| `-Version` (PS) | Version tag to install | `"latest"` |
| `-InstallDir` (PS) | Root install directory | `$env:LOCALAPPDATA\ezmk` |
| `-DryRun` (PS) | Preview without making changes | (off) |

## Documentation

| Document | Topic |
|----------|-------|
| [Tutorial](tutorial/en/) | Hands-on, zero-to-productive guide |
| [CLI Reference](docs/en/cli.md) | Full command & environment variable reference |
| [Config File](docs/en/config_file.md) | Complete `ezmk.toml` specification |
| [Package Management](docs/en/pkg.md) | Package format and lifecycle |
| [Repository System](docs/en/repo.md) | Git-based repo system |
| [Lua Plugins](docs/en/utils.md) | Plugin tool system and API reference |
| [FAQ / Troubleshooting](docs/en/faq.md) | Common questions and fixes |
| [Technical Details](docs/en/technical.md) | Dependencies, build from source, compiler support |
| [Glossary](docs/en/glossary.md) | Terminology reference |
| [Non-Goals](docs/en/non-goals.md) | Features EazyMake deliberately won't design |
| [Changelog](CHANGES.md) | Version history |

## Related links

- [Build from source & tests](docs/en/technical.md#building-eazymake)
- [MSVC support](docs/en/technical.md#using-msvc)
- [zsh completion](docs/en/technical.md#shell-completion-zsh)
- [Repository](https://github.com/3667808244/ezmk-repo) — official package repository
