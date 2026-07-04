// Unit tests for config.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// ===================================================================
// parse_language()
// ===================================================================

TEST_CASE("parse_language: C++ versions", "[config]") {
    using namespace ezmk::config;

    SECTION("C++17 → g++ with -std=c++17") {
        auto info = parse_language("C++17");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++17");
    }

    SECTION("C++20 → g++ with -std=c++20") {
        auto info = parse_language("C++20");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++20");
    }

    SECTION("C++14 → g++ with -std=c++14") {
        auto info = parse_language("C++14");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++14");
    }

    SECTION("C++11 → g++ with -std=c++11") {
        auto info = parse_language("C++11");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++11");
    }

    SECTION("C++23 → g++ with -std=c++23") {
        auto info = parse_language("C++23");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++23");
    }

    SECTION("C++98 → g++ with -std=c++98") {
        auto info = parse_language("C++98");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++98");
    }
}

TEST_CASE("parse_language: C versions", "[config]") {
    using namespace ezmk::config;

    SECTION("C11 → gcc with -std=c11") {
        auto info = parse_language("C11");
        REQUIRE(info.compiler == "gcc");
        REQUIRE(info.std_flag == "-std=c11");
    }

    SECTION("C99 → gcc with -std=c99") {
        auto info = parse_language("C99");
        REQUIRE(info.compiler == "gcc");
        REQUIRE(info.std_flag == "-std=c99");
    }

    SECTION("C17 → gcc with -std=c17") {
        auto info = parse_language("C17");
        REQUIRE(info.compiler == "gcc");
        REQUIRE(info.std_flag == "-std=c17");
    }

    SECTION("C89 → gcc with -std=c89") {
        auto info = parse_language("C89");
        REQUIRE(info.compiler == "gcc");
        REQUIRE(info.std_flag == "-std=c89");
    }
}

TEST_CASE("parse_language: LanguageInfo detected_compiler defaults", "[config]") {
    using namespace ezmk::config;

    SECTION("C++ language — detected_compiler is empty by default") {
        auto info = parse_language("C++17");
        REQUIRE(info.detected_compiler.empty());
    }

    SECTION("C language — detected_compiler is empty by default") {
        auto info = parse_language("C11");
        REQUIRE(info.detected_compiler.empty());
    }
}

TEST_CASE("parse_language: invalid inputs", "[config]") {
    using namespace ezmk::config;

    SECTION("Empty string throws") {
        REQUIRE_THROWS_AS(parse_language(""), std::runtime_error);
    }

    SECTION("Garbage string throws") {
        REQUIRE_THROWS_AS(parse_language("Rust2024"), std::runtime_error);
    }

    SECTION("Unknown version throws") {
        REQUIRE_THROWS_AS(parse_language("C++42"), std::runtime_error);
    }

    SECTION("Missing version throws") {
        REQUIRE_THROWS_AS(parse_language("C++"), std::runtime_error);
    }

    SECTION("Lowercase c++ throws") {
        REQUIRE_THROWS_AS(parse_language("c++17"), std::runtime_error);
    }
}

// ===================================================================
// parse_config() — full toml parsing
// ===================================================================

// Helper: write a temp toml file and return its path
static fs::path write_temp_toml(const std::string& content) {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()) + ".toml");
    std::ofstream f(tmp, std::ios::binary);
    f << content;
    f.close();
    return tmp;
}

TEST_CASE("parse_config: basic project section", "[config]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
type = "executable"
version = "0.1.0"
language = "C++17"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.project.name == "testapp");
    REQUIRE(cfg.project.type == "executable");
    REQUIRE(cfg.project.version == "0.1.0");
    REQUIRE(cfg.project.language == "C++17");
}

TEST_CASE("parse_config: version is required", "[config]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
type = "executable"
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
}

