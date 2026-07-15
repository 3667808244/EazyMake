# FAQ / Troubleshooting

This page collects common problems and their solutions across installation, building, package management, configuration, and cross-platform usage. If you don't find your issue here, check the other docs in `docs/en/` or file an issue.

---

## Installation

### Q: `install.sh` fails with "curl: command not found"

**Cause**: `curl` is not installed on your system.

**Solution**:
1. **MSYS2**: `pacman -S curl`
2. **Linux (Debian/Ubuntu)**: `sudo apt install curl`
3. **Linux (Arch)**: `sudo pacman -S curl`
4. **macOS**: `brew install curl`

---

### Q: `install.sh` fails with "Permission denied"

**Cause**: You don't have write permission to the install directory (default: `~/ezmk/` on Windows, `/usr/local/bin/` on Linux/macOS).

**Solution**:
1. For a user-local install, pass a writable prefix: `bash install.sh --prefix ~/.local`
2. For a system-wide install on Linux/macOS, use `sudo bash install.sh`
3. On Windows, make sure you're running the terminal as a normal user (not administrator) if installing to your home directory

---

### Q: `build.sh` fails with "g++: command not found"

**Cause**: GCC/g++ is not installed or not in your PATH.

**Solution**:
1. **MSYS2**: `pacman -S mingw-w64-ucrt-x86_64-gcc`
2. **Linux (Debian/Ubuntu)**: `sudo apt install g++`
3. **Linux (Arch)**: `sudo pacman -S gcc`
4. **macOS**: `brew install gcc` (or use Apple Clang with `CXX=clang++ bash build.sh`)

---

### Q: Network errors when running `install.sh`

**Cause**: `install.sh` downloads ezmk and the default repo from GitHub. Firewalls, proxies, or network issues can block this.

