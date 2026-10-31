# User-level config `config.toml`

> Project-level config (`ezmk.toml`) lives in [project_config.md](project_config.md); the global level is documented in [global_config.md](global_config.md).

The user-level file affects only the current user and is the middle layer of `global < user < project`. If it does not exist, behaviour is unchanged.

## Location

| Platform | Default |
|----------|---------|
| Linux | `$XDG_CONFIG_HOME/ezmk/config.toml`, or `~/.config/ezmk/config.toml` when `XDG_CONFIG_HOME` is unset |
| Windows | `%USERPROFILE%\.ezmk\config\config.toml` |
| Override | `config.toml` inside the directory named by `EZMK_USER_CONFIG_DIR` |

`ezmk --config <file>` replaces the user-level file for one invocation (the global file still applies); `--no-config` skips both levels.

## Merge order

Lowest to highest:

1. Built-in defaults
2. Global `config.toml`
3. User `config.toml` (this file)
4. Project `ezmk.toml` (only the keys listed below)
5. CLI / environment (`--config`, `--lang`, `--color`, `-j`, ...)

`[settings]` is merged key by key; `[base_dir]` keeps separate values per scope.

## `[base_dir]`

| Key | Type | Meaning |
|-----|------|---------|
| `installed_packages` | string | User-scope package install root (`pkg install -u` / `pkg list -u`) |
| `repo_cache` | string | User-scope repository cache root |
| `examples` | string | Output root for `ezmk example` when `-o` is omitted |
| `logs` | string | File-log directory; writes `<logs>/ezmk.log` |

Path rules:

- Relative paths resolve against the **directory containing config.toml**, not the current working directory.
- `~`, `~/` and `~\` are expanded.
- User `examples` / `logs` override the global level; `installed_packages` / `repo_cache` are per scope.

## `[settings]`

| Key | Type | Default | Meaning |
|-----|------|---------|---------|
| `natural_language` | string | `"auto"` | UI language; `auto` detects the system. Precedence: `--lang` > `EZMK_LANG` > this key > system |
| `color_mode` | string | `"auto"` | `auto` / `off` / `16colors` / `256colors` / `truecolor` |
| `color_theme` | string | `"default"` | Reserved; only `default` is accepted today |
| `log_level` | string | `"info"` | `off` / `error` / `warn` / `info` / `debug`; `fatal` is never filtered |
| `initial_commit` | bool / string | unset | `true` uses the default message; a string is the commit message; runs only after a successful `git init` |
| `auto_init_git` | bool | `true` | Whether `ezmk project new` runs `git init` |
| `gen_gitignore` | bool | `true` | Generate `.gitignore` |
| `gen_readme` | bool | `true` | Generate `README.md` |
| `gen_agent_md` | bool | `false` | Generate `AGENTS.md` |
| `default_project_type` | string | `"executable"` | Default `project new` type when `--type` is omitted |
| `default_language` | string | `"C++17"` | Default language standard; invalid values fail at load time |
| `default_project_version` | string | `"0.1.0"` | Default new-project version; must not be empty |
| `default_compile_flags` | string[] | `["-Wall", "-Wextra"]` | Default `[compile].flags` for new projects |
| `build_dir` | string | `"build"` | Default build directory; `[project].build_dir` overrides it. **Must be relative** (to the project root); an absolute path fails at load time to avoid sharing object directories between projects |
| `default_jobs` | int | `0` | Default parallelism; `0` = auto; max 1024 |
| `test_timeout` | int | `30` | Default test timeout in seconds; `0` = no timeout. The built-in ezmk framework defaults to 30s; Catch2 keeps **no timeout** unless configured explicitly (dev.6 R-03) |
| `test_report_dir` | string | `".ezmk/test-results"` | Default report directory (relative to the project root) |
| `watch_debounce_ms` | int | `300` | Watch debounce in ms; `0` falls back to 300; explicit values 50..5000 |
| `watch_ignore` | string[] | `[]` | Extra ignored prefixes (relative to the project root); `[watch].ignore` is **appended** after these |
| `pack_format` | string | `"tar.gz"` | `tar.gz` / `zip` / `tgz` |
| `pack_output_dir` | string | `"."` | Default pack output directory (relative to the process CWD) |
| `pack_name` | string | empty | Archive basename; empty = `<name>-<version>` |
| `pack_exclude` | string[] | `[]` | Extra excluded directory names / extensions; `[pack].exclude` is **appended** after these and built-in exclusions still apply |
| `default_pkg_scope` | string | `"project"` | Default `pkg install` scope: `project` / `user` / `global` |
| `auto_install_deps` | bool | `true` | Auto-install missing dependencies; `false` fails instead |
| `scan_skip_dirs` | string[] | `[]` | Extra directory names skipped by `workspace scan` |
| `git_path` | string | `"git"` | Git executable path |
| `editor` | string | empty | Editor executable; `EDITOR`/`VISUAL` take precedence, otherwise auto-detect. The whole string is treated as one executable path, so **arguments are not supported** (e.g. `"code -w"`); use an `EDITOR` wrapper script when arguments are needed |
| `auto_pack` | bool | `false` | `static` projects auto-pack after a successful `build`/`run` |

## Project-level overrides / appends

| User key | Project override / append |
|----------|------------------|
| `build_dir` | `[project].build_dir` |
| `default_jobs` | `[compile].jobs` (`0` = auto) |
| `test_timeout` | `[test].timeout` |
| `test_report_dir` | `[test].report_dir` |
| `watch_debounce_ms` / `watch_ignore` | `[watch].debounce_ms` / `[watch].ignore` |
| `pack_format` / `pack_output_dir` / `pack_name` / `pack_exclude` | `[pack].format` / `.output_dir` / `.name` / `.exclude` |
| `auto_install_deps` | `[pkg].auto_install` |
| `scan_skip_dirs` | `[workspace.options].scan_skip` |

Artifact naming (`[project].output_name`) is project-level only. Scalar keys **override**; `ignore` / `pack_exclude` are **appended** after the user/global values (a project cannot clear them).

## Example

~~~toml
[base_dir]
installed_packages = "~/ezmk/pkg"
repo_cache = "~/ezmk/repo"
examples = "~/ezmk/examples"
logs = "~/ezmk/logs"

[settings]
natural_language = "en"
color_mode = "auto"
log_level = "info"
default_jobs = 4
test_timeout = 60
watch_debounce_ms = 300
pack_format = "zip"
default_pkg_scope = "user"
auto_install_deps = true
git_path = "git"
~~~

## Notes

- Loading config creates no directories and writes no files; `logs` and friends are created only when used.
- Unknown keys, wrong types, invalid enums and out-of-range values fail immediately.
- A failed file-log write warns once and degrades to stderr-only; it never changes the command result.
