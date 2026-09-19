# 技术细节

本文档涵盖技术内部细节——依赖、从源码构建、编译器支持矩阵和项目结构。面向用户的文档参见 [README](../../README_ZH.md)。

## 依赖

除编译器与 MSYS2 外，以下依赖均内嵌，无需额外安装。

| 依赖                                    | 版本             | 是否必须             | 说明                                                        |
| --------------------------------------- | ---------------- | -------------------- | ----------------------------------------------------------- |
| GCC（g++/gcc）或 Clang（clang++/clang） | ≥ 8.0            | **构建与运行时**     | 需要 C++17 支持                                             |
| MSVC（Visual Studio）                   | ≥ 2019           | **可选**             | `cl.exe` + `link.exe`；通过 `vcvars64.bat` 自动检测          |
| Lua                                     | 5.4.7            | **内嵌**             | 静态链接进 `ezmk`                                           |
| nlohmann/json                           | 仅头文件         | **内嵌**             | JSON 支持（`include/vendor/nlohmann_json.hpp`）              |
| toml++                                  | 仅头文件         | **内嵌**             | TOML 解析（`include/vendor/toml.hpp`）                      |
| Catch2                                  | v3               | **仅测试**           | 单元测试框架                                                |
| miniz                                   | 内嵌（vendor 内版本串 2.2.0 / `MZ_VERSION` 10.2.0） | **内嵌**             | ZIP 解压（`src/vendor/miniz*.c` + `include/vendor/miniz*.h`） |
| Python                                  | ≥ 3.6            | **仅构建**           | locale 数据嵌入（`scripts/embed_locale.py`）                |
| MSYS2（Windows）                        | —                | **构建与运行时**     | 提供 g++ 与 bash 环境                                       |

> **为什么把所有依赖都内嵌？** 这样 `ezmk` 就是一个自包含的二进制，运行时只需一个编译器。
> 第三方代码以源码形式放在 `src/vendor/` 与 `include/vendor/` 并保持原样不动，构建可离线复现、
> 版本锁定，升级也更干净。

## 构建 EazyMake

