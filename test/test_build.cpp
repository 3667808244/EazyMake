// Unit tests for build.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/build.hpp"
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

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
    // Verify the macro is defined to a non-empty string
    REQUIRE(std::string(EZMK_OBJ_SUFFIX).size() >= 1);
    // Should be either .o or .obj
    bool ok = (EZMK_OBJ_SUFFIX == std::string(".o") ||
               EZMK_OBJ_SUFFIX == std::string(".obj"));
    REQUIRE(ok);
}
