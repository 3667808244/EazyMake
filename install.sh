#!/usr/bin/env bash
set -euo pipefail

# EazyMake one-line installer
#
#   curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
#
# Builds ezmk from source and installs it to $PREFIX/bin. Works on Linux, macOS,
# and MSYS2 (Windows). Bare Windows (non-MSYS2) users: download the prebuilt
# ezmk.exe from the GitHub Release instead.
#
# Configurable via environment variables (no interactive prompts, so it is safe
# to pipe from curl):
#   PREFIX               Install prefix.        Default: $HOME/.local
#   EZMK_REF             git tag/branch/commit. Default: repository default branch
#   EZMK_VERSION         Version string baked into the binary.
#                        Default: git describe --tags --always
#   EZMK_NO_COMPLETIONS  Set to 1 to skip zsh completion installation.
#   EZMK_NO_DEFAULT_REPO Set to 1 to skip official repo pre-registration.
#   CXX / CC / CXXFLAGS  Passed through to build.sh (compiler override).
#
# Prefer to review before running:
#   curl -fsSL <url> -o install.sh; less install.sh; bash install.sh

REPO_URL="https://github.com/3667808244/EazyMake.git"
PREFIX="${PREFIX:-$HOME/.local}"

# ------------------------------------------------------------------ logging ---
info()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
die()   { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

# ------------------------------------------------------------- environment ---
need() { command -v "$1" >/dev/null 2>&1; }

info "Checking build environment"
need git  || die "git is required but not found. Install git and re-run."
need bash || die "bash is required but not found."

CXX_BIN="${CXX:-}"
if [ -z "$CXX_BIN" ]; then
    if need g++;      then CXX_BIN=g++
    elif need clang++; then CXX_BIN=clang++
    else die "No C++ compiler found (need g++ or clang++, C++17). Install one and re-run."
    fi
fi
info "Using C++ compiler: $CXX_BIN"

if ! need python3 && ! need python; then
    warn "Python not found — locale data falls back to English only (build still works)."
fi

# Platform / binary name. ezmk-lua is the standalone Lua hook runtime that ships
# alongside ezmk (needed by exported CMake builds).
case "$(uname -s)" in
    Linux)        PLATFORM=linux;  BIN_NAME=ezmk;     LUA_BIN_NAME=ezmk-lua ;;
    Darwin)       PLATFORM=macos;  BIN_NAME=ezmk;     LUA_BIN_NAME=ezmk-lua ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM=windows; BIN_NAME=ezmk.exe; LUA_BIN_NAME=ezmk-lua.exe ;;
    *)            PLATFORM=unknown; BIN_NAME=ezmk;    LUA_BIN_NAME=ezmk-lua
                  warn "Unknown platform '$(uname -s)', attempting a generic build." ;;
esac

# --------------------------------------------------------------- work dir ---
WORK="$(mktemp -d "${TMPDIR:-/tmp}/ezmk-install.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
SRC_DIR="$WORK/EazyMake"

# ----------------------------------------------------------------- clone ----
info "Cloning EazyMake${EZMK_REF:+ (ref: $EZMK_REF)}"
if [ -n "${EZMK_REF:-}" ]; then
    git clone --depth 1 --branch "$EZMK_REF" "$REPO_URL" "$SRC_DIR" 2>/dev/null \
        || git clone "$REPO_URL" "$SRC_DIR"  # fall back for arbitrary commits
    ( cd "$SRC_DIR" && git checkout --quiet "$EZMK_REF" )
else
    git clone --depth 1 "$REPO_URL" "$SRC_DIR"
fi

# ----------------------------------------------------------------- build ----
RESOLVED_VERSION="${EZMK_VERSION:-$(cd "$SRC_DIR" && git describe --tags --always 2>/dev/null || echo unknown)}"
info "Building ezmk $RESOLVED_VERSION"
(
    cd "$SRC_DIR"
    EZMK_VERSION="$RESOLVED_VERSION" bash build.sh
)

BUILT_BIN="$SRC_DIR/build/$BIN_NAME"
[ -f "$BUILT_BIN" ] || die "Build did not produce $BIN_NAME (looked in $SRC_DIR/build/)."
BUILT_LUA_BIN="$SRC_DIR/build/$LUA_BIN_NAME"
[ -f "$BUILT_LUA_BIN" ] || die "Build did not produce $LUA_BIN_NAME (looked in $SRC_DIR/build/)."

# --------------------------------------------------------------- install ----
DEST_DIR="$PREFIX/bin"
DEST="$DEST_DIR/$BIN_NAME"
LUA_DEST="$DEST_DIR/$LUA_BIN_NAME"
mkdir -p "$DEST_DIR"
# Install atomically: copy to a temp name in the target dir, then mv into place.
TMP_DEST="$DEST_DIR/.ezmk.install.$$"
cp "$BUILT_BIN" "$TMP_DEST"
chmod 755 "$TMP_DEST"
mv -f "$TMP_DEST" "$DEST"
info "Installed: $DEST"
# ezmk-lua — standalone Lua hook runtime for exported CMake builds.
TMP_LUA_DEST="$DEST_DIR/.ezmk-lua.install.$$"
cp "$BUILT_LUA_BIN" "$TMP_LUA_DEST"
chmod 755 "$TMP_LUA_DEST"
mv -f "$TMP_LUA_DEST" "$LUA_DEST"
info "Installed: $LUA_DEST"

