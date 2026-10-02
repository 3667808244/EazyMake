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
// registry paths: repo_list_path() / legacy_repo_list_path() / cache_dir()
// ===================================================================

TEST_CASE("repo_list_path: returns non-empty paths for every scope", "[repo]") {
    REQUIRE_FALSE(repo_list_path(Scope::Project).empty());
    REQUIRE_FALSE(repo_list_path(Scope::User).empty());
    REQUIRE_FALSE(repo_list_path(Scope::Global).empty());
}

TEST_CASE("repo_list_path: different scopes are different paths", "[repo]") {
    auto proj = repo_list_path(Scope::Project);
    auto user = repo_list_path(Scope::User);
    auto global = repo_list_path(Scope::Global);

    REQUIRE(proj != user);
    REQUIRE(user != global);
    REQUIRE(proj != global);
}

TEST_CASE("registry paths: json is the written name, list_toml_path is the legacy alias (1.4.5)",
          "[repo][1.4.5]") {
    REQUIRE(repo_list_path(Scope::Project).filename() == "list.json");
    REQUIRE(legacy_repo_list_path(Scope::Project).filename() == "list.toml");

    // The historical name must keep returning the path its name promises; it is
    // removed in 2.0.0 (REMOVALS R-04).
    REQUIRE(list_toml_path(Scope::Project) == legacy_repo_list_path(Scope::Project));
    // …and both live in the same per-scope directory (the migration target).
    REQUIRE(repo_list_path(Scope::Project).parent_path() ==
            legacy_repo_list_path(Scope::Project).parent_path());
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

// Helper: create a temp scope-like directory for testing the registry
struct TempRepoScope {
    fs::path base;
    fs::path list_path;

    TempRepoScope() {
        base = fs::temp_directory_path() / ("ezmk_repo_test_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(base);
        list_path = base / "list.json";
    }
    ~TempRepoScope() {
        std::error_code ec;
        fs::remove_all(base, ec);
    }
};

TEST_CASE("load_repo_list: empty when file doesn't exist", "[repo]") {
    auto entries = load_repo_list(Scope::Project);
    // If no registry exists in the project, should return empty
    // (this test relies on the fact that there is no .ezmk/repo/list.{json,toml}
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
    fs::path list_path = repo_list_path(Scope::Project);
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

// ===================================================================
// 1.4.2 F-24: repo name validation
// ===================================================================

TEST_CASE("repo remove/info: reject an unsafe repo name (1.4.2 F-24)", "[repo][1.4.2]") {
    // `repo remove` deletes cache_dir(name) recursively — a name escaping the
    // cache tree must be rejected before any filesystem work.
    REQUIRE_THROWS_AS(ezmk::repo::remove("../evil", {Scope::Project}),
                      std::runtime_error);
    REQUIRE_THROWS_AS(ezmk::repo::remove("a/b", {Scope::Project}),
                      std::runtime_error);
    REQUIRE_THROWS_AS(ezmk::repo::info("../evil", {Scope::Project}),
                      std::runtime_error);
    REQUIRE_THROWS_AS(ezmk::repo::update("../evil", {Scope::Project}),
                      std::runtime_error);
}

TEST_CASE("load_repo_list: unsafe names from a legacy list.toml are skipped (1.4.2 F-24)", "[repo][1.4.2]") {
    CwdGuard cwd;  // a hand-editable registry lives under the (temp) CWD
    auto path = legacy_repo_list_path(Scope::Project);
    fs::create_directories(path.parent_path());
    ezmk::util::file_write(path,
        "[[repos]]\nname = \"../evil\"\nurl = \"https://example.com/x.git\"\n"
        "type = \"git\"\nbranch = \"main\"\nlast_update = \"\"\n\n"
        "[[repos]]\nname = \"good\"\nurl = \"https://example.com/y.git\"\n"
        "type = \"git\"\nbranch = \"main\"\nlast_update = \"\"\n");

    auto entries = load_repo_list(Scope::Project);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].name == "good");
}

// ===================================================================
// 1.4.5: list.toml → list.json (JSON writer, dual read, migration)
// ===================================================================

TEST_CASE("load_repo_list: unsafe names from list.json are skipped too (F-24 / 1.4.5)", "[repo][1.4.5]") {
    CwdGuard cwd;
    auto path = repo_list_path(Scope::Project);
    fs::create_directories(path.parent_path());
    ezmk::util::file_write(path,
        "{\n"
        "  \"version\": 1,\n"
        "  \"repos\": [\n"
        "    { \"name\": \"../evil\", \"url\": \"https://example.com/x.git\","
        " \"type\": \"git\", \"branch\": \"main\", \"last_update\": \"\" },\n"
        "    { \"name\": \"good\", \"url\": \"https://example.com/y.git\","
        " \"type\": \"git\", \"branch\": \"main\", \"last_update\": \"\" }\n"
        "  ]\n"
        "}\n");

    auto entries = load_repo_list(Scope::Project);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].name == "good");
}

