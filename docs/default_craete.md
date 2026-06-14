# 默认创建文件

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

[compile]
flags = ["-Wall", "-Wextra", "-O2"]

[link]
flags = []

[depends]
lib = []
```

---

## `<project_dir>/README.md`
不添加内容只创建