// Unit tests for build hooks (0.2.3+)
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/lua_api.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace ezmk::lua;
using namespace ezmk::config;
using namespace ezmk::util;

// ===================================================================
// Helpers
// ===================================================================

static fs::path write_hook_script(const fs::path& dir, const std::string& name,
                                    const std::string& code) {
    fs::path p = dir / (name + ".lua");
    std::ofstream of(p);
    of << code;
    return p;
}

static fs::path write_minimal_ezmk_toml(const fs::path& dir,
                                          const std::string& hooks_section = "") {
    fs::path p = dir / "ezmk.toml";
    std::ofstream of(p);
    of << "[project]\nname = \"hooktest\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n";
    of << "[compile]\nflags = []\ninclude_dirs = [\"include\"]\n\n";
    of << "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n";
    of << "[depends]\nlib = []\n\n";
    if (!hooks_section.empty()) {
        of << hooks_section;
    }
    return p;
}

struct TempProject {
    fs::path dir;
    TempProject() {
        dir = fs::temp_directory_path() / ("ezmk_test_hooks_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(dir);
    }
    ~TempProject() { fs::remove_all(dir); }
};

// ===================================================================
// Hook config parsing
// ===================================================================

TEST_CASE("hooks: parse_config parses [hooks] section", "[hooks][0.2.3][config]") {
    TempProject proj;
    fs::path toml = write_minimal_ezmk_toml(proj.dir, R"(
[hooks]
pre_build = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
)");

    auto cfg = parse_config(toml);
    REQUIRE(cfg.hooks.pre_build == "scripts/pre.lua");
    REQUIRE(cfg.hooks.post_build == "scripts/post.lua");
    REQUIRE(cfg.hooks.on_failure == "scripts/fail.lua");
}

TEST_CASE("hooks: empty [hooks] section yields empty strings", "[hooks][0.2.3][config]") {
    TempProject proj;
    fs::path toml = write_minimal_ezmk_toml(proj.dir, "[hooks]\n");

    auto cfg = parse_config(toml);
    REQUIRE(cfg.hooks.pre_build.empty());
    REQUIRE(cfg.hooks.post_build.empty());
    REQUIRE(cfg.hooks.on_failure.empty());
}

TEST_CASE("hooks: no [hooks] section yields empty strings", "[hooks][0.2.3][config]") {
    TempProject proj;
    fs::path toml = write_minimal_ezmk_toml(proj.dir);

    auto cfg = parse_config(toml);
    REQUIRE(cfg.hooks.pre_build.empty());
    REQUIRE(cfg.hooks.post_build.empty());
    REQUIRE(cfg.hooks.on_failure.empty());
}

TEST_CASE("hooks: partial hooks section", "[hooks][0.2.3][config]") {
    TempProject proj;
    fs::path toml = write_minimal_ezmk_toml(proj.dir, R"(
[hooks]
post_build = "scripts/post.lua"
)");

    auto cfg = parse_config(toml);
    REQUIRE(cfg.hooks.pre_build.empty());
    REQUIRE(cfg.hooks.post_build == "scripts/post.lua");
    REQUIRE(cfg.hooks.on_failure.empty());
}

// ===================================================================
// run_hook_script() - basic execution
// ===================================================================

TEST_CASE("hooks: run_hook_script executes successfully", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto script = write_hook_script(proj.dir, "post_build", R"lua(
function run(ctx)
    return 0
end
)lua");

    init();
    int rc = run_hook_script(state(), script, proj.dir / "build" / "app", proj.dir, "debug");
    REQUIRE(rc == 0);
}

TEST_CASE("hooks: run_hook_script receives ctx table", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto script = write_hook_script(proj.dir, "post_build", R"lua(
function run(ctx)
    -- Verify ctx table contains expected fields
    assert(type(ctx) == "table", "ctx must be a table")
    assert(type(ctx.output) == "string", "ctx.output must be a string")
    assert(type(ctx.project_root) == "string", "ctx.project_root must be a string")
    assert(type(ctx.profile) == "string", "ctx.profile must be a string")
    return 0
end
)lua");

    init();
    int rc = run_hook_script(state(), script,
                              proj.dir / "build" / "myapp.exe",
                              proj.dir, "release");
    REQUIRE(rc == 0);
}

TEST_CASE("hooks: run_hook_script with empty profile", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto script = write_hook_script(proj.dir, "post_build", R"lua(
function run(ctx)
    assert(ctx.profile == "", "profile should be empty string")
    return 0
end
)lua");

    init();
    int rc = run_hook_script(state(), script,
                              proj.dir / "build" / "app",
                              proj.dir, "");
    REQUIRE(rc == 0);
}

TEST_CASE("hooks: run_hook_script returns run() exit code", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto script = write_hook_script(proj.dir, "post_build", R"lua(
function run(ctx)
    return 7
end
)lua");

    init();
    int rc = run_hook_script(state(), script,
                              proj.dir / "build" / "app",
                              proj.dir, "");
    REQUIRE(rc == 7);
}

TEST_CASE("hooks: run_hook_script without run() function returns error", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto script = write_hook_script(proj.dir, "bad_hook", R"lua(
-- No run() function defined
function help()
    return "help"
end
)lua");

    init();
    int rc = run_hook_script(state(), script,
                              proj.dir / "build" / "app",
                              proj.dir, "");
    REQUIRE(rc != 0);
}

TEST_CASE("hooks: run_hook_script with Lua error returns error code", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto script = write_hook_script(proj.dir, "error_hook", R"lua(
function run(ctx)
    error("something went wrong")
end
)lua");

    init();
    int rc = run_hook_script(state(), script,
                              proj.dir / "build" / "app",
                              proj.dir, "");
    REQUIRE(rc != 0);
}

TEST_CASE("hooks: run_hook_script with syntax error returns error code", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto script = write_hook_script(proj.dir, "syntax_error", R"lua(
function run(ctx)
    return 0
-- missing 'end'
)lua");

    init();
    int rc = run_hook_script(state(), script,
                              proj.dir / "build" / "app",
                              proj.dir, "");
    REQUIRE(rc != 0);
}

TEST_CASE("hooks: hook script has access to ezmk API", "[hooks][0.2.3][lua]") {
    TempProject proj;

    // Create a minimal project structure
    fs::create_directories(proj.dir / "src");
    {
        std::ofstream f(proj.dir / "src" / "main.cpp");
        f << "int main() { return 0; }\n";
    }
    write_minimal_ezmk_toml(proj.dir);

    auto script = write_hook_script(proj.dir, "post_build", R"lua(
function run(ctx)
    -- Test that ezmk API is available
    local root = ezmk.project_root()
    assert(type(root) == "string", "project_root should return a string")
    local name = ezmk.project_name()
    assert(name == "hooktest", "project_name should match config")
    return 0
end
)lua");

    // Re-register API for the test project
    register_api(state(), proj.dir);
    init();

    int rc = run_hook_script(state(), script,
                              proj.dir / "build" / "app",
                              proj.dir, "");
    REQUIRE(rc == 0);
}

// ===================================================================
// build_hook_context table structure
// ===================================================================

TEST_CASE("hooks: ctx.output is correct", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto output_path = proj.dir / "build" / "myapp.exe";

    // 1.4.0-dev.5: the old test asserted `rc == 0 || rc == 1` and compared
    // ctx.output against an UNDEFINED Lua global (always nil → always failed
    // to a non-zero rc, but the test passed either way). Now the expected
    // path is rebuilt in Lua from ctx.project_root — tolerant of both '/' and
    // '\' separators (Windows native output vs Lua-side concatenation) — and
    // asserted; a wrong ctx.output makes the hook fatal.
    auto script = write_hook_script(proj.dir, "check_output", R"lua(
function run(ctx)
    -- Build the expected path accepting both separator styles.
    local sep = string.find(ctx.output, "\\", 1, true) and "\\" or "/"
    local expected = ctx.project_root .. sep .. "build" .. sep .. "myapp.exe"
    assert(ctx.output == expected,
        "ctx.output mismatch: got '" .. tostring(ctx.output) ..
        "', expected '" .. expected .. "'")
    return 0
end
)lua");

    init();
    int rc = run_hook_script(state(), script, output_path, proj.dir, "");
    REQUIRE(rc == 0);
}

TEST_CASE("hooks: ctx fields accessible via key name", "[hooks][0.2.3][lua]") {
    TempProject proj;
    auto script = write_hook_script(proj.dir, "ctx_keys", R"lua(
function run(ctx)
    -- Check all three keys exist
    if ctx.output == nil then return 1 end
    if ctx.project_root == nil then return 2 end
    if ctx.profile == nil then return 3 end
    return 0
end
)lua");

    init();
    int rc = run_hook_script(state(), script,
                              proj.dir / "build" / "app",
                              proj.dir, "debug");
    REQUIRE(rc == 0);
}
