# 包制作指南

本文档介绍如何创建 EazyMake 包——无论是供自己使用还是提交到官方默认仓库。

---

## 1. 包目录结构

标准库包布局如下：

```
<mypkg>/
├── ezmk.toml         # 包元数据和构建配置（必需）
├── include/          # 公共头文件（必需）
│   └── mylib/
│       └── mylib.h
├── src/              # 源文件（header-only 包可选）
│   └── mylib.cpp
└── script/           # 安装钩子（可选）
    ├── preinstall.sh   # 或 .ps1 / .bat（Windows）
    └── postinstall.sh
```

**Header-only** 包省略 `src/` 目录，并在 `ezmk.toml` 中设置 `header_only = true`。

**Utils** 工具包（`type = "utils"`）提供 Lua 工具：

```
<myutils>/
├── ezmk.toml
├── utils/
│   └── mytool.lua    # 包含 run() 入口函数的 Lua 脚本
├── include/          # 可选
└── src/              # 可选
```

---

## 2. `ezmk.toml` 参考

### 2.1 `[project]`（必需）

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `name` | string | **是** | — | 包名（小写，可用连字符，如 `"my-lib"`） |
| `version` | string | **是** | — | 语义化版本号，如 `"1.2.3"` |
| `type` | string | 否 | `"executable"` | `"static"`（静态库）、`"shared"`（动态库）、`"utils"`（Lua 工具） |
| `language` | string | 否 | `"C++17"` | 格式：`<语言><版本>`，如 `"C11"`、`"C++17"`、`"C++20"` |
| `header_only` | bool | 否 | `false` | **0.9.7+** 设为 `true` 可跳过编译（无需 `src/`） |
| `precompiled` | bool | 否 | `false` | **0.9.7+** 设为 `true` 使用预编译 `lib/*.a`（无需 `src/`），见下方 §3.3 |

### 2.2 `[depends]`

```toml
[depends]
lib = ["zlib@^1.3", "imgui@~1.91"]   # 硬依赖（缺失 → 报错）
want = ["sdl2"]                        # 可选依赖（0.2.2+）
```

**版本约束语法（0.9.6+）：**

| 语法 | 含义 | 示例 |
|------|------|------|
| `"pkg@1.2.3"` | 精确匹配 | `zlib@1.3.1` |
| `"pkg@^1.2"` | 兼容：`>=1.2.0 <2.0.0` | `glfw@^3.4` |
| `"pkg@~1.2"` | 近似：`>=1.2.0 <1.3.0` | `imgui@~1.91` |
| `"pkg@>=1.0"` | 大于等于 | `yaml-cpp@>=0.8` |
| `"pkg@>1.0"` | 严格大于 | — |

不带 `@` 的纯字符串视为无约束（接受任何版本）。

### 2.3 `[compile]`

```toml
[compile]
flags = ["-Wall", "-O2"]          # GCC/Clang 编译选项
msvc_flags = ["/W4", "/O2"]      # MSVC 专用选项（0.2.1+）
include_dirs = ["include"]        # -I 路径（默认：["include"]）
src_dirs = ["src"]               # 源码目录（默认：["src"]，0.2.2+）

[compile.macros]                  # 语义化宏定义（0.2.2+）
MY_DEFINE = "1"
MY_STRING = "hello"
```

### 2.4 `[link]`

```toml
[link]
flags = ["-pthread"]              # 消费者的链接选项
link_dirs = []                    # 额外的 -L 路径
system_target = ["pthread"]       # -l 系统库
```

### 2.5 `[hooks]`（0.2.3+）

Lua 构建钩子，用于作为依赖的包：

```toml
[hooks]
pre_build = "hooks/pre.lua"      # 编译前
post_build = "hooks/post.lua"    # 链接成功后
on_failure = "hooks/fail.lua"    # 构建失败时
```

> **注意：** 构建钩子在沙盒化的 Lua 环境中运行。详见 `docs/zh/config_file.md`。

---

## 3. 包类型

### 3.1 静态库（`type = "static"`）

最常见的包类型。`ezmk pkg install` 编译 `src/` → `lib<name>.a` 并复制所有文件到安装目录。

