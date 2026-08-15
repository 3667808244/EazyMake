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

    SECTION("Missing version defaults to C++17 / C11") {
        // 1.1.0-dev.4: C++ without version defaults to C++17
        auto info = parse_language("C++");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++17");
    }
}

// ===================================================================
// 1.1.0-dev.4: normalize_lang() tests
// ===================================================================

TEST_CASE("normalize_lang: basic normalization", "[config][1.1.0-dev.4]") {
    using namespace ezmk::config;

    SECTION("c++17 → CPP17") {
        REQUIRE(normalize_lang("c++17") == "C++17");
    }
    SECTION("C++17 → C++17 (unchanged but uppercase)") {
        REQUIRE(normalize_lang("C++17") == "C++17");
    }
    SECTION("cxx17 → CXX17") {
        REQUIRE(normalize_lang("cxx17") == "CXX17");
    }
    SECTION("CXX17 → CXX17") {
        REQUIRE(normalize_lang("CXX17") == "CXX17");
    }
    SECTION("cpp17 → CPP17") {
        REQUIRE(normalize_lang("cpp17") == "CPP17");
    }
    SECTION("c11 → C11") {
        REQUIRE(normalize_lang("c11") == "C11");
    }
    SECTION("c17 → C17") {
        REQUIRE(normalize_lang("c17") == "C17");
    }
    SECTION("c++2b → C++2B") {
        REQUIRE(normalize_lang("c++2b") == "C++2B");
    }
    SECTION("c++20 → C++20") {
        REQUIRE(normalize_lang("c++20") == "C++20");
    }
    SECTION("gnucpp17 → GNUCPP17") {
        REQUIRE(normalize_lang("gnucpp17") == "GNUCPP17");
    }
    SECTION("whitespace trim") {
        REQUIRE(normalize_lang("  C++17  ") == "C++17");
    }
    SECTION("tabs and newlines trim") {
        REQUIRE(normalize_lang("\tC11\r\n") == "C11");
    }
    SECTION("empty input returns empty") {
        REQUIRE(normalize_lang("") == "");
    }
    SECTION("whitespace only returns empty") {
        REQUIRE(normalize_lang("   \t  ") == "");
    }
    SECTION("stdlib: libstdc++ → LIBSTDC++") {
        REQUIRE(normalize_lang("libstdc++") == "LIBSTDC++");
    }
    SECTION("stdlib: libc++ → LIBC++") {
        REQUIRE(normalize_lang("libc++") == "LIBC++");
    }
    SECTION("stdlib: glibcxx → GLIBCXX") {
        REQUIRE(normalize_lang("glibcxx") == "GLIBCXX");
    }
    SECTION("stdlib: llvm → LLVM") {
        REQUIRE(normalize_lang("llvm") == "LLVM");
    }
}

// ===================================================================
// 1.1.0-dev.4: parse_language() extended tests
// ===================================================================

TEST_CASE("parse_language: case-insensitive variants", "[config][1.1.0-dev.4]") {
    using namespace ezmk::config;

    SECTION("c++17") {
        auto info = parse_language("c++17");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++17");
        REQUIRE(info.normalized_lang == "CPP17");
        REQUIRE(info.gnu_extensions == false);
    }
    SECTION("CPP17") {
        auto info = parse_language("CPP17");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++17");
    }
    SECTION("cxx17") {
        auto info = parse_language("cxx17");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++17");
    }
    SECTION("c++20") {
        auto info = parse_language("c++20");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=c++20");
    }
}

