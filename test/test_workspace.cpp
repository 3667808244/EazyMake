// Unit tests for workspace.cpp (1.3.0-dev.1)
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/workspace.hpp"

#include "ezmk/util.hpp"
#include "test_helpers.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using ezmk::workspace::Member;
using ezmk::workspace::Workspace;
using ezmk::workspace::load_from;
using ezmk::workspace::locate_workspace_root;

namespace {

// Write an ezmk-workspace.toml into `dir`.
void write_ws_toml(const fs::path& dir, const std::string& content) {
    ezmk::util::create_directories(dir);
    std::ofstream(dir / "ezmk-workspace.toml") << content;
}

// Write a minimal member project (dir/rel with a valid ezmk.toml).
// type: "executable" | "static" | "shared"; deps: optional [depends] workspace refs.
void write_member(const fs::path& root, const std::string& rel,
                  const std::string& type = "executable",
                  const std::vector<std::string>& ws_deps = {}) {
    auto dir = root / rel;
    ezmk::util::create_directories(dir);
    std::ofstream of(dir / "ezmk.toml");
    of << "[project]\nname = \"" << fs::path(rel).filename().string()
       << "\"\ntype = \"" << type << "\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n";
    if (!ws_deps.empty()) {
        of << "\n[depends]\nworkspace = [";
        for (size_t i = 0; i < ws_deps.size(); ++i) {
            if (i) of << ", ";
            of << "\"" << ws_deps[i] << "\"";
        }
        of << "]\n";
    }
}

// Find a member by relative path.
const Member* find_member(const Workspace& ws, const std::string& name) {
    for (const auto& m : ws.members) {
        if (m.name == name) return &m;
    }
    return nullptr;
}

} // anonymous namespace

// ===================================================================
// locate_workspace_root()
// ===================================================================

TEST_CASE("workspace locate: finds ezmk-workspace.toml at the start dir (level 0)", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"a\"]\n");

    auto found = locate_workspace_root(tmp.path);
    REQUIRE(found.has_value());
    REQUIRE(found.value() == tmp.path);
}

TEST_CASE("workspace locate: walks up to 5 parent levels, not beyond", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"a\"]\n");

    fs::path deep = tmp.path;
    for (int i = 0; i < 5; ++i) deep = deep / ("lvl" + std::to_string(i));
    ezmk::util::create_directories(deep);

    // deep is exactly 5 levels below the workspace root → still within the limit.
    REQUIRE(locate_workspace_root(deep).value_or(fs::path()) == tmp.path);
    // One level beyond → past max_up → not found.
    fs::path beyond = deep / "lvl5";
    ezmk::util::create_directories(beyond);
    REQUIRE_FALSE(locate_workspace_root(beyond).has_value());
}

TEST_CASE("workspace locate: no ezmk-workspace.toml anywhere returns nullopt", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    ezmk::util::create_directories(tmp.path / "a" / "b");

    REQUIRE_FALSE(locate_workspace_root(tmp.path / "a" / "b").has_value());
}

TEST_CASE("workspace locate: independent from project root (ezmk.toml alone is not a workspace)", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    // A project root without a workspace file is not a workspace root...
    write_member(tmp.path, "proj");
    REQUIRE_FALSE(locate_workspace_root(tmp.path).has_value());

    // ...and a workspace root may also be a project root (both files present).
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"proj\"]\n");
    REQUIRE(locate_workspace_root(tmp.path).value_or(fs::path()) == tmp.path);
}

// ===================================================================
// Parsing: [workspace] / [workspace.options]
// ===================================================================

TEST_CASE("workspace parse: members + options + name", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path,
                  "[workspace]\n"
                  "name = \"my-ws\"\n"
                  "members = [\"apps/tool-a\", \"libs/strutil\"]\n"
                  "\n"
                  "[workspace.options]\n"
                  "default_jobs = 4\n"
                  "stop_on_error = true\n");
    write_member(tmp.path, "apps/tool-a");
    write_member(tmp.path, "libs/strutil", "static");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    REQUIRE(ws->name == "my-ws");
    REQUIRE(ws->root == fs::weakly_canonical(tmp.path));
    REQUIRE(ws->options.default_jobs == 4);
    REQUIRE(ws->options.stop_on_error);
    REQUIRE(ws->members.size() == 2);
    REQUIRE(ws->members[0].name == "apps/tool-a");
    REQUIRE(ws->members[0].basename == "tool-a");
    REQUIRE(ws->members[1].name == "libs/strutil");
    REQUIRE(ws->members[1].basename == "strutil");
    REQUIRE(ws->members[0].valid);
    REQUIRE(ws->members[1].valid);
}

TEST_CASE("workspace parse: defaults when name/options are omitted", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"a\"]\n");
    write_member(tmp.path, "a");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    REQUIRE(ws->name.empty());
    REQUIRE(ws->options.default_jobs == 0);
    REQUIRE_FALSE(ws->options.stop_on_error);
}