```toml
[project]
name = "mylib"
version = "1.0.0"
type = "static"
language = "C++17"
```

### 3.2 Header-Only（`header_only = true`，0.9.7+）

适用于纯头文件库。无需编译——`ezmk` 仅复制 `include/` 目录。

```toml
[project]
name = "cli11"
version = "2.5.0"
type = "static"
header_only = true

# 无需 src/ 目录
```

Header-only 包：
- **不要求** `src/` 目录
- 安装时跳过编译和归档步骤
- 验证标准与其他包相同（必须有 `include/` 和 `ezmk.toml`）

### 3.3 预编译包（`precompiled = true`，0.9.7+）

适用于难以从源码编译的库（如需要 CMake、平台特定配置或复杂构建系统）。包在 `lib/` 中提供预编译的 `.a`/`.lib` 文件，而非 `src/` 中的源码。

```toml
[project]
name = "sdl2"
version = "2.32.10"
type = "static"
precompiled = true

# 无需 src/ 目录 — 预编译产物直接放 lib/
```

```
sdl2/
├── ezmk.toml
├── include/       # 头文件（跨平台共用）
└── lib/           # 预编译静态库
    └── libSDL2.a
```

**⚠️ 不推荐常规使用。** 预编译包只能在构建时对应的特定平台和架构上工作。优先使用源码包（`src/`），它们可在任何平台上编译。仅在以下情况使用 `precompiled`：

- 库无法用简单的 `gcc`/`g++` 命令编译（需要 CMake、autotools 等）
- 能枚举并为所有目标平台提供构建产物（后续多平台支持见 `plans/1.1.0-dev.2.md`）

当前版本（0.9.7）的预编译包仅包含单一平台的二进制文件。其他平台的用户会遇到链接错误。多平台共包（`lib<name>.<os>-<arch>.a`）计划在 1.1.0 中实现。

### 3.4 Utils 工具包（`type = "utils"`）

提供通过 `ezmk utils <name>` 访问的 Lua 工具。

```toml
[project]
name = "my-tools"
version = "1.0.0"
type = "utils"

[utils]
tools = ["my-tool"]

[utils.permissions]              # 细粒度权限（0.2.5+）
read = ["*.txt"]
write = ["build/*"]
run = ["git"]
```

---

## 4. 安装钩子（0.2.1+）

在 `script/` 中放置平台特定脚本，在安装前后执行：

| 钩子 | 文件 | 时机 |
|------|------|------|
| Preinstall | `script/preinstall.{sh,ps1,bat}` | 文件复制前 |
| Postinstall | `script/postinstall.{sh,ps1,bat}` | 安装完成后 |

- **Linux/macOS：** `.sh` 脚本
- **Windows：** `.ps1`（优先），其次 `.bat`
- 脚本会在用户编辑器中打开供审查
- 用户可跳过脚本执行（安装继续）
- 脚本失败可被覆盖（用户选择继续）

---

## 5. 创建包归档文件

仓库中的包以压缩归档形式（`.zip` 或 `.tar.gz`）分发。

### 5.1 手动打包

```bash
# 从包根目录：
tar czf mypkg-1.0.0.tar.gz mypkg/

# 或 zip：
zip -r mypkg-1.0.0.zip mypkg/
```

### 5.2 归档布局

归档文件应包含一个顶层目录（目录名无需与包名一致）：

```
mypkg-1.0.0.tar.gz
└── mylib-repo/
    ├── ezmk.toml
    ├── include/
    ├── src/
    └── script/        # 可选
```

EazyMake 自动在唯一顶层目录中查找 `ezmk.toml` 来确定包根目录。

---

## 6. 在仓库中注册

### 6.1 仓库结构

```
<repo>/
├── index.toml          # 包索引
└── packages/           # 包归档文件
    ├── mypkg-1.0.0.tar.gz
    └── mypkg-1.0.0.zip
```

### 6.2 `index.toml` 条目

在仓库的 `index.toml` 中添加条目：