TEST_CASE("save_repo_list: writes list.json and migrates a legacy file away (1.4.5)", "[repo][1.4.5]") {
    CwdGuard cwd;
    auto legacy = legacy_repo_list_path(Scope::Project);
    fs::create_directories(legacy.parent_path());
    ezmk::util::file_write(legacy,
        "[[repos]]\nname = \"old\"\nurl = \"https://example.com/old.git\"\n"
        "type = \"git\"\nbranch = \"main\"\nlast_update = \"\"\n");

    std::vector<RepoEntry> entries;
    RepoEntry e;
    e.name = "fresh";
    e.url = "https://example.com/fresh.git";
    e.type = "git";
    e.branch = "main";
    e.last_update = "2026-09-27T00:00:00Z";
    entries.push_back(e);

    save_repo_list(Scope::Project, entries);

    REQUIRE(fs::exists(repo_list_path(Scope::Project)));
    REQUIRE_FALSE(fs::exists(legacy));

    auto loaded = load_repo_list(Scope::Project);
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].name == "fresh");
    REQUIRE(loaded[0].branch == "main");
}

TEST_CASE("load_repo_list: a legacy list.toml still loads (1.4.5)", "[repo][1.4.5]") {
    CwdGuard cwd;
    auto legacy = legacy_repo_list_path(Scope::Project);
    fs::create_directories(legacy.parent_path());
    ezmk::util::file_write(legacy,
        "[[repos]]\nname = \"legacy-repo\"\nurl = \"https://example.com/l.git\"\n"
        "type = \"git\"\nbranch = \"develop\"\nlast_update = \"2026-09-26T00:00:00Z\"\n");

    auto entries = load_repo_list(Scope::Project);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].name == "legacy-repo");
    REQUIRE(entries[0].url == "https://example.com/l.git");
    REQUIRE(entries[0].type == "git");
    REQUIRE(entries[0].branch == "develop");
    REQUIRE(entries[0].last_update == "2026-09-26T00:00:00Z");
}

TEST_CASE("load_repo_list: json wins over a stale legacy file (1.4.5)", "[repo][1.4.5]") {
    CwdGuard cwd;
    auto legacy = legacy_repo_list_path(Scope::Project);
    auto json_path = repo_list_path(Scope::Project);
    fs::create_directories(json_path.parent_path());

    ezmk::util::file_write(legacy,
        "[[repos]]\nname = \"stale\"\nurl = \"https://example.com/s.git\"\n"
        "type = \"git\"\nbranch = \"main\"\nlast_update = \"\"\n");
    ezmk::util::file_write(json_path,
        "{ \"version\": 1, \"repos\": [ { \"name\": \"fresh\","
        " \"url\": \"https://example.com/f.git\", \"type\": \"git\","
        " \"branch\": \"main\", \"last_update\": \"\" } ] }\n");

    auto entries = load_repo_list(Scope::Project);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].name == "fresh");
}

TEST_CASE("load_repo_list: local repos carry no branch in json, default on load (1.4.5)", "[repo][1.4.5]") {
    CwdGuard cwd;
    std::vector<RepoEntry> entries;
    RepoEntry e;
    e.name = "local-dev";
    e.url = "E:/packages/local-dev";
    e.type = "local";
    e.last_update = "2026-09-27T00:00:00Z";
    entries.push_back(e);

    save_repo_list(Scope::Project, entries);
    auto text = ezmk::util::file_read(repo_list_path(Scope::Project));
    REQUIRE(text.find("branch") == std::string::npos);

    auto loaded = load_repo_list(Scope::Project);
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].type == "local");
    REQUIRE(loaded[0].branch == "main");   // default re-applied on read
}
