# 命令行参考

`ezmk` 命令行和环境变量的权威参考文档。本文档是唯一真相来源；README 中的命令表格是快速上手的子集。行为细节请参阅各专题文档（`pkg.md`、`repo.md`、`utils.md`、`config_file.md`、`cache.md`、`safety.md`）。

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

## 顶层别名（1.1.0+）

为方便使用，最常用的 `project` 子命令也可作为顶层命令使用：

| 别名 | 完整形式 |
|------|----------|
| `ezmk build` | `ezmk project build` |
| `ezmk run` | `ezmk project run` |
| `ezmk clean` | `ezmk project clean` |
| `ezmk watch` | `ezmk project watch` |
| `ezmk install` | `ezmk project install` |
| `ezmk test` | `ezmk project test` |
| `ezmk pack` | `ezmk project pack` |

两种形式完全等价——所有标志和参数行为一致。日常使用推荐短形式；完整 `project <action>` 形式保留用于脚本和习惯。

> **为什么保留两种形式？** 顶层别名在 1.1.0-pre.1 引入，目的是降低新用户的使用门槛——日常 `ezmk build` 比 `ezmk project build` 好记得多。完整形式保留是为了脚本与既有习惯不被破坏、语义无歧义。

---

## `project` — 构建你的代码

| 命令 | 描述 |
|---|---|
| `ezmk project new <name> [--type <t>]` | 创建新项目 |
| `ezmk build [build-opts]` | 增量构建（完整形式：`ezmk project build`） |
| `ezmk run [build-opts] [-- <program args>]` | 构建并运行（完整形式：`ezmk project run`） |
| `ezmk clean` | 清除缓存和临时文件（完整形式：`ezmk project clean`） |
| `ezmk install [install-opts]` | 安装构建产物到指定前缀，1.1.0+（完整形式：`ezmk project install`） |
| `ezmk pack [--output <dir>]` | 打包静态库项目为 `.tar.gz`，1.1.0+（完整形式：`ezmk project pack`） |
| `ezmk watch [build-opts] [--no-build-on-start]` | 监视源码并自动重新构建（完整形式：`ezmk project watch`） |
| `ezmk test [test-opts]` | 构建并运行项目测试，1.1.0+（完整形式：`ezmk project test`） |
| `ezmk project cc [-o <path>] [--profile <p>]` | 生成 `compile_commands.json`（clangd/LSP），1.2.0+ |
| `ezmk project export cmake [flags]` | 从 `ezmk.toml` 生成 `CMakeLists.txt`（单向快照），1.2.0+ |
| `ezmk project import [--from <fmt>] [--overwrite]` | 把 CMake 项目导入为 `ezmk.toml`（实验性，单向快照），1.2.0+ |

**`--type <t>`**（用于 `new`）：`executable`（默认）· `static` · `shared` · `utils`。

**`ezmk.toml` 向上查找（1.2.0-dev.7+）：** 所有需要项目配置的命令（`build` / `run` / `clean` / `install` / `pack` / `watch` / `test` / `project cc` / `project export` 等）会从当前目录**向上查找最多 5 层父目录**寻找 `ezmk.toml`——进入项目子目录后直接运行 `ezmk build` / `ezmk test` 即可，如同 `git` 的行为。找到的目录即项目根，产物、缓存与 `.ezmk/*`（cache / pkg / repo）都落在该根下；5 层内未找到时给出明确报错（`clean` 回退到当前目录）。`project new` / `project import` 这类创建命令不依赖已有配置。

**`project import`** 把当前目录的 `CMakeLists.txt` 转换为全新的 `ezmk.toml`
（单向快照——导入后以 `ezmk.toml` 为唯一事实源）。`--from` 默认 `cmake` 且大小写不敏感。
已存在 `ezmk.toml` 时默认拒绝，需显式 `--overwrite` 才覆盖；遇到不支持的非声明式写法
（自定义命令、生成器表达式、`function()`/`macro()`、`pkg_check_modules`）会**事务性中止**，
不产出半成品。**实验性**——导入后请手动校对库链接与平台宏。支持/不支持清单与手动迁移步骤
见 [migrate-from-cmake.md](migrate-from-cmake.md)。

**伴侣运行时：`ezmk-lua`（1.2.0-dev.8+）。** `ezmk project export cmake`
把 `[hooks]` 的 `pre_build` / `post_build` 映射为 `add_custom_command` 调用，
由独立 `ezmk-lua` 二进制执行（随 `ezmk` 进入所有安装渠道）：

```
ezmk-lua <hook.lua> [--project-root <目录>] [--profile <名称>] [--output <路径>]
```

