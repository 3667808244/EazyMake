# EazyMake

中文 · [English](README.md)

**写 C++，但不需要写 CMake。**

简单的 C/C++ 构建工具 —— `ezmk`。支持 GCC、Clang 和 MSVC。

**设计理念：** 易用优先，功能从简。[复杂构建](docs/zh/complex-builds.md)请使用 CMake。

> **v1.1.0 起公共 API 永久稳定。** 破坏性变更仅在 `2.0.0` 引入，并提前至少一个次版本发出弃用警告。详见 [CHANGES.md](CHANGES.md#api-stability)。

## 为什么选择 EazyMake

- **零配置** —— `ezmk project new my_app && cd my_app && ezmk build` 就能跑
- **内置包管理器** —— `ezmk pkg install fmt` 自动下载、编译、链接
- **跨编译器** —— 同一份 `ezmk.toml` 在 GCC、Clang、MSVC 下都能用
- **可扩展** —— Lua 脚本自定义构建逻辑和工具

## 快速开始

### 安装

**macOS / Linux —— 推荐（Homebrew）：**

```bash
brew tap 3667808244/eazymake && brew install ezmk
```

> Homebrew 覆盖 macOS（Apple Silicon）与 Linux x64。macOS Intel 暂无预编译产物 —— 请用下方的安装脚本。

**备选 —— 安装脚本：**

**Linux / macOS / MSYS2：**

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

**Windows（原生，无需 MSYS2）：**

```powershell
irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex
```

可通过 `PREFIX`、`EZMK_REF`、`EZMK_NO_DEFAULT_REPO` 自定义。详见[安装选项](#安装选项)。

### 第一个项目

```bash
ezmk project new hello
cd hello
ezmk build                # 编译 + 链接
ezmk run                  # 构建 + 运行
```

> **从子目录也能构建（1.2.0+）：** `ezmk build` / `ezmk test` 等命令会从当前目录
> 向上查找最多 5 层父目录中的 `ezmk.toml` —— 进入 `src/` 等子目录直接运行即可，如同 `git`。

### 安装包

```bash
ezmk pkg install fmt      # 按名称安装 — 官方仓库已预注册
ezmk pkg install ./mylib  # 从源目录安装（1.2.0+，免打包）
```

## 与 CMake 对比

| EazyMake | CMake |
|----------|-------|
| `ezmk project new app && cd app && ezmk build` | `mkdir build && cd build && cmake .. && make` |
| `ezmk pkg install fmt` | `find_package(fmt)` + 手动安装 |
| 1 个 TOML 文件 | 1+ 个 `CMakeLists.txt` |
| 自动检测编译器 | `-DCMAKE_CXX_COMPILER=...` |

## 配置

```toml
[project]
name = "myapp"
type = "executable"     # executable | static | shared | utils
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]

[link]
system_target = ["pthread"]

[depends]
lib = ["fmt", "zlib"]           # 硬性依赖
want = ["sqlite3"]              # 可选依赖
```

完整参考：[`docs/zh/config_file.md`](docs/zh/config_file.md)

## 命令速览

```bash
# 日常
ezmk build [flags]              # 增量构建
ezmk run [flags] [-- args]      # 构建并运行
ezmk clean                      # 清除缓存
ezmk watch [flags]              # 监视文件自动重建
ezmk install [flags]            # 安装构建产物到指定前缀
ezmk test [flags]               # 运行项目测试
ezmk pack [flags]               # 创建可分发的 .tar.gz（完整形式：ezmk project pack）

# 项目
ezmk project new <name>         # 创建新项目
ezmk project cc [flags]         # 为 clangd 生成 compile_commands.json
ezmk project export cmake [flags]  # 从 ezmk.toml 生成 CMakeLists.txt
ezmk project import [flags]     # 导入 CMake 项目为 ezmk.toml（实验性）

# 包管理
ezmk pkg install <pkg>          # 安装包
ezmk pkg search <pkg>           # 搜索已注册仓库
ezmk pkg list                   # 列出已安装的包
ezmk pkg update [<pkg>]         # 更新到最新版本

# 仓库
ezmk repo add <url>             # 注册仓库
ezmk repo update                # 刷新仓库索引

# 工具
ezmk utils cc                   # 生成 compile_commands.json（自 1.2.0 起弃用 → ezmk project cc）
```

完整参考：[`docs/zh/cli.md`](docs/zh/cli.md)

## 安装选项

| 变量 / 参数 | 作用 | 默认值 |
|-------------|------|--------|
| `PREFIX` | 安装前缀（二进制 → `$PREFIX/bin`） | `$HOME/.local` |
| `EZMK_REF` | 要构建的 git tag/分支/提交 | 默认分支 |
| `EZMK_NO_DEFAULT_REPO` | 设为 `1` 跳过官方仓库预注册 | （注册） |
| `-Version`（PS） | 要安装的版本标签 | `"latest"` |
| `-InstallDir`（PS） | 安装根目录 | `$env:LOCALAPPDATA\ezmk` |
| `-DryRun`（PS） | 预览操作，不做实际更改 | （关闭） |

## 文档

| 文档 | 内容 |
|------|------|
| [教程](tutorial/zh/) | 手把手上手教程 |
| [CLI 参考](docs/zh/cli.md) | 命令行与环境变量完整参考 |
| [配置文件](docs/zh/config_file.md) | `ezmk.toml` 完整格式说明 |
| [包管理](docs/zh/pkg.md) | 包格式与生命周期 |
| [仓库系统](docs/zh/repo.md) | 基于 git 的仓库系统 |
| [Lua 插件](docs/zh/utils.md) | 插件系统与 API 参考 |
| [FAQ / 故障排除](docs/zh/faq.md) | 常见问题与修复 |
| [技术细节](docs/en/technical.md) | 依赖、源码构建、编译器支持 |
| [术语表](docs/zh/glossary.md) | 术语参考 |
| [不会设计的功能](docs/zh/non-goals.md) | 刻意不做、也不会做的功能 |
| [更新日志](CHANGES.md) | 版本历史 |

## 相关链接

- [从源码构建 & 测试](docs/en/technical.md#building-eazymake)
- [MSVC 支持](docs/en/technical.md#using-msvc)
- [zsh 补全](docs/en/technical.md#shell-completion-zsh)
- [官方仓库](https://github.com/3667808244/ezmk-repo) — 包仓库