TEST_CASE("parse_config: defaults for missing sections", "[config]") {
    using namespace ezmk::config;

    // Minimal valid config
    auto toml = write_temp_toml(R"(
[project]
name = "minimal"
version = "1.0.0"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    // project defaults
    REQUIRE(cfg.project.name == "minimal");
    REQUIRE(cfg.project.type == "executable");
    REQUIRE(cfg.project.language == "C++17");

    // compile defaults
    REQUIRE(cfg.compile.include_dirs.size() == 1);
    REQUIRE(cfg.compile.include_dirs[0] == "include");
    REQUIRE(cfg.compile.flags.empty());

    // link defaults
    REQUIRE(cfg.link.flags.empty());
    REQUIRE(cfg.link.link_dirs.empty());
    REQUIRE(cfg.link.system_targets.empty());

    // depends defaults
    REQUIRE(cfg.depends.libs.empty());
}

TEST_CASE("parse_config: compile section with flags and include_dirs", "[config]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include", "thirdparty/include"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.flags.size() == 3);
    REQUIRE(cfg.compile.flags[0] == "-Wall");
    REQUIRE(cfg.compile.flags[1] == "-Wextra");
    REQUIRE(cfg.compile.flags[2] == "-O2");

    REQUIRE(cfg.compile.include_dirs.size() == 2);
    REQUIRE(cfg.compile.include_dirs[0] == "include");
    REQUIRE(cfg.compile.include_dirs[1] == "thirdparty/include");
}

TEST_CASE("parse_config: include_dir (singular) fallback", "[config]") {
    using namespace ezmk::config;

    // Old field name "include_dir" should be mapped to include_dirs
    auto toml = write_temp_toml(R"(
[project]
name = "oldstyle"
version = "0.1.0"

[compile]
flags = []
include_dir = ["old_include"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.include_dirs.size() == 1);
    REQUIRE(cfg.compile.include_dirs[0] == "old_include");
}

TEST_CASE("parse_config: link section fully populated", "[config]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "linked"
version = "1.0.0"

[link]
flags = ["-static"]
link_dirs = ["/usr/local/lib"]
system_target = ["pthread", "m"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.link.flags.size() == 1);
    REQUIRE(cfg.link.flags[0] == "-static");
    REQUIRE(cfg.link.link_dirs.size() == 1);
    REQUIRE(cfg.link.link_dirs[0] == "/usr/local/lib");
    REQUIRE(cfg.link.system_targets.size() == 2);
    REQUIRE(cfg.link.system_targets[0] == "pthread");
    REQUIRE(cfg.link.system_targets[1] == "m");
}

TEST_CASE("parse_config: depends section", "[config]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "depuser"
version = "0.2.0"

[depends]
lib = ["foo", "bar", "baz"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 3);
    REQUIRE(cfg.depends.libs[0] == "foo");
    REQUIRE(cfg.depends.libs[1] == "bar");
    REQUIRE(cfg.depends.libs[2] == "baz");
}

TEST_CASE("parse_config: file not found", "[config]") {
    using namespace ezmk::config;

    REQUIRE_THROWS_AS(parse_config("nonexistent_file.toml"), std::runtime_error);
}

TEST_CASE("parse_config: empty compile flags", "[config]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "emptyflags"
version = "0.1.0"

[compile]
flags = []
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.flags.empty());
    // include_dirs still defaults to ["include"]
    REQUIRE(cfg.compile.include_dirs.size() == 1);
    REQUIRE(cfg.compile.include_dirs[0] == "include");
}

// ===================================================================
// write_default_config() round-trip
// ===================================================================