它在**无沙箱**的 Lua 环境运行钩子（构建沙箱的严格超集），并从 CLI 参数构建
`ctx` 表（`output` / `project_root` / `profile`）。生成的 CMake 通过
`find_program(EZMK_LUA ezmk-lua)` 定位；未安装时回退为 `message(WARNING)`
（跳过钩子后处理，非致命）。`on_failure` 在 CMake 中无等价物，不导出。
完整映射见 [config_file.md](config_file.md) 的 `hooks` 节。

**`build-opts`**（`build` / `run` / `watch` 共用）：

| 标志 | 用途 |
|---|---|
| `--disable-cache` | 强制重新编译（之后仍会更新缓存） |
| `--verbose` / `-v` | 显示完整编译命令和缓存命中情况 |
| `-j <N>` / `--jobs <N>` | 并行编译任务数；`0` = 自动（`hardware_concurrency`），默认值 |
| `--profile <name>` | 应用 `[compile.profile.<name>]` / `[link.profile.<name>]` 中的构建配置 |
| `--auto-update` | 构建前运行 `ezmk repo update --pug`（默认关闭） |

> **为什么 `-j 0` 是默认值？** 自动并行（`hardware_concurrency`）无需任何配置就能获得不错的加速。注意 `--disable-cache` 之后仍会**更新**缓存——它只强制一次干净重编译，而不是让缓存永久失效，所以下一次构建依然很快。

> **构建耗时明细（1.2.0+）：** `ezmk build -v` 始终按耗时降序打印每个源文件的编译耗时明细。不带 `-v` 时，若构建总耗时超过 5 秒，则自动打印最慢的 10 个编译单元。无需配置、无新增标志——仅列出实际编译（非缓存命中）的文件，单线程路径只显示总耗时。

**`new` 专属标志：**

| 标志 | 用途 |
|---|---|
| `--disable-git-init` | 跳过 `git init` |
| `--disable-gitignore` | 跳过 `.gitignore` 生成 |

**生成的模板（1.2.0+）：** `project new` 生成的模板内建 `[compile.profile.debug]`（`-g -O0` / `/Zi /Od`）与 `[compile.profile.release]`（`-O2 -DNDEBUG` / `/O2 /DNDEBUG`）两个 profile，并设置 `default_profile = "debug"`——裸 `ezmk build` 开箱即可调试，`ezmk build --profile release` 切换到优化构建。基准 `[compile].flags` 仅为警告标志（`-Wall -Wextra`）。

**按类型生成的源码模板（1.2.1+）：** `project new` 按 `--type` 差异化生成源码：

| `--type` | 生成物 |
|---|---|
| `executable`（默认） | `src/main.cpp`（Hello world 入口，不变） |
| `static` / `shared` | `include/<name>.hpp`（公共 API 示例）+ `src/<name>.cpp`（实现），**不生成 `main.cpp`** |
| `utils` | 无 C++ 代码，仅 `utils/` 目录（放 Lua 脚本） |

库模板的**文件名保留原始项目名**（`my-lib` → `include/my-lib.hpp` + `src/my-lib.cpp`），C++ **namespace 将 `-` / `.` / 空格替换为 `_`**（`my-lib` → `namespace my_lib`）；头文件用 `#pragma once` 保护。生成的 `ezmk.toml` 末尾还附带**注释掉的 `[test]` 示例节**（`# [test]` / `# framework` / `# dirs` / `# default_profile` / `# include_dirs` / `# link_targets`）——取消注释并填写后即可 `ezmk test`；纯注释对解析零影响，字段与 `[test]` 配置完全一致（含 1.2.0-dev.12 新字段，刻意不展示已弃用的 `flags`）。

**`watch` 专属标志：** `--no-build-on-start` — 跳过初始构建，等待文件首次变更。

**`install` 专属标志：**

| 标志 | 用途 |
|---|---|
| `--prefix <path>` | 覆盖 `[install].prefix` |
| `--dry-run` | 仅显示将要安装的内容，不实际复制 |
| `--no-headers` | 跳过头文件安装 |
| `--no-data` | 跳过数据文件安装 |

**`pack` 专属标志：** `--output <dir>` — 输出目录（默认 `.`）。仅适用于 `type = "static"` 的项目。

**`test` 专属标志：**