```toml
[[packages]]
name = "mypkg"
version = "1.0.0"
file = "packages/mypkg-1.0.0.tar.gz"
sha256 = "a1b2c3d4e5f6..."  # 64 位十六进制字符串，强烈推荐
```

### 6.3 计算 SHA-256

```bash
# Linux/macOS：
sha256sum mypkg-1.0.0.tar.gz

# Windows PowerShell：
Get-FileHash -Algorithm SHA256 mypkg-1.0.0.tar.gz
```

### 6.4 多版本

每个版本作为单独的 `[[packages]]` 条目：

```toml
[[packages]]
name = "mypkg"
version = "1.0.0"
file = "packages/mypkg-1.0.0.tar.gz"
sha256 = "a1b2..."

[[packages]]
name = "mypkg"
version = "1.1.0"
file = "packages/mypkg-1.1.0.tar.gz"
sha256 = "c3d4..."
```

`ezmk pkg install` 默认选择最高版本；用户可指定约束。

---

## 7. 验证清单

提交包之前，验证以下项目：

1. **`ezmk.toml`** 是有效的 TOML，至少包含 `[project]` 的 `name` 和 `version`
2. **`include/`** 存在且包含公共头文件
3. **`src/`** 存在（除非 `header_only = true`）且能无错误编译
4. **依赖** `[depends]` 中声明的依赖准确且可用
5. **安装测试：** `ezmk pkg install <archive>` 成功
6. **链接测试：** 带有 `[depends] lib = ["<name>"]` 的测试项目能构建并链接
7. **头文件测试：** `#include` 主头文件不会缺少依赖
8. **SHA-256** 在 `index.toml` 中与归档文件匹配

---

## 8. 向官方仓库贡献

1. Fork 或克隆 `https://github.com/3667808244/ezmk-repo`
2. 将你的包归档文件添加到 `packages/`
3. 在 `index.toml` 中添加 `[[packages]]` 条目及 SHA-256
4. 提交 Pull Request

详细的贡献指南请参见官方仓库的 `CONTRIBUTING.md`。

---

## 9. 完整示例：`hello-lib`

### 目录结构：

```
hello-lib/
├── ezmk.toml
├── include/
│   └── hello/
│       └── hello.h
└── src/
    └── hello.cpp
```

### `ezmk.toml`：

```toml
[project]
name = "hello-lib"
version = "1.0.0"
type = "static"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra"]
include_dirs = ["include"]

[depends]
lib = []
```

### `include/hello/hello.h`：

```cpp
#pragma once
#include <string>
namespace hello {
    std::string greet(std::string_view name);
}
```

### `src/hello.cpp`：

```cpp
#include "hello/hello.h"
namespace hello {
    std::string greet(std::string_view name) {
        return "Hello, " + std::string(name) + "!";
    }
}
```

### 用户的 `ezmk.toml`：

```toml
[depends]
lib = ["hello-lib"]

[link]
system_target = ["hello-lib"]   # -lhello-lib
```

### 用户的 `src/main.cpp`：

```cpp
#include "hello/hello.h"
#include <iostream>
int main() {
    std::cout << hello::greet("World") << std::endl;
    return 0;
}
```

---

## 10. 平台特定注意事项

### Windows（MSYS2）

- 使用 MSYS2 UCRT64 或 MINGW64 的 `g++`
- 链接选项中添加 `-static`
- 测试 `.zip` 和 `.tar.gz` 两种归档格式

### Linux

- 静态链接可能需要 `-static-libgcc -static-libstdc++`
- 必须安装系统开发头文件（如 GLFW 需要 `libx11-dev`）

### macOS

- 静态链接部分支持（系统库仍为动态链接）
- 使用 `clang++`（Apple 默认编译器）
- 动态库使用 `.dylib` 而非 `.so`

---

## 参见

- **[pkg.md](pkg.md)** — 包管理（安装、更新、移除）
- **[repo.md](repo.md)** — 仓库管理和 `index.toml` 格式
- **[config_file.md](config_file.md)** — 完整的 `ezmk.toml` 规范
- **[@safety.md](@safety.md)** — 安全模型（SHA-256、沙盒）
