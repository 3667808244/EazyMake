# Package Authoring Guide

This guide explains how to create an EazyMake package — either for your own use or for submission to the official default repository.

---

## 1. Package Structure

A standard library package has the following layout:

```
<mypkg>/
├── ezmk.toml         # Package metadata and build configuration (required)
├── include/          # Public headers (required)
│   └── mylib/
│       └── mylib.h
├── src/              # Source files (optional for header-only packages)
│   └── mylib.cpp
└── script/           # Install hooks (optional)
    ├── preinstall.sh   # or .ps1 / .bat (Windows)
    └── postinstall.sh
```

A **header-only** package omits `src/` and sets `header_only = true` in `ezmk.toml`.

A **utils** package (`type = "utils"`) provides Lua-based tools:

```
<myutils>/
├── ezmk.toml
├── utils/
│   └── mytool.lua    # Lua script with run() entry point
├── include/          # Optional
└── src/              # Optional
```

---

## 2. `ezmk.toml` Reference

### 2.1 `[project]` (required)

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `name` | string | **Yes** | — | Package name (lowercase, hyphens OK, e.g. `"my-lib"`) |
| `version` | string | **Yes** | — | Semantic version, e.g. `"1.2.3"` |
| `type` | string | No | `"executable"` | `"static"` (library), `"shared"` (shared lib), `"utils"` (Lua tools) |
| `language` | string | No | `"C++17"` | Format: `<Lang><Ver>`, e.g. `"C11"`, `"C++17"`, `"C++20"` |
| `header_only` | bool | No | `false` | **0.9.7+** Set to `true` to skip compilation (no `src/` required) |
| `precompiled` | bool | No | `false` | **0.9.7+** Set to `true` to use pre-built `lib/*.a` (no `src/` required). See §3.2 below. |

### 2.2 `[depends]`

```toml
[depends]
lib = ["zlib@^1.3", "imgui@~1.91"]   # Hard dependencies (missing → error)
want = ["sdl2"]                        # Optional dependencies (0.2.2+)
```

**Version constraint syntax (0.9.6+):**

| Syntax | Meaning | Example |
|--------|---------|---------|
| `"pkg@1.2.3"` | Exact match | `zlib@1.3.1` |
| `"pkg@^1.2"` | Compatible: `>=1.2.0 <2.0.0` | `glfw@^3.4` |
| `"pkg@~1.2"` | Approximate: `>=1.2.0 <1.3.0` | `imgui@~1.91` |
| `"pkg@>=1.0"` | Greater or equal | `yaml-cpp@>=0.8` |
| `"pkg@>1.0"` | Strictly greater | — |

Plain strings without `@` are treated as unconstrained (any version).

### 2.3 `[compile]`

```toml
[compile]
flags = ["-Wall", "-O2"]          # GCC/Clang compile flags
msvc_flags = ["/W4", "/O2"]      # MSVC-only flags (0.2.1+)
include_dirs = ["include"]        # -I paths (default: ["include"])
src_dirs = ["src"]               # Source directories (default: ["src"], 0.2.2+)

[compile.macros]                  # Semantic macro definitions (0.2.2+)
MY_DEFINE = "1"
MY_STRING = "hello"
```

### 2.4 `[link]`

```toml
[link]
flags = ["-pthread"]              # Link flags for the consumer
link_dirs = []                    # Additional -L paths
system_target = ["pthread"]       # -l system libraries

[link.profile.release]            # 0.2.3+ profile-specific link flags
flags = ["-flto"]
```

### 2.5 `[hooks]` (0.2.3+)

Lua build hooks for packages used as dependencies:

```toml
[hooks]
pre_build = "hooks/pre.lua"      # Before compilation
post_build = "hooks/post.lua"    # After successful link
on_failure = "hooks/fail.lua"    # On build error
```

> **Note:** Build hooks run in a sandboxed Lua environment. See `docs/en/config_file.md` for details.

---

## 3. Package Types

### 3.1 Static Library (`type = "static"`)

The most common package type. `ezmk pkg install` compiles `src/` → `lib<name>.a` and copies everything to the install directory.

```toml
[project]
name = "mylib"
version = "1.0.0"
type = "static"
language = "C++17"
```

### 3.2 Header-Only (`header_only = true`, 0.9.7+)

For libraries that consist entirely of header files. No compilation step — `ezmk` only copies the `include/` directory.

```toml
[project]
name = "cli11"
version = "2.5.0"
type = "static"
header_only = true

# No src/ directory needed
```

Header-only packages:
- Do NOT require a `src/` directory
- Skip the compilation and archiving steps during install
- Are validated the same as other packages (must have `include/` and `ezmk.toml`)

### 3.3 Precompiled Package (`precompiled = true`, 0.9.7+)

For libraries that are difficult to build from source (e.g. require CMake, platform-specific configuration, or large build systems). The package ships pre-built `.a`/`.lib` files in `lib/` instead of source code in `src/`.

```toml
[project]
name = "sdl2"
version = "2.32.10"
type = "static"
precompiled = true

# No src/ directory — pre-built binaries in lib/ instead
```

```
sdl2/
├── ezmk.toml
├── include/       # Headers (cross-platform)
└── lib/           # Pre-built static libraries
    └── libSDL2.a
```

**⚠️ Not recommended for general use.** Precompiled packages only work on the specific platform and architecture they were built for. Prefer source-based packages (`src/`) whenever possible, as they compile on any platform. Only use `precompiled` when:

- The library cannot be compiled with a simple `gcc`/`g++` invocation (requires CMake, autotools, etc.)
- You can enumerate and provide builds for all target platforms (see `plans/1.1.0-dev.2.md` for upcoming multi-platform support)

