# EazyMake Tutorial

A hands-on, zero-to-productive guide to `ezmk`. Each chapter builds on the previous
one and ends with commands you can actually run.

This tutorial teaches **how to get things done**. For precise definitions and the full
option surface, see the [`docs/`](../docs/) reference (especially [`docs/cli.md`](../docs/cli.md)).

## Chapters

1. [Install & verify](01-install.md)
2. [Your first project](02-first-project.md)
3. [Understanding `ezmk.toml`](03-config.md)
4. [Incremental builds & caching](04-cache.md)
5. [Build profiles & parallelism](05-profiles-parallel.md)
6. [Using packages](06-packages.md)
7. [Watch mode & hooks](07-watch-hooks.md)
8. [Utils tools (clangd integration)](08-utils.md)

## Conventions

- Shell snippets assume Linux/macOS/MSYS2. On bare Windows, use the prebuilt
  `ezmk.exe` from the GitHub Release.
- `$` marks a command you type; lines without it are output.
- Every command has a short alias (e.g. `ezmk pb` = `ezmk project build`) — see
  [`docs/cli.md`](../docs/cli.md#command-shorthands-026).
