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

> **Why this template?** The scaffold ships a minimal runnable program so a new
> project builds and runs immediately (`ezmk project new` → `ezmk build`), with
> no setup between scaffolding and the user's first edits.

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

> **Why these defaults?** The generated config is pre-wired to the scaffolded
> layout — `type = "executable"` matches the `src/main.cpp` entry point, C++17 is
> a modern baseline, and `-Wall -Wextra -O2` keeps a new project warning-visible
> while still building fast.

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

> **Why this `.gitignore`?** It covers everything EazyMake generates — `build/`
> outputs, `.ezmk/` cache/temp/package state, and object files — so the first
> commit stays clean. It is created alongside `git init` and skipped via
> `--disable-gitignore` for projects not under version control.

---

## `<project_dir>/README.md`
An empty file is created with no content added.

> **Why an empty file?** The scaffold reserves a place for the project's own
> documentation without presuming what it should say — the user fills it in,
> and no boilerplate text needs to be deleted later.

---

## Git Initialization (0.1.5+)

After creating the project, `git init` is automatically executed (if git is available on the system). This can be skipped with `--disable-git-init`.

> **Why git init by default?** Most projects are version-controlled, so the
> scaffold initializes a repo (pairing with the generated `.gitignore`) to be
> immediately useful — and skips quietly if `git` is not installed.
> `--disable-git-init` is the opt-out for users who manage version control
> themselves or want a bare directory.

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

> **Why this layout?** `src/` and `include/` are the conventional input dirs that
> ezmk scans by default, `build/` collects all outputs, and `.ezmk/` keeps
> EazyMake's internal state (installed packages, temp files, cache) in one place —
> so `build/` and `.ezmk/` can be safely rebuilt, cleaned, or deleted at any time.