```bash
# 通过辅助脚本（生成 locale 数据 + 版本头 + 编译）
bash build.sh

# 或手动编译 — MSYS2 / Windows
# `src/*.cpp` 也会匹配两个入口（`main.cpp` + `ezmk_lua_main.cpp`），
# 同时链接两个 `main()` 定义必然失败——需排除 Lua 那个。
g++ -std=c++17 $(ls src/*.cpp | grep -v ezmk_lua_main.cpp) src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -lwinhttp -static

# Linux
g++ -std=c++17 $(ls src/*.cpp | grep -v ezmk_lua_main.cpp) src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk -static

# macOS
g++ -std=c++17 $(ls src/*.cpp | grep -v ezmk_lua_main.cpp) src/vendor/*.c src/vendor/lua/*.c \
  -I include/ -I include/vendor/ -I include/vendor/lua/ \
  -DLUA_COMPAT_5_3 -o build/ezmk
```

> **先跑生成脚本。** 这些命令假设生成文件已存在；新克隆里没有其中三个（被 `.gitignore` 忽略）。
> 用脚本生成它们：`scripts/embed_locale.py` → `src/locale_data.cpp`、
> `scripts/embed_examples.py` → `src/example_data.cpp`、`scripts/embed_logo.py` →
> `include/ezmk/logo.gen.h`，以及 `build.sh` → `include/ezmk/version.hpp`。

> **为什么用 bash 脚本来构建？** 所有一等构建环境——Linux、macOS、MSYS2——都自带 POSIX shell，
> 因此一个 `build.sh` 就能编排 locale 数据嵌入、版本头生成与编译，无需额外构建系统，处处行为一致。

### 运行测试

```bash
# 构建并运行单元测试（跳过集成测试）
bash build.sh test

# 构建并运行全部测试（单元 + 集成）
bash build.sh test-all

# 仅运行集成测试
bash build.sh integration

# 详细输出
bash build.sh test -v
```

- **单元测试**（`test/test_*.cpp`）：约 987 个用例，覆盖全部模块
- **集成测试**（`test/test_integration*.cpp`）：约 112 个端到端场景，标记为 `[integration]`，分布在 `test_integration.cpp`、`test_integration_workspace.cpp`、`test_integration_report.cpp`、`test_integration_git.cpp`（`test-all` = 1099 用例 / 6342 断言；1094 通过、5 跳过）
- 测试使用 [Catch2](https://github.com/catchorg/Catch2) v3
- 设置 `EZMK_TEST_BIN` 可覆盖集成测试使用的 ezmk 二进制路径

## 编译器支持

EazyMake 在构建时自动检测编译器（优先级：`$CXX` / `$CC` → 平台默认）。同一份 `ezmk.toml` 可在不同编译器下使用。

> **为什么要自动检测而不是让用户指定？** 自动探测工具链意味着项目用平台已有的编译器即可构建，
> 一份 `ezmk.toml` 在 Linux、macOS、Windows 之间保持可移植，无需为每种环境重写配置。

| 编译器 | 平台 | 检测方式 |
|--------|------|----------|
| **GCC**（g++/gcc） | Linux、macOS、MSYS2 | 各平台默认 |
| **Clang**（clang++/clang） | Linux、macOS | `$CXX=clang++` 或自动回退 |
| **MSVC**（`cl.exe`） | Windows | 通过 `vcvars64.bat` 自动检测（Visual Studio 2019+） |

### 使用 MSVC

在安装了 Visual Studio 的 Windows 上，EazyMake 通过加载 `vcvars64.bat` 环境自动检测 MSVC。无需额外配置——直接运行 `ezmk build` 即可。

`ezmk.toml` 中的 **MSVC 专用标志**：

```toml
[compile]
flags = ["-Wall", "-O2"]          # GCC/Clang 标志（MSVC 忽略）
msvc_flags = ["/W4", "/O2"]       # MSVC 专用标志（GCC/Clang 忽略）

[link]
msvc_flags = ["/SUBSYSTEM:CONSOLE"]
```

EazyMake 会自动将常见 GCC 标志翻译为 MSVC 等价形式（如 `-Wall` → `/W4`、`-O2` → `/O2`、`-g` → `/Zi`）。对于需要显式 MSVC 写法或无翻译规则的标志，使用 `msvc_flags`。

> **为什么要自动翻译常见标志？** 这样同一份 `ezmk.toml` 只需用熟悉的 GCC 风格标志表达一次意图，
> 不必为每个编译器重复维护。翻译无法覆盖所有标志，因此保留 `msvc_flags` 作为显式逃生口。

> **注意：** MSVC 支持用于构建*用户项目*，而非 EazyMake 自身。要从源码构建 `ezmk`，请通过 MSYS2 的 GCC 或 Linux/macOS。

### 跨编译器构建

同一项目无需修改即可在 GCC 和 MSVC 下构建——切换编译器不会复用陈旧产物：记录里保存检测到的编译器**版本串**（`compiler_version`），它一变就会使整份缓存失效。记录中的 `compiler` 名称字段仅用于诊断，不参与比较。

> **为什么缓存的失效依据是编译器版本？** 不同工具链编译出的目标文件二进制不兼容；
> 如果切换编译器（或升级编译器）不使缓存失效，过时的 GCC 目标文件就会在 MSVC 构建中被
> 静默复用（反之亦然）。版本串正是真正触发这次失效的字段。

## 项目结构

```
my_project/
  .ezmk/
    pkg/            # 已安装的包
    temp/           # 临时文件（自动清理）
    cache/          # 构建缓存（record.json + obj/）
    repo/           # 仓库注册表 + 克隆的仓库
      list.toml
      .cache/
  include/          # 项目头文件（*.h, *.hpp）
  src/              # 项目源文件（*.c, *.cpp, *.cxx）
  build/            # 构建输出
  ezmk.toml         # 项目配置
```

## Shell 补全（zsh）

EazyMake 附带了位于 `res/ezmk.zsh` 的静态 zsh 补全脚本。将其安装为 `_ezmk`，放到 `fpath` 中的某个目录即可：

```bash
# 系统级安装
cp res/ezmk.zsh /usr/share/zsh/site-functions/_ezmk

# 或当前用户安装
mkdir -p ~/.zsh/completions
cp res/ezmk.zsh ~/.zsh/completions/_ezmk
# 然后在 ~/.zshrc 中添加：fpath=(~/.zsh/completions $fpath)
```

安装后重启 shell，或运行 `autoload -Uz compinit && compinit`。

## 手册页（man）

四页手写 roff 手册构成离线速查：`ezmk.1`、`ezmk-lua.1`、`ezmk.toml.5`、`ezmk-workspace.toml.5`。完整规范仍在 `docs/`；各页的 SEE ALSO 指回此处，`ezmk help` 末行也打印同一指引。

| 渠道 | 手册页落位 |
|---|---|
| `install.sh`（Linux / macOS / MSYS2） | `$PREFIX/share/man/man1/{ezmk.1,ezmk-lua.1}` 与 `$PREFIX/share/man/man5/{ezmk.toml.5,ezmk-workspace.toml.5}`（`EZMK_NO_MAN=1` 跳过） |
| Arch / MSYS2 包（`publish/arch/PKGBUILD`） | `/usr/share/man/man1/{ezmk.1,ezmk-lua.1}` 与 `/usr/share/man/man5/{ezmk.toml.5,ezmk-workspace.toml.5}` |
| Release 资产 + Homebrew | macOS / Linux 压缩包含 `man/`；formula 通过 `man1.install` / `man5.install` 安装 |

使用非标准前缀（`$HOME/.local`）时 `man` 可能搜索不到，安装脚本会打印需要追加到 shell 配置的行：

```bash
export MANPATH="$HOME/.local/share/man:$MANPATH"
```

不安装也可直接从检出目录阅读：

```bash
man -l man/ezmk.1
man -l man/ezmk-lua.1
man -l man/ezmk.toml.5
man -l man/ezmk-workspace.toml.5
```

Windows（原生、无 MSYS2）没有 `man` 命令、也不分发手册页：PowerShell 安装脚本与 Windows 压缩包刻意保持不变；MSYS2 用户通过 `install.sh` 获得手册页。

**防漂移。** `scripts/check_man_sync.py` 把手册页与 CLI 选项规格（`src/cli.cpp`）、配置解析器（`src/config.cpp`）及环境变量做双向比对；CI 另用 `groff -man -Tutf8 -z -ww` 渲染两页。改动手册页前请先阅读 [CONTRIBUTING](../../CONTRIBUTING.md#man-pages)。