Currently (0.9.7), a precompiled package contains binaries for a single platform. Users on other platforms will see a link error. Multi-platform co-packaging (`lib<name>.<os>-<arch>.a`) is planned for 1.1.0.

### 3.4 Utils Package (`type = "utils"`)

Provides Lua-based tools accessible via `ezmk utils <name>`.

```toml
[project]
name = "my-tools"
version = "1.0.0"
type = "utils"

[utils]
tools = ["my-tool"]

[utils.permissions]              # Fine-grained permissions (0.2.5+)
read = ["*.txt"]
write = ["build/*"]
run = ["git"]
```

---

## 4. Install Hooks (0.2.1+)

Place platform-specific scripts in `script/` to run before/after installation:

| Hook | File | When |
|------|------|------|
| Preinstall | `script/preinstall.{sh,ps1,bat}` | Before files are copied |
| Postinstall | `script/postinstall.{sh,ps1,bat}` | After installation completes |

- **Linux/macOS:** `.sh` scripts
- **Windows:** `.ps1` (preferred) then `.bat`
- Scripts are opened in the user's editor for review before execution
- Users can skip script execution (install continues)
- Script failure can be overridden (user chooses to continue)

---

## 5. Creating a Package Archive

Packages in a repository are distributed as compressed archives (`.zip` or `.tar.gz`).

### 5.1 Manual Archiving

```bash
# From the package root:
tar czf mypkg-1.0.0.tar.gz mypkg/

# Or zip:
zip -r mypkg-1.0.0.zip mypkg/
```

### 5.2 Archive Layout

The archive should contain a single top-level directory (the package directory name does not need to match the package name):

```
mypkg-1.0.0.tar.gz
└── mylib-repo/
    ├── ezmk.toml
    ├── include/
    ├── src/
    └── script/        # Optional
```

EazyMake automatically finds the package root by looking for `ezmk.toml` inside a single top-level directory.

---

## 6. Registering in a Repository

### 6.1 Repository Structure

```
<repo>/
├── index.toml          # Package index
└── packages/           # Package archives
    ├── mypkg-1.0.0.tar.gz
    └── mypkg-1.0.0.zip
```

### 6.2 `index.toml` Entry

Add an entry to the repository's `index.toml`:

```toml
[[packages]]
name = "mypkg"
version = "1.0.0"
file = "packages/mypkg-1.0.0.tar.gz"
sha256 = "a1b2c3d4e5f6..."  # 64-char hex, strongly recommended
```

### 6.3 Computing SHA-256

```bash
# Linux/macOS:
sha256sum mypkg-1.0.0.tar.gz

# Windows PowerShell:
Get-FileHash -Algorithm SHA256 mypkg-1.0.0.tar.gz

# Or:
ezmk utils sha256 mypkg-1.0.0.tar.gz  # if available
```

### 6.4 Multiple Versions

List each version as a separate `[[packages]]` entry:

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

`ezmk pkg install` picks the highest version by default; users can specify constraints.

---

## 7. Validation Checklist

Before submitting a package, verify:

1. **`ezmk.toml`** is valid TOML and contains at minimum `[project]` with `name` and `version`
2. **`include/`** exists and contains the public headers
3. **`src/`** exists (unless `header_only = true`) and compiles without errors
4. **Dependencies** declared in `[depends]` are accurate and available
5. **Install test:** `ezmk pkg install <archive>` succeeds
6. **Link test:** A test project with `[depends] lib = ["<name>"]` builds and links
7. **Header test:** `#include` of the main header(s) succeeds without missing dependencies
8. **SHA-256** in `index.toml` matches the archive

---

## 8. Contributing to the Official Repository

1. Fork or clone `https://github.com/3667808244/ezmk-repo`
2. Add your package archive to `packages/`
3. Add the `[[packages]]` entry to `index.toml` with SHA-256
4. Submit a pull request

See the official repository's `CONTRIBUTING.md` for detailed contribution guidelines.

---

## 9. Complete Example: `hello-lib`

### Directory structure:

```
hello-lib/
├── ezmk.toml
├── include/
│   └── hello/
│       └── hello.h
└── src/
    └── hello.cpp
```

### `ezmk.toml`:

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

### `include/hello/hello.h`:

```cpp
#pragma once
#include <string>
namespace hello {
    std::string greet(std::string_view name);
}
```

### `src/hello.cpp`:

```cpp
#include "hello/hello.h"
namespace hello {
    std::string greet(std::string_view name) {
        return "Hello, " + std::string(name) + "!";
    }
}
```

### User's `ezmk.toml`:

```toml
[depends]
lib = ["hello-lib"]

[link]
system_target = ["hello-lib"]   # -lhello-lib
```

### User's `src/main.cpp`:

```cpp
#include "hello/hello.h"
#include <iostream>
int main() {
    std::cout << hello::greet("World") << std::endl;
    return 0;
}
```

---

## 10. Platform-Specific Notes

### Windows (MSYS2)

- Use `g++` from MSYS2 UCRT64 or MINGW64
- Static linking: `-static` in link flags
- Test both `.zip` and `.tar.gz` archive formats

### Linux

- Static linking may require `-static-libgcc -static-libstdc++`
- System development headers must be installed (e.g. `libx11-dev` for GLFW)

### macOS

- Static linking is partially supported (system libraries remain dynamic)
- Use `clang++` (Apple's default)
- `.dylib` for shared libraries instead of `.so`

---

## See Also

- **[pkg.md](pkg.md)** — Package management (install, update, remove)
- **[repo.md](repo.md)** — Repository management and `index.toml` format
- **[config_file.md](config_file.md)** — Full `ezmk.toml` specification
- **[`@safety.md`](@safety.md)** — Security model (SHA-256, sandbox)