| 标志 | 用途 |
|---|---|
| `--framework` / `-f <catch2\|ezmk>` | 临时覆盖 `test.framework` |
| `--filter <pattern>` | 过滤测试名称（Catch2: 测试名；ezmk: 文件名 glob） |
| `--profile <name>` | **1.2.0-dev.12+** 临时覆盖 `test.default_profile`（与 `ezmk build --profile` 对称） |
| `--verbose` / `-V` | 展示每个测试的详细输出（即使通过） |

`ezmk run`（及其完整形式 `ezmk project run`）将 `--` 之后的所有内容传递给构建后的程序。

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
| `--locked` | 仅按现有 `ezmk.lock` 安装，不一致则报错（1.1.0+） |
| `--no-lock` | 跳过 `ezmk.lock` 生成（1.1.0+） |

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

> **为什么预注册官方仓库？** 这样用户安装完 `ezmk` 后无需先配置任何仓库，就能直接 `ezmk pkg install <name>` 按名安装。`EZMK_NO_DEFAULT_REPO=1` 供离线或自建仓库的场景跳过。

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

`<name>` 之后的所有内容透传给工具。工具按 project → user → global 作用域查找。

### 官方工具（`ezmk-official-utils` 包，1.1.0+）

安装脚本自动预装 `ezmk-official-utils` 包（全局作用域），提供以下工具：

| 命令 | 描述 |
|---|---|
| `ezmk utils cc [--output <path>]` | 生成 `compile_commands.json`（clangd 兼容）——**自 1.2.0 起弃用**，请用 `ezmk project cc`（2.0.0 移除） |
| `ezmk utils link add <name> <path>` | 添加 `.ezmk/links.json` 链接 |
| `ezmk utils link remove <name>` | 删除链接 |
| `ezmk utils link list` | 列出所有链接 |
| `ezmk utils link show <name>` | 查看链接详情 |
| `ezmk utils gen-build-package [--output <dir>] [--name <name>]` | 生成自包含构建包 `.tar.gz` |

手动安装：`ezmk pkg install -g ezmk-official-utils -y`

### `.ezmk/links.json` 与 `@link:` 语法（1.1.0+）

项目根目录下的 `.ezmk/links.json` 定义跨目录链接映射（名称 → 相对路径），用于多项目共享源文件。在 `ezmk.toml` 中通过 `@link:<name>` 语法引用：

```toml
[compile]
src_dirs = ["src", "@link:shared/src"]
include_dirs = ["include", "@link:shared/include"]
```

支持链式解析（A→B→C，深度限制 10 层）和循环检测。链接值仅支持相对路径（保证项目可移植性）。

> **为什么只支持相对路径？** 链接可能指向项目根目录之外，绝对路径会把机器特定的位置写进配置，项目被共享或迁移时就不可移植。深度限制与循环检测用于拦截配置错误的链接链。

插件 API 参见 [`utils.md`](utils.md)。

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

> **为什么 `install`/`add` 只接受一个作用域？** 它们会把包/仓库写入某个具体位置（project / user / global），目标必须唯一明确。而 `list` / `info` / `search` 是只读查询，可以跨作用域聚合，所以允许 `-pug` 组合。

---

## 命令简写（0.2.6+）

简写仅在命令位置（`argv[1]`）生效；`ezmk project pn` 仍为未知子命令。简写仅为输入便利，**不属于** zsh 补全。

> **为什么只作用于命令位置且不属于补全？** 简写只是交互输入时的便利；限定在 `argv[1]` 可避免与子命令命名空间冲突。补全列表展示规范完整命令，保持可发现性，也不至于翻倍膨胀。

| 简写 | 展开为 | 简写 | 展开为 | 简写 | 展开为 |
|---|---|---|---|---|---|
| `pn` | `project new` | `ki` | `pkg install` | `ra` | `repo add` |
| `pb` | `project build` | `kr` | `pkg remove` | `rr` | `repo remove` |
| `pr` | `project run` | `ks` | `pkg search` | `rl` | `repo list` |
| `pc` | `project clean` | `kn` | `pkg info` | `ru` | `repo update` |
| `pi` | `project install` | `kl` | `pkg list` | `ri` | `repo info` |
| `pw` | `project watch` | `ku` | `pkg update` | | |
| `pp` | `project pack` | | | | |
| `pt` | `project test` | | | | |
| `u` | `utils` | | | `h` / `v` | `help` / `version` |

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

> **为什么只有 `auto` 遵守 `NO_COLOR`？** 显式指定 `--color=always|never` 比环境变量的意图更强，应优先（与 git/ls 行为一致）；`auto` 才是让环境变量生效的模式。

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
- [`cache.md`](cache.md) — 构建缓存算法
- [`safety.md`](safety.md) — 安全模型（确认机制、sha256、sandbox）
