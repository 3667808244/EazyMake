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

> **Per-type sources (1.2.1+):** the code above is the default `executable`
> template. `--type static` / `--type shared` do **not** create `main.cpp` —
> they scaffold a library skeleton `include/<name>.hpp` (`#pragma once` +
> `namespace <ns>` + a `greeting()` sample API) + `src/<name>.cpp`
> (implementation) — file names keep the original project name, and the C++
> namespace replaces `-` / `.` / spaces with `_`; `--type utils` generates no
> C++ code at all, only a `utils/` directory for Lua scripts.

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

# [test]                     # enable project tests: uncomment to run `ezmk test`
# framework = "catch2"       # "catch2" | "ezmk" (built-in framework)
# dirs = ["test"]
# default_profile = "debug"  # 1.2.0-dev.12+: default test profile
# include_dirs = ["test/helpers"]   # test-only -I (1.2.0-dev.12+)
# link_targets = ["pthread"]        # test-only -l (1.2.0-dev.12+)
```

> **Why these defaults?** The generated config is pre-wired to the scaffolded
> layout — `type = "executable"` matches the `src/main.cpp` entry point, C++17 is
> a modern baseline, and `-Wall -Wextra` keeps a new project warning-visible;
> optimization lives in the profiles (`debug` default, `release` requires an
> explicit `--profile release`). The `# [test]` lines at the end are a
> **commented-out example section** (1.2.1+): uncomment and fill them in to use
> `ezmk test`; pure comments have zero parse impact, the fields match the `[test]`
> config exactly (the deprecated `flags` is deliberately not shown).

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
