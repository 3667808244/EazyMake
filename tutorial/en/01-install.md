# 1. Install & verify

## Install

### Linux / macOS / MSYS2

On Linux, macOS, or MSYS2 (Windows), install with one line:

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

This builds `ezmk` from source and installs it to `$HOME/.local/bin`. To install
somewhere else:

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | PREFIX=/usr/local bash
```

Prefer to read the script first (recommended for any `curl | bash`):

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh -o install.sh
less install.sh
bash install.sh
```

### Windows (native, no MSYS2)

Download and run the PowerShell installer — no compiler or git required:

```powershell
# Recommended: review then run
# 1. Open https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1
# 2. Save as install.ps1, then run:
.\install.ps1

# Or one-line (convenient, but review first):
irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex
```

This downloads the prebuilt `ezmk.exe` from GitHub Releases, verifies SHA-256,
installs to `%LOCALAPPDATA%\ezmk\bin`, and configures your user `PATH`.
Customize with parameters:

```powershell
.\install.ps1 -Version "0.9.5"           # Install a specific version
.\install.ps1 -InstallDir "D:\tools\ezmk" # Custom install directory
.\install.ps1 -DryRun                     # Preview without making changes
.\install.ps1 -NoPath                     # Skip PATH configuration
```

> **MSYS2 users:** use the `install.sh` method above (builds from source with g++).

## Requirements

The installer checks these for you:

- `git`, `bash`
- A C++17 compiler (`g++` or `clang++`)
- `python3` (build-only; if missing, the UI falls back to English — the build still works)

## Verify

```bash
$ ezmk version
EazyMake 0.9.0
```

If the command is not found, `$HOME/.local/bin` may not be on your `PATH`. Add it:

```bash
export PATH="$HOME/.local/bin:$PATH"   # put this in ~/.bashrc or ~/.zshrc
```

## Language & color

- Set the UI language with `EZMK_LANG=zh` or `EZMK_LANG=en`.
- Force or disable ANSI color with `--color=always` / `--color=never` (default `auto`).

```bash
$ EZMK_LANG=zh ezmk help    # help text in Chinese
```

Next: [Your first project →](02-first-project.md)
