# 1. Install & verify

## Install

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

> **Bare Windows (no MSYS2):** download the prebuilt `ezmk.exe` from the
> [GitHub Release](https://github.com/3667808244/EazyMake/releases) and put it on your `PATH`.

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