TEST_CASE("parse_language: GNU extension prefix", "[config][1.1.0-dev.4]") {
    using namespace ezmk::config;

    SECTION("GNUCPP17 → -std=gnu++17") {
        auto info = parse_language("GNUCPP17");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=gnu++17");
        REQUIRE(info.gnu_extensions == true);
    }
    SECTION("GNU11 → -std=gnu11") {
        auto info = parse_language("GNU11");
        REQUIRE(info.compiler == "gcc");
        REQUIRE(info.std_flag == "-std=gnu11");
        REQUIRE(info.gnu_extensions == true);
    }
    SECTION("GNU17 → -std=gnu17") {
        auto info = parse_language("GNU17");
        REQUIRE(info.compiler == "gcc");
        REQUIRE(info.std_flag == "-std=gnu17");
        REQUIRE(info.gnu_extensions == true);
    }
    SECTION("gnucpp17 (lowercase) → -std=gnu++17") {
        auto info = parse_language("gnucpp17");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=gnu++17");
        REQUIRE(info.gnu_extensions == true);
    }
    SECTION("gnuc++20 → -std=gnu++20") {
        auto info = parse_language("gnuc++20");
        REQUIRE(info.compiler == "g++");
        REQUIRE(info.std_flag == "-std=gnu++20");
        REQUIRE(info.gnu_extensions == true);
    }
    SECTION("CPP17 has gnu_extensions=false") {
        auto info = parse_language("CPP17");
        REQUIRE(info.gnu_extensions == false);
        REQUIRE(info.std_flag == "-std=c++17");
    }
}

TEST_CASE("parse_language: normalized_lang field", "[config][1.1.0-dev.4]") {
    using namespace ezmk::config;

    SECTION("CPP17 from c++17") {
        auto info = parse_language("c++17");
        REQUIRE(info.normalized_lang == "CPP17");
    }
    SECTION("CPP17 from CXX17") {
        auto info = parse_language("CXX17");
        REQUIRE(info.normalized_lang == "CPP17");
    }
    SECTION("GNUCPP17 from gnucpp17") {
        auto info = parse_language("gnucpp17");
        REQUIRE(info.normalized_lang == "GNUCPP17");
    }
    SECTION("C11 from c11") {
        auto info = parse_language("c11");
        REQUIRE(info.normalized_lang == "C11");
    }
}

// 1.1.0-dev.4: stdlib parsing helper (uses same write_temp_toml as below)
static fs::path write_temp_toml(const std::string& content) {
    auto tmp = fs::temp_directory_path() / ("ezmk_test_" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()) + ".toml");
    std::ofstream f(tmp, std::ios::binary);
    f << content;
    f.close();
    return tmp;
}

// ===================================================================
// 1.1.0-dev.4: stdlib parsing tests
// ===================================================================

TEST_CASE("parse_config: stdlib default and valid values", "[config][1.1.0-dev.4]") {
    using namespace ezmk::config;

    SECTION("stdlib defaults to libstdc++") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.project.stdlib == "libstdc++");
    }

    SECTION("stdlib = 'libstdc++'") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
stdlib = "libstdc++"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.project.stdlib == "libstdc++");
    }

    SECTION("stdlib = 'libc++'") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
stdlib = "libc++"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.project.stdlib == "libc++");
    }

    SECTION("stdlib = 'glibcxx' → libstdc++") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
stdlib = "glibcxx"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.project.stdlib == "libstdc++");
    }

    SECTION("stdlib = 'gnu' → libstdc++") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
stdlib = "gnu"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.project.stdlib == "libstdc++");
    }

    SECTION("stdlib = 'llvm' → libc++") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
stdlib = "llvm"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.project.stdlib == "libc++");
    }

    SECTION("stdlib case insensitive") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
stdlib = "LibStdC++"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.project.stdlib == "libstdc++");
    }
}

TEST_CASE("parse_config: stdlib invalid throws", "[config][1.1.0-dev.4]") {
    using namespace ezmk::config;

    SECTION("stdlib = 'bogus' throws") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
stdlib = "bogus"
)");
        REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
        fs::remove(toml);
    }
}

