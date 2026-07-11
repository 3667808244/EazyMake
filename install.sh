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

# Platform / binary name.
case "$(uname -s)" in
    Linux)        PLATFORM=linux;  BIN_NAME=ezmk ;;
    Darwin)       PLATFORM=macos;  BIN_NAME=ezmk ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM=windows; BIN_NAME=ezmk.exe ;;
    *)            PLATFORM=unknown; BIN_NAME=ezmk
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

# --------------------------------------------------------------- install ----
DEST_DIR="$PREFIX/bin"
DEST="$DEST_DIR/$BIN_NAME"
mkdir -p "$DEST_DIR"
# Install atomically: copy to a temp name in the target dir, then mv into place.
TMP_DEST="$DEST_DIR/.ezmk.install.$$"
cp "$BUILT_BIN" "$TMP_DEST"
chmod 755 "$TMP_DEST"
mv -f "$TMP_DEST" "$DEST"
info "Installed: $DEST"

# ------------------------------------------------- bundled utils packages ---
# Global-scope packages live next to the binary in <exe_dir>/pkg. Ship the
# built-in ezmk-cc tool so `ezmk utils cc` works out of the box.
if [ -d "$SRC_DIR/pkg" ]; then
    mkdir -p "$DEST_DIR/pkg"
    cp -R "$SRC_DIR/pkg/." "$DEST_DIR/pkg/"
    info "Installed bundled packages to $DEST_DIR/pkg"
fi

# ----------------------------------------------------- zsh completions ------
if [ "${EZMK_NO_COMPLETIONS:-}" != "1" ] && need zsh; then
    COMP_SRC="$SRC_DIR/completions/_ezmk"
    if [ -f "$COMP_SRC" ]; then
        COMP_DIR="$HOME/.zsh/completions"
        mkdir -p "$COMP_DIR"
        cp "$COMP_SRC" "$COMP_DIR/_ezmk"
        ZSHRC="$HOME/.zshrc"
        touch "$ZSHRC"
        # Idempotent: only append the fpath/compinit lines if not already present.
        if ! grep -q 'zsh/completions' "$ZSHRC" 2>/dev/null; then
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