TEST_CASE("write_default_config: round-trip executable", "[config]") {
    using namespace ezmk::config;

    auto tmp = fs::temp_directory_path() / "ezmk_test_roundtrip.toml";

    write_default_config(tmp, "roundtrip_app", "executable");
    REQUIRE(fs::exists(tmp));

    auto cfg = parse_config(tmp);
    fs::remove(tmp);

    REQUIRE(cfg.project.name == "roundtrip_app");
    REQUIRE(cfg.project.type == "executable");
    REQUIRE(cfg.project.version == "0.1.0");
    REQUIRE(cfg.project.language == "C++17");
    REQUIRE(cfg.compile.flags.size() == 3);
    REQUIRE(cfg.compile.include_dirs.size() == 1);
    REQUIRE(cfg.compile.include_dirs[0] == "include");
    REQUIRE(cfg.depends.libs.empty());
}

TEST_CASE("write_default_config: round-trip static", "[config]") {
    using namespace ezmk::config;

    auto tmp = fs::temp_directory_path() / "ezmk_test_static.toml";

    write_default_config(tmp, "mylib", "static");
    REQUIRE(fs::exists(tmp));

    auto cfg = parse_config(tmp);
    fs::remove(tmp);

    REQUIRE(cfg.project.name == "mylib");
    REQUIRE(cfg.project.type == "static");
}

TEST_CASE("write_default_config: round-trip shared", "[config]") {
    using namespace ezmk::config;

    auto tmp = fs::temp_directory_path() / "ezmk_test_shared.toml";

    write_default_config(tmp, "myshared", "shared");
    REQUIRE(fs::exists(tmp));

    auto cfg = parse_config(tmp);
    fs::remove(tmp);

    REQUIRE(cfg.project.name == "myshared");
    REQUIRE(cfg.project.type == "shared");
}

// ===================================================================
// 0.2.2+: src_dirs parsing
// ===================================================================

TEST_CASE("parse_config: src_dirs default value", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    // Default src_dirs when not specified
    REQUIRE(cfg.compile.src_dirs.size() == 1);
    REQUIRE(cfg.compile.src_dirs[0] == "src");
}

TEST_CASE("parse_config: src_dirs custom value", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
src_dirs = ["app", "lib", "vendor"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.src_dirs.size() == 3);
    REQUIRE(cfg.compile.src_dirs[0] == "app");
    REQUIRE(cfg.compile.src_dirs[1] == "lib");
    REQUIRE(cfg.compile.src_dirs[2] == "vendor");
}

TEST_CASE("parse_config: src_dirs empty array throws", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
src_dirs = []
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
}

// ===================================================================
// 0.2.2+: compile.macros parsing
// ===================================================================

TEST_CASE("parse_config: macros empty value", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.macros]
DEBUG = ""
ENABLE_FEATURE = ""
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.macros.size() == 2);
    REQUIRE(cfg.compile.macros["DEBUG"] == "");
    REQUIRE(cfg.compile.macros["ENABLE_FEATURE"] == "");
}

TEST_CASE("parse_config: macros string value", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.macros]
VERSION = "2.0.0"
APP_NAME = "MyApp"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.macros.size() == 2);
    REQUIRE(cfg.compile.macros["VERSION"] == "2.0.0");
    REQUIRE(cfg.compile.macros["APP_NAME"] == "MyApp");
}

TEST_CASE("parse_config: macros integer value", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.macros]
MAX_SIZE = 4096
BUFFER_SIZE = 1024
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.macros.size() == 2);
    REQUIRE(cfg.compile.macros["MAX_SIZE"] == "4096");
    REQUIRE(cfg.compile.macros["BUFFER_SIZE"] == "1024");
}

TEST_CASE("parse_config: macros boolean value", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.macros]
ENABLED = true
DISABLED = false
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    // true → "1", false → skipped
    REQUIRE(cfg.compile.macros.size() == 1);
    REQUIRE(cfg.compile.macros["ENABLED"] == "1");
    // DISABLED=false should not appear
    REQUIRE(cfg.compile.macros.find("DISABLED") == cfg.compile.macros.end());
}

TEST_CASE("parse_config: macros invalid key name throws", "[config][0.2.2]") {
    using namespace ezmk::config;

    // Macro name starting with digit
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.macros]
"123INVALID" = ""
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
}

