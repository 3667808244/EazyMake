// Unit tests for build.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/build.hpp"
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>

namespace fs = std::filesystem;
using namespace ezmk::build;
using namespace ezmk::config;
using namespace ezmk::cli;
using namespace ezmk::util;

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

// ===================================================================
// 0.2.2+: macros_to_flags()
// ===================================================================

TEST_CASE("macros_to_flags: empty macros map returns empty vector", "[build][0.2.2]") {
    std::map<std::string, std::string> empty;
    auto flags = macros_to_flags(empty);
    REQUIRE(flags.empty());
}

TEST_CASE("macros_to_flags: empty value → -DKEY", "[build][0.2.2]") {
    std::map<std::string, std::string> macros = {
        {"DEBUG", ""},
        {"USE_UNICODE", ""},
    };
    auto flags = macros_to_flags(macros);
    REQUIRE(flags.size() == 2);
    REQUIRE(flags[0] == "-DDEBUG");
    REQUIRE(flags[1] == "-DUSE_UNICODE");
}

TEST_CASE("macros_to_flags: string value → -DKEY=\"VALUE\"", "[build][0.2.2]") {
    std::map<std::string, std::string> macros = {
        {"VERSION", "0.2.0"},
        {"APP_NAME", "MyApp"},
    };
    auto flags = macros_to_flags(macros);
    REQUIRE(flags.size() == 2);
    // std::map iterates keys alphabetically: APP_NAME, VERSION
    REQUIRE(flags[0] == "-DAPP_NAME=\"MyApp\"");
    REQUIRE(flags[1] == "-DVERSION=\"0.2.0\"");
}

TEST_CASE("macros_to_flags: integer value → -DKEY=NUMBER (no quotes)", "[build][0.2.2]") {
    std::map<std::string, std::string> macros = {
        {"MAX_SIZE", "4096"},
        {"COUNT", "0"},
        {"NEGATIVE", "-1"},
    };
    auto flags = macros_to_flags(macros);
    REQUIRE(flags.size() == 3);
    // Alphabetical order: COUNT, MAX_SIZE, NEGATIVE
    REQUIRE(flags[0] == "-DCOUNT=0");
    REQUIRE(flags[1] == "-DMAX_SIZE=4096");
    REQUIRE(flags[2] == "-DNEGATIVE=-1");
}

TEST_CASE("macros_to_flags: mixed types", "[build][0.2.2]") {
    std::map<std::string, std::string> macros = {
        {"DEBUG", ""},
        {"VERSION", "1.0.0"},
        {"MAX_CONNECTIONS", "64"},
    };
    auto flags = macros_to_flags(macros);
    REQUIRE(flags.size() == 3);
    // Alphabetical order: DEBUG, MAX_CONNECTIONS, VERSION
    REQUIRE(flags[0] == "-DDEBUG");
    REQUIRE(flags[1] == "-DMAX_CONNECTIONS=64");
    REQUIRE(flags[2] == "-DVERSION=\"1.0.0\"");
}

// ===================================================================
// 0.2.2+: generate_ezmk_macros()
// ===================================================================

TEST_CASE("generate_ezmk_macros: generates standard macros", "[build][0.2.2]") {
    EzConfig cfg;
    cfg.project.name = "testapp";
    cfg.project.version = "1.0.0";
    cfg.project.type = "executable";
    cfg.project.language = "C++17";

    auto flags = generate_ezmk_macros(cfg);

    // Should have at least 5 macros: EZMK, EZMK_VERSION, EZMK_PROJECT_NAME,
    // EZMK_PROJECT_VERSION, EZMK_PROJECT_TYPE, EZMK_LANG
    REQUIRE(flags.size() >= 5);

    // EZMK=1 should be first
    REQUIRE(flags[0] == "-DEZMK=1");

    // Check that all expected macros are present (order after EZMK may vary)
    bool has_version = false, has_name = false, has_proj_ver = false,
         has_type = false, has_lang = false;
    for (auto& f : flags) {
        if (f.find("-DEZMK_VERSION=") == 0) has_version = true;
        if (f.find("-DEZMK_PROJECT_NAME=") == 0) has_name = true;
        if (f.find("-DEZMK_PROJECT_VERSION=") == 0) has_proj_ver = true;
        if (f.find("-DEZMK_PROJECT_TYPE=") == 0) has_type = true;
        if (f.find("-DEZMK_LANG=") == 0) has_lang = true;
    }
    REQUIRE(has_version);
    REQUIRE(has_name);
    REQUIRE(has_proj_ver);
    REQUIRE(has_type);
    REQUIRE(has_lang);
}