**Solution**:
1. Check your internet connection
2. If behind a proxy, set `http_proxy` / `https_proxy` environment variables
3. For offline installation, see [Offline / Air-gapped Usage](#offline--air-gapped-usage)

---

## Building

### Q: "fatal error: xxx.h: No such file or directory"

**Cause**: The compiler can't find a header file in the configured include directories.

**Solution**:
1. Check `ezmk.toml` → `[compile]` → `include_dirs` includes the directory containing the header
2. Verify the header file name is spelled correctly (case-sensitive on Linux/macOS)
3. If the header comes from a dependency package, make sure you've installed it: `ezmk pkg install <name>`
4. For system headers, check that the compiler toolchain is properly installed
5. Try cleaning the cache: delete `.ezmk/cache/` and rebuild

---

### Q: "undefined reference to ..." (linker errors)

**Cause**: The linker can't find a function or symbol definition.

**Solution**:
1. Check that all `.cpp` / `.c` files are in directories listed in `[compile]` → `src_dirs`
2. If the symbol is from a library, add it to `[depends]` → `lib` in `ezmk.toml`
3. If the symbol is from a system library, add `-l<name>` to `[link]` → `flags`
4. Make sure the library is in one of the directories listed in `[link]` → `link_dirs`
5. For C libraries called from C++ code, make sure you have `extern "C"` wrappers

---

### Q: Build succeeds but the program crashes immediately

**Cause**: Common causes include mismatched compile/link flags, stale object files, or runtime dependency issues.

**Solution**:
1. Clean and rebuild: `ezmk project clean && ezmk project build`
2. Run with `--disable-cache` to force full recompilation
3. Check that `[compile]` → `flags` and `[link]` → `flags` are consistent (e.g., don't mix debug and release flags)
4. On Windows, check that required DLLs are in PATH

---

### Q: Cache seems corrupted — same source compiles differently each time

**Cause**: The build cache (`.ezmk/cache/`) may contain stale or corrupted entries.

**Solution**:
1. Delete the cache directory: `rm -rf .ezmk/cache/` (or `ezmk project clean`)
2. Rebuild: `ezmk project build`
3. If the problem persists, build with `--disable-cache --verbose` and inspect the compile commands

---

### Q: "ezmk.toml not found" when running build

**Cause**: You're not in a project directory, or the project was not initialized correctly.

**Solution**:
1. Make sure you're in the project root directory (where `ezmk.toml` lives)
2. If you don't have a project yet, create one: `ezmk project new <name>`
3. Check that `ezmk.toml` exists and is readable

---

### Q: Compile flags are not taking effect

**Cause**: Profile flags are appending to base flags, not replacing them. Or you're editing the wrong section.

**Solution**:
1. Base flags go in `[compile]` → `flags`; profile-specific flags go in `[compile.profile.<name>]` → `flags`
2. Profile flags **append** to base flags (they don't replace)
3. Make sure you're passing `--profile <name>` on the command line — profiles don't auto-apply
4. Use `--verbose` to see the actual compile commands being run

---

### Q: "src/ directory not found" but my sources are in a different directory

**Cause**: The default `src_dirs` is `["src"]`, but your sources are elsewhere.

**Solution**:
Add your source directories to `ezmk.toml`:
```toml
[compile]
src_dirs = ["src", "lib", "vendor"]
```

---

## Package Management

### Q: "Package not found: xxx"

**Cause**: The package name isn't in any registered repository's index.

**Solution**:
1. Check registered repos: `ezmk repo list`
2. If no repos are registered, add the default: `ezmk repo add https://github.com/3667808244/ezmk-repo.git`
3. Update repo indices: `ezmk repo update`
4. Search for the correct package name: `ezmk pkg search <keyword>`
5. For offline/manual install, download the archive and use `ezmk pkg install ./<file>.tar.gz --type file`

---

### Q: SHA-256 verification fails

**Cause**: The downloaded archive doesn't match the expected hash — possible corruption or tampering.

**Solution**:
1. Clear the download cache and retry: `ezmk repo update` then retry install
2. If using `--sha256 <hash>`, double-check the hash value
3. If the package was recently updated in the repo, the index may be stale — run `ezmk repo update`
4. As a last resort, delete the repo cache and re-clone: delete the repo directory under the relevant scope's cache, then `ezmk repo add` again

---

### Q: Circular dependency detected

**Cause**: Package A depends on B, which depends on A (directly or transitively).

**Solution**:
1. This is a packaging error in one of the packages — check which packages are involved from the error message
2. Report the issue to the package/repo maintainer
3. As a workaround, try to install one of the packages from source or a different repo

---

### Q: Global install fails with permission error

**Cause**: The global install directory requires elevated privileges.

**Solution**:
1. Use user-scope install instead: `ezmk pkg install -u <name>` (installs to `~/.local/ezmk/pkg/`)
2. Use project-scope install: `ezmk pkg install -p <name>` (installs to `.ezmk/pkg/`)
3. On Linux/macOS, run with `sudo` for global install if you must

---

### Q: Package installs successfully but headers are not found during build

**Cause**: The package's include directory isn't automatically added to your project.

**Solution**:
1. Make sure the package is listed in `[depends]` → `lib` in your project's `ezmk.toml`
2. After installing, rebuild your project — ezmk automatically discovers includes from dependencies
3. Check that the package scope matches: if you installed with `-u` (user), make sure the build can find user-scope packages

---

### Q: `pkg update` says "no updates available" but I know there's a newer version

**Cause**: The repo index may be stale, or the newer version is in a different repo.

**Solution**:
1. Update repo indices: `ezmk repo update`
2. Check which repos are registered: `ezmk repo list`
3. Add the repo containing the newer version if needed

---

## Configuration

### Q: TOML syntax error in ezmk.toml

**Cause**: TOML is strict about syntax — common mistakes include wrong quote types, missing equals signs, or invalid table nesting.

**Solution**:
1. Use double quotes for string values: `name = "my-project"`, not `name = 'my-project'`
2. Section headers use `[brackets]`: `[compile]`, not `(compile)`
3. Inline tables use `{key = value}` with equals signs, not colons
4. Boolean values are lowercase: `true` / `false`
5. Arrays use square brackets: `src_dirs = ["src", "lib"]`
6. Check for trailing commas (not allowed in TOML arrays)

---

### Q: "invalid project type" error

**Cause**: The `type` field in `[project]` has an unrecognized value.

**Solution**:
Valid types are:
- `"executable"` — produces an executable binary
- `"static"` — produces a static library (`.a` / `.lib`)
- `"shared"` — produces a shared library (`.dll` / `.so` / `.dylib`)
- `"utils"` — a tools package providing Lua-based utilities

---

### Q: "invalid language format" error

**Cause**: The `language` field must follow the `<Lang><Version>` format.

**Solution**:
Valid examples: `"C++17"`, `"C++20"`, `"C++23"`, `"C11"`, `"C17"`, `"C23"`, `"C99"`.
Do not use standalone names like `"C"` or `"C++"` — include the version number.

---

### Q: Profile is not being applied

**Cause**: Profiles don't auto-apply — you must pass `--profile <name>` explicitly.

**Solution**:
```bash
ezmk project build --profile debug
ezmk project run --profile release
```

---

### Q: "macro name invalid" error

**Cause**: A macro name in `[compile.macros]` is not a valid C identifier.

**Solution**:
Valid macro names match `[A-Za-z_][A-Za-z0-9_]*`. Examples:
- ✅ `ENABLE_FEATURE`, `BUFFER_SIZE`, `_DEBUG`
- ❌ `123abc`, `my-macro`, `enable feature`

---

## Cross-platform

### Q: Build works on Linux but fails on Windows

**Cause**: Platform-specific API usage, path separators, or compiler differences.

**Solution**:
1. Use forward slashes (`/`) in paths in `ezmk.toml` — ezmk normalizes them on all platforms
2. Use `#ifdef _WIN32` guards for Windows-specific code
3. Check that MSYS2/MinGW toolchain has all required libraries
4. For MSVC builds, use `[compile]` → `msvc_flags` for MSVC-specific flags
5. Make sure you're using portable system APIs (e.g., `std::filesystem` instead of POSIX-only calls)

---

### Q: MSVC flags are being ignored on Linux (or GCC flags are ignored on Windows with MSVC)

**Cause**: `msvc_flags` are only used when MSVC is detected; regular `flags` are translated to MSVC equivalents when using MSVC.

**Solution**:
1. Put GCC/Clang flags in `[compile]` → `flags` — they are automatically translated for MSVC
2. Put MSVC-only flags in `[compile]` → `msvc_flags` — they are silently ignored on non-MSVC toolchains
3. Use `--verbose` to see the actual flags being passed to the compiler

---

### Q: `-lwinhttp` linker error on Linux/macOS

**Cause**: `-lwinhttp` is a Windows-only library.

**Solution**:
If you're building ezmk itself from source on Linux/macOS, use the manual build command without `-lwinhttp`:
```bash
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/ezmk -static
```

---

## Offline / Air-gapped Usage

### Q: How do I use ezmk without internet access?

**Solution**: Three approaches:

**1. Local repository mirror**
```bash
# On a machine with internet, clone the repo
git clone https://github.com/3667808244/ezmk-repo.git /path/to/ezmk-repo

# Copy to the offline machine, then register as a local repo
ezmk repo add /path/to/ezmk-repo --type local
```

**2. Manual package download and install**
```bash
# Download the .tar.gz archive from GitHub Releases on a connected machine
# Transfer to the offline machine, then:
ezmk pkg install ./<pkg>-<version>.tar.gz --type file
```

**3. Pre-staged mirror on USB / network share**
```bash
# Prepare a full repo mirror on USB or a network share
git clone https://github.com/3667808244/ezmk-repo.git /mnt/usb/ezmk-repo

# On each offline machine
ezmk repo add /mnt/usb/ezmk-repo --type local
```

### Q: How do I install ezmk itself offline?

**Solution**:
1. Download the release binary from [GitHub Releases](https://github.com/3667808244/EazyMake/releases) on a connected machine
2. Copy the binary to the offline machine
3. Place it in a directory in your PATH
4. For packages, use one of the offline package methods above

---

## Lua / Utils

### Q: "unknown utils command" error

**Cause**: No installed package provides a tool with that name.

**Solution**:
1. Check which utils are available: look in `.ezmk/pkg/*/utils/`, `~/.local/ezmk/pkg/*/utils/`, and `<ezmk_install>/pkg/*/utils/`
2. Install a utils package: `ezmk pkg install example-utils`
3. Make sure the package has `type = "utils"` and lists the tool name in `[utils].tools`

---

### Q: Lua script fails with "permission denied"

**Cause**: The package's `[utils.permissions]` configuration restricts the requested access.

**Solution**:
1. Check the package's `ezmk.toml` for `[utils.permissions]` settings
2. Add the denied path/command to the appropriate allowlist (`read`, `write`, or `run`)
3. If the package has no `[utils.permissions]` section, it operates in legacy (unrestricted) mode — the denial is from ezmk's hard sandbox limits (e.g., writing outside project root)

---

### Q: How do I check the Lua API version for compatibility?

**Solution**:
In your Lua script, read `ezmk.api_version`:
```lua
if ezmk.api_version >= 2 then
    -- use newer API
else
    -- fallback for older ezmk
end
```
See [Utils Tool System](utils.md) for the full API reference and versioning policy.
