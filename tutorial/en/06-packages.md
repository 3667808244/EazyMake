# 6. Using packages

Packages are prebuilt-from-source static libraries (`.a`) that your project links
against. You get them from **repositories** (git repos with an `index.toml`) or install
them directly from a file/URL.

## Add a repository

```bash
$ ezmk repo add https://github.com/example/ezmk-repo.git --name example
$ ezmk repo update            # git pull the latest index
$ ezmk repo list
$ ezmk repo info example      # packages & versions available
```

You can also use a local directory as a repo (`type = "local"`).

## Install a package

```bash
$ ezmk pkg install fmt         # from a registered repo, by name
$ ezmk pkg install ./fmt.zip   # from a local archive
$ ezmk pkg install https://.../fmt.tar.gz --sha256 <hash>   # verify integrity
```

### Scope

Where a package installs is controlled by a scope flag:

| Flag | Scope | Path |
|---|---|---|
| `-p` | Project (default) | `.ezmk/pkg/` |
| `-u` | User | `~/.local/ezmk/pkg/` |
| `-g` | Global | `<install_dir>/pkg/` (asks for confirmation) |

```bash
$ ezmk pkg install -u fmt      # install for the current user
```

## Depend on it

Reference the package in `ezmk.toml`:

```toml
[depends]
lib  = ["fmt"]      # required
want = ["spdlog"]   # optional (used only if installed)
```

Now build — `ezmk` resolves the dependency chain, compiles each package to a static
library, and links it in:

```bash
$ ezmk project build
```

Use it in code:

```cpp
#include <fmt/core.h>
int main(){ fmt::print("Hello {}!\n", "packages"); }
```

## Manage installed packages

```bash
$ ezmk pkg list                # what's installed (add -p/-u/-g to filter scope)
$ ezmk pkg update fmt          # update one package from repos
$ ezmk pkg update --all        # update everything
$ ezmk pkg remove fmt          # uninstall
```

> Build with `--auto-update` to run `ezmk repo update --pug` first, so names resolve
> against the freshest index.

See [`docs/en/pkg.md`](../../docs/en/pkg.md) and [`docs/en/repo.md`](../../docs/en/repo.md) for details.

Next: [Watch mode & hooks →](07-watch-hooks.md)
