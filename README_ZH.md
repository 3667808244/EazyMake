# EazyMake

中文 · [English](README.md)

简单的 C/C++ 构建工具 —— `ezmk`。基于 GCC/g++（Windows 上使用 MSYS2，Linux/macOS 原生）。

**设计理念：** 易用优先，功能从简。复杂构建请使用 CMake。

## 依赖声明

| 依赖 | 版本 | 要求 | 说明 |
|---|---|---|---|
| GCC (g++/gcc) 或 Clang (clang++/clang) | ≥ 8.0 | **编译 & 运行时** | 需支持 C++17 |
| Lua | 5.4.7 | **嵌入式** | 静态链接进 `ezmk` |
| nlohmann/json | header-only | **嵌入式** | JSON 支持（`include/vendor/nlohmann_json.hpp`） |
| toml++ | header-only | **嵌入式** | TOML 解析（`include/vendor/toml.hpp`） |
| Catch2 | v3 (header-only) | **仅测试** | 单元测试框架 |
| miniz | v3.0.2 | **嵌入式** | ZIP 解压（`src/vendor/miniz/*`） |
| Python | ≥ 3.6 | **仅构建** | Locale 数据嵌入（`scripts/embed_locale.py`） |
| MSYS2 (Windows) | — | **编译 & 运行时** | 提供 g++ 和 bash 环境 |

## 快速开始

### 安装（Linux / macOS / MSYS2）

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

从源码构建并将 `ezmk` 安装到 `$HOME/.local/bin`。可用环境变量自定义：

```bash
# 系统级安装
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | PREFIX=/usr/local bash

# 先审阅再执行（推荐）
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh -o install.sh
less install.sh
bash install.sh
```

| 变量 | 作用 | 默认值 |
|---|---|---|
| `PREFIX` | 安装前缀（二进制 → `$PREFIX/bin`） | `$HOME/.local` |
| `EZMK_REF` | 要构建的 git tag/分支/提交 | 默认分支 |
| `EZMK_NO_COMPLETIONS` | 设为 `1` 跳过 zsh 补全安装 | （检测到 zsh 则安装） |
| `CXX` / `CC` / `CXXFLAGS` | 覆盖编译器（透传给 `build.sh`） | 自动探测 |

