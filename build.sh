#!/usr/bin/env bash
set -euo pipefail

# EazyMake build script
# Works on MSYS2 (Windows), Linux, and macOS with g++.

cd "$(dirname "$0")"

SRC="src/*.cpp src/vendor/*.c"
INCLUDES="-I include/ -I include/vendor/"
OUTPUT="build/ezmk"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17}"

# Platform-specific settings
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # MSYS2 / Windows
        LIBS="-lwinhttp"
        LDFLAGS="-static"
        ;;
    Linux|Darwin)
        LIBS=""
        LDFLAGS="-static"
        ;;
    *)
        echo "Warning: unknown platform, trying generic build" >&2
        LIBS=""
        LDFLAGS=""
        ;;
esac

echo "=== Building EazyMake ==="
echo "Compiler: $CXX"
echo "Flags:    $CXXFLAGS"

$CXX $CXXFLAGS $SRC $INCLUDES -o "$OUTPUT" $LIBS $LDFLAGS

echo "=== Build successful: $OUTPUT ==="
