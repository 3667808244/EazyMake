# 15. Workspaces: managing a set of projects

> 1.3.0+. `ezmk workspace` organizes **several independent projects (members) under one directory** and manages them as a unit: one command builds/tests/cleans all members, and members can declare dependencies with automatic artifact reuse — the most common monorepo shape, "a shared base library plus several executables", is handled by a single `ezmk workspace build`.

## The scenario

Say you have a monorepo: a static library `strutil` (string utilities) plus two executables `tool-a` / `tool-b`, both depending on `strutil`:

```
ws/
├── ezmk-workspace.toml          # workspace config
├── libs/strutil/                # static-library member
│   ├── ezmk.toml
│   ├── include/strutil.hpp
│   └── src/strutil.cpp
└── apps/
    ├── tool-a/                  # executable member, depends on strutil
    │   ├── ezmk.toml
    │   └── src/main.cpp
    └── tool-b/                  # executable member, depends on strutil
        ├── ezmk.toml
        └── src/main.cpp
```

Without a workspace you would `cd` into each project and run `ezmk build` separately, and `tool-a` would have to add `strutil`'s header directory and `libstrutil.a` to its own config by hand. The workspace automates all of that.

## Step 1: declare the workspace

Write `ezmk-workspace.toml` at the root:

```toml
[workspace]
members = ["apps/tool-a", "apps/tool-b", "libs/strutil"]
```

`members` are paths relative to the workspace root (required, non-empty). Optional `[workspace.options]`:

```toml
[workspace.options]
default_jobs = 4        # intra-layer parallelism (default 0 = auto)
stop_on_error = false   # stop dispatching after the first failure (default false)
```

## Step 2: members declare dependencies

Every member is still an **independent `ezmk` project** (its own `ezmk.toml`). `tool-a` declares the sibling dependency in one line:

```toml
[depends]
workspace = ["strutil"]        # sibling member: basename (when unique) or full relative path "libs/strutil"
```

`strutil` itself needs **no changes**. When building `tool-a`, `ezmk` automatically injects `strutil`'s headers and static library:

```
-I ws/libs/strutil/include -L ws/libs/strutil/build -lstrutil
```

> **Zero environment variables**: injection is **self-discovery** by the member's build process (it reads the workspace file and its own `[depends] workspace`) — no `EZK_WS_*` variables, so the command line never grows with workspace size.

## Step 3: build everything with one command

```bash
$ cd ws
$ ezmk workspace build -j 4
workspace build: 3 member(s), 4 job(s)
[libs/strutil] build...
[apps/tool-a] build...
[apps/tool-b] build...
workspace build: 3 succeeded, 0 failed, 0 skipped
```

- **Topological order**: dependency layers build first — `strutil` before `tool-a` / `tool-b`.
- **Intra-layer parallelism**: independent members build concurrently (`-j` controls the degree; `-w` redirects — `ezmk build -w` ≡ `ezmk workspace build`, usable from any subdirectory of the workspace).

## Step 4: cross-member incrementality

The core value of a workspace: **change library code and dependents relink/recompile automatically — no manual `clean`**.

```bash
# Change strutil's implementation (.cpp) → next workspace build:
#   strutil recompiles + tool-a/tool-b only RELINK (their compiles are cache hits)
$ ezmk workspace build -j 4

# Change strutil's header (.hpp) → next workspace build:
#   members referencing that header recompile automatically (depfile tracks injected headers)
$ ezmk workspace build -j 4
```

## Step 5: testing and cleaning

```bash
$ ezmk workspace test             # per-member `ezmk test`; members without tests are skipped (no error)
$ ezmk workspace clean            # clean members in reverse dependency order (caches/temp only, build/ artifacts kept)
```

## Building a subset

- **`--member <name>` includes the dependency closure**: `ezmk workspace build --member tool-a` first builds `tool-a`'s dependency `strutil`, then `tool-a` — artifacts stay fresh. `--member apps/tool-a` (full path) and `--member tool-a` (basename) are equivalent.
- **A single member without the closure**: `cd apps/tool-a && ezmk build` — builds only the current member, injecting **already-existing** sibling artifacts (a missing `strutil` build prints a hint and may fail at link time).

## Adopting existing projects: `ezmk workspace scan` (1.4.0-dev.7+)

You don't have to hand-write `ezmk-workspace.toml`. When you already have a directory full of ezmk projects (cloned repos, projects imported from CMake, hand-written ones), one command adopts them all:

```bash
$ cd ws
$ ezmk workspace scan        # or the shorthand: ezmk ws
found 3 member(s) — created ezmk-workspace.toml
```

`scan` recursively collects every subdirectory containing `ezmk.toml` into `members` (sorted, `/`-separated). Rules:

- **Hidden** entries (`.git`, `.ezmk`, …) are skipped entirely.
- A subdirectory that is itself a **nested workspace root** is skipped (its whole subtree belongs to that workspace).
- The **scan root** itself is never a member, even if it has its own `ezmk.toml`.

Running it again is a **sync**: when `ezmk-workspace.toml` already exists, `scan` merges — it keeps your `name`, `[workspace.options]` and comments, and appends only newly-discovered members (existing order preserved). A merge prompts for confirmation (`-y` accepts, `--dry-run` previews):

```bash
$ cd ws/apps/tool-a && ezmk ws -y      # from a member subdir — updates the ROOT file
$ ezmk workspace scan --dry-run        # preview without writing
```

After adoption, declare sibling dependencies by hand (`[depends] workspace = [...]` in each member) — `scan` never guesses your dependency intent.

## When something fails

```bash
$ ezmk workspace build --stop-on-error -j 4
```

`--stop-on-error` semantics: after a member fails, **no new tasks are dispatched** — not-yet-started members of the current layer and all later layers are marked `skipped` (visible in the summary); members already running **finish naturally, never killed**. Without the flag, all members run and any failure gives a non-zero exit. `clean` does not support the flag.

## Constraints and limits

- Member dependencies must be **one-way acyclic** — cycles (including self-loops) are rejected when the config loads.
- The depended-on member must be `type = "static"` (`executable` / `shared` cannot be depended on).
- Member dependencies have **no versions** — develop-and-use; for versioned, distributable reuse, use packages (`[depends] lib` + `ezmk pkg install`).
- Full semantics: the [`docs/en/cli.md`](../../../docs/en/cli.md) `workspace` section; config: `[depends] workspace` in [`docs/en/config_file.md`](../../../docs/en/config_file.md).