// ===================================================================
// parse_config() — full toml parsing
// ===================================================================

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
    REQUIRE(cfg.depends.libs[0].name == "foo");
    REQUIRE(cfg.depends.libs[1].name == "bar");
    REQUIRE(cfg.depends.libs[2].name == "baz");
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
    // 1.2.0-dev.3: base flags are warnings-only (no -O*); optimization belongs to profiles
    REQUIRE(cfg.compile.flags.size() == 2);
    REQUIRE(cfg.compile.flags[0] == "-Wall");
    REQUIRE(cfg.compile.flags[1] == "-Wextra");
    REQUIRE(cfg.compile.default_profile == "debug");
    REQUIRE(cfg.compile_profiles.count("debug") == 1);
    REQUIRE(cfg.compile_profiles.count("release") == 1);
    REQUIRE(cfg.compile.include_dirs.size() == 1);
    REQUIRE(cfg.compile.include_dirs[0] == "include");
    REQUIRE(cfg.depends.libs.empty());
}

TEST_CASE("write_default_config: escapes special chars in project name", "[config][1.1.2]") {
    using namespace ezmk::config;

    auto tmp = fs::temp_directory_path() / "ezmk_test_quote.toml";

    // 1.1.2 C5: a name with a quote / newline must still produce parseable TOML
    write_default_config(tmp, "my\"app\n2", "executable");
    REQUIRE(fs::exists(tmp));

    auto cfg = parse_config(tmp);
    fs::remove(tmp);

    REQUIRE(cfg.project.name == "my\"app\n2");
    REQUIRE(cfg.project.type == "executable");
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
    REQUIRE(cfg.depends.libs[0].name == "fmt");
    REQUIRE(cfg.depends.want.size() == 2);
    REQUIRE(cfg.depends.want[0].name == "sqlite3");
    REQUIRE(cfg.depends.want[1].name == "zlib");
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
// 0.9.6+: depends version constraint parsing
// ===================================================================

TEST_CASE("parse_config: depends with exact version constraint (@)", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["fmt@10.2.1"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 1);
    REQUIRE(cfg.depends.libs[0].name == "fmt");
    REQUIRE(cfg.depends.libs[0].constraint.op == VersionConstraint::Exact);
    REQUIRE(cfg.depends.libs[0].constraint.version == "10.2.1");
}

TEST_CASE("parse_config: depends with compatible version constraint (^)", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["spdlog^1.14.0"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 1);
    REQUIRE(cfg.depends.libs[0].name == "spdlog");
    REQUIRE(cfg.depends.libs[0].constraint.op == VersionConstraint::Compatible);
    REQUIRE(cfg.depends.libs[0].constraint.version == "1.14.0");
}

TEST_CASE("parse_config: depends with approximate version constraint (~)", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["nlohmann_json~3.11.0"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 1);
    REQUIRE(cfg.depends.libs[0].name == "nlohmann_json");
    REQUIRE(cfg.depends.libs[0].constraint.op == VersionConstraint::Approx);
    REQUIRE(cfg.depends.libs[0].constraint.version == "3.11.0");
}

TEST_CASE("parse_config: depends with >= constraint", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["zlib>=1.2.0"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 1);
    REQUIRE(cfg.depends.libs[0].name == "zlib");
    REQUIRE(cfg.depends.libs[0].constraint.op == VersionConstraint::Gte);
    REQUIRE(cfg.depends.libs[0].constraint.version == "1.2.0");
}

TEST_CASE("parse_config: depends with > constraint", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["boost>1.80.0"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 1);
    REQUIRE(cfg.depends.libs[0].name == "boost");
    REQUIRE(cfg.depends.libs[0].constraint.op == VersionConstraint::Gt);
    REQUIRE(cfg.depends.libs[0].constraint.version == "1.80.0");
}

TEST_CASE("parse_config: depends mixed old and new format", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["foo", "bar@1.0.0", "baz^2.0"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 3);
    // foo — no constraint
    REQUIRE(cfg.depends.libs[0].name == "foo");
    REQUIRE(cfg.depends.libs[0].constraint.op == VersionConstraint::None);
    // bar — exact
    REQUIRE(cfg.depends.libs[1].name == "bar");
    REQUIRE(cfg.depends.libs[1].constraint.op == VersionConstraint::Exact);
    REQUIRE(cfg.depends.libs[1].constraint.version == "1.0.0");
    // baz — compatible
    REQUIRE(cfg.depends.libs[2].name == "baz");
    REQUIRE(cfg.depends.libs[2].constraint.op == VersionConstraint::Compatible);
    REQUIRE(cfg.depends.libs[2].constraint.version == "2.0");
}

TEST_CASE("parse_config: depends with whitespace around operator", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["pkg @ 3.0", "lib ^ 1.2", "dep>= 4.5"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 3);
    REQUIRE(cfg.depends.libs[0].name == "pkg");
    REQUIRE(cfg.depends.libs[0].constraint.op == VersionConstraint::Exact);
    REQUIRE(cfg.depends.libs[0].constraint.version == "3.0");
    REQUIRE(cfg.depends.libs[1].name == "lib");
    REQUIRE(cfg.depends.libs[1].constraint.op == VersionConstraint::Compatible);
    REQUIRE(cfg.depends.libs[1].constraint.version == "1.2");
    REQUIRE(cfg.depends.libs[2].name == "dep");
    REQUIRE(cfg.depends.libs[2].constraint.op == VersionConstraint::Gte);
    REQUIRE(cfg.depends.libs[2].constraint.version == "4.5");
}

TEST_CASE("parse_config: depends want with version constraints", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["fmt@10.0.0"]
want = ["sqlite3", "yaml-cpp~0.8"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.depends.libs.size() == 1);
    REQUIRE(cfg.depends.libs[0].constraint.op == VersionConstraint::Exact);
    REQUIRE(cfg.depends.want.size() == 2);
    REQUIRE(cfg.depends.want[0].name == "sqlite3");
    REQUIRE(cfg.depends.want[0].constraint.op == VersionConstraint::None);
    REQUIRE(cfg.depends.want[1].name == "yaml-cpp");
    REQUIRE(cfg.depends.want[1].constraint.op == VersionConstraint::Approx);
}

TEST_CASE("parse_config: depends with operator but missing version throws", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["pkg@"]
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
}

TEST_CASE("parse_config: depends with empty entry throws", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = [""]
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
}

TEST_CASE("parse_config: depends with spaces only entry throws", "[config][0.9.6]") {
    using namespace ezmk::config;
    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[depends]
lib = ["   "]
)");
    REQUIRE_THROWS_AS(parse_config(toml), std::runtime_error);
    fs::remove(toml);
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

TEST_CASE("parse_config: compile_commands set to true (1.1.1)", "[config][1.1.1]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
flags = ["-Wall"]
compile_commands = true
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.compile_commands == true);
}

