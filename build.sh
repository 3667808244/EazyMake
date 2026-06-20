#!/usr/bin/env bash
set -euo pipefail

# EazyMake build script
# Works on MSYS2 (Windows), Linux, and macOS with g++.
#
# Usage:
#   bash build.sh          # Normal build
#   bash build.sh -v       # Verbose (show full compile command and flags)

cd "$(dirname "$0")"

SRC="src/*.cpp src/vendor/*.c"
INCLUDES="-I include/ -I include/vendor/"
OUTPUT="build/ezmk"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17}"

# Parse flags
VERBOSE=false
for arg in "$@"; do
    case "$arg" in
        -v|--verbose) VERBOSE=true ;;
        -h|--help)
            echo "Usage: bash build.sh [-v|--verbose]"
            echo "  -v, --verbose  Show full compile command"
            exit 0
            ;;
        *) echo "Unknown flag: $arg"; exit 1 ;;
    esac
done

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

if $VERBOSE; then
    echo "=== Building EazyMake ==="
    echo "Compiler: $CXX"
    echo "Flags:    $CXXFLAGS"
    echo "Sources:  $SRC"
    echo "Includes: $INCLUDES"
    echo "Libs:     $LIBS"
    echo "LDFlags:  $LDFLAGS"
    echo "Output:   $OUTPUT"
    echo ""
fi

mkdir -p build

$CXX $CXXFLAGS $SRC $INCLUDES -o "$OUTPUT" $LIBS $LDFLAGS

echo "=== Build successful: $OUTPUT ==="