TEST_CASE("parse_config: macros key with special chars throws", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.macros]
"MY-FLAG" = ""
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
}

TEST_CASE("parse_config: macros valid key with underscore", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.macros]
MY_MACRO = "value"
_private = ""
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.macros.size() == 2);
    REQUIRE(cfg.compile.macros["MY_MACRO"] == "value");
    REQUIRE(cfg.compile.macros["_private"] == "");
}

// ===================================================================
// 0.2.2+: depends.want parsing
// ===================================================================

TEST_CASE("parse_config: depends.want array", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["fmt"]
want = ["sqlite3", "zlib"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 1);
    REQUIRE(cfg.depends.libs[0] == "fmt");
    REQUIRE(cfg.depends.want.size() == 2);
    REQUIRE(cfg.depends.want[0] == "sqlite3");
    REQUIRE(cfg.depends.want[1] == "zlib");
}

TEST_CASE("parse_config: want defaults to empty", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.want.empty());
}

// ===================================================================
// 0.2.2+: compile.ezmk_macros parsing
// ===================================================================

TEST_CASE("parse_config: ezmk_macros defaults to true", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.ezmk_macros == true);
}

TEST_CASE("parse_config: ezmk_macros set to false", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
ezmk_macros = false
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.ezmk_macros == false);
}

TEST_CASE("parse_config: ezmk_macros non-boolean throws", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
ezmk_macros = "yes"
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
}

// ===================================================================
// 0.2.2+: msvc_flags parsing (0.2.1+ field still works)
// ===================================================================

TEST_CASE("parse_config: msvc_flags in compile section", "[config][0.2.2]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
flags = ["-Wall"]
msvc_flags = ["/utf-8", "/MD"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.msvc_flags.size() == 2);
    REQUIRE(cfg.compile.msvc_flags[0] == "/utf-8");
    REQUIRE(cfg.compile.msvc_flags[1] == "/MD");
}

// ===================================================================
// 0.2.3+: [compile.profile.<name>] parsing
// ===================================================================

TEST_CASE("parse_config: compile profile basic parsing", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.profile.debug]
flags = ["-g", "-O0"]
msvc_flags = ["/Zi", "/Od"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile_profiles.size() == 1);
    REQUIRE(cfg.compile_profiles.count("debug") == 1);
    auto& debug = cfg.compile_profiles["debug"];
    REQUIRE(debug.flags.size() == 2);
    REQUIRE(debug.flags[0] == "-g");
    REQUIRE(debug.flags[1] == "-O0");
    REQUIRE(debug.msvc_flags.size() == 2);
    REQUIRE(debug.msvc_flags[0] == "/Zi");
    REQUIRE(debug.msvc_flags[1] == "/Od");
}

TEST_CASE("parse_config: multiple compile profiles", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.profile.debug]
flags = ["-g", "-O0"]

[compile.profile.release]
flags = ["-O3", "-DNDEBUG"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile_profiles.size() == 2);
    REQUIRE(cfg.compile_profiles.count("debug") == 1);
    REQUIRE(cfg.compile_profiles.count("release") == 1);
    REQUIRE(cfg.compile_profiles["debug"].flags.size() == 2);
    REQUIRE(cfg.compile_profiles["release"].flags.size() == 2);
}

TEST_CASE("parse_config: compile profile with macros", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.profile.debug]
flags = ["-g"]

[compile.profile.debug.macros]
DEBUG = ""
LOG_LEVEL = "2"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile_profiles.count("debug") == 1);
    auto& debug = cfg.compile_profiles["debug"];
    REQUIRE(debug.macros.size() == 2);
    REQUIRE(debug.macros["DEBUG"] == "");
    REQUIRE(debug.macros["LOG_LEVEL"] == "2");
}

