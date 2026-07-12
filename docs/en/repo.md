# Repository Management

An EazyMake repository is a **git repository**. Users register repositories via `ezmk repo add <git_url>`, the tool automatically runs `git clone` to a local cache, after which `pkg install` can install by package name (rather than URL).

---

## Repository Structure

An EazyMake repository is a git repository whose root directory contains:

```
<repo>.git/
  index.toml           # repository metadata + package index (required)
  packages/            # package archive directory (standard libraries and utils tools)
    foo-0.1.0.zip
    bar-1.2.0.tar.gz
    bar-1.1.0.tar.gz
    ezmk-cc-0.1.0.zip  # utils tool packages are also normal packages, placed under packages/
    ...
```

### Design Rationale

| Feature        | git approach                           | Static directory approach             |
| -------------- | -------------------------------------- | ------------------------------------- |
| Incremental update | `git pull`, only fetches diffs     | Re-download entire `index.toml`       |
| Version tracking | git log / tag, naturally traceable  | Must maintain version files manually  |
| Hosting        | GitHub, GitLab, Gitee, self-hosted    | Requires a file server                |
| Offline        | Usable locally after clone            | Also possible, but acquisition is manual |

---

## `index.toml` Format

```toml
[repo]
name = "my-repo"
description = "My project's package repository"

[[packages]]
name = "foo"
version = "0.1.0"
file = "packages/foo-0.1.0.zip"
sha256 = "a1b2c3d4e5f6..."   # optional, but strongly recommended

[[packages]]
name = "bar"
version = "1.2.0"
file = "packages/bar-1.2.0.tar.gz"
sha256 = "d4e5f6a7b8c9..."

[[packages]]
name = "bar"
version = "1.1.0"
file = "packages/bar-1.1.0.tar.gz"
sha256 = "g7h8i9j0k1l2..."

[[packages]]
name = "ezmk-cc"
version = "0.1.0"
file = "packages/ezmk-cc-0.1.0.zip"
sha256 = "hsiqno182bl2..."
```

### `[repo]` Section

| Field         | Type   | Required | Description                                    |
| ------------- | ------ | -------- | ---------------------------------------------- |
| `name`        | string | Yes      | Repository name, used as identifier after registration |
| `description` | string | No       | Repository description, shown by `ezmk repo list` |

### `[[packages]]` Section (repeatable)

| Field     | Type   | Required | Description                                         |
| --------- | ------ | -------- | --------------------------------------------------- |
| `name`    | string | Yes      | Package name                                        |
| `version` | string | Yes      | Package version, SemVer recommended                 |
| `file`    | string | Yes      | Path to the package archive relative to the repo root |
| `sha256`  | string | No       | SHA-256 checksum of the archive (recommended)       |

Multiple versions of the same package are represented by repeating `[[packages]]` entries with the same `name` but different `version`. `pkg install` installs the latest version by default.

---

## Repository Registration and Local Cache

### Registry `list.toml`

Storage paths for the registered repository list:

| Scope   | Path                                 |
| ------- | ------------------------------------ |
| Global  | `<ezmk_install_dir>/repo/list.toml`  |
| User    | `~/.local/ezmk/repo/list.toml`       |
| Project | `<project_dir>/.ezmk/repo/list.toml` |

Format:

```toml
[[repos]]
name = "my-repo"
url = "git@github.com:user/ezmk-repo.git"
type = "git"
branch = "main"
last_update = "2026-06-19T12:00:00Z"

[[repos]]
name = "community"
url = "https://gitee.com/example/ezmk-repo.git"
type = "git"
branch = "main"
last_update = ""

[[repos]]
name = "local-dev"
url = "E:/packages/my-dev-repo"
type = "local"
last_update = "2026-06-19T10:00:00Z"
```

| Field         | Description                                                          |
| ------------- | -------------------------------------------------------------------- |
| `name`        | Unique repository identifier                                         |
| `url`         | git clone URL (`type = "git"`) or local directory path (`type = "local"`) |
| `type`        | `"git"` or `"local"`                                                 |
| `branch`      | Tracked branch, valid when `type = "git"`, default `main`            |
| `last_update` | Timestamp of the last `update`                                       |

### Local Cache Paths

Destination directories for `git clone`:

| Scope   | Cache Path                                      |
| ------- | ----------------------------------------------- |
| Global  | `<ezmk_install_dir>/repo/.cache/<repo_name>/`   |
| User    | `~/.local/ezmk/repo/.cache/<repo_name>/`        |
| Project | `<project_dir>/.ezmk/repo/.cache/<repo_name>/`  |

For `type = "local"` repositories, there is no `.cache/` directory — the local path pointed to by `url` is used directly.

---

## Subcommands

### `ezmk repo add`

Register a new repository and clone it to the local cache.

```
ezmk repo add [-p|-u|-g] <url> [--name <name>] [--branch <branch>]
```

| Parameter           | Description                                                                 |
| ------------------- | --------------------------------------------------------------------------- |
| `-p`                | Project scope (default)                                                     |
| `-u`                | User scope                                                                  |
| `-g`                | Global scope                                                                |
| `<url>`             | git clone URL (e.g. `https://github.com/user/repo.git`) or local directory path |
| `--name <name>`     | Repository name (optional). If omitted, inferred from URL: last path segment, `.git` stripped |
| `--branch <branch>` | Branch to track, git repos only, default `main`                             |

**Examples**:

```bash
# Register a GitHub repository (default project scope)
ezmk repo add git@github.com:user/ezmk-repo.git

# Register with custom name and branch
ezmk repo add -u https://github.com/example/repo.git --name community --branch stable

# Register a local directory (for development/debugging)
ezmk repo add -p E:/packages/my-dev-repo --name local-dev

# Global scope
ezmk repo add -g https://gitee.com/org/public-repo.git
```