TEST_CASE("parse_config: compile_commands defaults to false (1.1.1)", "[config][1.1.1]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
flags = ["-Wall"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.compile.compile_commands == false);
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

// 1.2.0-dev.3: [compile].default_profile — default profile when no --profile given
TEST_CASE("parse_config: [compile].default_profile parsing", "[config][1.2.0-dev.3]") {
    using namespace ezmk::config;

    SECTION("valid profile name is parsed") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
default_profile = "release"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.compile.default_profile == "release");
    }

    SECTION("absent field defaults to empty") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.compile.default_profile.empty());
    }

    SECTION("round-trip: empty default_profile stays empty") {
        auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[compile]
flags = ["-Wall"]
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.compile.default_profile.empty());
        REQUIRE(cfg.compile.flags.size() == 1);
        REQUIRE(cfg.compile.flags[0] == "-Wall");
    }
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
// 1.2.0-dev.3: write_default_config embeds debug/release profiles + default_profile
// (supersedes 0.2.3 "no profile or hooks sections": the default template now
// ships compile profiles, but still no link profiles and no hooks)
// ===================================================================

TEST_CASE("write_default_config: built-in profiles + default_profile", "[config][1.2.0-dev.3]") {
    using namespace ezmk::config;

    auto tmp = fs::temp_directory_path() / "ezmk_test_nodefaults.toml";
    write_default_config(tmp, "testapp", "executable");
    REQUIRE(fs::exists(tmp));

    auto cfg = parse_config(tmp);
    fs::remove(tmp);

    // Template embeds debug/release compile profiles + default_profile = "debug";
    // base flags are warnings-only (no -O*); still no link profiles or hooks.
    REQUIRE(cfg.compile.default_profile == "debug");
    REQUIRE(cfg.compile.flags == std::vector<std::string>{"-Wall", "-Wextra"});
    REQUIRE(cfg.compile_profiles.size() == 2);
    REQUIRE(cfg.compile_profiles.count("debug") == 1);
    REQUIRE(cfg.compile_profiles.count("release") == 1);
    REQUIRE(cfg.compile_profiles["debug"].flags ==
            std::vector<std::string>{"-g", "-O0"});
    REQUIRE(cfg.compile_profiles["debug"].msvc_flags ==
            std::vector<std::string>{"/Zi", "/Od"});
    REQUIRE(cfg.compile_profiles["release"].flags ==
            std::vector<std::string>{"-O2", "-DNDEBUG"});
    REQUIRE(cfg.compile_profiles["release"].msvc_flags ==
            std::vector<std::string>{"/O2", "/DNDEBUG"});
    REQUIRE(cfg.link_profiles.empty());
    REQUIRE(cfg.hooks.pre_build.empty());
    REQUIRE(cfg.hooks.post_build.empty());
    REQUIRE(cfg.hooks.on_failure.empty());
}