# ------------------------------------------------- built-in ezmk-cc ------
# ezmk-cc (`ezmk utils cc`) is now part of the ezmk-official-utils package (§8).
# The package is pre-installed below so users have cc + link + gen-build-package.
# We keep the pkg/ezmk-cc/ copy as a buildless fallback for offline installs.
if [ -d "$SRC_DIR/pkg/ezmk-cc" ]; then
    mkdir -p "$DEST_DIR/pkg/ezmk-cc"
    cp -R "$SRC_DIR/pkg/ezmk-cc/." "$DEST_DIR/pkg/ezmk-cc/"
    info "Installed built-in fallback: ezmk-cc"
fi

# ------------------------------------------------- default repo -----------
# Pre-register the official EazyMake package repository (user scope) so that
# `ezmk pkg install <name>` works without manual `ezmk repo add`.
# Set EZMK_NO_DEFAULT_REPO=1 to skip this.
OFFICIAL_REPO_URL="https://github.com/3667808244/ezmk-repo.git"
OFFICIAL_REPO_NAME="official"

if [ "${EZMK_NO_DEFAULT_REPO:-}" != "1" ]; then
    if "$DEST" repo add -u "$OFFICIAL_REPO_URL" --name "$OFFICIAL_REPO_NAME" 2>/dev/null; then
        info "Registered official repo ($OFFICIAL_REPO_NAME) to user scope"
        if "$DEST" repo update -u "$OFFICIAL_REPO_NAME" 2>/dev/null; then
            info "Updated official repo index"
        else
            warn "Could not update official repo (no network?); run 'ezmk repo update -u $OFFICIAL_REPO_NAME' later"
        fi
    else
        warn "Could not register official repo (it may already exist or network is unavailable)"
    fi
else
    info "EZMK_NO_DEFAULT_REPO=1 — skipping official repo registration"
fi

# ------------------------------------------- pre-install ezmk-official-utils --
# 1.1.0-dev.5: Pre-install official utils package (cc + link + gen-build-package).
# Uses -g (global scope) so all projects can use the tools.
# Failure is non-fatal — network may be unavailable (user can install later).
OFFICIAL_UTILS_PKG="ezmk-official-utils"
if "$DEST" pkg install -g "$OFFICIAL_UTILS_PKG" -y 2>/dev/null; then
    info "Pre-installed official utils package: $OFFICIAL_UTILS_PKG"
else
    warn "Could not pre-install $OFFICIAL_UTILS_PKG (no network?)."
    warn "Run 'ezmk pkg install -g $OFFICIAL_UTILS_PKG -y' later to get cc, link, and gen-build-package."
fi

# ----------------------------------------------------- zsh completions ------
if [ "${EZMK_NO_COMPLETIONS:-}" != "1" ] && need zsh; then
    COMP_SRC="$SRC_DIR/res/ezmk.zsh"
    if [ -f "$COMP_SRC" ]; then
        COMP_DIR="$HOME/.zsh/completions"
        mkdir -p "$COMP_DIR"
        cp "$COMP_SRC" "$COMP_DIR/_ezmk"
        # Honor ZDOTDIR: zsh sources $ZDOTDIR/.zshrc (defaults to $HOME/.zshrc).
        ZSHRC="${ZDOTDIR:-$HOME}/.zshrc"
        touch "$ZSHRC"
        # Idempotent: only append the fpath/compinit lines if OUR marker is absent.
        # Check the marker, NOT a bare "zsh/completions" substring — a comment or
        # unrelated line containing that string must not suppress the setup.
        if ! grep -qF '# Added by EazyMake installer' "$ZSHRC"; then
            {
                echo ''
                echo '# Added by EazyMake installer'
                echo 'fpath=(~/.zsh/completions $fpath)'
                echo 'autoload -Uz compinit && compinit'
            } >> "$ZSHRC"
            info "Installed zsh completions to $COMP_DIR and updated $ZSHRC"
        else
            info "Installed zsh completions to $COMP_DIR (fpath already configured)"
        fi
    fi
fi

# ----------------------------------------------------------------- done -----
echo
info "Done. Installed version:"
"$DEST" version || true

case ":$PATH:" in
    *":$DEST_DIR:"*) ;;
    *) echo
       warn "$DEST_DIR is not in your PATH."
       echo "  Add it, e.g.:  export PATH=\"$DEST_DIR:\$PATH\""
       echo "  (put that line in your shell rc: ~/.bashrc, ~/.zshrc, ...)" ;;
esac

if [ "${EZMK_NO_COMPLETIONS:-}" != "1" ] && need zsh; then
    echo "Restart your terminal or run: source ~/.zshrc"
fi
