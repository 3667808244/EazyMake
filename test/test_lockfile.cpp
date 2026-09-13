// Unit tests for lockfile.cpp — 1.1.2 C3 (direct_deps + depends_changed)
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/lockfile.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"
#include "ezmk/crypto.hpp"

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

TEST_CASE("lockfile save/load: escapes and round-trips special characters", "[lockfile][1.1.2]") {
    TempDir tmp;
    Lockfile lf;
    lf.version = 1;
    lf.direct_deps = { "my\"lib@^1.0" };
    LockedPackage p;
    p.name = "a\"b";
    p.version = "1.0\nx";   // must not corrupt the lockfile
    lf.packages = { p };

    ezmk::lockfile::save(tmp.path, lf);
    auto loaded = ezmk::lockfile::load(tmp.path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->direct_deps == lf.direct_deps);
    REQUIRE(loaded->packages.size() == 1);
    REQUIRE(loaded->packages[0].name == "a\"b");
    REQUIRE(loaded->packages[0].version == "1.0\nx");
}

// 1.4.1: git-source commit field — optional; written only when non-empty so
// old lockfile output stays byte-stable; parsed back on load.
TEST_CASE("lockfile save/load: optional git commit field round-trips (1.4.1)", "[lockfile][1.4.1]") {
    TempDir tmp;
    Lockfile lf;
    lf.version = 1;
    LockedPackage p;
    p.name = "greet";
    p.version = "1.0.0";
    p.source = "git";
    p.source_url = "https://github.com/user/repo.git";
    p.commit = "0123456789abcdef0123456789abcdef01234567";
    lf.packages = { p };

    ezmk::lockfile::save(tmp.path, lf);
    auto text = ezmk::util::file_read(tmp.path / "ezmk.lock");
    REQUIRE(text.find("commit = \"0123456789abcdef0123456789abcdef01234567\"") != std::string::npos);

    auto loaded = ezmk::lockfile::load(tmp.path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->packages.size() == 1);
    REQUIRE(loaded->packages[0].source == "git");
    REQUIRE(loaded->packages[0].commit == "0123456789abcdef0123456789abcdef01234567");
}

// 1.4.1: a pre-1.4.1 lockfile has no commit field → load leaves it empty
// (no crash, no drift — backward compatible).
TEST_CASE("lockfile load: old lockfile without commit field parses (1.4.1)", "[lockfile][1.4.1]") {
    TempDir tmp;
    // Hand-written old-format ezmk.lock (source/source_url absent).
    ezmk::util::file_write(tmp.path / "ezmk.lock",
        "# ezmk.lock\n"
        "[metadata]\n"
        "version = 1\n"
        "generated_by = \"ezmk 1.4.0\"\n"
        "direct_deps = []\n"
        "\n"
        "[[packages]]\n"
        "name = \"greet\"\n"
        "version = \"1.0.0\"\n"
        "sha256 = \"\"\n"
        "type = \"static\"\n"
        "scope = \"project\"\n"
        "platform = \"windows_x86_64_gcc\"\n"
        "dependencies = []\n");

    auto loaded = ezmk::lockfile::load(tmp.path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->packages.size() == 1);
    REQUIRE(loaded->packages[0].name == "greet");
    REQUIRE(loaded->packages[0].commit.empty());
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

// ===================================================================
// 1.4.2 F-04: archive hash vs artifact hash are different values
// ===================================================================

TEST_CASE("lockfile save/load: archive + lib hashes round-trip (1.4.2 F-04)", "[lockfile][1.4.2]") {
    TempDir tmp;
    Lockfile lf;
    lf.version = 1;
    LockedPackage p;
    p.name = "greet";
    p.version = "1.0.0";
    p.source = "repo1";
    p.source_url = "https://example.com/greet-1.0.0.tar.gz";
    p.archive_sha256 = std::string(64, 'a');
    p.lib_sha256 = std::string(64, 'b');
    p.sha256 = p.lib_sha256;  // legacy alias keeps pre-1.4.2 readers working
    lf.packages = { p };

    ezmk::lockfile::save(tmp.path, lf);
    auto text = ezmk::util::file_read(tmp.path / "ezmk.lock");
    REQUIRE(text.find("archive_sha256 = \"" + p.archive_sha256 + "\"") != std::string::npos);
    REQUIRE(text.find("lib_sha256 = \"" + p.lib_sha256 + "\"") != std::string::npos);
    REQUIRE(text.find("sha256 = \"" + p.lib_sha256 + "\"") != std::string::npos);

    auto loaded = ezmk::lockfile::load(tmp.path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->packages.size() == 1);
    REQUIRE(loaded->packages[0].archive_sha256 == p.archive_sha256);
    REQUIRE(loaded->packages[0].lib_sha256 == p.lib_sha256);
    REQUIRE(ezmk::lockfile::archive_hash(loaded->packages[0]) == p.archive_sha256);
    REQUIRE(ezmk::lockfile::artifact_hash(loaded->packages[0]) == p.lib_sha256);
}

TEST_CASE("lockfile load: legacy sha256 is the artifact hash, archive hash empty (1.4.2 F-04)", "[lockfile][1.4.2]") {
    TempDir tmp;
    ezmk::util::file_write(tmp.path / "ezmk.lock",
        "# ezmk.lock\n"
        "[metadata]\n"
        "version = 1\n"
        "generated_by = \"ezmk 1.4.1\"\n"
        "direct_deps = []\n"
        "\n"
        "[[packages]]\n"
        "name = \"greet\"\n"
        "version = \"1.0.0\"\n"
        "source = \"repo1\"\n"
        "source_url = \"\"\n"
        "sha256 = \"deadbeef\"\n"
        "type = \"static\"\n"
        "scope = \"project\"\n"
        "platform = \"windows_x86_64_gcc\"\n"
        "dependencies = []\n");

    auto loaded = ezmk::lockfile::load(tmp.path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->packages.size() == 1);
    const auto& p = loaded->packages[0];
    // Pre-1.4.2 files only carry the artifact hash — it must be used for the
    // verify-side check, and there is no archive hash to re-verify.
    REQUIRE(ezmk::lockfile::artifact_hash(p) == "deadbeef");
    REQUIRE(ezmk::lockfile::archive_hash(p).empty());
}

TEST_CASE("lockfile verify uses the artifact hash, never the archive hash (1.4.2 F-04)", "[lockfile][1.4.2]") {
    TempDir tmp;
    fs::create_directories(tmp.path / ".ezmk/pkg/greet/build");
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/greet/build/libgreet.a", "artifact-bytes");

    Lockfile lf;
    LockedPackage p;
    p.name = "greet";
    p.scope = "project";
    p.type = "static";
    p.lib_sha256 = ezmk::crypto::sha256_file(tmp.path / ".ezmk/pkg/greet/build/libgreet.a");
    p.archive_sha256 = std::string(64, 'c');  // must NOT be compared to the artifact
    lf.packages = { p };
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf).empty());

    // A wrong artifact hash is still detected — the (correct) archive hash must
    // not mask a tampered library.
    Lockfile lf2 = lf;
    lf2.packages[0].lib_sha256 = std::string(64, 'd');
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf2) ==
            std::vector<std::string>{"greet"});
}

