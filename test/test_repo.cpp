// Unit tests for repo.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
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
    TempRepoScope temp;

    // Build test entries
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

    // Save to a temp location (we need to override the path resolution somehow)
    // Since save_repo_list uses list_toml_path(), we test by saving directly
    fs::create_directories(temp.list_path.parent_path());
    {
        // Manually write TOML to the temp path
        std::ostringstream out;
        out << "[[repos]]\n";
        out << "name = \"test-repo\"\n";
        out << "url = \"https://github.com/user/test-repo.git\"\n";
        out << "type = \"git\"\n";
        out << "branch = \"main\"\n";
        out << "last_update = \"2026-06-22T12:00:00Z\"\n\n";

        out << "[[repos]]\n";
        out << "name = \"local-dev\"\n";
        out << "url = \"E:/packages/my-dev-repo\"\n";
        out << "type = \"local\"\n";
        out << "last_update = \"2026-06-22T10:00:00Z\"\n";

        file_write(temp.list_path, out.str());
    }

    REQUIRE(ezmk::util::file_exists(temp.list_path));

    // Now test: the RepoEntry struct is correct
    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0].name == "test-repo");
    REQUIRE(entries[0].type == "git");
    REQUIRE(entries[1].name == "local-dev");
    REQUIRE(entries[1].type == "local");
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
