# 用户级配置 `config.toml`

> 项目级配置（`ezmk.toml`）见 [project_config.md](project_config.md)；全局级配置见 [global_config.md](global_config.md)。

用户级配置只影响当前用户，是「全局 < 用户 < 项目」三层中的中间层。未创建该文件时行为与之前完全一致。

## 位置

| 平台 | 默认路径 |
|------|----------|
| Linux | `$XDG_CONFIG_HOME/ezmk/config.toml`；未设置 `XDG_CONFIG_HOME` 时为 `~/.config/ezmk/config.toml` |
| Windows | `%USERPROFILE%\.ezmk\config\config.toml` |
| 覆盖 | 环境变量 `EZMK_USER_CONFIG_DIR` 指定的目录下的 `config.toml` |

可用 `ezmk --config <file>` 临时替换用户级文件（全局级仍生效）；`--no-config` 跳过两级配置。

## 合并与优先级

从低到高：

1. 内置默认值
2. 全局级 `config.toml`
3. 用户级 `config.toml`（本文件）
4. 项目级 `ezmk.toml`（仅部分键可覆盖，见下）
5. 命令行 / 环境变量（`--config`、`--lang`、`--color`、`-j` 等）

`[settings]` 逐键覆盖；`[base_dir]` 按作用域分别保存，不会互相覆盖。

## `[base_dir]`

| 键 | 类型 | 说明 |
|----|------|------|
| `installed_packages` | string | 用户级包的安装目录；影响 `pkg install -u` / `pkg list -u` |
| `repo_cache` | string | 用户级仓库缓存目录；影响 `repo add -u` 等 |
| `examples` | string | `ezmk example` 省略 `-o` 时的输出根目录 |
| `logs` | string | 文件日志目录；启用后写 `<logs>/ezmk.log` |

路径规则：

- 相对路径相对于 **config.toml 所在目录** 解析（不是当前工作目录）。
- 支持 `~`、`~/`、`~\` 展开。
- 用户级 `examples` / `logs` 优先于全局级；`installed_packages` / `repo_cache` 按 scope 各取所属层。

## `[settings]`

| 键 | 类型 | 默认值 | 说明 |
|----|------|--------|------|
| `natural_language` | string | `"auto"` | 界面语言；`auto` 表示按系统检测。优先级：`--lang` > `EZMK_LANG` > 本键 > 系统 |
| `color_mode` | string | `"auto"` | `auto` / `off` / `16colors` / `256colors` / `truecolor` |
| `color_theme` | string | `"default"` | 预留主题名；当前仅接受 `default` |
| `log_level` | string | `"info"` | `off` / `error` / `warn` / `info` / `debug`；`fatal` 始终输出 |
| `initial_commit` | bool / string | 未设置 | `true` 用默认消息；字符串作为提交消息；仅在 `git init` 成功后执行 |
| `auto_init_git` | bool | `true` | `ezmk project new` 是否执行 `git init` |
| `gen_gitignore` | bool | `true` | 是否生成 `.gitignore` |
| `gen_readme` | bool | `true` | 是否生成 `README.md` |
| `gen_agent_md` | bool | `false` | 是否生成 `AGENTS.md` |
| `default_project_type` | string | `"executable"` | `project new` 未传 `--type` 时的类型 |
| `default_language` | string | `"C++17"` | 新项目默认语言标准；非法值加载即报错 |
| `default_project_version` | string | `"0.1.0"` | 新项目默认版本；不得为空 |
| `default_compile_flags` | string[] | `["-Wall", "-Wextra"]` | 新项目默认 `[compile].flags` |
| `build_dir` | string | `"build"` | 默认构建目录；项目可用 `[project].build_dir` 覆盖。**必须是相对路径**（相对项目根）；绝对路径加载即报错，避免多个项目共享对象目录 |
| `default_jobs` | int | `0` | 默认并行度；`0` = 自动；上限 1024 |
| `test_timeout` | int | `30` | 测试默认超时秒数；`0` = 不超时。内置 ezmk 框架默认 30s；Catch2 在未显式配置时**不超时**（dev.6 R-03） |
| `test_report_dir` | string | `".ezmk/test-results"` | 默认测试报告目录（相对项目根） |
| `watch_debounce_ms` | int | `300` | watch 去抖毫秒；`0` 表示回退默认 300，显式值 50..5000 |
| `watch_ignore` | string[] | `[]` | 额外忽略前缀（相对项目根）；项目级 `[watch].ignore` **追加**在本值之后 |
| `pack_format` | string | `"tar.gz"` | `tar.gz` / `zip` / `tgz` |
| `pack_output_dir` | string | `"."` | 默认打包输出目录（相对进程 CWD） |
| `pack_name` | string | 空 | 归档 basename；空 = `<name>-<version>` |
| `pack_exclude` | string[] | `[]` | 额外排除的目录名 / 扩展名；项目级 `[pack].exclude` **追加**在本值之后，内置排除仍生效 |
| `default_pkg_scope` | string | `"project"` | `pkg install` 未给 scope 时的默认：`project` / `user` / `global` |
| `auto_install_deps` | bool | `true` | 缺依赖时是否自动安装；`false` 直接报错 |
| `scan_skip_dirs` | string[] | `[]` | `workspace scan` 额外跳过的目录名 |
| `git_path` | string | `"git"` | git 可执行文件路径 |
| `editor` | string | 空 | 编辑器可执行文件；`EDITOR`/`VISUAL` 优先，空则自动探测。整串按一个可执行路径处理，**不支持带参数**（如 `"code -w"`），需要参数请用 `EDITOR` 包装脚本 |
| `auto_pack` | bool | `false` | `static` 项目在 `build`/`run` 成功后自动打源码包 |

## 项目级可覆盖 / 追加的键

| 用户级键 | 项目级覆盖 / 追加 |
|----------|------------|
| `build_dir` | `[project].build_dir` |
| `default_jobs` | `[compile].jobs`（`0` 表示自动） |
| `test_timeout` | `[test].timeout` |
| `test_report_dir` | `[test].report_dir` |
| `watch_debounce_ms` / `watch_ignore` | `[watch].debounce_ms` / `[watch].ignore` |
| `pack_format` / `pack_output_dir` / `pack_name` / `pack_exclude` | `[pack].format` / `.output_dir` / `.name` / `.exclude` |
| `auto_install_deps` | `[pkg].auto_install` |
| `scan_skip_dirs` | `[workspace.options].scan_skip` |

产物命名 `[project].output_name` 只属于项目级。标量键是**覆盖**；`ignore` / `pack_exclude` 是**追加**（项目在用户/全局值之后追加，不能清空）。

## 示例

~~~toml
[base_dir]
installed_packages = "~/ezmk/pkg"
repo_cache = "~/ezmk/repo"
examples = "~/ezmk/examples"
logs = "~/ezmk/logs"

[settings]
natural_language = "zh"
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

## 注意

- 加载配置不会创建目录、不会写文件；只有实际使用 `logs` 等功能时才创建。
- 未知键、类型错误、非法枚举、越界值都会立即报错。
- 文件日志写入失败只警告一次并降级为仅终端输出，不影响命令结果。
