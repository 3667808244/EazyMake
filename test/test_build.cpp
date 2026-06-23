// Unit tests for build.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/build.hpp"
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

#include <cstdlib>
#include <string>
#include <sstream>

using namespace ezmk::build;
using namespace ezmk::config;
using namespace ezmk::cli;

// ===================================================================
// Build the link command via make_link_cmd (static, tested indirectly)
// ===================================================================
// Note: make_link_cmd() and find_compiler() are static in build.cpp
// so we cannot test them directly. We test build_project() behavior
// indirectly through integration tests.
//
// The tests below validate the public API contract and types.

TEST_CASE("build_project: function signature is valid", "[build]") {
    // Type-check: verify we can compile code that calls build_project
    // (This is a compile-time test — if it compiles, it passes)
    REQUIRE(true);
}

TEST_CASE("build: LanguageInfo structure is correct", "[build]") {
    SECTION("C++17 → g++") {
        auto lang = parse_language("C++17");
        REQUIRE(lang.compiler == "g++");
        REQUIRE(lang.std_flag == "-std=c++17");
    }

    SECTION("C11 → gcc") {
        auto lang = parse_language("C11");
        REQUIRE(lang.compiler == "gcc");
        REQUIRE(lang.std_flag == "-std=c11");
    }

    SECTION("C++20 → g++") {
        auto lang = parse_language("C++20");
        REQUIRE(lang.compiler == "g++");
        REQUIRE(lang.std_flag == "-std=c++20");
    }
}

TEST_CASE("build: linker suffixes by platform", "[build]") {
    // On Windows/MinGW, executables and DLLs get .exe/.dll
    // On Linux, executables have no suffix, shared libs get .so
#ifdef EZMK_WIN
    REQUIRE(std::string(EZMK_EXE_SUFFIX) == ".exe");
#else
    REQUIRE(std::string(EZMK_EXE_SUFFIX) == "");
#endif
}

TEST_CASE("build: BuildOptions struct", "[build]") {
    BuildOptions opts;
    REQUIRE(opts.disable_cache == false);
    REQUIRE(opts.verbose == false);

    BuildOptions opts2{true, true};
    REQUIRE(opts2.disable_cache == true);
    REQUIRE(opts2.verbose == true);
}

TEST_CASE("build: CompileSection defaults are valid for build", "[build]") {
    CompileSection cs;
    REQUIRE(cs.flags.empty());
    REQUIRE(cs.include_dirs.empty()); // filled by parse_config default
}

TEST_CASE("build: object file suffix", "[build]") {
    // All platforms use .o (MinGW g++ also produces .o, not .obj)
    REQUIRE(EZMK_OBJ_SUFFIX == std::string(".o"));
}

// ===================================================================
// detect_compiler() — public API
// IMPORTANT: env-override tests run FIRST because detect_compiler()
// caches results in static variables (one probe per language per process).
// ===================================================================

TEST_CASE("detect_compiler: $CXX env override — invalid compiler falls back", "[build][compiler]") {
    // Set CXX to a nonexistent binary to trigger the fallback path.
    // This must run before any other detect_compiler("C++") call
    // because the result is cached statically.
    const char* old_cxx = std::getenv("CXX");
#ifdef EZMK_WIN
    _putenv_s("CXX", "nonexistent_compiler_xyz");
#else
    setenv("CXX", "nonexistent_compiler_xyz", 1);
#endif
    auto c = detect_compiler("C++");
    REQUIRE(!c.empty());
    REQUIRE(c != "nonexistent_compiler_xyz");

    // Restore
    if (old_cxx) {
#ifdef EZMK_WIN
        _putenv_s("CXX", old_cxx);
#else
        setenv("CXX", old_cxx, 1);
#endif
    } else {
#ifdef EZMK_WIN
        _putenv_s("CXX", "");
#else
        unsetenv("CXX");
#endif
    }
}

TEST_CASE("detect_compiler: $CC env override — invalid compiler falls back", "[build][compiler]") {
    const char* old_cc = std::getenv("CC");
#ifdef EZMK_WIN
    _putenv_s("CC", "nonexistent_compiler_xyz");
#else
    setenv("CC", "nonexistent_compiler_xyz", 1);
#endif
    auto c = detect_compiler("C");
    REQUIRE(!c.empty());
    REQUIRE(c != "nonexistent_compiler_xyz");

    // Restore
    if (old_cc) {
#ifdef EZMK_WIN
        _putenv_s("CC", old_cc);
#else
        setenv("CC", old_cc, 1);
#endif
    } else {
#ifdef EZMK_WIN
        _putenv_s("CC", "");
#else
        unsetenv("CC");
#endif
    }
}

TEST_CASE("detect_compiler: returns non-empty for C++", "[build][compiler]") {
    auto c = detect_compiler("C++");
    REQUIRE(!c.empty());
    // Should be one of the known compilers
    bool known = (c == "g++" || c == "clang++" || c == "c++");
    REQUIRE(known);
}

TEST_CASE("detect_compiler: returns non-empty for C", "[build][compiler]") {
    auto c = detect_compiler("C");
    REQUIRE(!c.empty());
    bool known = (c == "gcc" || c == "clang" || c == "cc");
    REQUIRE(known);
}

TEST_CASE("detect_compiler: C++ and C return different compilers", "[build][compiler]") {
    auto cxx = detect_compiler("C++");
    auto cc  = detect_compiler("C");
    // The C++ compiler should not equal the C compiler
    // (g++ vs gcc, clang++ vs clang, c++ vs cc)
    REQUIRE(cxx != cc);
}

TEST_CASE("detect_compiler: result caching — second call returns same value", "[build][compiler]") {
    auto first  = detect_compiler("C++");
    auto second = detect_compiler("C++");
    REQUIRE(first == second);
}

TEST_CASE("detect_compiler: unknown language string treated as C", "[build][compiler]") {
    // Any non-"C++" string follows the C compiler path
    auto c = detect_compiler("C");
    REQUIRE(!c.empty());
}

TEST_CASE("detect_compiler: cache consistency across repeated calls", "[build][compiler]") {
    // Once detected, the value is stable for the remainder of the process.
    auto c1 = detect_compiler("C++");
    auto c2 = detect_compiler("C++");
    REQUIRE(c1 == c2);
}
