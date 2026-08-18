# 默认创建文件

`ezmk project new <name> [--type <type>] [--disable-git-init] [--disable-gitignore]` 生成以下文件。

---

## `<project_dir>/src/main.cpp`
```cpp
#include <iostream>

int main(int argc, char **argv){
    std::cout << "Hello world!" << std::endl;
    return 0;
}
```

> **为什么是这个模板？** 脚手架生成的是一个最小的可运行程序，让新项目开箱即用（`ezmk project new` → `ezmk build` 即可运行），从脚手架到用户第一次改代码之间无需任何配置。

> **按类型差异化（1.2.1+）：** 上面是默认的 `executable` 模板。`--type static` / `--type shared`
> **不生成 `main.cpp`**，而是生成库骨架 `include/<name>.hpp`（`#pragma once` + `namespace <ns>` +
> `greeting()` 示例公共 API）+ `src/<name>.cpp`（实现）——文件名保留原始项目名，C++ namespace
> 将 `-` / `.` / 空格替换为 `_`；`--type utils` 不生成任何 C++ 代码，只有 `utils/` 目录放 Lua 脚本。

---

## `<project_dir>/ezmk.toml`
```toml
[project]
name = "{project_name}"
type = "executable"
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra"]
default_profile = "debug"
include_dirs = ["include"]

[compile.profile.debug]
flags = ["-g", "-O0"]
msvc_flags = ["/Zi", "/Od"]

[compile.profile.release]
flags = ["-O2", "-DNDEBUG"]
msvc_flags = ["/O2", "/DNDEBUG"]

[link]
flags = []
link_dirs = []
system_target = []

[depends]
lib = []

# [test]                     # 启用项目测试：取消注释后运行 `ezmk test`
# framework = "catch2"       # "catch2" | "ezmk"（内置框架）
# dirs = ["test"]
# default_profile = "debug"  # 1.2.0-dev.12+：测试默认 profile
# include_dirs = ["test/helpers"]   # 测试专属 -I（1.2.0-dev.12+）
# link_targets = ["pthread"]        # 测试专属 -l（1.2.0-dev.12+）
```

> **为什么是这些默认值？** 生成的配置与脚手架布局一一对应——`type = "executable"` 对应 `src/main.cpp` 入口，C++17 是现代的基线标准，`-Wall -Wextra` 让新项目尽早看到警告；优化归 profile（`debug` 默认，`release` 需显式 `--profile release`）。文件末尾的 `# [test]` 是**注释掉的示例节**（1.2.1+）：取消注释并填写后即可 `ezmk test`，纯注释对解析零影响，字段与 `[test]` 配置一致（刻意不展示已弃用的 `flags`）。

---

## `<project_dir>/.gitignore`

自动生成（可通过 `--disable-gitignore` 跳过）：

```gitignore
# EazyMake build artifacts
build/
.ezmk/
*.o
*.obj
*.tmp.o
*.tmp.obj
```

> **为什么是这份 `.gitignore`？** 条目覆盖了 EazyMake 生成的所有内容——`build/` 产物、`.ezmk/` 缓存/临时/包状态、目标文件——保证首次提交保持干净。它与 `git init` 一起生成，项目不在版本控制下时可通过 `--disable-gitignore` 跳过。

---

## `<project_dir>/README.md`
不添加内容只创建空文件。

> **为什么是空文件？** 脚手架为项目自己的文档预留位置，但不臆测其内容——由用户填写，也无需删除任何样板文本。

---

## Git 初始化（0.1.5+）

创建项目后自动执行 `git init`（如果系统中 git 可用）。可通过 `--disable-git-init` 跳过。

> **为什么默认执行 git init？** 大多数项目都使用版本控制，脚手架默认初始化仓库（与生成的 `.gitignore` 配套）以便开箱即用——若系统未安装 git 则安静跳过。`--disable-git-init` 供自行管理版本控制或想要一个纯净目录的用户关闭。

---

## 可选参数

| 参数 | 说明 |
|------|------|
| `--type <type>` | 项目类型：`executable`（默认）、`static`、`shared`、`utils` |
| `--disable-git-init` | 跳过 `git init` |
| `--disable-gitignore` | 跳过 `.gitignore` 生成 |

## 生成的目录结构

```
<project_dir>/
  .ezmk/
    pkg/
    temp/
    cache/
  include/
  src/
    main.cpp
  build/
  ezmk.toml
  .gitignore
  README.md
```

> **为什么是这种布局？** `src/` 和 `include/` 是 ezmk 默认扫描的约定输入目录，`build/` 集中存放所有产物，`.ezmk/` 则把 EazyMake 的内部状态（已安装包、临时文件、缓存）放在一处——因此 `build/` 和 `.ezmk/` 可随时安全地重建、清理或删除。
