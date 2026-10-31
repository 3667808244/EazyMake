# 全局级配置 `config.toml`

> 项目级配置见 [project_config.md](project_config.md)；用户级配置见 [user_config.md](user_config.md)。

全局级配置面向整台机器上的所有用户，优先级最低。它通常由管理员维护；普通用户可用用户级配置覆盖其中的 `[settings]` 键。

## 位置

| 平台 | 默认路径 |
|------|----------|
| Linux | `/etc/ezmk/config.toml` |
| Windows | `C:\.ezmk\config\config.toml` |
| 覆盖 | 环境变量 `EZMK_GLOBAL_CONFIG_DIR` 指定的目录下的 `config.toml` |

`--no-config` 会跳过全局级与用户级；`--config <file>` 只替换用户级，全局级仍然生效。

## 权限与安全

- 该文件对所有用户生效，应只允许管理员写入（Linux 建议 `root:root 0644`，目录 `0755`）。
- 不要把可执行路径指向用户可写目录（例如 `git_path` / `editor`），否则可能被替换为恶意程序。
- 加载阶段不会创建目录或写文件；未知键、类型错误、非法枚举会立即报错并中止命令。

## 合并与优先级

从低到高：

1. 内置默认值
2. **全局级** `config.toml`（本文件）
3. 用户级 `config.toml`
4. 项目级 `ezmk.toml`（仅部分键）
5. 命令行 / 环境变量

全局级的 `[settings]` 是整机默认值；用户级按键覆盖，未写的键继续使用全局值。

## 配置结构

全局级与用户级使用**相同的 schema**：`[base_dir]` + `[settings]`。完整的键、类型、默认值与项目级覆盖表见 [用户级配置](user_config.md#settings)，这里只列全局级特有的语义：

| 键 | 全局级语义 |
|----|------------|
| `installed_packages` | `pkg install -g` / `pkg list -g` 的安装根目录；默认在 `ezmk` 可执行文件旁的 `pkg/` |
| `repo_cache` | `repo add -g` 等全局仓库缓存；默认在可执行文件旁的 `repo/` |
| `examples` | `ezmk example` 的输出根目录；用户级可覆盖 |
| `logs` | 文件日志目录；用户级可覆盖 |
| `default_jobs` | 整机默认并行度；项目级 `[compile].jobs` 可覆盖 |
| `build_dir` | 整机默认构建目录；项目级 `[project].build_dir` 可覆盖。**必须是相对路径**（相对各项目根）；绝对路径加载即报错，避免所有项目共享对象目录 |
| `default_pkg_scope` | `pkg install` 未给 scope 时的默认作用域 |
| `auto_install_deps` | 缺依赖时是否自动安装；项目级 `[pkg].auto_install` 可覆盖 |

其余 `[settings]` 键（语言、颜色、日志、project new 默认、测试、watch、pack、scan、git、editor、auto_pack）与用户级完全相同。

## 示例

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

## 注意

- 路径相对 **config.toml 所在目录** 解析；支持 `~` 展开。
- 文件日志目录必须对运行用户可写，否则只警告一次并降级为终端输出。
- 全局级不参与项目级覆盖（`[project].output_name` 等只在 `ezmk.toml` 中）。