TEST_CASE("parse_config: compile profile name must be alphanumeric", "[config][0.2.3]") {
    using namespace ezmk::config;

    // Profile name with special characters should be rejected
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.profile."my profile"]
flags = ["-g"]
)");
    // The toml parser handles quoted keys differently. Let's test with a malformed name.
    // Actually, toml++ will accept quoted keys. We test invalid pattern names.
    fs::remove(toml);

    // Test with name containing spaces (via explicit TOML quoting)
    auto toml2 = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.profile."bad name"]
flags = ["-g"]
)");
    REQUIRE_THROWS_AS(parse_config(toml2), std::runtime_error);
    fs::remove(toml2);
}

TEST_CASE("parse_config: compile profile empty name rejected", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.profile.""]
flags = ["-g"]
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
}

TEST_CASE("parse_config: compile profile with underscore and hyphen ok", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile.profile.debug-fast]
flags = ["-g", "-O2"]

[compile.profile.release_safe]
flags = ["-O3", "-D_FORTIFY_SOURCE=2"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile_profiles.size() == 2);
    REQUIRE(cfg.compile_profiles.count("debug-fast") == 1);
    REQUIRE(cfg.compile_profiles.count("release_safe") == 1);
}

// ===================================================================
// 0.2.3+: [link.profile.<name>] parsing
// ===================================================================

TEST_CASE("parse_config: link profile basic parsing", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[link.profile.release]
flags = ["-s", "--strip-all"]
msvc_flags = ["/OPT:REF"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.link_profiles.size() == 1);
    REQUIRE(cfg.link_profiles.count("release") == 1);
    auto& rel = cfg.link_profiles["release"];
    REQUIRE(rel.flags.size() == 2);
    REQUIRE(rel.flags[0] == "-s");
    REQUIRE(rel.flags[1] == "--strip-all");
    REQUIRE(rel.msvc_flags.size() == 1);
    REQUIRE(rel.msvc_flags[0] == "/OPT:REF");
}

TEST_CASE("parse_config: link profile empty allowed", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[link.profile.minimal]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.link_profiles.count("minimal") == 1);
    REQUIRE(cfg.link_profiles["minimal"].flags.empty());
}

// ===================================================================
// 0.2.3+: [hooks] section parsing
// ===================================================================

TEST_CASE("parse_config: hooks section with all fields", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[hooks]
pre_build = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.hooks.pre_build == "scripts/pre.lua");
    REQUIRE(cfg.hooks.post_build == "scripts/post.lua");
    REQUIRE(cfg.hooks.on_failure == "scripts/fail.lua");
}

TEST_CASE("parse_config: hooks section partial", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[hooks]
post_build = "scripts/notify.lua"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.hooks.pre_build.empty());
    REQUIRE(cfg.hooks.post_build == "scripts/notify.lua");
    REQUIRE(cfg.hooks.on_failure.empty());
}

TEST_CASE("parse_config: no hooks section defaults to empty", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.hooks.pre_build.empty());
    REQUIRE(cfg.hooks.post_build.empty());
    REQUIRE(cfg.hooks.on_failure.empty());
}

// ===================================================================
// 0.2.3+: write_default_config does NOT include profiles or hooks
// ===================================================================

TEST_CASE("write_default_config: no profile or hooks sections", "[config][0.2.3]") {
    using namespace ezmk::config;

    auto tmp = fs::temp_directory_path() / "ezmk_test_nodefaults.toml";
    write_default_config(tmp, "testapp", "executable");
    REQUIRE(fs::exists(tmp));

    auto cfg = parse_config(tmp);
    fs::remove(tmp);

    // Verify no profiles or hooks are present in the default template
    REQUIRE(cfg.compile_profiles.empty());
    REQUIRE(cfg.link_profiles.empty());
    REQUIRE(cfg.hooks.pre_build.empty());
    REQUIRE(cfg.hooks.post_build.empty());
    REQUIRE(cfg.hooks.on_failure.empty());
}
