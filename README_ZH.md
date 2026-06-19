# EazyMake

简单的 C/C++ 构建工具 —— `ezmk`。基于 GCC/g++（Windows 上使用 MSYS2，Linux 原生）。

**设计理念：** 易用优先，功能从简。复杂构建请使用 CMake。

## 快速开始

### 构建 EazyMake 本身

```bash
# 使用辅助脚本
bash build.sh

# 或手动构建 — MSYS2 / Windows
g++ -std=c++17 src/*.cpp src/vendor/*.c -I include/ -I include/vendor/ -o ezmk -lwinhttp -static

# Linux
g++ -std=c++17 src/*.cpp src/vendor/*.c -I include/ -I include/vendor/ -o ezmk -static
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

## CLI 参考

### `project` — 构建项目

| 命令 | 说明 |
|---|---|
| `ezmk project new <name> [--type executable\|static\|shared]` | 脚手架生成新项目 |
| `ezmk project build [--disable-cache]` | 增量构建 |
| `ezmk project run [--disable-cache]` | 构建并运行 |
| `ezmk project clean` | 清除缓存和临时文件 |

### `pkg` — 管理包

| 命令 | 说明 |
|---|---|
| `ezmk pkg install [-p\|-u\|-g] <文件或URL或名称>` | 安装包 |
| `ezmk pkg remove [-p\|-u\|-g] <名称>` | 移除包 |
| `ezmk pkg search [-p\|-u\|-g] <名称>` | 搜索包 |
| `ezmk pkg info [-p\|-u\|-g] <名称>` | 查看包详情 |

### `repo` — 管理仓库

| 命令 | 说明 |
|---|---|
| `ezmk repo add [-p\|-u\|-g] <git地址或路径> [--name <名称>] [--branch <分支>]` | 注册并 clone |
| `ezmk repo remove [-p\|-u\|-g] <名称>` | 注销并删除缓存 |
| `ezmk repo update [-p\|-u\|-g] [<名称>]` | `git pull` 刷新 |
| `ezmk repo list [-p\|-u\|-g]` | 列出已注册仓库 |

### 作用域参数

| 参数 | 作用域 | 安装路径 |
|---|---|---|
| `-p` | 项目 | `<项目>/.ezmk/pkg/` |
| `-u` | 用户 | `~/.local/ezmk/pkg/` |
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
type = "executable"     # executable | static | shared
version = "0.1.0"
language = "C++17"      # C++17 | C++20 | C11 | ...

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]

[link]
flags = []
link_dirs = []
system_target = ["pthread"]

[depends]
lib = ["foo", "bar"]
```

## 仓库

仓库是一个 **git 仓库**，包含 `index.toml` + `packages/` 目录。`ezmk repo add` 自动 clone 到本地缓存；`ezmk repo update` 执行 `git pull` 增量更新。也支持本地目录作为仓库源。详见 `docs/repo.md`。

## 设计文档

| 文档 | 内容 |
|---|---|
| `docs/config_file.md` | `ezmk.toml` 完整格式说明 |
| `docs/pkg.md` | 包格式与生命周期 |
| `docs/repo.md` | 基于 git 的仓库系统 |
| `docs/@cache.md` | 增量构建缓存 |
| `docs/@safety.md` | 安全规范 |
| `plan.md` | 当前版本计划与进度 |
