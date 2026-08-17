# 13. Third-party & private repositories

The official repository pre-registers common packages, but your team may need its own package sources: internal libraries, private branches, offline mirrors. EazyMake's repository system is git-based — `ezmk repo add <url>` registers, `ezmk repo update` refreshes.

## Registering a repository

```bash
$ ezmk repo add git@github.com:your-org/ezmk-repo.git
$ ezmk repo add -u https://github.com/example/repo.git --name community --branch stable
$ ezmk repo add -p E:/packages/my-dev-repo --name local-dev
```

| Flag | Meaning |
| ---- | ------- |
| `-p` / `-u` / `-g` | project / user / global scope (default: project) |
| `<url>` | git clone URL or local directory path |
| `--name <name>` | repo name (inferred from URL when omitted: trailing path, minus `.git`) |
| `--branch <branch>` | branch to track, git repos only, default `main` |

- **git repository**: ezmk clones it into the scope's cache directory, then you can install packages by name.
- **local directory**: after validating `index.toml`, ezmk reads it **in place** — changes take effect immediately; great for in-development packages or offline mirrors.

## index.toml: the repository's package index

A repository root must have an `index.toml`:

```toml
[repo]
name = "my-repo"
description = "My project's package repository"

[[packages]]
name = "foo"
version = "0.1.0"
file = "packages/foo-0.1.0.zip"
sha256 = "a1b2c3d4e5f6..."   # optional, but strongly recommended
```

- Multiple versions of the same package are repeated `[[packages]]` entries (same `name`, different `version`).
- `pkg install` picks the highest version by default; versions that don't satisfy constraints are skipped.

### `[platform]`: platform-specific archive paths (1.1.0+)

When different platforms need different archives, map platform keys to path prefixes with `[platform]`:

```toml
[platform]
windows_x86_64_msvc = "win/msvc"
windows_x86_64      = "win/gcc"
linux_x86_64_gcc    = "linux/gcc"
darwin_arm64_clang  = "mac/clang"
```

Keys are `{os}_{arch}[_{toolchain}]` (`os`: `windows` / `linux` / `darwin`; `toolchain`: `gcc` / `clang` / `msvc`). ezmk tries the triple key (with toolchain) first, then falls back to the double key; the value is used as the path prefix for that platform's archives.

## Refreshing the index

After `repo add` (and whenever the repository updates), refresh the cached index:

```bash
$ ezmk repo update
```

> **Why `repo update` before the first install?** Name-based search reads the cached `index.toml` — the snapshot from the last clone or update. `repo update` runs `git pull` to refresh it; otherwise the index may be stale.

## Private repository authentication

`repo add` / `repo update` run `git clone` / `git pull` as subprocesses, inheriting the full environment and git global config — configure git the way you already do (SSH keys, credential helpers, CI tokens) and ezmk pulls as usual.

## Pitfalls

- **`-p/-u/-g`: pick exactly one** — `pkg install` and `repo add` accept a single scope flag.
- **Local directories aren't cloned and have no version snapshot** — changes apply instantly; fine for development, but publish via a git repo for distribution.
- **Run `repo update` before the first by-name install** — otherwise you use the stale index snapshot from registration.