// ===================================================================
// 1.4.2 F-28: verify the previously blind spots
// ===================================================================

TEST_CASE("lockfile verify: header-only payload is content-checked (F-28)", "[lockfile][1.4.2]") {
    TempDir tmp;
    fs::create_directories(tmp.path / ".ezmk/pkg/hdr/include");
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/hdr/include/hdr.hpp",
                           "#pragma once\nint hdr();\n");
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/hdr/include/extra.hpp",
                           "#pragma once\nint extra();\n");

    const std::string manifest =
        ezmk::lockfile::payload_manifest_hash(tmp.path / ".ezmk/pkg/hdr");
    REQUIRE_FALSE(manifest.empty());

    Lockfile lf;
    LockedPackage p;
    p.name = "hdr";
    p.scope = "project";
    p.type = "header-only";
    p.lib_sha256 = manifest;
    p.sha256 = manifest;
    lf.packages = { p };
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf).empty());

    // Tampering with a header breaks the manifest hash.
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/hdr/include/hdr.hpp",
                           "#pragma once\nint hdr(); // tampered\n");
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf) == std::vector<std::string>{"hdr"});

    // Adding a file also breaks it — the manifest covers the whole payload.
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/hdr/include/hdr.hpp",
                           "#pragma once\nint hdr();\n");
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/hdr/include/injected.hpp",
                           "#pragma once\n");
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf) == std::vector<std::string>{"hdr"});
}

TEST_CASE("lockfile verify: legacy header-only entry without hash still passes (F-28)", "[lockfile][1.4.2]") {
    TempDir tmp;
    fs::create_directories(tmp.path / ".ezmk/pkg/hdr/include");
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/hdr/include/hdr.hpp", "#pragma once\n");

    Lockfile lf;
    LockedPackage p;
    p.name = "hdr";
    p.scope = "project";
    p.type = "header-only";   // no lib_sha256 / sha256 recorded (pre-1.4.2 lockfile)
    lf.packages = { p };
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf).empty());
}

TEST_CASE("lockfile verify: git source checks the commit marker (F-28)", "[lockfile][1.4.2]") {
    TempDir tmp;
    fs::create_directories(tmp.path / ".ezmk/pkg/gitpkg");
    const std::string commit = "0123456789abcdef0123456789abcdef01234567";
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/gitpkg/.ezmk-git-source",
                           "https://example.com/x.git\n" + commit + "\n");

    Lockfile lf;
    LockedPackage p;
    p.name = "gitpkg";
    p.scope = "project";
    p.type = "static";
    p.source = "git";
    p.source_url = "https://example.com/x.git";
    p.commit = commit;
    lf.packages = { p };
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf).empty());

    // Deleted marker → mismatch.
    fs::remove(tmp.path / ".ezmk/pkg/gitpkg/.ezmk-git-source");
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf) == std::vector<std::string>{"gitpkg"});

    // Marker pointing at a DIFFERENT commit → mismatch (source drifted).
    ezmk::util::file_write(tmp.path / ".ezmk/pkg/gitpkg/.ezmk-git-source",
                           "https://example.com/x.git\n" + std::string(40, 'f') + "\n");
    REQUIRE(ezmk::lockfile::verify(tmp.path, lf) == std::vector<std::string>{"gitpkg"});
}
