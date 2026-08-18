# EazyMake Tutorial

A hands-on, zero-to-productive guide to `ezmk`. Read the basics in order (each
chapter builds on the previous one and ends with commands you can actually run);
read the advanced groups on demand.

This tutorial teaches **how to get things done**. For precise definitions and the full
option surface, see the [`docs/en/`](../../docs/en/) reference (especially [`docs/en/cli.md`](../../docs/en/cli.md)).

## Chapters

### Basics

Start with "Install & verify" and read in order:

1. [Install & verify](basic/01-install.md)
2. [Your first project](basic/02-first-project.md)
3. [Understanding `ezmk.toml`](basic/03-config.md)
4. [Incremental builds & caching](basic/04-cache.md)
5. [Build profiles & parallelism](basic/05-profiles-parallel.md)

### Package management

Read on demand, in any order:

1. [Using packages](packages/01-packages.md)
2. [Semantic version constraints & deterministic builds](packages/02-version-lockfile.md)
3. [Third-party & private repositories](packages/03-third-party-repos.md)

### Developer experience

Read on demand, in any order:

1. [Watch mode & hooks](dev/01-watch-hooks.md)
2. [Utils tools (clangd integration)](dev/02-utils.md)
3. [Testing your project](dev/03-test.md)
4. [Top-level aliases (quick reference)](dev/04-top-level-aliases.md)

### Toolchain interop

Read on demand, in any order:

1. [Importing a CMake project](interop/01-import-cmake.md)
2. [Multi-platform, multi-toolchain precompiled packages](interop/02-precompiled-packages.md)

## Conventions

- Shell snippets assume Linux/macOS/MSYS2. On bare Windows, use the
  [PowerShell installer](basic/01-install.md#windows-native-no-msys2) (`install.ps1`)
  or download the prebuilt `ezmk.exe` from the
  [GitHub Release](https://github.com/3667808244/EazyMake/releases).
- `$` marks a command you type; lines without it are output.
- Every command has a short alias (e.g. `ezmk pb` = `ezmk project build`) — see
  [`docs/en/cli.md`](../../docs/en/cli.md#command-shorthands-026).
- Most `project` actions also have **top-level aliases** (`ezmk build`, `ezmk run`,
  `ezmk clean`, `ezmk watch`, `ezmk install`, `ezmk test`, `ezmk pack`) — the
  tutorials use these short forms; the full `ezmk project <action>` forms are
  equivalent.
