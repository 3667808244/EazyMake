// Unit tests for pkg.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/pkg.hpp"
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::pkg;
using namespace ezmk::cli;
using namespace ezmk::util;

// ===================================================================
// pkg_install_dir()
// ===================================================================

TEST_CASE("pkg_install_dir: returns non-empty paths", "[pkg]") {
    auto proj = pkg_install_dir(Scope::Project);
    auto user = pkg_install_dir(Scope::User);
    auto global = pkg_install_dir(Scope::Global);

    REQUIRE_FALSE(proj.empty());
    REQUIRE_FALSE(user.empty());
    REQUIRE_FALSE(global.empty());
}

TEST_CASE("pkg_install_dir: project scope is under .ezmk/pkg", "[pkg]") {
    auto dir = pkg_install_dir(Scope::Project);
    REQUIRE(dir.filename() == "pkg");
}

TEST_CASE("pkg_install_dir: different scopes are different", "[pkg]") {
    auto proj = pkg_install_dir(Scope::Project);
    auto user = pkg_install_dir(Scope::User);
    auto global = pkg_install_dir(Scope::Global);

    // All three should be different paths
    REQUIRE(proj != user);
    REQUIRE(user != global);
    REQUIRE(proj != global);
}

// ===================================================================
// pkg_search_dirs()
// ===================================================================

TEST_CASE("pkg_search_dirs: single scope", "[pkg]") {
    auto dirs = pkg_search_dirs({Scope::Project});
    REQUIRE(dirs.size() == 1);
}

TEST_CASE("pkg_search_dirs: multiple scopes", "[pkg]") {
    auto dirs = pkg_search_dirs({Scope::Project, Scope::User, Scope::Global});
    REQUIRE(dirs.size() == 3);
}

TEST_CASE("pkg_search_dirs: order is preserved", "[pkg]") {
    auto dirs = pkg_search_dirs({Scope::Global, Scope::Project});
    REQUIRE(dirs.size() == 2);
    REQUIRE(dirs[0] == pkg_install_dir(Scope::Global));
    REQUIRE(dirs[1] == pkg_install_dir(Scope::Project));
}

TEST_CASE("pkg_search_dirs: empty scopes", "[pkg]") {
    auto dirs = pkg_search_dirs({});
    REQUIRE(dirs.empty());
}

// ===================================================================
// search()
// ===================================================================

TEST_CASE("pkg search: non-existent package returns empty", "[pkg]") {
    auto results = search("nonexistent_pkg_12345", {Scope::Project});
    REQUIRE(results.empty());
}

// ===================================================================
// resolve_dependency_order() — topological sort
// ===================================================================

// Helper: create minimal package directories with ezmk.toml + deps
struct PkgDir {
    fs::path dir;
    std::string name;

    PkgDir(const fs::path& base, const std::string& n,
           const std::vector<std::string>& deps = {})
        : dir(base / n), name(n)
    {
        fs::create_directories(dir / "include");
        fs::create_directories(dir / "src");

        std::string toml = "[project]\n";
        toml += "name = \"" + name + "\"\n";
        toml += "version = \"1.0.0\"\n\n";
        toml += "[depends]\n";
        toml += "lib = [";
        for (size_t i = 0; i < deps.size(); ++i) {
            if (i > 0) toml += ", ";
            toml += "\"" + deps[i] + "\"";
        }
        toml += "]\n";

        file_write(dir / "ezmk.toml", toml);
    }
};

TEST_CASE("resolve_dependency_order: empty list", "[pkg]") {
    auto result = resolve_dependency_order({});
    REQUIRE(result.empty());
}

TEST_CASE("resolve_dependency_order: single package with no deps", "[pkg]") {
    auto base = fs::temp_directory_path() / ("ezmk_rdo_single_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base);

    PkgDir a(base, "a");

    auto result = resolve_dependency_order({a.dir});
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == a.dir);

    fs::remove_all(base);
}

TEST_CASE("resolve_dependency_order: two independent packages", "[pkg]") {
    auto base = fs::temp_directory_path() / ("ezmk_rdo_indep_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base);

    PkgDir a(base, "a");
    PkgDir b(base, "b");

    auto result = resolve_dependency_order({a.dir, b.dir});
    REQUIRE(result.size() == 2);

    fs::remove_all(base);
}

TEST_CASE("resolve_dependency_order: linear chain A → B → C", "[pkg]") {
    auto base = fs::temp_directory_path() / ("ezmk_rdo_linear_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base);

    PkgDir a(base, "a", {"b"});
    PkgDir b(base, "b", {"c"});
    PkgDir c(base, "c");

    auto result = resolve_dependency_order({a.dir, b.dir, c.dir});
    REQUIRE(result.size() == 3);

    // C must come before B, B before A
    auto pos_c = std::find(result.begin(), result.end(), c.dir) - result.begin();
    auto pos_b = std::find(result.begin(), result.end(), b.dir) - result.begin();
    auto pos_a = std::find(result.begin(), result.end(), a.dir) - result.begin();

    REQUIRE(pos_c < pos_b);
    REQUIRE(pos_b < pos_a);

    fs::remove_all(base);
}

TEST_CASE("resolve_dependency_order: diamond dependency", "[pkg]") {
    // A → B, A → C, B → D, C → D
    auto base = fs::temp_directory_path() / ("ezmk_rdo_diamond_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base);

    PkgDir a(base, "a", {"b", "c"});
    PkgDir b(base, "b", {"d"});
    PkgDir c(base, "c", {"d"});
    PkgDir d(base, "d");

    auto result = resolve_dependency_order({a.dir, b.dir, c.dir, d.dir});
    REQUIRE(result.size() == 4);

    // D must be first
    REQUIRE(result[0] == d.dir);
    // A must be last
    REQUIRE(result[3] == a.dir);

    fs::remove_all(base);
}

TEST_CASE("resolve_dependency_order: circular dependency throws", "[pkg]") {
    auto base = fs::temp_directory_path() / ("ezmk_rdo_circular_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base);

    PkgDir a(base, "a", {"b"});
    PkgDir b(base, "b", {"a"});

    REQUIRE_THROWS_AS(resolve_dependency_order({a.dir, b.dir}), std::runtime_error);

    fs::remove_all(base);
}

TEST_CASE("resolve_dependency_order: missing dependency throws", "[pkg]") {
    auto base = fs::temp_directory_path() / ("ezmk_rdo_missing_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base);

    PkgDir a(base, "a", {"nonexistent_dep"});

    REQUIRE_THROWS_AS(resolve_dependency_order({a.dir}), std::runtime_error);

    fs::remove_all(base);
}
