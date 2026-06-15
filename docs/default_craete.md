# 默认创建文件

`ezmk project new <name>` 生成以下文件。

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

## `<project_dir>/README.md`
不添加内容只创建