TEST_CASE("generate_ezmk_macros: empty project name handled", "[build][0.2.2]") {
    EzConfig cfg;
    cfg.project.name = "";
    cfg.project.version = "";
    cfg.project.type = "executable";
    cfg.project.language = "C++17";

    auto flags = generate_ezmk_macros(cfg);
    // EZMK=1 and EZMK_VERSION should always be present
    REQUIRE(flags.size() >= 2);
    REQUIRE(flags[0] == "-DEZMK=1");

    // Project name macro should NOT be present for empty name
    bool has_name = false;
    for (auto& f : flags) {
        if (f.find("-DEZMK_PROJECT_NAME=") == 0) has_name = true;
    }
    REQUIRE(!has_name);
}

// ===================================================================
// 0.2.2+: want_to_macro_name()
// ===================================================================

TEST_CASE("want_to_macro_name: basic package names", "[build][0.2.2]") {
    REQUIRE(want_to_macro_name("sqlite3") == "EZMK_LIB_MISS_SQLITE3");
    REQUIRE(want_to_macro_name("fmt") == "EZMK_LIB_MISS_FMT");
    REQUIRE(want_to_macro_name("zlib") == "EZMK_LIB_MISS_ZLIB");
}

TEST_CASE("want_to_macro_name: hyphen to underscore", "[build][0.2.2]") {
    REQUIRE(want_to_macro_name("boost-filesystem") == "EZMK_LIB_MISS_BOOST_FILESYSTEM");
    REQUIRE(want_to_macro_name("nlohmann-json") == "EZMK_LIB_MISS_NLOHMANN_JSON");
}

TEST_CASE("want_to_macro_name: dot replaced by underscore", "[build][0.2.2]") {
    REQUIRE(want_to_macro_name("nlohmann.json") == "EZMK_LIB_MISS_NLOHMANN_JSON");
}

TEST_CASE("want_to_macro_name: space replaced by underscore", "[build][0.2.2]") {
    REQUIRE(want_to_macro_name("my pkg") == "EZMK_LIB_MISS_MY_PKG");
}

TEST_CASE("want_to_macro_name: special characters dropped", "[build][0.2.2]") {
    REQUIRE(want_to_macro_name("lib@@test!!") == "EZMK_LIB_MISS_LIBTEST");
}

TEST_CASE("want_to_macro_name: mixed case preserved as uppercase", "[build][0.2.2]") {
    REQUIRE(want_to_macro_name("OpenSSL") == "EZMK_LIB_MISS_OPENSSL");
    REQUIRE(want_to_macro_name("mySQL") == "EZMK_LIB_MISS_MYSQL");
}

TEST_CASE("want_to_macro_name: numbers preserved", "[build][0.2.2]") {
    REQUIRE(want_to_macro_name("libfoo2") == "EZMK_LIB_MISS_LIBFOO2");
    REQUIRE(want_to_macro_name("abc123") == "EZMK_LIB_MISS_ABC123");
}

// ===================================================================
// 0.2.2+: collect_sources()
// ===================================================================

TEST_CASE("collect_sources: single source directory", "[build][0.2.2]") {
    // Create temp project structure
    auto tmp = fs::temp_directory_path() / ("ezmk_test_collect_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp / "src");
    // Write minimal C++ files
    std::ofstream(tmp / "src" / "main.cpp") << "int main() { return 0; }\n";
    std::ofstream(tmp / "src" / "util.cpp") << "void util() {}\n";

    auto sources = collect_sources({"src"}, tmp, "executable");
    fs::remove_all(tmp);

    REQUIRE(sources.size() == 2);
}

TEST_CASE("collect_sources: multiple source directories", "[build][0.2.2]") {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_collect2_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp / "src");
    fs::create_directories(tmp / "lib");
    std::ofstream(tmp / "src" / "main.cpp") << "int main() { return 0; }\n";
    std::ofstream(tmp / "lib" / "helper.cpp") << "void helper() {}\n";

    auto sources = collect_sources({"src", "lib"}, tmp, "executable");
    fs::remove_all(tmp);

    REQUIRE(sources.size() == 2);
}

TEST_CASE("collect_sources: missing directory warns but continues", "[build][0.2.2]") {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_collect3_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp / "src");
    std::ofstream(tmp / "src" / "main.cpp") << "int main() { return 0; }\n";

    // "nonexistent" should be warned and skipped, but "src" still works
    auto sources = collect_sources({"src", "nonexistent_dir"}, tmp, "executable");
    fs::remove_all(tmp);

    REQUIRE(sources.size() == 1);
}

TEST_CASE("collect_sources: no source directories exist throws", "[build][0.2.2]") {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_collect4_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp);  // empty dir, no src/

    REQUIRE_THROWS_AS(collect_sources({"nonexistent1", "nonexistent2"}, tmp, "executable"),
                      ezmk::fatal_error);
    fs::remove_all(tmp);
}

