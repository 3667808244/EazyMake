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
| miniz                                   | v3.0.2           | **内嵌**             | ZIP 解压（`src/vendor/miniz/*`）                            |
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

- **单元测试**（`test/test_*.cpp`）：546 个用例，覆盖全部模块
- **集成测试**（`test/test_integration.cpp`）：8 个端到端场景，标记为 `[integration]`（`test-all` = 556 用例 / 2666 断言）
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

同一项目无需修改即可在 GCC 和 MSVC 下构建——缓存记录按编译器隔离，切换编译器不会导致缓存冲突。

> **为什么缓存要按编译器隔离？** 不同工具链编译出的目标文件二进制不兼容；缓存按编译器做键，
> 可以防止过时的 GCC 目标文件在 MSVC 构建中被静默复用（反之亦然）。

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
