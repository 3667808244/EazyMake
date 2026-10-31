# Global-level config `config.toml`

> Project-level config lives in [project_config.md](project_config.md); the user level is documented in [user_config.md](user_config.md).

The global file applies to every user on the machine and has the lowest precedence. It is normally maintained by an administrator; users can override its `[settings]` keys in their user-level file.

## Location

| Platform | Default |
|----------|---------|
| Linux | `/etc/ezmk/config.toml` |
| Windows | `C:\.ezmk\config\config.toml` |
| Override | `config.toml` inside the directory named by `EZMK_GLOBAL_CONFIG_DIR` |

`--no-config` skips both the global and user levels; `--config <file>` replaces only the user level, so the global file still applies.

## Permissions and safety

- The file affects all users; only administrators should be able to write it (on Linux prefer `root:root 0644` for the file and `0755` for the directory).
- Do not point executable settings (`git_path` / `editor`) at user-writable directories; they could be replaced by a malicious binary.
- Loading creates no directories and writes no files; unknown keys, wrong types and invalid enums fail immediately.

## Merge order

Lowest to highest:

1. Built-in defaults
2. **Global** `config.toml` (this file)
3. User `config.toml`
4. Project `ezmk.toml` (selected keys only)
5. CLI / environment

The global `[settings]` provides machine-wide defaults; the user level overrides key by key and inherits everything it does not set.

## Structure

The global and user levels share the **same schema**: `[base_dir]` + `[settings]`. The complete key/type/default table and project override list live in [user_config.md](user_config.md#settings); only the global-specific semantics are repeated here:

| Key | Global meaning |
|-----|----------------|
| `installed_packages` | Install root for `pkg install -g` / `pkg list -g`; defaults to `pkg/` next to the `ezmk` executable |
| `repo_cache` | Global repository cache for `repo add -g` etc.; defaults to `repo/` next to the executable |
| `examples` | Output root for `ezmk example`; overridable at the user level |
| `logs` | File-log directory; overridable at the user level |
| `default_jobs` | Machine-wide parallelism; `[compile].jobs` overrides it per project |
| `build_dir` | Machine-wide default build dir; `[project].build_dir` overrides it. **Must be relative** (to each project root); an absolute path fails at load time to avoid sharing object directories across projects |
| `default_pkg_scope` | Default scope for `pkg install` when no scope flag is given |
| `auto_install_deps` | Auto-install missing dependencies; `[pkg].auto_install` overrides it |

All other `[settings]` keys (language, colour, logging, project-new defaults, test, watch, pack, scan, git, editor, auto_pack) are identical to the user level.

## Example

~~~toml
# /etc/ezmk/config.toml
[base_dir]
installed_packages = "/opt/ezmk/pkg"
repo_cache = "/var/cache/ezmk/repo"
examples = "/opt/ezmk/examples"
logs = "/var/log/ezmk"

[settings]
natural_language = "auto"
color_mode = "auto"
log_level = "warn"
default_jobs = 8
test_timeout = 60
default_pkg_scope = "global"
auto_install_deps = true
git_path = "git"
~~~

## Notes

- Relative paths resolve against the **directory containing config.toml**; `~` is expanded.
- The log directory must be writable by the running user; otherwise a one-time warning is printed and logging degrades to stderr.
- Project-level overrides (`[project].output_name` and friends) do not apply to the global level.