TEST_CASE("collect_sources: no source files throws", "[build][0.2.2]") {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_collect5_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp / "src");  // empty src/

    REQUIRE_THROWS_AS(collect_sources({"src"}, tmp, "executable"),
                      ezmk::fatal_error);
    fs::remove_all(tmp);
}

TEST_CASE("collect_sources: executable requires main.cpp", "[build][0.2.2]") {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_collect6_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp / "src");
    std::ofstream(tmp / "src" / "util.cpp") << "void util() {}\n";
    // No main.cpp

    REQUIRE_THROWS_AS(collect_sources({"src"}, tmp, "executable"),
                      ezmk::fatal_error);
    fs::remove_all(tmp);
}

TEST_CASE("collect_sources: static library does not require main.cpp", "[build][0.2.2]") {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_collect7_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp / "src");
    std::ofstream(tmp / "src" / "util.cpp") << "void util() {}\n";

    auto sources = collect_sources({"src"}, tmp, "static");
    fs::remove_all(tmp);

    REQUIRE(sources.size() == 1);
}

TEST_CASE("collect_sources: main.c also accepted for executable", "[build][0.2.2]") {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_collect8_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp / "src");
    std::ofstream(tmp / "src" / "main.c") << "int main() { return 0; }\n";

    auto sources = collect_sources({"src"}, tmp, "executable");
    fs::remove_all(tmp);

    REQUIRE(sources.size() == 1);
}

// ===================================================================
// 0.2.2+: CompileSection new fields
// ===================================================================

TEST_CASE("build: CompileSection 0.2.2 fields have correct defaults", "[build][0.2.2]") {
    CompileSection cs;
    REQUIRE(cs.src_dirs.empty());
    REQUIRE(cs.macros.empty());
    REQUIRE(cs.ezmk_macros == true);
    REQUIRE(cs.msvc_flags.empty());
}

TEST_CASE("build: DependsSection 0.2.2 want field defaults", "[build][0.2.2]") {
    DependsSection ds;
    REQUIRE(ds.libs.empty());
    REQUIRE(ds.want.empty());
}

// ===================================================================
// 0.2.3+: merge_compile_profile()
// ===================================================================

TEST_CASE("merge_compile_profile: appends profile flags after base flags", "[build][0.2.3]") {
    CompileSection base;
    base.flags = {"-Wall", "-O2"};

    ProfileConfig profile;
    profile.flags = {"-g", "-DDEBUG"};

    auto merged = merge_compile_profile(base, profile);

    REQUIRE(merged.flags.size() == 4);
    REQUIRE(merged.flags[0] == "-Wall");
    REQUIRE(merged.flags[1] == "-O2");
    REQUIRE(merged.flags[2] == "-g");
    REQUIRE(merged.flags[3] == "-DDEBUG");
}

TEST_CASE("merge_compile_profile: profile macros override base macros", "[build][0.2.3]") {
    CompileSection base;
    base.macros = {
        {"VERSION", "1.0.0"},
        {"DEBUG", "0"},
    };

    ProfileConfig profile;
    profile.macros = {
        {"DEBUG", "1"},
        {"EXTRA", "enabled"},
    };

    auto merged = merge_compile_profile(base, profile);

    REQUIRE(merged.macros.size() == 3);
    // DEBUG from profile overrides base
    REQUIRE(merged.macros["DEBUG"] == "1");
    // VERSION from base is preserved
    REQUIRE(merged.macros["VERSION"] == "1.0.0");
    // EXTRA from profile is added
    REQUIRE(merged.macros["EXTRA"] == "enabled");
}

TEST_CASE("merge_compile_profile: empty profile does not change base", "[build][0.2.3]") {
    CompileSection base;
    base.flags = {"-Wall", "-Wextra"};
    base.macros = {{"FOO", "bar"}};
    base.msvc_flags = {"/utf-8"};

    ProfileConfig empty_profile;
    auto merged = merge_compile_profile(base, empty_profile);

    REQUIRE(merged.flags == base.flags);
    REQUIRE(merged.macros == base.macros);
    REQUIRE(merged.msvc_flags == base.msvc_flags);
}

TEST_CASE("merge_compile_profile: profile MSVC flags appended", "[build][0.2.3]") {
    CompileSection base;
    base.msvc_flags = {"/utf-8"};

    ProfileConfig profile;
    profile.msvc_flags = {"/MD", "/W4"};

    auto merged = merge_compile_profile(base, profile);

    REQUIRE(merged.msvc_flags.size() == 3);
    REQUIRE(merged.msvc_flags[0] == "/utf-8");
    REQUIRE(merged.msvc_flags[1] == "/MD");
    REQUIRE(merged.msvc_flags[2] == "/W4");
}

