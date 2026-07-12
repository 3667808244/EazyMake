# Default Created Files

`ezmk project new <name> [--type <type>] [--disable-git-init] [--disable-gitignore]` generates the following files.

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

Auto-generated (can be skipped with `--disable-gitignore`):

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
An empty file is created with no content added.

---

## Git Initialization (0.1.5+)

After creating the project, `git init` is automatically executed (if git is available on the system). This can be skipped with `--disable-git-init`.

---

## Optional Parameters

| Parameter | Description |
|-----------|-------------|
| `--type <type>` | Project type: `executable` (default), `static`, `shared`, `utils` |
| `--disable-git-init` | Skip `git init` |
| `--disable-gitignore` | Skip `.gitignore` generation |

## Generated Directory Structure

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
