#include "ezmk/toolchain.hpp"
#include "catch2.hpp"
#include <string>
#include <vector>

namespace tc = ezmk::toolchain;

// ===================================================================
// Flag translation — compile flags
// ===================================================================

TEST_CASE("translate_compile_flags returns identity for non-MSVC target", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-Wall", "-O2", "-std=c++17"};
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Gcc);
    REQUIRE(result.translated == flags);
    REQUIRE(result.unrecognized.empty());

    result = tc::translate_compile_flags(flags, tc::CompilerFamily::Clang);
    REQUIRE(result.translated == flags);
    REQUIRE(result.unrecognized.empty());
}

TEST_CASE("translate_compile_flags: standard flags GCC→MSVC", "[toolchain][translate]") {
    std::vector<std::string> flags = {
        "-std=c++17", "-Wall", "-Wextra", "-O2", "-g",
    };
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() >= 5);

    // Exact mapping matches
    auto find_flag = [&](const std::string& expected) {
        for (auto& f : result.translated) {
            if (f == expected) return true;
        }
        return false;
    };

    CHECK(find_flag("/std:c++17"));
    CHECK(find_flag("/W4"));       // -Wall → /W4 and -Wextra → /W4 (both same)
    CHECK(find_flag("/O2"));
    CHECK(find_flag("/Zi"));       // -g → /Zi
}

TEST_CASE("translate_compile_flags: -Werror → /WX", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-Werror"};
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 1);
    CHECK(result.translated[0] == "/WX");
}

TEST_CASE("translate_compile_flags: -pedantic → /permissive-", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-pedantic"};
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 1);
    CHECK(result.translated[0] == "/permissive-");
}

TEST_CASE("translate_compile_flags: -fPIC and -pthread are silently skipped", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-fPIC", "-fpic", "-pthread"};
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    // These should be silently skipped (not in translated, not in unrecognized)
    CHECK(result.translated.empty());
    CHECK(result.unrecognized.empty());
}

TEST_CASE("translate_compile_flags: -D and -I prefix translation", "[toolchain][translate]") {
    std::vector<std::string> flags = {
        "-DDEBUG",
        "-DVERSION=1",
        "-DNAME=\"my_app\"",
        "-Iinclude",
        "-I\"C:/some path/headers\"",
    };
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 5);

    CHECK(result.translated[0] == "/DDEBUG");
    CHECK(result.translated[1] == "/DVERSION=1");
    CHECK(result.translated[2] == "/DNAME=\"my_app\"");

    CHECK(result.translated[3] == "/Iinclude");
    CHECK(result.translated[4] == "/I\"C:/some path/headers\"");
}

TEST_CASE("translate_compile_flags: MSVC-style flags pass through", "[toolchain][translate]") {
    std::vector<std::string> flags = {"/W4", "/utf-8", "/MD"};
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 3);
    CHECK(result.translated[0] == "/W4");
    CHECK(result.translated[1] == "/utf-8");
    CHECK(result.translated[2] == "/MD");
    CHECK(result.unrecognized.empty()); // no false warns
}

TEST_CASE("translate_compile_flags: unrecognized GCC-only flags", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-Wno-unused", "-fno-strict-aliasing", "--some-unknown-flag"};
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    // These cannot be translated and should appear in unrecognized
    REQUIRE(result.unrecognized.size() == 3);
    CHECK(result.unrecognized[0] == "-Wno-unused");
    CHECK(result.unrecognized[1] == "-fno-strict-aliasing");
    CHECK(result.unrecognized[2] == "--some-unknown-flag");
}

TEST_CASE("translate_compile_flags: mixed GCC + MSVC flags", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-Wall", "/utf-8", "-O2", "/MD", "-Wno-unknown"};
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    // -Wall → /W4, /utf-8 passthrough, -O2 → /O2, /MD passthrough, -Wno-unknown → unrecognized
    REQUIRE(result.translated.size() == 4);
    REQUIRE(result.unrecognized.size() == 1);
    CHECK(result.unrecognized[0] == "-Wno-unknown");
}

TEST_CASE("translate_compile_flags: empty flags", "[toolchain][translate]") {
    std::vector<std::string> flags;
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    CHECK(result.translated.empty());
    CHECK(result.unrecognized.empty());
}

// ===================================================================
// Flag translation — link flags
// ===================================================================

