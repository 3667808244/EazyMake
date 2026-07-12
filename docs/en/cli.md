# CLI Reference

Authoritative reference for the `ezmk` command line and environment variables.
This document is the single source of truth; the README command tables are a
quick-start subset. For behavior details see the per-topic docs
(`pkg.md`, `repo.md`, `utils.md`, `config_file.md`, `@cache.md`, `@safety.md`).

## Synopsis

```
ezmk <command> [subcommand] [options] [arguments]
ezmk <shorthand> [options] [arguments]
```

Global options may appear on any command (see [Global options](#global-options)).

---

## `project` — build your code

| Command | Description |
|---|---|
| `ezmk project new <name> [--type <t>]` | Scaffold a new project |
| `ezmk project build [build-opts]` | Incremental build |
| `ezmk project run [build-opts] [-- <program args>]` | Build and execute |
| `ezmk project clean` | Remove cache and temp files |
| `ezmk project watch [build-opts] [--no-build-on-start]` | Watch sources and auto-rebuild |

**`--type <t>`** (for `new`): `executable` (default) · `static` · `shared` · `utils`.

**`build-opts`** (shared by `build` / `run` / `watch`):

| Flag | Purpose |
|---|---|
| `--disable-cache` | Force recompilation (cache is still updated afterward) |
| `--verbose` / `-v` | Show full compile commands and cache hits |
| `-j <N>` / `--jobs <N>` | Parallel compile jobs; `0` = auto (`hardware_concurrency`), the default |
| `--profile <name>` | Apply a build profile from `[compile.profile.<name>]` / `[link.profile.<name>]` |
| `--auto-update` | Run `ezmk repo update --pug` before building (default off) |

**`new`-only flags:**

| Flag | Purpose |
|---|---|
| `--disable-git-init` | Skip `git init` |
| `--disable-gitignore` | Skip `.gitignore` generation |

**`watch`-only flag:** `--no-build-on-start` — skip the initial build; wait for the first change.

`ezmk project run` passes everything after `--` to the built program.

---

## `pkg` — manage packages

| Command | Description |
|---|---|
| `ezmk pkg install [scope] [pkg-opts] <file\|url\|name>` | Install a package |
| `ezmk pkg remove [scope] <name>` | Remove a package |
| `ezmk pkg search [scope] <name>` | Search registered repos |
| `ezmk pkg info [scope] <name>` | Show package details |
| `ezmk pkg list [scope]` | List installed packages (0.2.3+) |
| `ezmk pkg update [scope] <name>` | Update a package from repos (0.2.3+) |
| `ezmk pkg update [scope] --all` | Update all installed packages (0.2.4+) |

**`install`-only options:**

| Flag | Purpose |
|---|---|
| `--sha256 <hash>` | Verify archive integrity before installing |
| `-y` / `--yes` | Skip confirmation prompts (non-interactive) |

See [`pkg.md`](pkg.md) for the package format and dependency resolution.

---

## `repo` — manage repositories

| Command | Description |
|---|---|
| `ezmk repo add [scope] <git_url\|path> [--name <n>] [--branch <b>]` | Register and clone |
| `ezmk repo remove [scope] <name>` | Unregister and delete cache |
| `ezmk repo update [scope] [<name>]` | `git pull` to refresh (all if `<name>` omitted) |
| `ezmk repo list [scope]` | List registered repos |
| `ezmk repo info [scope] <name>` | Show repo details (packages, versions) |

Local directories are supported via `type = "local"`. See [`repo.md`](repo.md).

**Official default repository:** `install.sh` automatically pre-registers the official
repo (user scope, `--name official`) so `ezmk pkg install` works by name out of the box.
Set `EZMK_NO_DEFAULT_REPO=1` to skip this during install.

| URL | Target |
|-----|--------|
| `https://github.com/3667808244/ezmk-repo.git` | GitHub (global) |
| `https://gitee.com/egglzh/ezmk-repo.git` | Gitee mirror (China) |

Manual registration (if skipped during install, or to add the mirror as a fallback):

```bash
ezmk repo add -u https://github.com/3667808244/ezmk-repo.git --name official
ezmk repo update -u official
```

The registration is user-scoped (`-u`) so it can be removed with `ezmk repo remove -u official`.

---

## `utils` — Lua-based tools (0.2.0+)

| Command | Description |
|---|---|
| `ezmk utils <name> [args...]` | Run a Lua tool from an installed `type = "utils"` package |

Everything after `<name>` is passed through to the tool. Built-in: `ezmk utils cc`
generates `compile_commands.json` (`-o <path>` for a custom location). See
[`utils.md`](utils.md) for the plugin API.

---

## `version` · `help`

| Command | Description |
|---|---|
| `ezmk version` / `-V` / `--version` / `v` | Print version |
| `ezmk help` / `-h` / `--help` / `h` | Print usage |

---

## Scope flags

| Flag | Scope | Install path |
|---|---|---|
| `-p` | Project | `<project>/.ezmk/pkg/` |
| `-u` | User | `~/.local/ezmk/pkg/` (Unix) · `%LOCALAPPDATA%\ezmk\pkg\` (Windows) |
| `-g` | Global | `<ezmk_install_dir>/pkg/` |

`pkg install` and `repo add` accept **only one** scope flag. Other commands accept
combined flags like `-pug` (equivalent to `-p -u -g`).

---

## Command shorthands (0.2.6+)

Aliases apply only at the command position (`argv[1]`); `ezmk project pn` is still an
unknown subcommand. Shorthands are typing sugar and are **not** part of zsh completion.

| Alias | Expands to | Alias | Expands to | Alias | Expands to |
|---|---|---|---|---|---|
| `pn` | `project new` | `ki` | `pkg install` | `ra` | `repo add` |
| `pb` | `project build` | `kr` | `pkg remove` | `rr` | `repo remove` |
| `pr` | `project run` | `ks` | `pkg search` | `rl` | `repo list` |
| `pc` | `project clean` | `kn` | `pkg info` | `ru` | `repo update` |
| `pw` | `project watch` | `kl` | `pkg list` | `ri` | `repo info` |
| `u` | `utils` | `ku` | `pkg update` | `h` / `v` | `help` / `version` |

---

## Option syntax (GNU conventions)

- **Long options:** `--flag=value` and `--flag value` are equivalent.
- **Short grouping:** `-pug` equals `-p -u -g`.
- **Attached values:** `-j4` equals `-j 4`.
- **Interleaving:** options and positional arguments can be freely mixed.
- **`--` terminator:** everything after `--` is a positional argument (pass-through
  for `utils` and `project run`).

---

## Global options

These may appear on any command and are consumed before per-command parsing.

### `--color=<mode>` (0.2.6+)

| Mode | Aliases | Behavior |
|---|---|---|
| `always` | `enable` | Force color (also enables VT100 on legacy Windows terminals) |
| `auto` | `default` | Color only on an interactive terminal (**default**) |
| `never` | `disable` | Disable color |

Values are case-insensitive. Both `--color=always` and `--color always` are accepted.
An explicit `always` / `never` overrides `NO_COLOR`; only `auto` honors it (matching
git/ls). Tokens after `--` are left untouched for pass-through.

---

## Environment variables

| Variable | Scope | Purpose |
|---|---|---|
| `EZMK_LANG` | runtime | UI language (`zh` / `en`), overrides system detection (`src/i18n.cpp`) |
| `NO_COLOR` | runtime | Disable colored output (honored only by `--color=auto`) (`src/util.cpp`) |
| `CXX` / `CC` | runtime + build | Override compiler detection (0.1.8+) |
| `CXXFLAGS` | build | Extra compiler flags, passed through by `build.sh` |
| `EZMK_VERSION` | build | Version string baked into the binary (`build.sh`) |
| `PREFIX` | install | Install prefix; binary goes to `$PREFIX/bin` (default `$HOME/.local`) (`install.sh`) |
| `EZMK_REF` | install | git tag/branch/commit to build (`install.sh`) |
| `EZMK_NO_COMPLETIONS` | install | Set to `1` to skip zsh completion install (`install.sh`) |
| `EZMK_NO_DEFAULT_REPO` | install | Set to `1` to skip official repo pre-registration (`install.sh`) |

---

## Related documents

- [`config_file.md`](config_file.md) — full `ezmk.toml` spec
- [`pkg.md`](pkg.md) — package format and management
- [`repo.md`](repo.md) — repository system
- [`utils.md`](utils.md) — Lua plugin API
- [`@cache.md`](@cache.md) — build cache algorithm
- [`@safety.md`](@safety.md) — security model (confirmations, sha256, sandbox)