**Behavior**:
1. Parse URL type — contains `://` or starts with `git@` and does not contain `://` → git repository; plain path (starts with `/` or a drive letter) → local directory
2. For git repositories:
   - Create a subdirectory under `.cache/` for the corresponding scope
   - Execute `git clone --branch <branch> <url> <cache_path>`
   - If clone fails → error and clean up
3. For local directories:
   - Validate that `index.toml` exists and is well-formed
   - Do not clone; record the path directly
4. Write to `list.toml`
5. If a repository with the same name already exists → error (run `remove` first if replacement is needed)

### `ezmk repo remove`

Remove a registered repository and delete its local cache.

```
ezmk repo remove [-p|-u|-g] <name>
```

| Parameter  | Description                  |
| ---------- | ---------------------------- |
| `-p`       | Project scope                |
| `-u`       | User scope                   |
| `-g`       | Global scope                 |
| `<name>`   | Name of the repository to remove |

- Default scope: `-pug` (search all scopes and remove the first match)
- Removal deletes the cache directory (`.cache/<name>/`), but does not affect the original path for `type = "local"`
- If not found → error

**Examples**:

```bash
ezmk repo remove my-repo           # all scopes
ezmk repo remove -g community      # global scope only
```

### `ezmk repo update`

Update repository index — runs `git pull` for git repositories, re-reads for local directories.

```
ezmk repo update [-p|-u|-g] [<name>]
```

| Parameter       | Description                                      |
| --------------- | ------------------------------------------------ |
| `[-p\|-u\|-g]` | Scope, default `-pug`                            |
| `<name>`        | Repository name (optional). If omitted, updates all registered repos |

**Behavior**:
1. **Git repositories** (`type = "git"`):
   - Run `git pull origin <branch>` in the cache directory
   - If local cache does not exist (previous clone failed or was manually deleted), re-run `git clone`
   - `git pull` failure → warning, not an error (network issues should not block builds)
2. **Local directories** (`type = "local"`):
   - Re-read `index.toml`
3. Update the `last_update` timestamp in `list.toml`

**Examples**:

```bash
ezmk repo update                    # update all
ezmk repo update -p my-repo        # update only my-repo in project scope
```

### `ezmk repo list`

List registered repositories and their status.

```
ezmk repo list [-p|-u|-g]
```

| Parameter       | Description                         |
| --------------- | ----------------------------------- |
| `[-p\|-u\|-g]` | Filter by scope, default `-pug`     |

**Example output**:

```
Repositories (project scope):
  my-repo       git@github.com:user/repo.git (main)    2026-06-19 12:00
  local-dev     E:/packages/my-dev-repo (local)         2026-06-19 10:00

Repositories (user scope):
  community     https://gitee.com/example/repo.git (stable)  2026-06-19 08:30

Repositories (global scope):
  (none)
```

---

## Integration with `pkg install`

Once repositories are registered, packages can be installed by name (instead of a full URL):

```bash
# Old way: provide a full URL every time
ezmk pkg install -p https://raw.githubusercontent.com/user/repo/main/packages/foo.zip

# New way: install by name after registration
ezmk repo add -p git@github.com:user/ezmk-repo.git --name my-repo
ezmk pkg install -p foo
```

### Search Order

When the argument to `pkg install` is neither a local file path nor a URL containing `://`:

1. Search the registry `list.toml` in **project → user → global** order
2. Within each scope, iterate repositories in registration order
3. For each repository, read its `index.toml` (local cache for git repos, source path for local repos)
4. Search `[[packages]]` by `name`
5. If multiple versions of the same package exist, pick the highest version number
6. Retrieve the archive file from the repository's `packages/` directory (local cache for git repos; re-install if missing)
7. If no repository contains the package → error

### Caveats

- Ensure `ezmk repo update` has been run before the first `pkg install foo`, otherwise an outdated `index.toml` may be used
- It is recommended to automatically run `ezmk repo update --pug` before `ezmk project build` (optional, may be added in a future version)
- If a package in a repository has a `sha256`, it must be verified during installation

---

## Security

Repository-related security policies (no confirmation required for global registration, secondary confirmation for global install, `sha256` verification, `git clone`/`pull` failure handling) have been consolidated in [`@safety.md`](@safety.md).

---

## Directory Convention Overview

| Path                                     | Description                  |
| ---------------------------------------- | ---------------------------- |
| `<ezmk_install_dir>/repo/list.toml`      | Global repo registry         |
| `<ezmk_install_dir>/repo/.cache/<name>/` | Global repo clone cache      |
| `~/.local/ezmk/repo/list.toml`           | User repo registry           |
| `~/.local/ezmk/repo/.cache/<name>/`      | User repo clone cache        |
| `.ezmk/repo/list.toml`                   | Project repo registry        |
| `.ezmk/repo/.cache/<name>/`              | Project repo clone cache     |

---

## Design Decisions

- **Git-based**: Leverages git's incremental transfer, version traceability, and distributed hosting — no custom server needed
- **Local path compatibility**: `type = "local"` supports local repository directories during development, not forcibly tied to git
- **No server-side dependency resolution**: Dependency resolution is done entirely on the client (topological sort already exists in `pkg.cpp`); the repository is only responsible for "providing packages"
- **SHA-256 optional**: Recommended but not mandatory — internal repos may omit it, public repos should provide it
- **Independent clone per scope**: Project, user, and global repo caches are independent, avoiding permission and concurrency conflicts
