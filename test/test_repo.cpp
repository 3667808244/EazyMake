// Unit tests for repo.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "test_helpers.hpp"
#include "ezmk/repo.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::repo;
using namespace ezmk::cli;
using namespace ezmk::util;

// ===================================================================
// list_toml_path() / cache_dir()
// ===================================================================

TEST_CASE("list_toml_path: returns non-empty paths", "[repo]") {
    auto proj = list_toml_path(Scope::Project);
    auto user = list_toml_path(Scope::User);
    auto global = list_toml_path(Scope::Global);

    REQUIRE_FALSE(proj.empty());
    REQUIRE_FALSE(user.empty());
    REQUIRE_FALSE(global.empty());
}

TEST_CASE("list_toml_path: different scopes are different paths", "[repo]") {
    auto proj = list_toml_path(Scope::Project);
    auto user = list_toml_path(Scope::User);
    auto global = list_toml_path(Scope::Global);

    REQUIRE(proj != user);
    REQUIRE(user != global);
    REQUIRE(proj != global);
}

TEST_CASE("list_toml_path: filename is list.toml", "[repo]") {
    auto path = list_toml_path(Scope::Project);
    REQUIRE(path.filename() == "list.toml");
}

TEST_CASE("cache_dir: returns non-empty paths", "[repo]") {
    auto proj = cache_dir(Scope::Project, "test-repo");
    auto user = cache_dir(Scope::User, "test-repo");
    auto global = cache_dir(Scope::Global, "test-repo");

    REQUIRE_FALSE(proj.empty());
    REQUIRE_FALSE(user.empty());
    REQUIRE_FALSE(global.empty());
}

TEST_CASE("cache_dir: different scopes are different paths", "[repo]") {
    auto proj = cache_dir(Scope::Project, "repo");
    auto user = cache_dir(Scope::User, "repo");

    REQUIRE(proj != user);
}

TEST_CASE("cache_dir: includes repo name in path", "[repo]") {
    auto path = cache_dir(Scope::Project, "my-cool-repo");
    REQUIRE(path.filename() == "my-cool-repo");
}

// ===================================================================
// load_repo_list() / save_repo_list() round-trip
// ===================================================================

// Helper: create a temp scope-like directory for testing list.toml
struct TempRepoScope {
    fs::path base;
    fs::path list_path;

    TempRepoScope() {
        base = fs::temp_directory_path() / ("ezmk_repo_test_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(base);
        list_path = base / "list.toml";
    }
    ~TempRepoScope() {
        std::error_code ec;
        fs::remove_all(base, ec);
    }
};

TEST_CASE("load_repo_list: empty when file doesn't exist", "[repo]") {
    auto entries = load_repo_list(Scope::Project);
    // If no list.toml exists in the project, should return empty
    // (this test relies on the fact that there is no .ezmk/repo/list.toml
    //  in the test binary's working directory)
    REQUIRE(entries.empty());
}

TEST_CASE("load_repo_list + save_repo_list: round-trip", "[repo]") {
    // 1.4.0-dev.5: the old test never called either function (it hand-wrote
    // TOML and asserted on a locally-built vector) — zero coverage of the
    // serialize/deserialize pair. Now: chdir into a temp project (project
    // scope list.toml resolves to CWD when no ezmk.toml is found, or to the
    // located root), save via the real API, load back, and compare.
    TempDir tmp;
    CwdGuard cwd;  // chdirs to a temp dir; Project scope resolves to it
    fs::path list_path = list_toml_path(Scope::Project);
    REQUIRE_FALSE(list_path.empty());

    std::vector<RepoEntry> entries;
    RepoEntry e1;
    e1.name = "test-repo";
    e1.url = "https://github.com/user/test-repo.git";
    e1.type = "git";
    e1.branch = "main";
    e1.last_update = "2026-06-22T12:00:00Z";
    entries.push_back(e1);
    RepoEntry e2;
    e2.name = "local-dev";
    e2.url = "E:/packages/my-dev-repo";
    e2.type = "local";
    e2.last_update = "2026-06-22T10:00:00Z";
    entries.push_back(e2);

    save_repo_list(Scope::Project, entries);
    REQUIRE(fs::exists(list_path));

    auto loaded = load_repo_list(Scope::Project);
    REQUIRE(loaded.size() == 2);
    REQUIRE(loaded[0].name == "test-repo");
    REQUIRE(loaded[0].url == "https://github.com/user/test-repo.git");
    REQUIRE(loaded[0].type == "git");
    REQUIRE(loaded[0].branch == "main");
    REQUIRE(loaded[0].last_update == "2026-06-22T12:00:00Z");
    REQUIRE(loaded[1].name == "local-dev");
    REQUIRE(loaded[1].url == "E:/packages/my-dev-repo");
    REQUIRE(loaded[1].type == "local");
    REQUIRE(loaded[1].last_update == "2026-06-22T10:00:00Z");
}

// ===================================================================
// RepoEntry struct
// ===================================================================

TEST_CASE("RepoEntry: default values", "[repo]") {
    RepoEntry e;
    REQUIRE(e.name.empty());
    REQUIRE(e.url.empty());
    REQUIRE(e.type == "git");
    REQUIRE(e.branch == "main");
    REQUIRE(e.last_update.empty());
}

TEST_CASE("RepoEntry: assigned values", "[repo]") {
    RepoEntry e;
    e.name = "test";
    e.url = "https://example.com/repo.git";
    e.type = "git";
    e.branch = "develop";
    e.last_update = "2026-01-01T00:00:00Z";

    REQUIRE(e.name == "test");
    REQUIRE(e.url == "https://example.com/repo.git");
    REQUIRE(e.type == "git");
    REQUIRE(e.branch == "develop");
    REQUIRE(e.last_update == "2026-01-01T00:00:00Z");
}

// ===================================================================
// search_package: empty when no repos registered
// ===================================================================

TEST_CASE("search_package: returns empty for unknown package", "[repo]") {
    auto result = search_package("definitely_not_a_real_package",
                                  {Scope::Project});
    REQUIRE(result.archive_path.empty());
    REQUIRE(result.sha256.empty());
}

// ===================================================================
// 0.2.5 — repo info
// ===================================================================

TEST_CASE("repo info: not found does not throw", "[repo][info]") {
    REQUIRE_NOTHROW(info("nonexistent_repo_xyz_12345", {Scope::Project}));
}

// ===================================================================
// 0.2.5 — search_package: cross-repo features
// ===================================================================

TEST_CASE("search_package: repo_name field default empty", "[repo][search]") {
    PkgSearchResult r;
    REQUIRE(r.repo_name.empty());
}

TEST_CASE("search_package: repo_name can be set", "[repo][search]") {
    PkgSearchResult r;
    r.repo_name = "community";
    REQUIRE(r.repo_name == "community");
}

// ===================================================================
// 0.2.5 — local repo validation (structural tests)
// ===================================================================

TEST_CASE("RepoEntry: local repo fields", "[repo][validate]") {
    RepoEntry e;
    e.name = "local-dev";
    e.type = "local";
    e.url = "E:/packages/my-dev-repo";
    REQUIRE(e.type == "local");
    REQUIRE(e.url == "E:/packages/my-dev-repo");
}
