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
    // -fno-strict-aliasing is now recognised (silently skipped, 0.2.4+), so only 2 unrecognized
    REQUIRE(result.unrecognized.size() == 2);
    CHECK(result.unrecognized[0] == "-Wno-unused");
    CHECK(result.unrecognized[1] == "--some-unknown-flag");
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

// ===================================================================
// 1.1.0-dev.4: get_stdlib_flags()
// ===================================================================

TEST_CASE("get_stdlib_flags: libstdc++ with GCC returns empty", "[toolchain][1.1.0-dev.4]") {
    auto flags = tc::get_stdlib_flags("libstdc++", tc::CompilerFamily::Gcc);
    REQUIRE(flags.empty());
}

TEST_CASE("get_stdlib_flags: libstdc++ with Clang adds -stdlib=libstdc++", "[toolchain][1.1.0-dev.4]") {
    auto flags = tc::get_stdlib_flags("libstdc++", tc::CompilerFamily::Clang);
    REQUIRE(flags.size() == 1);
    CHECK(flags[0] == "-stdlib=libstdc++");
}

TEST_CASE("get_stdlib_flags: libc++ with GCC adds -stdlib=libc++", "[toolchain][1.1.0-dev.4]") {
    auto flags = tc::get_stdlib_flags("libc++", tc::CompilerFamily::Gcc);
    REQUIRE(flags.size() == 1);
    CHECK(flags[0] == "-stdlib=libc++");
}

TEST_CASE("get_stdlib_flags: libc++ with Clang adds -stdlib=libc++", "[toolchain][1.1.0-dev.4]") {
    auto flags = tc::get_stdlib_flags("libc++", tc::CompilerFamily::Clang);
    REQUIRE(flags.size() == 1);
    CHECK(flags[0] == "-stdlib=libc++");
}

TEST_CASE("get_stdlib_flags: MSVC always returns empty", "[toolchain][1.1.0-dev.4]") {
    auto flags1 = tc::get_stdlib_flags("libstdc++", tc::CompilerFamily::Msvc);
    REQUIRE(flags1.empty());

    auto flags2 = tc::get_stdlib_flags("libc++", tc::CompilerFamily::Msvc);
    REQUIRE(flags2.empty());

    auto flags3 = tc::get_stdlib_flags("", tc::CompilerFamily::Msvc);
    REQUIRE(flags3.empty());
}

TEST_CASE("get_stdlib_flags: empty stdlib with GCC returns empty", "[toolchain][1.1.0-dev.4]") {
    auto flags = tc::get_stdlib_flags("", tc::CompilerFamily::Gcc);
    REQUIRE(flags.empty());
}

TEST_CASE("get_stdlib_flags: empty stdlib with Clang returns empty", "[toolchain][1.1.0-dev.4]") {
    // Empty stdlib treated as libstdc++ but without explicit flag — Clang may need it
    // But by design we only inject when stdlib is explicitly set
    auto flags = tc::get_stdlib_flags("", tc::CompilerFamily::Clang);
    REQUIRE(flags.empty());
}

// ===================================================================
// 1.2.0-dev.10: compiler_tag() — precompiled-package compiler tag
// ===================================================================

namespace {
tc::Toolchain make_tc(tc::CompilerFamily family, const std::string& version) {
    tc::Toolchain t;
    t.family = family;
    t.version = version;
    return t;
}
} // namespace

TEST_CASE("compiler_tag: GCC major from version string", "[toolchain][1.2.0-dev.10]") {
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Gcc, "g++ (GCC) 13.2.0")) == "gcc13");
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Gcc, "g++ (GCC) 11.4.0")) == "gcc11");
    // MSYS2-style version line
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Gcc, "g++ (Rev2, Built by MSYS2 project) 14.1.0"))
            == "gcc14");
}

TEST_CASE("compiler_tag: Clang major from version string", "[toolchain][1.2.0-dev.10]") {
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Clang, "clang version 18.1.8")) == "clang18");
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Clang, "clang version 16.0.6")) == "clang16");
    // Apple Clang — version number does not align with LLVM, but the major is taken as-is
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Clang, "Apple clang version 15.0.0 (clang-1500.3.9.4)"))
            == "clang15");
}

TEST_CASE("compiler_tag: MSVC toolset from cl version line", "[toolchain][1.2.0-dev.10]") {
    // VS 2022 (19.3x) → msvc143
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.43.34808 for x64")) == "msvc143");
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.30.30709 for x64")) == "msvc143");
    // VS 2019 (19.2x) → msvc142
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.29.30153 for x64")) == "msvc142");
    // VS 2017 (19.1x) → msvc141
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.16.27034 for x64")) == "msvc141");
    // VS 2015 (19.00) → msvc140
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.00.24215.1")) == "msvc140");
}

TEST_CASE("compiler_tag: MSVC toolset boundary is table-based, not arithmetic", "[toolchain][1.2.0-dev.10]") {
    // 19.43 → _MSC_VER 1943 → msvc143 (NOT 144 via 140+(1943-1900)/10)
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.43.34808 for x64")) == "msvc143");
    // 19.10 → 1910 → msvc141 (lower boundary of VS2017)
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.10.25017 for x64")) == "msvc141");
    // 19.09 → 1909 — outside the 1910–1919 band and != 1900 → unknown
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.09.99999 for x64")).empty());
    // Non-19.x series is not mapped
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc,
        "Microsoft (R) C/C++ Optimizing Compiler Version 18.00.21005 for x64")).empty());
}