TEST_CASE("workspace parse: members is required", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;

    SECTION("no members key") {
        write_ws_toml(tmp.path, "[workspace]\nname = \"x\"\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("empty members array") {
        write_ws_toml(tmp.path, "[workspace]\nmembers = []\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("members not an array") {
        write_ws_toml(tmp.path, "[workspace]\nmembers = \"a\"\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
}

TEST_CASE("workspace parse: non-string member entry throws", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"a\", 42]\n");
    REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
}

TEST_CASE("workspace parse: [workspace] section missing throws", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "members = [\"a\"]\n");
    REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
}

TEST_CASE("workspace parse: unknown sections and keys throw", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;

    SECTION("unknown top-level section") {
        write_ws_toml(tmp.path, "[workspace]\nmembers = [\"a\"]\n[other]\nx = 1\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("unknown key in [workspace]") {
        write_ws_toml(tmp.path, "[workspace]\nmembers = [\"a\"]\nfrobnicate = true\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("unknown key in [workspace.options]") {
        write_ws_toml(tmp.path,
                      "[workspace]\nmembers = [\"a\"]\n"
                      "[workspace.options]\nparallel = true\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
}

TEST_CASE("workspace parse: invalid options types throw", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;

    SECTION("negative default_jobs") {
        write_ws_toml(tmp.path,
                      "[workspace]\nmembers = [\"a\"]\n"
                      "[workspace.options]\ndefault_jobs = -1\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("default_jobs not an integer") {
        write_ws_toml(tmp.path,
                      "[workspace]\nmembers = [\"a\"]\n"
                      "[workspace.options]\ndefault_jobs = \"auto\"\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("stop_on_error not a boolean") {
        write_ws_toml(tmp.path,
                      "[workspace]\nmembers = [\"a\"]\n"
                      "[workspace.options]\nstop_on_error = \"yes\"\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
}

TEST_CASE("workspace parse: malformed TOML throws", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace\nmembers = [\"a\"]\n");
    REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
}

// ===================================================================
// Member validation
// ===================================================================

TEST_CASE("workspace member: missing directory marks member invalid (no throw)", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"ghost\"]\n");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    const auto* m = find_member(*ws, "ghost");
    REQUIRE(m != nullptr);
    REQUIRE_FALSE(m->valid);
    REQUIRE_FALSE(m->error.empty());
}

TEST_CASE("workspace member: missing ezmk.toml marks member invalid", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"no-config\"]\n");
    ezmk::util::create_directories(tmp.path / "no-config");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    const auto* m = find_member(*ws, "no-config");
    REQUIRE(m != nullptr);
    REQUIRE_FALSE(m->valid);
    REQUIRE_FALSE(m->error.empty());
}

TEST_CASE("workspace member: nested ezmk-workspace.toml marks member invalid", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"inner\"]\n");
    write_member(tmp.path, "inner");
    write_ws_toml(tmp.path / "inner", "[workspace]\nmembers = [\"x\"]\n");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    const auto* m = find_member(*ws, "inner");
    REQUIRE(m != nullptr);
    REQUIRE_FALSE(m->valid);
    REQUIRE_FALSE(m->error.empty());
}

TEST_CASE("workspace member: path escape throws at config time", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;

    SECTION("parent traversal (../)") {
        write_ws_toml(tmp.path, "[workspace]\nmembers = [\"../evil\"]\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("absolute path") {
        auto abs = fs::absolute(tmp.path / "evil");
        write_ws_toml(tmp.path,
                      "[workspace]\nmembers = [\"" + abs.string() + "\"]\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
#ifdef _WIN32
    SECTION("drive letter") {
        write_ws_toml(tmp.path, "[workspace]\nmembers = [\"C:\\\\evil\"]\n");
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
#endif
}

TEST_CASE("workspace member: symlink escaping the root throws", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    // Create the symlink target outside the workspace root.
    TempDir outside;
    ezmk::util::create_directories(outside.path / "target");
    std::error_code ec;
    fs::create_directory_symlink(outside.path / "target", tmp.path / "link", ec);
    if (ec) {
        // Symlink creation unsupported on this platform (e.g. Windows without
        // developer mode) — the canonical-escape check cannot be exercised.
        WARN("directory symlink creation unsupported, skipping");
        return;
    }

    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"link\"]\n");
    REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
}

TEST_CASE("workspace member: dot (root itself) is allowed when explicitly listed", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\".\"]\n");
    // The root itself is a project: ezmk.toml at the workspace root.
    write_member(tmp.path, ".");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    REQUIRE(ws->members.size() == 1);
    REQUIRE(ws->members[0].valid);
}

// ===================================================================
// Member dependencies
// ===================================================================

TEST_CASE("workspace deps: valid static dependency by basename", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"apps/tool-a\", \"libs/strutil\"]\n");
    write_member(tmp.path, "libs/strutil", "static");
    write_member(tmp.path, "apps/tool-a", "executable", {"strutil"});

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    const auto* tool = find_member(*ws, "apps/tool-a");
    const auto* lib = find_member(*ws, "libs/strutil");
    REQUIRE(tool != nullptr);
    REQUIRE(lib != nullptr);
    REQUIRE(tool->valid);
    REQUIRE(lib->valid);
    REQUIRE(tool->ws_deps.size() == 1);
    REQUIRE(tool->ws_deps[0] == "strutil");
    REQUIRE(tool->type == "executable");
    REQUIRE(lib->type == "static");
}

TEST_CASE("workspace deps: valid dependency by full relative path", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"apps/tool-a\", \"libs/strutil\"]\n");
    write_member(tmp.path, "libs/strutil", "static");
    write_member(tmp.path, "apps/tool-a", "executable", {"libs/strutil"});

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    const auto* tool = find_member(*ws, "apps/tool-a");
    REQUIRE(tool != nullptr);
    REQUIRE(tool->valid);
}

TEST_CASE("workspace deps: unknown reference throws", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"apps/tool-a\"]\n");
    write_member(tmp.path, "apps/tool-a", "executable", {"nope"});

    REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
}

TEST_CASE("workspace deps: self-loop throws", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"tool-a\"]\n");
    write_member(tmp.path, "tool-a", "executable", {"tool-a"});

    REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
}

TEST_CASE("workspace deps: A -> B -> A cycle throws", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"a\", \"b\"]\n");
    write_member(tmp.path, "a", "static", {"b"});
    write_member(tmp.path, "b", "static", {"a"});

    REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
}

TEST_CASE("workspace deps: non-static dependency throws", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;

    SECTION("dependency is executable") {
        write_ws_toml(tmp.path, "[workspace]\nmembers = [\"app\", \"helper\"]\n");
        write_member(tmp.path, "helper", "executable");
        write_member(tmp.path, "app", "executable", {"helper"});
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("dependency is shared") {
        write_ws_toml(tmp.path, "[workspace]\nmembers = [\"app\", \"helper\"]\n");
        write_member(tmp.path, "helper", "shared");
        write_member(tmp.path, "app", "executable", {"helper"});
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
}

TEST_CASE("workspace deps: referencing an invalid member marks the referencer invalid", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"app\", \"ghost\"]\n");
    write_member(tmp.path, "app", "executable", {"ghost"});
    // ghost has no directory at all.

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    const auto* app = find_member(*ws, "app");
    const auto* ghost = find_member(*ws, "ghost");
    REQUIRE(app != nullptr);
    REQUIRE(ghost != nullptr);
    REQUIRE_FALSE(ghost->valid);
    REQUIRE_FALSE(app->valid);
    REQUIRE_FALSE(app->error.empty());
}

TEST_CASE("workspace deps: ambiguous basename throws, full path disambiguates", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path,
                  "[workspace]\nmembers = [\"app\", \"libs/strutil\", \"ext/strutil\"]\n");
    write_member(tmp.path, "libs/strutil", "static");
    write_member(tmp.path, "ext/strutil", "static");

    SECTION("ambiguous basename") {
        write_member(tmp.path, "app", "executable", {"strutil"});
        REQUIRE_THROWS_AS(load_from(tmp.path), std::runtime_error);
    }
    SECTION("full path disambiguates") {
        write_member(tmp.path, "app", "executable", {"libs/strutil"});
        auto ws = load_from(tmp.path);
        REQUIRE(ws.has_value());
        const auto* app = find_member(*ws, "app");
        REQUIRE(app != nullptr);
        REQUIRE(app->valid);
    }
}

TEST_CASE("workspace deps: dependency chain of three members validates", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path,
                  "[workspace]\nmembers = [\"app\", \"mid\", \"base\"]\n");
    write_member(tmp.path, "base", "static");
    write_member(tmp.path, "mid", "static", {"base"});
    write_member(tmp.path, "app", "executable", {"mid"});

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    for (const auto& m : ws->members) REQUIRE(m.valid);
}

TEST_CASE("workspace deps: diamond dependency validates", "[workspace][1.3.0-dev.1]") {
    TempDir tmp;
    write_ws_toml(tmp.path,
                  "[workspace]\nmembers = [\"app\", \"l1\", \"l2\", \"base\"]\n");
    write_member(tmp.path, "base", "static");
    write_member(tmp.path, "l1", "static", {"base"});
    write_member(tmp.path, "l2", "static", {"base"});
    write_member(tmp.path, "app", "executable", {"l1", "l2"});

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    for (const auto& m : ws->members) REQUIRE(m.valid);
}