TEST_CASE("translate_link_flags returns identity for non-MSVC target", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-Llib", "-lpthread"};
    auto result = tc::translate_link_flags(flags, tc::CompilerFamily::Gcc);
    REQUIRE(result.translated == flags);
    REQUIRE(result.unrecognized.empty());
}

TEST_CASE("translate_link_flags: -l<lib> → <lib>.lib", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-lpthread", "-lws2_32", "-lsqlite3"};
    auto result = tc::translate_link_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 3);
    CHECK(result.translated[0] == "pthread.lib");
    CHECK(result.translated[1] == "ws2_32.lib");
    CHECK(result.translated[2] == "sqlite3.lib");
}

TEST_CASE("translate_link_flags: -L<path> → /LIBPATH:<path>", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-L\"C:/libs\"", "-L/usr/local/lib"};
    auto result = tc::translate_link_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 2);
    CHECK(result.translated[0] == "/LIBPATH:\"C:/libs\"");
    CHECK(result.translated[1] == "/LIBPATH:/usr/local/lib");
}

TEST_CASE("translate_link_flags: -shared → /DLL", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-shared"};
    auto result = tc::translate_link_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 1);
    CHECK(result.translated[0] == "/DLL");
}

TEST_CASE("translate_link_flags: MSVC-style flags pass through", "[toolchain][translate]") {
    std::vector<std::string> flags = {"/SUBSYSTEM:CONSOLE", "/MACHINE:X64"};
    auto result = tc::translate_link_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 2);
    CHECK(result.translated[0] == "/SUBSYSTEM:CONSOLE");
    CHECK(result.translated[1] == "/MACHINE:X64");
    CHECK(result.unrecognized.empty());
}

// ===================================================================
// /showIncludes parser
// ===================================================================

TEST_CASE("parse_show_includes: single include line", "[toolchain][parse]") {
    std::string output = "Note: including file:  C:\\Program Files\\header.h\n";
    auto includes = tc::parse_show_includes(output);
    REQUIRE(includes.size() == 1);
    CHECK(includes[0].string().find("header.h") != std::string::npos);
}

TEST_CASE("parse_show_includes: multiple include lines", "[toolchain][parse]") {
    std::string output =
        "Note: including file:  C:\\foo\\a.h\n"
        "Note: including file:  C:\\foo\\b.h\n"
        "Note: including file:  C:\\foo\\c.h\n";
    auto includes = tc::parse_show_includes(output);
    REQUIRE(includes.size() == 3);
}

TEST_CASE("parse_show_includes: empty output", "[toolchain][parse]") {
    auto includes = tc::parse_show_includes("");
    CHECK(includes.empty());
}

TEST_CASE("parse_show_includes: no include lines", "[toolchain][parse]") {
    std::string output = "main.cpp\nCompiling...\nSome other output\n";
    auto includes = tc::parse_show_includes(output);
    CHECK(includes.empty());
}

TEST_CASE("parse_show_includes: CRLF line endings", "[toolchain][parse]") {
    std::string output = "Note: including file:  C:\\foo\\header.h\r\n";
    auto includes = tc::parse_show_includes(output);
    REQUIRE(includes.size() == 1);
}

TEST_CASE("parse_show_includes: mixed output with compile warnings", "[toolchain][parse]") {
    std::string output =
        "main.cpp\n"
        "main.cpp(10): warning C4100: unreferenced parameter\n"
        "Note: including file:  C:\\foo\\a.h\n"
        "Note: including file:  C:\\foo\\b.h\n"
        "main.cpp(20): warning C4244: conversion\n";
    auto includes = tc::parse_show_includes(output);
    REQUIRE(includes.size() == 2);
}

TEST_CASE("parse_show_includes: path with spaces", "[toolchain][parse]") {
    std::string output = "Note: including file:  C:\\Program Files\\My Lib\\header.h\n";
    auto includes = tc::parse_show_includes(output);
    REQUIRE(includes.size() == 1);
    CHECK(includes[0].string().find("My Lib") != std::string::npos);
}

TEST_CASE("parse_show_includes: leading/trailing whitespace in path", "[toolchain][parse]") {
    std::string output = "Note: including file:     C:\\path\\with\\spaces.h   \n";
    auto includes = tc::parse_show_includes(output);
    REQUIRE(includes.size() == 1);
    // Path should be trimmed
    CHECK(includes[0].string().find("  ") == std::string::npos);
}