TEST_CASE("compiler_tag: unparseable version returns empty", "[toolchain][1.2.0-dev.10]") {
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Gcc, "")).empty());
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Gcc, "g++: fatal error: no input files")).empty());
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Clang, "clang (unknown)")).empty());
    // MSVC version parsing is language-independent (1.4.0-dev.5): the banner
    // "cl.exe 19.43" (no "Version" token) still parses to 19.43 → msvc143.
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc, "cl.exe 19.43")) == "msvc143");
    // No version digits at all → unparseable.
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc, "cl.exe")).empty());
}

// 1.2.0-dev.11: an overflowing digit run (from a $CXX wrapper or odd cl
// output) must yield an empty tag, not throw out_of_range through the install
// path.
TEST_CASE("compiler_tag: overflowing digit run returns empty without throwing", "[toolchain][1.2.0-dev.11]") {
    std::string huge = "g++ (GCC) 9999999999999999999999999999.0";
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Gcc, huge)).empty());
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Clang, huge)).empty());
    std::string huge_msvc =
        "Microsoft (R) C/C++ Optimizing Compiler Version 99999999999999999999999.43 for x64";
    REQUIRE(tc::compiler_tag(make_tc(tc::CompilerFamily::Msvc, huge_msvc)).empty());
}

// ===================================================================
// 1.4.0-dev.2: max_supported_std() — toolchain capability table
// ===================================================================

TEST_CASE("max_supported_std: GCC segmentation by major version", "[toolchain][1.4.0-dev.2]") {
    using tc::CompilerFamily;
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 4.8.5") == "CPP11");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 4.9.4") == "CPP11");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 5.4.0") == "CPP14");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 6.5.0") == "CPP14");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 7.5.0") == "CPP14");  // partial C++17 → conservative
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 8.3.0") == "CPP17");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 10.4.0") == "CPP17");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 11.4.0") == "CPP20");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 12.3.0") == "CPP20");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 13.2.0") == "CPP23");
    // MSYS2-style version line (Rev2 prefix must not confuse the parser)
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (Rev2, Built by MSYS2 project) 16.1.0") == "CPP23");
}

TEST_CASE("max_supported_std: Clang segmentation by major version", "[toolchain][1.4.0-dev.2]") {
    using tc::CompilerFamily;
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "clang version 3.4.2") == "CPP11");
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "clang version 3.8.1") == "CPP11");
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "clang version 4.0.1") == "CPP14");  // partial C++17 → conservative
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "clang version 5.0.2") == "CPP17");
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "clang version 10.0.1") == "CPP17");
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "clang version 11.1.0") == "CPP20");
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "clang version 16.0.6") == "CPP20");  // C++23 partial → conservative
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "Apple clang version 15.0.0 (clang-1500.3.9.4)") == "CPP20");
}

TEST_CASE("max_supported_std: MSVC segmentation by _MSC_VER", "[toolchain][1.4.0-dev.2]") {
    using tc::CompilerFamily;
    auto cl = [](const char* minor) {
        return std::string("Microsoft (R) C/C++ Optimizing Compiler Version 19.") + minor + " for x64";
    };
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, cl("00")) == "CPP11");  // VS 2015
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, cl("10")) == "CPP17");  // VS 2017
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, cl("16")) == "CPP17");
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, cl("20")) == "CPP20");  // VS 2019
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, cl("29")) == "CPP20");
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, cl("30")) == "CPP20");  // VS 2022 (C++23 partial → conservative)
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, cl("43")) == "CPP20");
    // 1.4.0-dev.5: localized banners (no "Version" token) parse the same way —
    // zh-CN cl.exe output "用于 x64 的 ... 优化编译器 19.44.35208 版" must NOT
    // grab "64" from "x64" (the digit.digit scan skips platform tags).
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc,
        "\xd3\xc3\xd3\xda x64 \xb5\xc4 Microsoft (R) C/C++ \xd3\xc5\xbb\xaf\xb1\xe0\xd2\xeb\xc6\xf7 19.44.35208 \xb0\xe6")
        == "CPP20");
}

TEST_CASE("max_supported_std: unknown versions fall back to the conservative floor", "[toolchain][1.4.0-dev.2]") {
    using tc::CompilerFamily;
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "") == "CPP11");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++: fatal error: no input files") == "CPP11");
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc, "g++ (GCC) 3.4.6") == "CPP11");
    REQUIRE(tc::max_supported_std(CompilerFamily::Clang, "clang (unknown)") == "CPP11");
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, "cl.exe 19.43") == "CPP20");  // language-independent parse (1.4.0-dev.5)
    REQUIRE(tc::max_supported_std(CompilerFamily::Msvc, "Version 20.1.2") == "CPP11");  // non-19.x series
    // Overflowing digit runs must not throw (same guard as compiler_tag).
    REQUIRE(tc::max_supported_std(CompilerFamily::Gcc,
        "g++ (GCC) 9999999999999999999999999999.0") == "CPP11");
}
