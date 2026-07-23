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

    try {
        resolve_dependency_order({a.dir});
        FAIL("expected exception was not thrown");
    } catch (const std::runtime_error& e) {
        std::string msg(e.what());
        REQUIRE(msg.find("missing dependency") != std::string::npos);
        REQUIRE(msg.find("nonexistent_dep") != std::string::npos);
    }

    fs::remove_all(base);
}

// ===================================================================
// 0.2.3+: list()
// ===================================================================

TEST_CASE("pkg list: empty install directory shows none", "[pkg][0.2.3]") {
    // list() writes to util::info() — we verify it doesn't crash
    REQUIRE_NOTHROW(list({Scope::Project}));
}

TEST_CASE("pkg list: all scopes listable", "[pkg][0.2.3]") {
    // Verify list() works for all scope combinations
    REQUIRE_NOTHROW(list({Scope::Project, Scope::User, Scope::Global}));
    REQUIRE_NOTHROW(list({Scope::User}));
    REQUIRE_NOTHROW(list({Scope::Global}));
}

// ===================================================================
// 0.2.3+: update() - basic error paths
// ===================================================================

TEST_CASE("pkg update: non-existent package shows error", "[pkg][0.2.3]") {
    // update() of a non-existent package should output an error via util::error()
    // and return without throwing
    REQUIRE_NOTHROW(update("nonexistent_pkg_xyz_12345", {Scope::Project}));
}

// ===================================================================
// 0.9.6+: satisfies_version_constraint()
// ===================================================================

TEST_CASE("satisfies_version_constraint: None constraint always matches", "[pkg][0.9.6]") {
    using namespace ezmk::config;
    VersionConstraint c;  // op = None
    REQUIRE(ezmk::pkg::satisfies_version_constraint("1.0.0", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("0.0.1", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("99.99.99", c));
}

TEST_CASE("satisfies_version_constraint: Exact (@) matches only equal", "[pkg][0.9.6]") {
    using namespace ezmk::config;
    VersionConstraint c;
    c.op = VersionConstraint::Exact;
    c.version = "1.2.3";
    REQUIRE(ezmk::pkg::satisfies_version_constraint("1.2.3", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("1.2.2", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("1.2.4", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("1.3.0", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("2.0.0", c));
}

TEST_CASE("satisfies_version_constraint: Compatible (^) matches within major", "[pkg][0.9.6]") {
    using namespace ezmk::config;
    VersionConstraint c;
    c.op = VersionConstraint::Compatible;
    c.version = "3.6.0";
    // >= 3.6.0, < 4.0.0
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.6.0", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.6.1", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.7.0", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.99.99", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("3.5.0", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("3.5.99", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("4.0.0", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("5.0.0", c));
}

TEST_CASE("satisfies_version_constraint: Approx (~) matches within minor", "[pkg][0.9.6]") {
    using namespace ezmk::config;
    VersionConstraint c;
    c.op = VersionConstraint::Approx;
    c.version = "3.11.0";
    // >= 3.11.0, < 3.12.0
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.11.0", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.11.1", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.11.99", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("3.10.0", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("3.10.99", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("3.12.0", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("4.0.0", c));
}

TEST_CASE("satisfies_version_constraint: Gte (>=) matches equal or greater", "[pkg][0.9.6]") {
    using namespace ezmk::config;
    VersionConstraint c;
    c.op = VersionConstraint::Gte;
    c.version = "2.0.0";
    REQUIRE(ezmk::pkg::satisfies_version_constraint("2.0.0", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("2.0.1", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("2.1.0", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.0.0", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("1.99.99", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("1.0.0", c));
}

TEST_CASE("satisfies_version_constraint: Gt (>) matches strictly greater", "[pkg][0.9.6]") {
    using namespace ezmk::config;
    VersionConstraint c;
    c.op = VersionConstraint::Gt;
    c.version = "2.0.0";
    REQUIRE(ezmk::pkg::satisfies_version_constraint("2.0.1", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("2.1.0", c));
    REQUIRE(ezmk::pkg::satisfies_version_constraint("3.0.0", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("2.0.0", c));
    REQUIRE_FALSE(ezmk::pkg::satisfies_version_constraint("1.99.99", c));
}

// ===================================================================
// detect_install_script() — 0.9.10
// ===================================================================

namespace {
// RAII helper for temporary test directories
struct TempPkg {
    fs::path dir;
    fs::path script_dir;

    explicit TempPkg(const fs::path& base) {
        dir = base / "test_pkg";
        script_dir = dir / "script";
        fs::create_directories(script_dir);
    }

    void create_file(const std::string& name) {
        std::ofstream ofs(script_dir / name);
        ofs << "-- test\n";
    }

    ~TempPkg() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};
} // anonymous namespace

TEST_CASE("detect_install_script: returns .lua when only .lua exists", "[pkg][0.9.10]") {
    TempPkg pkg(fs::temp_directory_path());
    pkg.create_file("test-hook.lua");

    auto result = ezmk::pkg::detect_install_script(pkg.dir, "test-hook");
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.extension() == ".lua");
    REQUIRE(result.filename() == "test-hook.lua");
}

TEST_CASE("detect_install_script: returns empty path when no script exists", "[pkg][0.9.10]") {
    TempPkg pkg(fs::temp_directory_path());
    // script/ directory exists but is empty

    auto result = ezmk::pkg::detect_install_script(pkg.dir, "no-script");
    REQUIRE(result.empty());
}

TEST_CASE("detect_install_script: returns empty path when script/ dir is missing", "[pkg][0.9.10]") {
    fs::path tmp = fs::temp_directory_path() / "ezmk_no_script_dir_test";
    fs::create_directory(tmp);
    // No script/ subdirectory

    auto result = ezmk::pkg::detect_install_script(tmp, "anything");
    REQUIRE(result.empty());

    std::error_code ec;
    fs::remove_all(tmp, ec);
}

TEST_CASE("detect_install_script: .lua preferred over platform-specific script", "[pkg][0.9.10]") {
    TempPkg pkg(fs::temp_directory_path());
    // Create both .lua and platform-specific scripts
    pkg.create_file("hook.lua");
#ifdef EZMK_WIN
    pkg.create_file("hook.ps1");
#else
    pkg.create_file("hook.sh");
#endif

    auto result = ezmk::pkg::detect_install_script(pkg.dir, "hook");
    REQUIRE_FALSE(result.empty());
    // .lua must be preferred regardless of platform
    REQUIRE(result.extension() == ".lua");
    REQUIRE(result.filename() == "hook.lua");
}

TEST_CASE("detect_install_script: falls back to platform script when no .lua", "[pkg][0.9.10]") {
    TempPkg pkg(fs::temp_directory_path());
#ifdef EZMK_WIN
    pkg.create_file("setup.ps1");
#else
    pkg.create_file("setup.sh");
#endif

    auto result = ezmk::pkg::detect_install_script(pkg.dir, "setup");
    REQUIRE_FALSE(result.empty());
    // Must return the platform-specific script
#ifdef EZMK_WIN
    REQUIRE(result.extension() == ".ps1");
#else
    REQUIRE(result.extension() == ".sh");
#endif
}
