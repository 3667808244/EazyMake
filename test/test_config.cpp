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