// 1.1.3 C2: install prefix `~` expansion must be bounded — only `~/`, `~\` or a
// bare `~` expand; `"~abc"` must not be truncated to `"c"`.
TEST_CASE("parse_config: install prefix ~ expansion is bounded", "[config][1.1.3]") {
    using namespace ezmk::config;

    SECTION("bare ~ expands to home dir") {
        auto toml = write_temp_toml(R"(
[project]
name = "t"
version = "0.1.0"

[install]
prefix = "~"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.install.prefix == ezmk::util::get_home_dir().string());
    }

    SECTION("~/x expands to home/x") {
        auto toml = write_temp_toml(R"(
[project]
name = "t"
version = "0.1.0"

[install]
prefix = "~/x"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.install.prefix == (ezmk::util::get_home_dir() / "x").string());
    }

    SECTION("~abc is left as-is, not truncated") {
        auto toml = write_temp_toml(R"(
[project]
name = "t"
version = "0.1.0"

[install]
prefix = "~abc"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.install.prefix == "~abc");
    }

    SECTION("~\\x expands to home/x (Windows separator)") {
        auto toml = write_temp_toml(R"(
[project]
name = "t"
version = "0.1.0"

[install]
prefix = "~\\x"
)");
        auto cfg = parse_config(toml);
        fs::remove(toml);
        REQUIRE(cfg.install.prefix == (ezmk::util::get_home_dir() / "x").string());
    }
}

// 1.1.3 Q1: .ezmk/links.json parsed via nlohmann/json — malformed JSON errors
// cleanly as runtime_error (not a raw parse_error), and standard escapes /
// Unicode are now handled (the old hand-written parser didn't).
TEST_CASE("load_links_json: parses valid links", "[config][1.1.3]") {
    auto tmp = fs::temp_directory_path() / "ezmk_links_q1";
    fs::create_directories(tmp / ".ezmk");
    std::ofstream(tmp / ".ezmk/links.json")
        << R"({"libs": "vendor/libs", "util": "tools"})";
    auto links = ezmk::config::load_links_json(tmp);
    fs::remove_all(tmp);
    REQUIRE(links.size() == 2);
    REQUIRE(links["libs"] == "vendor/libs");
    REQUIRE(links["util"] == "tools");
}

