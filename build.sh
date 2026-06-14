#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# EazyMake build script
# Produces a single statically-linked ezmk executable.
# ============================================================

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$PROJECT_ROOT/src"
INCLUDE_DIR="$PROJECT_ROOT/include"
VENDOR_INCLUDE="$INCLUDE_DIR/vendor"
VENDOR_SRC="$SRC_DIR/vendor"
BUILD_DIR="$PROJECT_ROOT/build"

# ---- platform detection ----
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        EXE_SUFFIX=".exe"
        OBJ_SUFFIX=".obj"
        PLATFORM_LIBS="-lwinhttp -lws2_32"
        ;;
    Linux|Darwin)
        EXE_SUFFIX=""
        OBJ_SUFFIX=".o"
        PLATFORM_LIBS=""
        ;;
    *)
        echo "Unsupported platform: $(uname -s)" >&2
        exit 1
        ;;
esac

OUTPUT="$BUILD_DIR/ezmk$EXE_SUFFIX"

# ---- compiler detection ----
CXX="${CXX:-g++}"
if ! command -v "$CXX" &>/dev/null; then
    echo "Error: $CXX not found. Install g++ (MSYS2: pacman -S mingw-w64-x86_64-gcc)." >&2
    exit 1
fi

# ---- flags ----
CXXFLAGS="-std=c++17 -Wall -Wextra -O2"
LDFLAGS="-static"

# Collect include paths that actually exist
INCLUDES=("-I$INCLUDE_DIR")
[ -d "$VENDOR_INCLUDE" ] && INCLUDES+=("-I$VENDOR_INCLUDE")

# ---- collect source files ----
SOURCES=()
for d in "$SRC_DIR" "$VENDOR_SRC"; do
    if [ -d "$d" ]; then
        for f in "$d"/*.cpp "$d"/*.c; do
            [ -f "$f" ] && SOURCES+=("$f")
        done
    fi
done

if [ ${#SOURCES[@]} -eq 0 ]; then
    echo "Error: no source files found in src/ or src/vendor/" >&2
    exit 1
fi

# ---- compile ----
mkdir -p "$BUILD_DIR"

echo "=== EazyMake Build ==="
echo "Compiler : $CXX"
echo "Sources  : ${SOURCES[*]}"
echo "Output   : $OUTPUT"
echo ""

set -x
"$CXX" $CXXFLAGS "${INCLUDES[@]}" "${SOURCES[@]}" $LDFLAGS $PLATFORM_LIBS -o "$OUTPUT"
set +x

echo ""
echo "Build successful: $OUTPUT"
