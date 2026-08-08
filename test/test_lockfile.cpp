// Unit tests for lockfile.cpp — 1.1.2 C3 (direct_deps + depends_changed)
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/lockfile.hpp"
#include "ezmk/config.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::config;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("ezmk_lockfile_test_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

DependsEntry dep_plain(const std::string& name) {
    DependsEntry d;
    d.name = name;
    return d;
}

DependsEntry dep_compat(const std::string& name, const std::string& ver) {
    DependsEntry d;
    d.name = name;
    d.constraint.op = VersionConstraint::Compatible;
    d.constraint.version = ver;
    return d;
}

} // namespace

TEST_CASE("direct_dep_specs: formats name / name@spec, sorted, lib+want", "[lockfile][1.1.2]") {
    EzConfig cfg;
    cfg.depends.libs = { dep_plain("mylib"), dep_compat("fmt", "1.2.3") };
    cfg.depends.want = { dep_plain("sdl2") };

    auto specs = ezmk::lockfile::direct_dep_specs(cfg);
    REQUIRE(specs == std::vector<std::string>({"fmt@^1.2.3", "mylib", "sdl2"}));
}

TEST_CASE("lockfile save/load: round-trips direct_deps", "[lockfile][1.1.2]") {
    TempDir tmp;
    Lockfile lf;
    lf.version = 1;
    lf.generated_by = "test";
    lf.direct_deps = { "a@^1.0", "b" };

    LockedPackage p;
    p.name = "a";
    p.version = "1.0";
    lf.packages = { p };

    ezmk::lockfile::save(tmp.path, lf);
    auto loaded = ezmk::lockfile::load(tmp.path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->direct_deps == lf.direct_deps);
    REQUIRE(loaded->packages.size() == 1);
    REQUIRE(loaded->packages[0].name == "a");
}

TEST_CASE("depends_changed: transitive deps in packages do NOT trip a change", "[lockfile][1.1.2]") {
    // The bug this guards: cfg direct deps were compared against ALL lockfile
    // packages (incl. transitive/auto-installed), so --locked always fataled.
    EzConfig cfg;
    cfg.depends.libs = { dep_plain("mylib") };

    Lockfile lf;
    lf.direct_deps = { "mylib" };            // matches cfg
    LockedPackage p1; p1.name = "mylib";
    LockedPackage p2; p2.name = "fmt";   // transitive, NOT a direct dep
    lf.packages = { p1, p2 };

    REQUIRE_FALSE(ezmk::lockfile::depends_changed(cfg, lf));
}

TEST_CASE("depends_changed: direct dep constraint change trips a change", "[lockfile][1.1.2]") {
    Lockfile lf;
    lf.direct_deps = { "fmt@^1.0" };

    EzConfig cfg;
    cfg.depends.libs = { dep_compat("fmt", "2.0") };

    REQUIRE(ezmk::lockfile::depends_changed(cfg, lf));
}

TEST_CASE("depends_changed: added [depends.want] trips a change", "[lockfile][1.1.2]") {
    Lockfile lf;
    lf.direct_deps = { "mylib" };

    EzConfig cfg;
    cfg.depends.libs = { dep_plain("mylib") };
    cfg.depends.want = { dep_plain("sdl2") };

    REQUIRE(ezmk::lockfile::depends_changed(cfg, lf));
}

TEST_CASE("depends_changed: legacy lockfile without direct_deps is treated as changed", "[lockfile][1.1.2]") {
    // Pre-1.1.2 lockfile: has packages but no direct_deps field.
    Lockfile lf;
    LockedPackage p; p.name = "mylib";
    lf.packages = { p };

    EzConfig cfg;
    cfg.depends.libs = { dep_plain("mylib") };

    REQUIRE(ezmk::lockfile::depends_changed(cfg, lf));
}

TEST_CASE("depends_changed: no deps anywhere → not changed", "[lockfile][1.1.2]") {
    EzConfig cfg;
    Lockfile lf;  // empty direct_deps AND empty packages
    REQUIRE_FALSE(ezmk::lockfile::depends_changed(cfg, lf));
}