TEST_CASE("load_links_json: handles escapes and unicode", "[config][1.1.3]") {
    auto tmp = fs::temp_directory_path() / "ezmk_links_uni";
    fs::create_directories(tmp / ".ezmk");
    std::ofstream(tmp / ".ezmk/links.json")
        << R"({"esc": "a\"b\\c", "uni": "中文"})";
    auto links = ezmk::config::load_links_json(tmp);
    fs::remove_all(tmp);
    REQUIRE(links.size() == 2);
    REQUIRE(links["esc"] == "a\"b\\c");
    REQUIRE(links["uni"] == "中文");
}

TEST_CASE("load_links_json: malformed JSON throws runtime_error", "[config][1.1.3]") {
    auto tmp = fs::temp_directory_path() / "ezmk_links_bad";
    fs::create_directories(tmp / ".ezmk");
    std::ofstream(tmp / ".ezmk/links.json") << "{ not valid json ]";
    REQUIRE_THROWS_AS(ezmk::config::load_links_json(tmp), std::runtime_error);
    fs::remove_all(tmp);
}

TEST_CASE("load_links_json: non-object JSON throws runtime_error", "[config][1.1.3]") {
    auto tmp = fs::temp_directory_path() / "ezmk_links_arr";
    fs::create_directories(tmp / ".ezmk");
    std::ofstream(tmp / ".ezmk/links.json") << "[1,2,3]";
    REQUIRE_THROWS_AS(ezmk::config::load_links_json(tmp), std::runtime_error);
    fs::remove_all(tmp);
}

TEST_CASE("load_links_json: absolute path value rejected", "[config][1.1.3]") {
    auto tmp = fs::temp_directory_path() / "ezmk_links_abs";
    fs::create_directories(tmp / ".ezmk");
    std::ofstream(tmp / ".ezmk/links.json") << R"({"bad": "/etc/passwd"})";
    REQUIRE_THROWS_AS(ezmk::config::load_links_json(tmp), std::runtime_error);
    fs::remove_all(tmp);
}

// ===================================================================
// 1.2.0-dev.12: [test] — default_profile / include_dirs / link_targets
// ===================================================================

TEST_CASE("parse_config: [test] new fields parsed", "[config][1.2.0-dev.12]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[test]
dirs = ["test"]
framework = "catch2"
default_profile = "release"
include_dirs = ["test/helpers", "misc"]
link_targets = ["pthread"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.test.dirs.size() == 1);
    REQUIRE(cfg.test.dirs[0] == "test");
    REQUIRE(cfg.test.framework == "CATCH2");
    REQUIRE(cfg.test.default_profile == "release");
    REQUIRE(cfg.test.include_dirs.size() == 2);
    REQUIRE(cfg.test.include_dirs[0] == "test/helpers");
    REQUIRE(cfg.test.include_dirs[1] == "misc");
    REQUIRE(cfg.test.link_targets.size() == 1);
    REQUIRE(cfg.test.link_targets[0] == "pthread");
}

TEST_CASE("parse_config: [test] defaults stay empty/absent", "[config][1.2.0-dev.12]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[test]
dirs = ["test"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.test.default_profile.empty());
    REQUIRE(cfg.test.include_dirs.empty());
    REQUIRE(cfg.test.link_targets.empty());
}

TEST_CASE("parse_config: [test].flags still parsed (deprecated)", "[config][1.2.0-dev.12]") {
    using namespace ezmk::config;

    auto toml = write_temp_toml(R"(
[project]
name = "testapp"
version = "0.1.0"

[test]
dirs = ["test"]
flags = ["-DTESTING"]
)");
    auto cfg = parse_config(toml);
    fs::remove(toml);

    REQUIRE(cfg.test.flags.size() == 1);
    REQUIRE(cfg.test.flags[0] == "-DTESTING");
}
