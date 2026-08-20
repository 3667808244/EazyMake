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

> **Why this layout?** A package is just a standard EazyMake project with a
> fixed convention (`ezmk.toml` + `include/`, optional `src/`/`script/`), so
> `ezmk pkg install` can compile and install every package identically — no
> per-package special cases.

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

> **Why lowercase with hyphens?** Package names map directly to file names and
> link names (`-l<name>`), so the lowercase-hyphen convention keeps them portable
> across case-sensitive filesystems and free of ambiguous characters.

### 2.2 `[depends]`

```toml
[depends]
lib = ["zlib@^1.3", "imgui@~1.91"]   # Hard dependencies (missing → error)
want = ["sdl2"]                        # Optional dependencies (0.2.2+)
```

> **Why optional dependencies?** A `want` dependency is used when available and
> silently dropped otherwise (the code is expected to fall back to a pure-standard
> implementation), so one package can serve users with and without the optional
> library.

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

> **`src_dirs` / `include_dirs` take effect for packages (1.2.0-dev.9+)**: package sources are collected from `src_dirs` (default `["src"]`) — multiple directories, missing dirs warn + skip, filename dedup, same as the project build; `include_dirs` resolve relative to the package root as `-I`, with order-preserving dedup against the default `include/`. Packages are **always** compiled as static libraries; `[project].type` never triggers the `main.cpp` check (the docs' default `type = "executable"` works fine). `header_only` / `precompiled` / utils packages short-circuit before source collection — no `src_dirs` directory is required; a regular package with all `src_dirs` directories missing or empty fails the install (fatal).

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

The most common package type. `ezmk pkg install` compiles `[compile].src_dirs` (default `src/`) → `lib<name>.a` and copies everything to the install directory.

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

> **Why do header-only packages exist?** Many widely used libraries (CLI11, stb,
> most of Boost) are implemented entirely in headers — compiling them would
> produce a trivially empty archive. Shipping headers only, and skipping the
> compile step at install, makes them cheaper to build and install.

### 3.3 Precompiled Package (`precompiled = true`, 0.9.7+)

For libraries that are difficult to build from source (e.g. require CMake, platform-specific configuration, or large build systems). The package ships pre-built `.a`/`.lib` files in `lib/` instead of source code in `src/`.

> **Why an opt-in field?** `precompiled = true` trades portability for install
> speed: a source package compiles anywhere, while a prebuilt binary only runs on
> the exact platform and architecture it was built for. Making it opt-in keeps the
> default (source-based) path portable and reserves precompiled builds for
> libraries that genuinely need them.
>
> **`ezmk project pack` (1.2.5+) produces a source package by default** (`src/` + `include/` + `ezmk.toml` as-is; platform-independent, compiled on the consumer side). Use `ezmk project pack --precompiled` for a prebuilt archive (`static` projects only; ships `include/` + `lib/` + a `precompiled = true` marker) — consistent with the "source first" stance above.

```toml
[project]
name = "sdl2"
version = "2.32.10"
type = "static"
precompiled = true

# No src/ directory — pre-built binaries in lib/ instead
```

**Multi-platform co-packaging** (1.1.0-dev.2+): A single package can include precompiled binaries for multiple platforms in `lib/`, with automatic matching via naming convention:

```
sdl2/
├── ezmk.toml
├── include/       # Headers (cross-platform)
└── lib/           # Pre-built static libraries
    ├── libSDL2.win-x64-msvc143.a
    ├── libSDL2.linux-x64-gcc13-abi11.a
    ├── libSDL2.mac-arm64-clang15.a
    └── libSDL2.win-x64.a          # untagged (legacy — degraded match)
```

**Naming convention** (1.2.0-dev.10+): `lib<name>.<os>-<arch>[-<compiler>][-<abi>].<ext>`

| OS | Arch | Tag |
|----|------|-----|
| Windows | x86_64 | `win-x64` |
| Windows | x86 | `win-x86` |
| Linux | x86_64 | `linux-x64` |
| Linux | aarch64 | `linux-arm64` |
| macOS | x86_64 | `mac-x64` |
| macOS | aarch64 | `mac-arm64` |

Compiler tag (optional) is derived from the consumer toolchain: `gcc<major>` (e.g. `gcc13`), `clang<major>` (e.g. `clang18`), `msvc143` (VS toolset lookup: 140/141/142/143). ABI tag (optional) follows toolchain defaults: GCC / Clang on Linux (libstdc++ default) → `abi11` (CXX11 ABI); Clang on macOS (libc++ default) and MSVC → none.

**Selection priority** (1.2.0-dev.10+, ABI-safe 4-level match):
1. **L4 full tag**: `os-arch-compiler-abi` all equal (e.g. `linux-x64-gcc13-abi11` vs `linux-x64-gcc13-abi11`)
2. **L3 same compiler**: `os-arch-compiler` equal and the artifact has no abi segment (same compiler = same default ABI, safe)
3. **L2 platform**: only `os-arch` equal (legacy untagged artifacts)
4. **L1 bare**: bare `lib<name>.a` (backward compatible with single-platform legacy packages)

> Same compiler but an explicitly different abi segment (e.g. `gcc11-abi8` vs `gcc11-abi11`) is **ABI-incompatible** and skipped; ties break by lexicographically smallest filename (deterministic).

**ABI fallback warning** (1.2.0-dev.10+): when the consumer has a toolchain tag but selection falls to **L2/L1** (possibly cross-toolchain), an explicit warning is printed with the current toolchain tag and the available variants — no more silent ABI mismatches that only explode at link time.

**Strict mode** (`[project].precompiled_strict = true`, 1.2.0-dev.10+): L2/L1 fallback becomes a **fail-fast error**. Official-repo packages can use this to enforce "refuse to link without a matching toolchain".

> **Why extend the naming convention?** The 1.1.0 `os-arch` tag deliberately omitted the toolchain, on the grounds that "a prebuilt archive is not bound to a specific compiler family" — true for the **C ABI**, but not for the **C++ ABI**: GCC/Clang/MSVC are mutually ABI-incompatible, and the same platform+arch can still fail to link across toolchains/ABIs. `os-arch[-compiler][-abi]` lets package authors split artifacts per toolchain/ABI, and the consumer picks with ABI-safe priority, warning explicitly on fallback. The bare `lib<name>.a` fallback is kept, so older single-platform packages keep working.

> **Known limitations**: Apple Clang version numbers do not align with LLVM (ABI can shift within the same major); clang-cl does not produce `msvc1xx` tags (use real MSVC for MSVC-ABI artifacts); explicit `-D_GLIBCXX_USE_CXX11_ABI=0` (old ABI) consumer builds are not auto-detected — package authors can ship a dedicated `abi8` artifact for that scenario, matched by default as `abi11`.

**⚠️ Not recommended for general use.** Precompiled packages only work on the specific platform, architecture, toolchain, and ABI they were built for. Prefer source-based packages (`src/`) whenever possible, as they compile on any platform. Only use `precompiled` when:

- The library cannot be compiled with a simple `gcc`/`g++` invocation (requires CMake, autotools, OpenSSL's `Configure`, or other complex build systems)
- The library takes extremely long to compile (e.g. gRPC, Qt) — precompilation dramatically improves user experience
- You can enumerate and provide builds for all target platforms

**Four compatibility dimensions** (a prebuilt artifact must match the consumer on all four):

1. **Operating system + instruction set architecture**: `win-x64` ≠ `linux-x64` — the most basic layer.
2. **Compiler family**: GCC / Clang / MSVC artifacts are mutually incompatible (C++ ABIs differ; pure C ABI is the exception).
3. **Toolchain version / standard-library ABI**:
   - libstdc++: `_GLIBCXX_USE_CXX11_ABI` (`abi11` new / `abi8` old) — mixing causes `std::__cxx11` undefined references;
   - MSVC toolset: `msvc143` (VS2022) ≠ `msvc142` (VS2019);
   - Apple Clang / clang-cl version misalignment limitations (see "Known limitations" above).
4. **MSVC runtime (/MD vs /MT)**: a static library's CRT binding must match the consumer (dev.10 does not add a runtime-dimension tag yet).

**Failure case** (the real narrative from the dev.10 background):

> The docs did say "platform and architecture". But as a developer who has been burned by the C++ ABI countless times, the instinctive reading of "platform and architecture" is: "oh, a .a built on Windows just can't be used on Linux". So you confidently drop a `.a` built with GCC 11 into the repo and have a colleague link it with GCC 13 — the linker spews `std::__cxx11` undefined references and you debug for a whole day.

**Best practice (precompiled packages only)**:

> Place multiple toolchain/ABI artifacts side by side within one package using `os-arch[-compiler][-abi]` names, and let `ezmk` auto-select for the current toolchain (see the naming convention and 4-level matching above). **Source distribution (`src/`) is still far better than precompiled** — precompiled artifacts only work on the platforms/toolchains/ABIs you declared; source packages compile everywhere.

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

> **Why review-before-run?** Install scripts execute arbitrary code on the user's
> machine, so they are shown for inspection first and can be skipped or overridden —
> the same "show before trust" stance as the rest of the security model.

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

> **Why is `sha256` strongly recommended?** The hash lets `ezmk pkg install`
> verify the archive is exactly what the index promises before anything is
> extracted — the official repository's CI recomputes it, so a tampered or stale
> archive fails validation.

### 6.3 Computing SHA-256

```bash
# Linux/macOS:
sha256sum mypkg-1.0.0.tar.gz

# Windows PowerShell:
Get-FileHash -Algorithm SHA256 mypkg-1.0.0.tar.gz
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
- **[`safety.md`](safety.md)** — Security model (SHA-256, sandbox)