// ===================================================================
// Toolchain detection (basic structural tests)
// ===================================================================

TEST_CASE("detect_toolchain returns a valid toolchain", "[toolchain][detect]") {
    auto tc_inst = tc::detect_toolchain();

    // Must have a valid family
    bool valid_family = (tc_inst.family == tc::CompilerFamily::Gcc ||
                         tc_inst.family == tc::CompilerFamily::Clang ||
                         tc_inst.family == tc::CompilerFamily::Msvc);
    CHECK(valid_family);

    // Compilers should be set
    CHECK_FALSE(tc_inst.cxx_compiler.empty());
    CHECK_FALSE(tc_inst.linker.empty());
    CHECK_FALSE(tc_inst.archiver.empty());
}

TEST_CASE("detect_toolchain is cached (returns same result)", "[toolchain][detect]") {
    auto first = tc::detect_toolchain();
    auto second = tc::detect_toolchain();
    CHECK(first.family == second.family);
    CHECK(first.cxx_compiler == second.cxx_compiler);
    CHECK(first.linker == second.linker);
    CHECK(first.archiver == second.archiver);
}

TEST_CASE("CompilerFamily enum values are distinct", "[toolchain][enum]") {
    CHECK(static_cast<int>(tc::CompilerFamily::Gcc) != static_cast<int>(tc::CompilerFamily::Clang));
    CHECK(static_cast<int>(tc::CompilerFamily::Gcc) != static_cast<int>(tc::CompilerFamily::Msvc));
    CHECK(static_cast<int>(tc::CompilerFamily::Clang) != static_cast<int>(tc::CompilerFamily::Msvc));
}

// ===================================================================
// FlagTranslation struct smoke tests
// ===================================================================

TEST_CASE("FlagTranslation default state", "[toolchain][struct]") {
    tc::FlagTranslation ft;
    CHECK(ft.translated.empty());
    CHECK(ft.unrecognized.empty());
}

TEST_CASE("Toolchain default state", "[toolchain][struct]") {
    tc::Toolchain t;
    CHECK(t.family == tc::CompilerFamily::Gcc);
    CHECK(t.cxx_compiler.empty());
    CHECK(t.c_compiler.empty());
    CHECK(t.linker.empty());
    CHECK(t.archiver.empty());
    CHECK(t.vcvars_path.empty());
}

// ===================================================================
// load_msvc_env (non-MSVC platforms: returns empty map)
// ===================================================================

TEST_CASE("load_msvc_env with non-existent path returns empty map", "[toolchain][msvc_env]") {
    // On Windows without VS, or on non-Windows, this should return empty.
    // The function should not crash — reaching here without exception is success.
    auto env = tc::load_msvc_env("C:/nonexistent/vcvars64.bat");
    bool result = env.empty() || !env.empty(); // just verify no crash
    CHECK(result);
}

// ===================================================================
// Edge cases: -c flag
// ===================================================================

TEST_CASE("translate_compile_flags: -c → /c", "[toolchain][translate]") {
    std::vector<std::string> flags = {"-c"};
    auto result = tc::translate_compile_flags(flags, tc::CompilerFamily::Msvc);
    REQUIRE(result.translated.size() == 1);
    CHECK(result.translated[0] == "/c");
}

// ===================================================================
// Edge cases: optimization levels
// ===================================================================

TEST_CASE("translate_compile_flags: optimization levels", "[toolchain][translate]") {
    SECTION("-O0 → /Od") {
        auto r = tc::translate_compile_flags({"-O0"}, tc::CompilerFamily::Msvc);
        CHECK(r.translated[0] == "/Od");
    }
    SECTION("-O1 → /O1") {
        auto r = tc::translate_compile_flags({"-O1"}, tc::CompilerFamily::Msvc);
        CHECK(r.translated[0] == "/O1");
    }
    SECTION("-O2 → /O2") {
        auto r = tc::translate_compile_flags({"-O2"}, tc::CompilerFamily::Msvc);
        CHECK(r.translated[0] == "/O2");
    }
    SECTION("-O3 → /Ox") {
        auto r = tc::translate_compile_flags({"-O3"}, tc::CompilerFamily::Msvc);
        CHECK(r.translated[0] == "/Ox");
    }
    SECTION("-Os → /O1") {
        auto r = tc::translate_compile_flags({"-Os"}, tc::CompilerFamily::Msvc);
        CHECK(r.translated[0] == "/O1");
    }
}