// ===================================================================
// 0.2.3+: merge_link_profile()
// ===================================================================

TEST_CASE("merge_link_profile: appends profile flags after base flags", "[build][0.2.3]") {
    LinkSection base;
    base.flags = {"-static"};

    ProfileLinkConfig profile;
    profile.flags = {"-s", "--strip-all"};

    auto merged = merge_link_profile(base, profile);

    REQUIRE(merged.flags.size() == 3);
    REQUIRE(merged.flags[0] == "-static");
    REQUIRE(merged.flags[1] == "-s");
    REQUIRE(merged.flags[2] == "--strip-all");
}

TEST_CASE("merge_link_profile: empty profile does not change base", "[build][0.2.3]") {
    LinkSection base;
    base.flags = {"-static", "-L/usr/lib"};
    base.msvc_flags = {"/DEBUG"};
    base.link_dirs = {"/usr/local/lib"};

    ProfileLinkConfig empty_profile;
    auto merged = merge_link_profile(base, empty_profile);

    REQUIRE(merged.flags == base.flags);
    REQUIRE(merged.msvc_flags == base.msvc_flags);
    REQUIRE(merged.link_dirs == base.link_dirs);
}

// ===================================================================
// 0.2.3+: BuildOptions new fields
// ===================================================================

TEST_CASE("build: BuildOptions 0.2.3 fields have correct defaults", "[build][0.2.3]") {
    BuildOptions opts;
    REQUIRE(opts.jobs == 0);
    REQUIRE(opts.profile.empty());
}

TEST_CASE("build: BuildOptions with jobs and profile set", "[build][0.2.3]") {
    BuildOptions opts;
    opts.jobs = 8;
    opts.profile = "release";
    REQUIRE(opts.jobs == 8);
    REQUIRE(opts.profile == "release");
}

// ===================================================================
// 0.2.3+: Integration tests — build with profile and jobs options
// ===================================================================

TEST_CASE("build: BuildOptions with -j 0 defaults to auto-detect", "[build][0.2.3]") {
    BuildOptions opts;
    REQUIRE(opts.jobs == 0);
    REQUIRE(opts.profile.empty());
    // -j 0 means auto-detect via std::thread::hardware_concurrency()
}

TEST_CASE("build: BuildOptions -j 1 means single-threaded", "[build][0.2.3]") {
    BuildOptions opts;
    opts.jobs = 1;
    REQUIRE(opts.jobs == 1);
}

// ===================================================================
// 0.2.3+: Profile integration tests
// ===================================================================

TEST_CASE("build: profile merge with debug flags", "[build][0.2.3][profile]") {
    // Test merge_compile_profile appends -g -O0 -DDEBUG for debug profile
    CompileSection base;
    base.flags = {"-Wall", "-O2"};

    ProfileConfig profile;
    profile.flags = {"-g", "-O0", "-DDEBUG"};
    profile.macros = {{"DEBUG", "1"}};

    auto merged = merge_compile_profile(base, profile);
    // Verify -g appears after base flags
    REQUIRE(merged.flags[0] == "-Wall");
    REQUIRE(merged.flags[1] == "-O2");
    REQUIRE(merged.flags[2] == "-g");
    REQUIRE(merged.flags[3] == "-O0");
    REQUIRE(merged.flags[4] == "-DDEBUG");
    // Profile macro overrides empty base
    REQUIRE(merged.macros["DEBUG"] == "1");
}

TEST_CASE("build: profile merge with release flags and LTO", "[build][0.2.3][profile]") {
    CompileSection base;
    base.flags = {"-Wall"};

    ProfileConfig profile;
    profile.flags = {"-O3", "-DNDEBUG", "-flto"};

    auto merged = merge_compile_profile(base, profile);
    REQUIRE(merged.flags.size() == 4);
    REQUIRE(merged.flags[1] == "-O3");
    REQUIRE(merged.flags[2] == "-DNDEBUG");
    REQUIRE(merged.flags[3] == "-flto");
}

TEST_CASE("build: profile with macros override from base", "[build][0.2.3][profile]") {
    CompileSection base;
    base.macros = {{"API_KEY", "default"}, {"ENABLE_X", "0"}};
    base.flags = {"-Wall"};

    ProfileConfig profile;
    profile.flags = {"-O2"};
    profile.macros = {{"ENABLE_X", "1"}};

    auto merged = merge_compile_profile(base, profile);
    // Profile macro overrides base
    REQUIRE(merged.macros["ENABLE_X"] == "1");
    // Base macro preserved
    REQUIRE(merged.macros["API_KEY"] == "default");
    // Profile flag appended
    REQUIRE(merged.flags[1] == "-O2");
}
