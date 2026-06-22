// Unit tests for project.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/project.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace ezmk::project;
using namespace ezmk::config;
using namespace ezmk::util;

// create_project() always creates under fs::current_path() / name.
// We save/restore cwd around tests to isolate them.
struct CwdGuard {
    fs::path original;
    fs::path temp_dir;

    CwdGuard() : original(fs::current_path()) {
        temp_dir = fs::temp_directory_path() / ("ezmk_prj_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(temp_dir);
        fs::current_path(temp_dir);
    }
    ~CwdGuard() {
        fs::current_path(original);
        std::error_code ec;
        fs::remove_all(temp_dir, ec);
    }
};

// ===================================================================
// create_project() — basic
// ===================================================================

TEST_CASE("create_project: executable type creates full structure", "[project]") {
    CwdGuard guard;

    REQUIRE_NOTHROW(create_project("exe_prj", "executable"));

    fs::path root = guard.temp_dir / "exe_prj";
    REQUIRE(file_exists(root));
    REQUIRE(file_exists(root / "src/main.cpp"));
    REQUIRE(file_exists(root / "ezmk.toml"));
    REQUIRE(file_exists(root / "README.md"));
    REQUIRE(file_exists(root / ".gitignore"));
    REQUIRE(file_exists(root / "include"));
    REQUIRE(file_exists(root / "src"));
    REQUIRE(file_exists(root / "build"));
    REQUIRE(file_exists(root / ".ezmk/pkg"));
    REQUIRE(file_exists(root / ".ezmk/temp"));
    REQUIRE(file_exists(root / ".ezmk/cache"));
}

TEST_CASE("create_project: sets correct name in ezmk.toml", "[project]") {
    CwdGuard guard;

    create_project("my_cool_app", "executable");

    auto cfg = parse_config(guard.temp_dir / "my_cool_app/ezmk.toml");
    REQUIRE(cfg.project.name == "my_cool_app");
    REQUIRE(cfg.project.type == "executable");
    REQUIRE(cfg.project.version == "0.1.0");
    REQUIRE(cfg.project.language == "C++17");
}

TEST_CASE("create_project: static type", "[project]") {
    CwdGuard guard;

    create_project("static_lib", "static");

    auto cfg = parse_config(guard.temp_dir / "static_lib/ezmk.toml");
    REQUIRE(cfg.project.type == "static");
    // main.cpp is still created as a starting point
    REQUIRE(file_exists(guard.temp_dir / "static_lib/src/main.cpp"));
}

TEST_CASE("create_project: shared type", "[project]") {
    CwdGuard guard;

    create_project("shared_lib", "shared");

    auto cfg = parse_config(guard.temp_dir / "shared_lib/ezmk.toml");
    REQUIRE(cfg.project.type == "shared");
}

// ===================================================================
// create_project() — main.cpp content
// ===================================================================

TEST_CASE("create_project: main.cpp has expected content", "[project]") {
    CwdGuard guard;

    create_project("main_test", "executable");

    auto content = file_read(guard.temp_dir / "main_test/src/main.cpp");
    REQUIRE_FALSE(content.empty());
    REQUIRE(content.find("main") != std::string::npos);
    REQUIRE(content.find("iostream") != std::string::npos);
    REQUIRE(content.find("Hello world!") != std::string::npos);
}

// ===================================================================
// create_project() — .gitignore
// ===================================================================

TEST_CASE("create_project: .gitignore created by default", "[project]") {
    CwdGuard guard;

    create_project("gi_default", "executable");

    auto gi = file_read(guard.temp_dir / "gi_default/.gitignore");
    REQUIRE(gi.find("build/") != std::string::npos);
    REQUIRE(gi.find(".ezmk/") != std::string::npos);
    REQUIRE(gi.find("*.o") != std::string::npos);
}

TEST_CASE("create_project: --disable-gitignore skips .gitignore", "[project]") {
    CwdGuard guard;

    create_project("gi_skip", "executable", false, true);

    REQUIRE_FALSE(file_exists(guard.temp_dir / "gi_skip/.gitignore"));
    // Other files still exist
    REQUIRE(file_exists(guard.temp_dir / "gi_skip/ezmk.toml"));
}

// ===================================================================
// create_project() — git init
// ===================================================================

TEST_CASE("create_project: --disable-git-init skips .git", "[project]") {
    CwdGuard guard;

    create_project("git_skip", "executable", true, false);

    REQUIRE_FALSE(file_exists(guard.temp_dir / "git_skip/.git"));
    // .gitignore is still created
    REQUIRE(file_exists(guard.temp_dir / "git_skip/.gitignore"));
}

TEST_CASE("create_project: both disable flags", "[project]") {
    CwdGuard guard;

    create_project("no_both", "executable", true, true);

    REQUIRE_FALSE(file_exists(guard.temp_dir / "no_both/.gitignore"));
    REQUIRE_FALSE(file_exists(guard.temp_dir / "no_both/.git"));
    // Core files still created
    REQUIRE(file_exists(guard.temp_dir / "no_both/ezmk.toml"));
    REQUIRE(file_exists(guard.temp_dir / "no_both/src/main.cpp"));
    REQUIRE(file_exists(guard.temp_dir / "no_both/README.md"));
}

// ===================================================================
// create_project() — default config content
// ===================================================================

TEST_CASE("create_project: compile section defaults", "[project]") {
    CwdGuard guard;

    create_project("def_cfg", "executable");

    auto cfg = parse_config(guard.temp_dir / "def_cfg/ezmk.toml");
    REQUIRE(cfg.compile.flags.size() == 3);
    REQUIRE(cfg.compile.flags[0] == "-Wall");
    REQUIRE(cfg.compile.flags[1] == "-Wextra");
    REQUIRE(cfg.compile.flags[2] == "-O2");
    REQUIRE(cfg.compile.include_dirs.size() == 1);
    REQUIRE(cfg.compile.include_dirs[0] == "include");
}

TEST_CASE("create_project: link section defaults are empty", "[project]") {
    CwdGuard guard;

    create_project("link_def", "executable");

    auto cfg = parse_config(guard.temp_dir / "link_def/ezmk.toml");
    REQUIRE(cfg.link.flags.empty());
    REQUIRE(cfg.link.link_dirs.empty());
    REQUIRE(cfg.link.system_targets.empty());
}

TEST_CASE("create_project: depends section defaults are empty", "[project]") {
    CwdGuard guard;

    create_project("dep_def", "executable");

    auto cfg = parse_config(guard.temp_dir / "dep_def/ezmk.toml");
    REQUIRE(cfg.depends.libs.empty());
}

TEST_CASE("create_project: README.md exists and is empty", "[project]") {
    CwdGuard guard;

    create_project("readme", "executable");

    auto content = file_read(guard.temp_dir / "readme/README.md");
    REQUIRE(content.empty());
}

// ===================================================================
// create_project() — error: directory already exists
// ===================================================================

TEST_CASE("create_project: existing directory throws", "[project]") {
    CwdGuard guard;

    fs::create_directories(guard.temp_dir / "exists_dir");

    REQUIRE_THROWS_AS(create_project("exists_dir", "executable"), ezmk::fatal_error);
}
