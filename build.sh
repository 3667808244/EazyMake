#!/usr/bin/env bash
set -euo pipefail

# EazyMake build script
# Works on MSYS2 (Windows), Linux, and macOS with g++.
#
# Usage:
#   bash build.sh                # Normal build
#   bash build.sh test           # Build and run tests
#   bash build.sh test -v        # Build and run tests (verbose)
#   bash build.sh -v             # Verbose (show full compile command and flags)

cd "$(dirname "$0")"

SRC="src/*.cpp src/vendor/*.c"
# Exclude main.cpp for tests (it has its own main() function; catch2_impl.cpp provides main)
TEST_SRC="src/cache.cpp src/cli.cpp src/config.cpp src/crypto.cpp src/pkg.cpp src/project.cpp src/repo.cpp src/util.cpp src/vendor/*.c src/vendor/catch2_impl.cpp"
INCLUDES="-I include/ -I include/vendor/"
OUTPUT="build/ezmk"
TEST_OUTPUT="build/test_ezmk"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17}"

# Parse flags
VERBOSE=false
BUILD_TEST=false
RUN_TEST=false
for arg in "$@"; do
    case "$arg" in
        -v|--verbose) VERBOSE=true ;;
        test)
            BUILD_TEST=true
            RUN_TEST=true
            ;;
        -h|--help)
            echo "Usage: bash build.sh [test] [-v|--verbose]"
            echo "  test            Build and run test suite"
            echo "  -v, --verbose   Show full compile command"
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

mkdir -p build

if $BUILD_TEST; then
    echo "=== Building EazyMake test suite ==="
    if $VERBOSE; then
        echo "Compiler: $CXX"
        echo "Flags:    $CXXFLAGS"
        echo "TestSrc:  $TEST_SRC"
        echo "Includes: $INCLUDES"
        echo "Libs:     $LIBS"
        echo "LDFlags:  $LDFLAGS"
        echo "Output:   $TEST_OUTPUT"
        echo ""
    fi

    $CXX $CXXFLAGS test/test_*.cpp $TEST_SRC $INCLUDES -o "$TEST_OUTPUT" $LIBS $LDFLAGS

    echo "=== Test build successful: $TEST_OUTPUT ==="

    if $RUN_TEST; then
        echo ""
        echo "=== Running tests ==="
        ./"$TEST_OUTPUT" --verbosity high || true
        echo ""
        echo "=== Tests complete ==="
    fi
else
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

    $CXX $CXXFLAGS $SRC $INCLUDES -o "$OUTPUT" $LIBS $LDFLAGS

    echo "=== Build successful: $OUTPUT ==="
fi
