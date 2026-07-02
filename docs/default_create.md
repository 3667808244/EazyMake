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

---

## `<project_dir>/ezmk.toml`
```toml
[project]
name = "{project_name}"
type = "executable"
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]

[link]
flags = []
link_dirs = []
system_target = []

[depends]
lib = []
```

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

---

## `<project_dir>/README.md`
不添加内容只创建空文件。

---

## Git 初始化（0.1.5+）

创建项目后自动执行 `git init`（如果系统中 git 可用）。可通过 `--disable-git-init` 跳过。

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
