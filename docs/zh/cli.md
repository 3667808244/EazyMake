# 命令行参考

`ezmk` 命令行和环境变量的权威参考文档。本文档是唯一真相来源；README 中的命令表格是快速上手的子集。行为细节请参阅各专题文档（`pkg.md`、`repo.md`、`utils.md`、`config_file.md`、`@cache.md`、`@safety.md`）。

## 概要

```
ezmk <command> [subcommand] [options] [arguments]
ezmk <shorthand> [options] [arguments]
```

全局选项可出现在任何命令中（参见[全局选项](#全局选项)）。

---

## 安装

### Linux / macOS / MSYS2

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

从源码构建并安装 `ezmk` 到 `$HOME/.local/bin`。自定义选项和环境变量参见 [README](../../README.md#quick-start)。

### Windows（原生，无需 MSYS2）

```powershell
# 下载并运行 PowerShell 安装脚本：
.\install.ps1

# 或一行远程执行：
irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex
```

从 GitHub Releases 下载预编译的 `ezmk.exe`，校验 SHA-256，安装到 `%LOCALAPPDATA%\ezmk\bin`，并配置用户 PATH。支持 `-Version`、`-InstallDir`、`-NoPath`、`-DryRun` 参数。详见 [README](../../README.md#quick-start)。

---

## `project` — 构建你的代码

| 命令 | 描述 |
|---|---|
| `ezmk project new <name> [--type <t>]` | 创建新项目 |
| `ezmk project build [build-opts]` | 增量构建 |
| `ezmk project run [build-opts] [-- <program args>]` | 构建并运行 |
| `ezmk project clean` | 清除缓存和临时文件 |
| `ezmk project watch [build-opts] [--no-build-on-start]` | 监视源码并自动重新构建 |

**`--type <t>`**（用于 `new`）：`executable`（默认）· `static` · `shared` · `utils`。

**`build-opts`**（`build` / `run` / `watch` 共用）：

| 标志 | 用途 |
|---|---|
| `--disable-cache` | 强制重新编译（之后仍会更新缓存） |
| `--verbose` / `-v` | 显示完整编译命令和缓存命中情况 |
| `-j <N>` / `--jobs <N>` | 并行编译任务数；`0` = 自动（`hardware_concurrency`），默认值 |
| `--profile <name>` | 应用 `[compile.profile.<name>]` / `[link.profile.<name>]` 中的构建配置 |
| `--auto-update` | 构建前运行 `ezmk repo update --pug`（默认关闭） |

**`new` 专属标志：**

| 标志 | 用途 |
|---|---|
| `--disable-git-init` | 跳过 `git init` |
| `--disable-gitignore` | 跳过 `.gitignore` 生成 |

**`watch` 专属标志：** `--no-build-on-start` — 跳过初始构建，等待文件首次变更。

`ezmk project run` 将 `--` 之后的所有内容传递给构建后的程序。

---

## `pkg` — 管理包

| 命令 | 描述 |
|---|---|
| `ezmk pkg install [scope] [pkg-opts] <file\|url\|name>` | 安装包 |
| `ezmk pkg remove [scope] <name>` | 移除包 |
| `ezmk pkg search [scope] <name>` | 在已注册仓库中搜索 |
| `ezmk pkg info [scope] <name>` | 显示包详情 |
| `ezmk pkg list [scope]` | 列出已安装的包（0.2.3+） |
| `ezmk pkg update [scope] <name>` | 从仓库更新包（0.2.3+） |
| `ezmk pkg update [scope] --all` | 更新所有已安装的包（0.2.4+） |

**`install` 专属选项：**

| 标志 | 用途 |
|---|---|
| `--sha256 <hash>` | 安装前校验归档文件完整性 |
| `-y` / `--yes` | 跳过确认提示（非交互模式） |

包格式和依赖解析参见 [`pkg.md`](pkg.md)。

---

## `repo` — 管理仓库

| 命令 | 描述 |
|---|---|
| `ezmk repo add [scope] <git_url\|path> [--name <n>] [--branch <b>]` | 注册并克隆仓库 |
| `ezmk repo remove [scope] <name>` | 取消注册并删除缓存 |
| `ezmk repo update [scope] [<name>]` | `git pull` 刷新（省略 `<name>` 则刷新全部） |
| `ezmk repo list [scope]` | 列出已注册仓库 |
| `ezmk repo info [scope] <name>` | 显示仓库详情（包列表、版本） |

支持本地目录（`type = "local"`）。参见 [`repo.md`](repo.md)。

**官方默认仓库：** `install.sh` 会自动预注册官方仓库（用户作用域，`--name official`），使 `ezmk pkg install` 可直接按包名安装。设置 `EZMK_NO_DEFAULT_REPO=1` 可在安装时跳过此步骤。

| URL | 目标 |
|-----|------|
| `https://github.com/3667808244/ezmk-repo.git` | GitHub（全球） |
| `https://gitee.com/egglzh/ezmk-repo.git` | Gitee 镜像（国内） |

手动注册（如果在安装时跳过，或需添加镜像作为备用）：

```bash
ezmk repo add -u https://github.com/3667808244/ezmk-repo.git --name official
ezmk repo update -u official
```

注册为用户作用域（`-u`），因此可通过 `ezmk repo remove -u official` 移除。

---

## `utils` — 基于 Lua 的工具（0.2.0+）

| 命令 | 描述 |
|---|---|
| `ezmk utils <name> [args...]` | 运行已安装 `type = "utils"` 包中的 Lua 工具 |

`<name>` 之后的所有内容透传给工具。内置工具：`ezmk utils cc` 生成 `compile_commands.json`（使用 `-o <path>` 指定自定义位置）。插件 API 参见 [`utils.md`](utils.md)。

---

## `version` · `help`

| 命令 | 描述 |
|---|---|
| `ezmk version` / `-V` / `--version` / `v` | 显示版本信息 |
| `ezmk help` / `-h` / `--help` / `h` | 显示使用帮助 |

---

## 作用域标志

| 标志 | 作用域 | 安装路径 |
|---|---|---|
| `-p` | 项目 | `<project>/.ezmk/pkg/` |
| `-u` | 用户 | `~/.local/ezmk/pkg/`（Unix）· `%LOCALAPPDATA%\ezmk\pkg\`（Windows） |
| `-g` | 全局 | `<ezmk_install_dir>/pkg/` |

`pkg install` 和 `repo add` 只接受**一个**作用域标志。其他命令接受组合标志，如 `-pug`（等价于 `-p -u -g`）。

---

## 命令简写（0.2.6+）

简写仅在命令位置（`argv[1]`）生效；`ezmk project pn` 仍为未知子命令。简写仅为输入便利，**不属于** zsh 补全。

| 简写 | 展开为 | 简写 | 展开为 | 简写 | 展开为 |
|---|---|---|---|---|---|
| `pn` | `project new` | `ki` | `pkg install` | `ra` | `repo add` |
| `pb` | `project build` | `kr` | `pkg remove` | `rr` | `repo remove` |
| `pr` | `project run` | `ks` | `pkg search` | `rl` | `repo list` |
| `pc` | `project clean` | `kn` | `pkg info` | `ru` | `repo update` |
| `pw` | `project watch` | `kl` | `pkg list` | `ri` | `repo info` |
| `u` | `utils` | `ku` | `pkg update` | `h` / `v` | `help` / `version` |

---

## 选项语法（GNU 约定）

- **长选项：** `--flag=value` 和 `--flag value` 等价。
- **短选项合并：** `-pug` 等价于 `-p -u -g`。
- **附带值：** `-j4` 等价于 `-j 4`。
- **交错排列：** 选项和位置参数可自由混合。
- **`--` 终止符：** `--` 之后的所有内容均为位置参数（透传给 `utils` 和 `project run`）。

---

## 全局选项

以下选项可出现在任何命令中，并在各命令解析之前处理。

### `--color=<mode>`（0.2.6+）

| 模式 | 别名 | 行为 |
|---|---|---|
| `always` | `enable` | 强制彩色输出（同时启用在旧版 Windows 终端上的 VT100 支持） |
| `auto` | `default` | 仅在交互终端输出彩色（**默认**） |
| `never` | `disable` | 禁用彩色输出 |

选项值不区分大小写。`--color=always` 和 `--color always` 均接受。显式指定 `always` / `never` 会覆盖 `NO_COLOR`；仅 `auto` 遵守 `NO_COLOR`（行为与 git/ls 对齐）。`--` 之后的 token 保持原样以用于透传。

---

## 环境变量

| 变量 | 作用范围 | 用途 |
|---|---|---|
| `EZMK_LANG` | 运行时 | 界面语言（`zh` / `en`），覆盖系统检测（`src/i18n.cpp`） |
| `NO_COLOR` | 运行时 | 禁用彩色输出（仅 `--color=auto` 时遵守）（`src/util.cpp`） |
| `CXX` / `CC` | 运行时 + 构建 | 覆盖编译器检测（0.1.8+） |
| `CXXFLAGS` | 构建 | 额外编译器标志，由 `build.sh` 透传 |
| `EZMK_VERSION` | 构建 | 编译进二进制的版本字符串（`build.sh`） |
| `PREFIX` | 安装 | 安装前缀；二进制安装至 `$PREFIX/bin`（默认 `$HOME/.local`）（`install.sh`） |
| `EZMK_REF` | 安装 | 要构建的 git tag/分支/提交（`install.sh`） |
| `EZMK_NO_COMPLETIONS` | 安装 | 设为 `1` 跳过 zsh 补全安装（`install.sh`） |
| `EZMK_NO_DEFAULT_REPO` | 安装 | 设为 `1` 跳过官方仓库预注册（`install.sh`） |
| `EZMK_TEST_BIN` | 测试 | 集成测试使用的 `ezmk` 二进制路径（默认 `build/ezmk[.exe]`） |

---

## 相关文档

- [`config_file.md`](config_file.md) — 完整的 `ezmk.toml` 规范
- [`pkg.md`](pkg.md) — 包格式与管理
- [`repo.md`](repo.md) — 仓库系统
- [`utils.md`](utils.md) — Lua 插件 API
- [`@cache.md`](@cache.md) — 构建缓存算法
- [`@safety.md`](@safety.md) — 安全模型（确认机制、sha256、sandbox）
