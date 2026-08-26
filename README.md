# EazyMake

[中文](README_ZH.md) · English

**写 C++，但不需要写 CMake。**

A simple C/C++ build tool — `ezmk`. Supports GCC, Clang, and MSVC.

**Design philosophy:** ease of use over feature richness. For [complex builds](docs/en/complex-builds.md), use CMake.

> **Stable public API as of v1.1.0.** Breaking changes are introduced only in `2.0.0`, preceded by deprecation warnings in at least one minor version. See [CHANGES.md](CHANGES.md#api-stability).

## Contents

- [Quick start](#quick-start)
- [Install](#install)
- [Commands at a glance](#commands-at-a-glance)
- [Advanced features](#advanced-features)
- [Documentation](#documentation)

## Why EazyMake

- **Zero config** — `ezmk project new my_app && cd my_app && ezmk build` and you're running
- **Built-in package manager** — `ezmk pkg install fmt` downloads, compiles, and links automatically
- **Cross-compiler** — same `ezmk.toml` works with GCC, Clang, and MSVC
- **Extensible** — Lua scripting for custom build logic and utility tools

## Quick start

### Your first project

```bash
ezmk project new hello
cd hello
ezmk build                # compile + link
ezmk run                  # build + run
```

> **Build from a subdirectory too (1.2.0+):** `ezmk build` / `ezmk test` and
> friends locate `ezmk.toml` by walking **up at most 5 parent directories** from
> the current directory — drop into `src/` and run directly, just like `git`.

### Install a package

```bash
ezmk pkg install fmt      # by name — official repo pre-registered
ezmk pkg install ./mylib  # from a source directory (1.2.0+, no packing needed)
```

## Install

**macOS / Linux — recommended (Homebrew):**

```bash
brew tap 3667808244/eazymake && brew install ezmk
```

> Homebrew covers macOS (Apple Silicon) and Linux x64. macOS Intel has no prebuilt binary yet — use the script below.

**Arch Linux / MSYS2 — pacman (1.2.0+):**

```bash
# Arch Linux
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/publish/arch/PKGBUILD -o PKGBUILD
makepkg -si

# MSYS2 (MINGW64 shell)
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/publish/arch/PKGBUILD -o PKGBUILD
makepkg -si --nodeps
```

> Source build (statically linked); requires `base-devel` (Arch) or the MINGW64 toolchain (MSYS2; `--nodeps` skips the dependency check). Not on AUR yet (registration unavailable) — self-serve `publish/arch/PKGBUILD` + `makepkg -si` for now.

**Alternative — install script:**

**Linux / macOS / MSYS2:**

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

**Windows (native, no MSYS2):**

```powershell
irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex
```

Customize with `PREFIX`, `EZMK_REF`, `EZMK_NO_DEFAULT_REPO`. See [install options](#install-options).

### Install options

| Variable / Flag | Purpose | Default |
|-----------------|---------|---------|
| `PREFIX` | Install prefix (binary → `$PREFIX/bin`) | `$HOME/.local` |
| `EZMK_REF` | git tag/branch/commit to build | default branch |
| `EZMK_NO_DEFAULT_REPO` | Set to `1` to skip official repo registration | (registers) |
| `-Version` (PS) | Version tag to install | `"latest"` |
| `-InstallDir` (PS) | Root install directory | `$env:LOCALAPPDATA\ezmk` |
| `-DryRun` (PS) | Preview without making changes | (off) |

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
ezmk pack [flags]               # create distributable archive (full: ezmk project pack; --format zip|tar.gz, 1.3.5+)

# Project
ezmk project new <name>         # scaffold new project
ezmk project cc [flags]         # generate compile_commands.json for clangd
ezmk project export cmake [flags]  # generate CMakeLists.txt from ezmk.toml
ezmk project import [flags]     # import a CMake project into ezmk.toml (experimental)

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

# Examples (1.2.3+)
ezmk example list               # list built-in examples
ezmk example hello              # scaffold an example into ./hello/

# Workspaces (1.3.0+) — manage a set of projects as one unit
ezmk workspace list             # list workspace members
ezmk workspace build [flags]    # build all members (topological, parallel)
ezmk workspace test [flags]     # run member tests
ezmk workspace clean [flags]    # clean member caches
# build/test/clean accept -w to redirect: ezmk build -w ≡ ezmk workspace build
```

Full reference: [`docs/en/cli.md`](docs/en/cli.md)

## Advanced features

| Feature | One-liner | Links |
|---------|-----------|-------|
| Semantic version constraints | `[depends]` entries like `fmt@1.2.3` / `^1.0` / `~1.2` / `>=1.0` pin dependency versions precisely | [`docs/en/config_file.md`](docs/en/config_file.md) · Tutorial [Pkg 02](tutorial/en/packages/02-version-lockfile.md) |
| `ezmk.lock` deterministic builds | Lock dependency versions and content hashes; `--locked` gives reproducible CI builds | [`docs/en/config_file.md`](docs/en/config_file.md) · Tutorial [Pkg 02](tutorial/en/packages/02-version-lockfile.md) |
| Multi-platform / multi-toolchain precompiled packages | One package ships `win-x64-msvc143` / `linux-x64-gcc13-abi11` artifacts, auto-selected for the current toolchain | [`docs/en/package_authoring.md`](docs/en/package_authoring.md) · Tutorial [Interop 02](tutorial/en/interop/02-precompiled-packages.md) |
| Third-party / private repos | `ezmk repo add <url>` wires in git-repo third-party sources | [`docs/en/repo.md`](docs/en/repo.md) · Tutorial [Pkg 03](tutorial/en/packages/03-third-party-repos.md) |
| CMake interop | `project export cmake` to export / `project import --from cmake` to import (experimental) | [`docs/en/cli.md`](docs/en/cli.md) · Tutorial [Interop 01](tutorial/en/interop/01-import-cmake.md) |
| compile_commands | `project cc` generates compile_commands.json for clangd/IDEs | [`docs/en/cli.md`](docs/en/cli.md) · Tutorial [Dev 02](tutorial/en/dev/02-utils.md) |
| Workspace batch management (1.3.0+) | `ezmk-workspace.toml` declares a member set; `workspace build/test/clean` with topological ordering + parallelism; one-way acyclic member deps with automatic static-library artifact injection | [`docs/en/cli.md`](docs/en/cli.md) · Tutorial [Dev 05](tutorial/en/dev/05-workspace.md) |

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
- [Gitee mirror](https://gitee.com/egglzh/EazyMake) — main repo mirror (auto-synced from GitHub, for China access)