> **裸 Windows（非 MSYS2）：** 请从 [GitHub Release](https://github.com/3667808244/EazyMake/releases) 下载预编译的 `ezmk.exe`。

### 构建 EazyMake 本身

```bash
# 使用辅助脚本（自动生成 locale 数据 + 版本头文件 + 编译）
bash build.sh

# 或手动构建 — MSYS2 / Windows
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -lwinhttp -static

# Linux
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -static

# macOS
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk
```

### 创建并构建项目

```bash
ezmk project new hello          # 脚手架生成 hello/
cd hello
ezmk project build               # 编译 + 链接
ezmk project run                 # 构建 + 运行
```

### 安装包

```bash
# 从本地文件安装
ezmk pkg install -p ./foo-0.1.0.zip

# 从 URL 安装
ezmk pkg install -p https://example.com/packages/bar-1.2.0.tar.gz

# 按名称安装（需先注册仓库）
ezmk repo add -p git@github.com:user/ezmk-repo.git --name my-repo
ezmk repo update
ezmk pkg install -p foo
```

### 运行工具

```bash
# 生成 compile_commands.json（供 clangd/LSP 使用）
ezmk utils cc

# 自定义输出路径
ezmk utils cc -o build/compile_commands.json
```

## CLI 参考

### `project` — 构建项目

| 命令 | 说明 |
|---|---|
| `ezmk project new <name> [--type <type>] [--disable-git-init] [--disable-gitignore]` | 脚手架生成新项目 |
| `ezmk project build [--disable-cache] [--verbose] [-j <N>] [--profile <name>]` | 增量构建 |
| `ezmk project run [--disable-cache] [--verbose] [-j <N>] [--profile <name>]` | 构建并运行 |
| `ezmk project clean` | 清除 `.ezmk/cache/` 和临时文件 |
| `ezmk project watch [--profile <name>] [--no-build-on-start] [-j <N>]` | 监视文件并自动重新构建（0.2.3+） |

### `pkg` — 管理包

| 命令 | 说明 |
|---|---|
| `ezmk pkg install [-p\|-u\|-g] [--sha256 <hash>] [-y] <包文件或URL>` | 安装包（默认：`-p`） |
| `ezmk pkg remove [-p\|-u\|-g] <包名>` | 移除包（默认：`-pug`） |
| `ezmk pkg search [-p\|-u\|-g] <包名>` | 搜索包（默认：`-pug`） |
| `ezmk pkg info [-p\|-u\|-g] <包名>` | 查看包详情（默认：`-pug`） |
| `ezmk pkg list [-p\|-u\|-g]` | 列出已安装的包（0.2.3+） |
| `ezmk pkg update [-p\|-u\|-g] <包名>` | 从仓库更新已安装的包（0.2.3+） |

### `repo` — 管理仓库

| 命令 | 说明 |
|---|---|
| `ezmk repo add [-p\|-u\|-g] <git地址或路径> [--name <名称>] [--branch <分支>]` | 注册基于 git 的仓库（clone 到本地缓存） |
| `ezmk repo remove [-p\|-u\|-g] <名称>` | 注销仓库并删除缓存 |
| `ezmk repo update [-p\|-u\|-g] [<名称>]` | `git pull` 刷新仓库缓存（或重新读取本地目录） |
| `ezmk repo list [-p\|-u\|-g]` | 列出已注册仓库（含 URL、分支、最后更新时间） |

### `utils` — Lua 工具（0.2.0+）

| 命令 | 说明 |
|---|---|
| `ezmk utils <名称> [参数...]` | 运行已安装 utils 包中的 Lua 工具 |

Utils 工具是通过 `ezmk pkg install` 安装的包（`type = "utils"`），暴露 `utils/<name>.lua` 脚本。详见 `docs/utils.md`。

**内置工具：**

| 工具 | 说明 |
|---|---|
| `ezmk utils cc` | 生成 `compile_commands.json`（兼容 clangd） |
| `ezmk utils cc -o <路径>` | 输出到自定义路径 |

### 作用域参数

| 参数 | 作用域 | 安装路径 |
|---|---|---|
| `-p` | 项目 | `<项目>/.ezmk/pkg/` |
| `-u` | 用户 | `~/.local/ezmk/pkg/`（Linux/macOS）或 `%LOCALAPPDATA%/ezmk/pkg/`（Windows） |
| `-g` | 全局 | `<ezmk安装目录>/pkg/` |

`install` 和 `repo add` 仅接受单个作用域参数；其余命令支持组合使用，如 `-pug`。

## 项目结构

```
my_project/
  .ezmk/
    pkg/            # 已安装的包
    temp/           # 临时文件（自动清理）
    cache/          # 构建缓存（record.json + obj/）
    repo/           # 仓库注册表 + clone 缓存
      list.toml
      .cache/
  include/          # 项目头文件（*.h, *.hpp）
  src/              # 项目源文件（*.c, *.cpp, *.cxx）
  build/            # 构建输出目录
  ezmk.toml         # 项目配置文件
```

## 配置文件（`ezmk.toml`）

```toml
[project]
name = "myapp"
type = "executable"     # executable | static | shared | utils
version = "0.1.0"
language = "C++17"      # C++17 | C++20 | C11 | ...

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]
src_dirs = ["src"]                    # 0.2.2+ 多源目录扫描
ezmk_macros = true                    # 0.2.2+ 注入 EZMK_* 标准宏
msvc_flags = []                       # 0.2.1+ MSVC 专用标志

[compile.macros]                      # 0.2.2+ 语义化宏定义
DEBUG = ""
VERSION = "0.1.0"

[link]
flags = []
link_dirs = []
system_target = ["pthread"]
msvc_flags = []                       # 0.2.1+ MSVC 专用链接标志

[depends]
lib = ["foo", "bar"]                  # 硬性依赖（缺失 → 错误）
want = ["sqlite3", "zlib"]            # 0.2.2+ 可选依赖
```

### Utils 包配置

```toml
[project]
name = "ezmk-cc"
type = "utils"
version = "0.1.0"

[utils]
tools = ["cc"]
```

## 仓库

仓库是一个 **git 仓库**，包含 `index.toml` + `packages/` 目录。`ezmk repo add` 自动 clone 到本地缓存；`ezmk repo update` 执行 `git pull` 增量更新。也支持本地目录作为仓库源。详见 `docs/repo.md`。

## 设计文档

| 文档 | 内容 |
|---|---|
| `tutorial/` | 手把手上手教程（从零到用包） |
| `docs/cli.md` | 命令行与环境变量权威参考 |
| `docs/config_file.md` | `ezmk.toml` 完整格式说明 |
| `docs/pkg.md` | 包格式与生命周期 |
| `docs/repo.md` | 基于 git 的仓库系统 |
| `docs/utils.md` | Lua 插件系统与 API 参考 |
| `docs/@cache.md` | 增量构建缓存 |
| `docs/@safety.md` | 安全规范 |
| `CHANGES.md` | 版本更新日志 |
| `plans/` | 版本里程碑计划 |
| `plan.md` | 当前执行计划 |
