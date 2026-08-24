// Unit tests for pkg.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "test_helpers.hpp"
#include "ezmk/pkg.hpp"
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
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

// ===================================================================
// build_archive_command() — 1.1.2 S2 (ar/lib.exe command escaping)
// ===================================================================

TEST_CASE("build_archive_command: escapes paths for ar/lib", "[pkg][1.1.2]") {
    SECTION("gcc ar escapes special chars") {
        auto cmd = build_archive_command(false, "build/lib demo.a",
            {"build/a b$c.o", "build/$(touch /tmp/x).o"});
        REQUIRE(cmd.rfind("ar rcs ", 0) == 0);
        REQUIRE(cmd.find("\"build/lib demo.a\"") != std::string::npos);
        // `$` must be backslash-escaped so sh -c cannot expand it
        REQUIRE(cmd.find("\"build/a b\\$c.o\"") != std::string::npos);
        // `$` must be backslash-escaped inside the quoted segment
        REQUIRE(cmd.find("\\$(touch /tmp/x)") != std::string::npos);
        // and no UNESCAPED `$(` directly after an opening quote (sh -c would expand it)
        REQUIRE(cmd.find("\"$(touch") == std::string::npos);
    }
    SECTION("msvc lib escapes special chars") {
        auto cmd = build_archive_command(true, "lib demo.lib",
            {"build/a b$c.obj"});
        REQUIRE(cmd.rfind("lib.exe /OUT:", 0) == 0);
        REQUIRE(cmd.find("\"lib demo.lib\"") != std::string::npos);
        REQUIRE(cmd.find("\"build/a b\\$c.obj\"") != std::string::npos);
    }
    SECTION("empty object list") {
        auto cmd = build_archive_command(false, "build/lib.a", {});
        REQUIRE(cmd == "ar rcs \"build/lib.a\"");
    }
    SECTION("backtick and backslash escaped") {
        auto cmd = build_archive_command(false, "lib`x.a", {"build/obj\\sub.o"});
        REQUIRE(cmd.find("\"lib\\`x.a\"") != std::string::npos);
        REQUIRE(cmd.find("\"build/obj\\\\sub.o\"") != std::string::npos);
    }
}

// 1.1.3 S2: package name validation — path-traversal defense for names that get
// interpolated into install paths (dest_dir / name).
TEST_CASE("validate_pkg_name: rejects path traversal / unsafe names", "[pkg][1.1.3]") {
    const char* bad[] = {"", ".", "..", "../evil", "a/b", "a\\b", "C:x",
                         "/abs", "\\abs", ".hidden", "foo:bar", "..\\up"};
    for (auto n : bad) {
        INFO("should reject: " << n);
        REQUIRE_THROWS_AS(validate_pkg_name(n), std::runtime_error);
    }
}

TEST_CASE("validate_pkg_name: accepts ordinary package names", "[pkg][1.1.3]") {
    const char* good[] = {"fmt", "zlib", "my-pkg", "libx_1", "pkg123"};
    for (auto n : good) {
        INFO("should accept: " << n);
        REQUIRE_NOTHROW(validate_pkg_name(n));
    }
}

// 1.1.3 S3: URL 安装完整性前置确认
TEST_CASE("url_integrity_confirm: no sha256 without -y cancels URL install", "[pkg][1.1.3]") {
    // Non-interactive stdin → confirm() reads EOF → returns false (cancel)
    std::istringstream empty("");
    auto old = std::cin.rdbuf(empty.rdbuf());
    auto result = url_integrity_confirm("https://example.com/pkg.tar.gz", false, false);
    std::cin.rdbuf(old);
    REQUIRE_FALSE(result);
}

TEST_CASE("url_integrity_confirm: no sha256 but -y proceeds", "[pkg][1.1.3]") {
    REQUIRE(url_integrity_confirm("https://example.com/pkg.tar.gz", false, true));
}

TEST_CASE("url_integrity_confirm: explicit http:// without -y cancels", "[pkg][1.1.3]") {
    std::istringstream empty("");
    auto old = std::cin.rdbuf(empty.rdbuf());
    auto result = url_integrity_confirm("http://example.com/pkg.tar.gz", true, false);
    std::cin.rdbuf(old);
    REQUIRE_FALSE(result);
}

TEST_CASE("url_integrity_confirm: https with sha256 needs no prompt", "[pkg][1.1.3]") {
    REQUIRE(url_integrity_confirm("https://example.com/pkg.tar.gz", true, false));
}

// ===================================================================
// 1.2.0-dev.9: compile_package — [compile].src_dirs / include_dirs 收敛
// ===================================================================

namespace {
fs::path dev9_pkg_dir(const std::string& tag) {
    return fs::temp_directory_path() / ("ezmk_pkg_dev9_" + tag + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}
} // namespace

// header_only 短路前移：无 src_dirs 的 header-only 包不得触发 src_dir_missing，
// 也不得创建 build/ 目录。
TEST_CASE("compile_package: header_only short-circuits without sources", "[pkg][1.2.0-dev.9]") {
    auto pkg = dev9_pkg_dir("header");
    fs::create_directories(pkg / "include");
    std::ofstream(pkg / "ezmk.toml") <<
        "[project]\nname = \"hdronly\"\ntype = \"static\"\nversion = \"1.0.0\"\n"
        "header_only = true\n";
    std::ofstream(pkg / "include" / "hdr.hpp") << "#pragma once\n";

    auto result = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
    bool build_dir_created = fs::exists(pkg / "build");
    fs::remove_all(pkg);

    REQUIRE(result.empty());
    REQUIRE_FALSE(build_dir_created);
}

// precompiled 短路不变：直接返回 lib/ 下选中归档路径，不编译。
TEST_CASE("compile_package: precompiled short-circuits to selected archive", "[pkg][1.2.0-dev.9]") {
    auto pkg = dev9_pkg_dir("precomp");
    fs::create_directories(pkg / "lib");
    std::ofstream(pkg / "ezmk.toml") <<
        "[project]\nname = \"pre\"\ntype = \"static\"\nversion = \"1.0.0\"\n"
        "precompiled = true\n";
    // bare fallback archive（无平台 tag）
    std::ofstream(pkg / "lib" / "libpre.a", std::ios::binary) << "dummy";

    auto result = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
    fs::remove_all(pkg);

    REQUIRE(!result.empty());
    REQUIRE(result.filename() == "libpre.a");
}

// 空源收紧为 fatal：非 header_only/precompiled 却无任何 src_dirs → 对齐项目语义。
TEST_CASE("compile_package: no source files is a fatal error", "[pkg][1.2.0-dev.9]") {
    auto pkg = dev9_pkg_dir("nosrc");
    fs::create_directories(pkg / "include");
    std::ofstream(pkg / "ezmk.toml") <<
        "[project]\nname = \"nosrc\"\ntype = \"static\"\nversion = \"1.0.0\"\n";

    REQUIRE_THROWS_AS(compile_package(pkg, {}, ezmk::toolchain::Toolchain{}),
                      ezmk::fatal_error);
    fs::remove_all(pkg);
}

// 多 src_dirs 生效 + require_main=false：type = "executable" 但无 main.cpp
// 的包（文档默认写法）必须正常编译成静态库，且两个目录的源都被收集。
TEST_CASE("compile_package: multi src_dirs compile, require_main=false", "[pkg][1.2.0-dev.9]") {
    auto pkg = dev9_pkg_dir("multi");
    fs::create_directories(pkg / "include");
    fs::create_directories(pkg / "src");
    fs::create_directories(pkg / "generated");
    std::ofstream(pkg / "ezmk.toml") <<
        "[project]\nname = \"multi\"\ntype = \"executable\"\nversion = \"1.0.0\"\n\n"
        "[compile]\nsrc_dirs = [\"src\", \"generated\"]\n";
    std::ofstream(pkg / "src" / "a.cpp") << "int a() { return 1; }\n";
    std::ofstream(pkg / "generated" / "b.cpp") << "int b() { return 2; }\n";

    auto result = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
    bool lib_exists = fs::exists(pkg / "build" / "libmulti.a");
    fs::remove_all(pkg);

    REQUIRE(!result.empty());
    REQUIRE(result.filename() == "libmulti.a");
    REQUIRE(lib_exists);
}

// 自定义 include_dirs 自编译生效：源文件引用非默认 include/ 目录的头文件，
// 依赖 include_dirs 相对包根解析出 -I。
TEST_CASE("compile_package: custom include_dirs resolve for self-compile", "[pkg][1.2.0-dev.9]") {
    auto pkg = dev9_pkg_dir("incdirs");
    fs::create_directories(pkg / "include");
    fs::create_directories(pkg / "extra");
    fs::create_directories(pkg / "src");
    std::ofstream(pkg / "ezmk.toml") <<
        "[project]\nname = \"incdirs\"\ntype = \"static\"\nversion = \"1.0.0\"\n\n"
        "[compile]\ninclude_dirs = [\"include\", \"extra\"]\n";
    std::ofstream(pkg / "extra" / "extra.hpp") << "#pragma once\n#define EXTRA 7\n";
    std::ofstream(pkg / "src" / "e.cpp") << "#include \"extra.hpp\"\nint e() { return EXTRA; }\n";

    auto result = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
    bool lib_exists = fs::exists(pkg / "build" / "libincdirs.a");
    fs::remove_all(pkg);

    REQUIRE(!result.empty());
    REQUIRE(lib_exists);
}

// ===================================================================
// 1.2.0-dev.10: select_precompiled_variant — 4-level matching matrix
// ===================================================================

namespace {
fs::path dev10_lib(const std::string& tag) {
    auto d = fs::temp_directory_path() / ("ezmk_pkg_dev10_" + tag + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(d);
    return d;
}
void dev10_touch(const fs::path& p) { std::ofstream(p) << "x"; }
} // namespace

TEST_CASE("precompiled: L4 full tag beats os-arch and bare", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("l4");
    dev10_touch(lib / "libex.linux-x64-gcc13-abi11.a");
    dev10_touch(lib / "libex.linux-x64-gcc11-abi11.a");
    dev10_touch(lib / "libex.linux-x64.a");
    dev10_touch(lib / "libex.a");

    auto r = select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.linux-x64-gcc13-abi11.a");
}

TEST_CASE("precompiled: L3 same compiler without abi beats os-arch", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("l3");
    dev10_touch(lib / "libex.linux-x64-gcc13.a");
    dev10_touch(lib / "libex.linux-x64.a");

    auto r = select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.linux-x64-gcc13.a");
}

TEST_CASE("precompiled: L2 os-arch selected when no compiler variant", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("l2");
    dev10_touch(lib / "libex.win-x64.a");
    dev10_touch(lib / "libex.linux-x64.a");

    auto r = select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.linux-x64.a");
}

TEST_CASE("precompiled: L1 bare fallback", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("l1");
    dev10_touch(lib / "libex.a");

    auto r = select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.a");
}

TEST_CASE("precompiled: ABI mismatch skips same-compiler variant", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("abi");
    // gcc13-abi8 is ABI-incompatible with consumer gcc13-abi11 — must be skipped
    dev10_touch(lib / "libex.linux-x64-gcc13-abi8.a");
    REQUIRE_THROWS_AS(select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false),
                      std::runtime_error);
    // ... but the matching abi11 variant wins when present
    dev10_touch(lib / "libex.linux-x64-gcc13-abi11.a");
    auto r = select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.linux-x64-gcc13-abi11.a");
}

TEST_CASE("precompiled: unknown tag segment is not matched", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("unknown");
    dev10_touch(lib / "libex.linux-x64-gcc13-abi11-extra.a");  // "extra" unknown segment
    REQUIRE_THROWS_AS(select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false),
                      std::runtime_error);
    dev10_touch(lib / "libex.linux-x64-gcc13-abi11.a");
    auto r = select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.linux-x64-gcc13-abi11.a");
}

TEST_CASE("precompiled: different compiler is never matched", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("diffcomp");
    dev10_touch(lib / "libex.linux-x64-gcc11.a");
    REQUIRE_THROWS_AS(select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false),
                      std::runtime_error);
    fs::remove_all(lib);
}

TEST_CASE("precompiled: same score ties by lexicographic filename", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("tie");
    dev10_touch(lib / "libex.linux-x64.lib");
    dev10_touch(lib / "libex.linux-x64.a");

    auto r = select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.linux-x64.a");  // ".a" < ".lib"
}

TEST_CASE("precompiled: degradation to os-arch still selects with warning", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("warn");
    dev10_touch(lib / "libex.linux-x64.a");

    auto r = select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.linux-x64.a");  // selected; warning emitted to stderr
}

TEST_CASE("precompiled: strict mode turns degradation into fatal", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("strict");
    dev10_touch(lib / "libex.linux-x64.a");
    REQUIRE_THROWS_AS(select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", true),
                      ezmk::fatal_error);
    fs::remove_all(lib);
}

TEST_CASE("precompiled: no consumer compiler tag never warns or fails strict", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("nocomp");
    dev10_touch(lib / "libex.linux-x64.a");
    // empty consumer compiler tag → L2 is the norm; strict must not fire
    auto r1 = select_precompiled_variant(lib, "ex", "linux-x64", "", "", false);
    auto r2 = select_precompiled_variant(lib, "ex", "linux-x64", "", "", true);
    fs::remove_all(lib);
    REQUIRE(r1.filename() == "libex.linux-x64.a");
    REQUIRE(r2.filename() == "libex.linux-x64.a");
}

TEST_CASE("precompiled: no-match error lists toolchain + available variants", "[pkg][1.2.0-dev.10]") {
    auto lib = dev10_lib("err");
    dev10_touch(lib / "libex.win-x64.a");
    dev10_touch(lib / "libex.linux-x64-gcc11-abi11.a");
    try {
        select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
        FAIL("expected a throw");
    } catch (const std::runtime_error& e) {
        std::string m = e.what();
        fs::remove_all(lib);
        REQUIRE(m.find("gcc13") != std::string::npos);       // full toolchain tag
        REQUIRE(m.find("available") != std::string::npos);
        REQUIRE(m.find("win-x64") != std::string::npos);
        REQUIRE(m.find("gcc11") != std::string::npos);
    }
}

// 1.2.0-dev.11: a missing lib/ dir must be a friendly "no build" error, not a
// raw std::filesystem::filesystem_error from the directory iterator.
TEST_CASE("precompiled: missing lib/ throws friendly error (dev.11)", "[pkg][1.2.0-dev.11]") {
    fs::path lib = fs::temp_directory_path() / "ezmk_pkg_dev10_nonexistent_lib";
    try {
        select_precompiled_variant(lib, "ex", "linux-x64", "gcc13", "abi11", false);
        FAIL("expected a throw");
    } catch (const std::runtime_error& e) {
        REQUIRE(std::string(e.what()).find("no lib/") != std::string::npos);
    }
}

// 1.2.0-dev.11: on an equal-score tie an MSVC consumer prefers .lib (the
// archive format matches the toolchain); GCC/Clang keep the lexicographic
// tie-break (.a).
TEST_CASE("precompiled: MSVC tie prefers .lib over .a (dev.11)", "[pkg][1.2.0-dev.11]") {
    auto lib = dev10_lib("msvctie");
    dev10_touch(lib / "libex.win-x64.a");
    dev10_touch(lib / "libex.win-x64.lib");
    auto r = select_precompiled_variant(lib, "ex", "win-x64", "msvc143", "", false);
    fs::remove_all(lib);
    REQUIRE(r.filename() == "libex.win-x64.lib");

    auto lib2 = dev10_lib("gcctie");
    dev10_touch(lib2 / "libex.win-x64.a");
    dev10_touch(lib2 / "libex.win-x64.lib");
    auto r2 = select_precompiled_variant(lib2, "ex", "win-x64", "gcc13", "abi11", false);
    fs::remove_all(lib2);
    REQUIRE(r2.filename() == "libex.win-x64.a");
}

// ===================================================================
// 1.3.1: install-time language-standard compatibility check (warn, never fail)
// ===================================================================

namespace {

// Capture everything written to std::cerr during `fn` and return it.
template <typename F>
std::string capture_cerr(F&& fn) {
    std::ostringstream captured;
    auto* old = std::cerr.rdbuf(captured.rdbuf());
    fn();
    std::cerr.rdbuf(old);
    return captured.str();
}

// A consumer project root (ezmk.toml with the given language) + CWD guard.
// CWD must be inside the project for locate_project_root() to find it.
struct ConsumerProject {
    fs::path root;
    std::unique_ptr<CwdGuard> guard;

    explicit ConsumerProject(const std::string& language,
                             const std::string& name = "consumer") {
        guard = std::make_unique<CwdGuard>();
        root = guard->temp_dir;
        std::ofstream(root / "ezmk.toml")
            << "[project]\nname = \"" << name << "\"\ntype = \"executable\"\n"
            << "version = \"0.1.0\"\nlanguage = \"" << language << "\"\n\n"
            << "[compile]\ninclude_dirs = [\"include\"]\nsrc_dirs = [\"src\"]\n";
        fs::create_directories(root / "src");
    }
};

// A source package that compiles to a static library.
fs::path stdtest_source_pkg(const std::string& tag, const std::string& language) {
    auto pkg = fs::temp_directory_path() / ("ezmk_pkg_std131_" + tag + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(pkg / "include");
    fs::create_directories(pkg / "src");
    std::ofstream(pkg / "ezmk.toml") <<
        "[project]\nname = \"stdpkg\"\ntype = \"static\"\nversion = \"1.0.0\"\n"
        "language = \"" << language << "\"\n";
    std::ofstream(pkg / "src" / "s.cpp") << "int stdpkg_value() { return 1; }\n";
    return pkg;
}

// A precompiled package dir (lib/ + bare archive).
fs::path stdtest_precompiled_pkg(const std::string& tag, const std::string& language) {
    auto pkg = fs::temp_directory_path() / ("ezmk_pkg_std131pc_" + tag + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(pkg / "lib");
    std::ofstream(pkg / "ezmk.toml") <<
        "[project]\nname = \"stdpkg\"\ntype = \"static\"\nversion = \"1.0.0\"\n"
        "precompiled = true\nlanguage = \"" << language << "\"\n";
    std::ofstream(pkg / "lib" / "libstdpkg.a", std::ios::binary) << "dummy";
    return pkg;
}

} // namespace

// Package needs MORE than the consumer → warn, but install still proceeds.
TEST_CASE("std compat: package min > consumer min warns (source pkg)", "[pkg][1.3.1]") {
    ConsumerProject consumer("C++11");
    auto pkg = stdtest_source_pkg("hi", "C++17");
    std::string out;
    try {
        out = capture_cerr([&] {
            auto r = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
            REQUIRE_FALSE(r.empty());
            REQUIRE(r.filename() == "libstdpkg.a");
        });
    } catch (...) {
        fs::remove_all(pkg);
        throw;
    }
    fs::remove_all(pkg);
    REQUIRE(out.find("requires at least C++17") != std::string::npos);
    REQUIRE(out.find("compiles at C++11") != std::string::npos);
}

// Package needs LESS than the consumer → no warning.
TEST_CASE("std compat: package min <= consumer min is silent (source pkg)", "[pkg][1.3.1]") {
    ConsumerProject consumer("C++17");
    auto pkg = stdtest_source_pkg("lo", "C++11");
    std::string out;
    try {
        out = capture_cerr([&] {
            auto r = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
            REQUIRE_FALSE(r.empty());
        });
    } catch (...) {
        fs::remove_all(pkg);
        throw;
    }
    fs::remove_all(pkg);
    REQUIRE(out.find("requires at least") == std::string::npos);
}

// Range declarations work on both sides: ">=C++17" package vs C++11 consumer.
TEST_CASE("std compat: range declarations participate (source pkg)", "[pkg][1.3.1]") {
    ConsumerProject consumer("C++11");
    auto pkg = stdtest_source_pkg("range", ">=C++17");
    std::string out;
    try {
        out = capture_cerr([&] {
            auto r = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
            REQUIRE_FALSE(r.empty());
        });
    } catch (...) {
        fs::remove_all(pkg);
        throw;
    }
    fs::remove_all(pkg);
    REQUIRE(out.find("requires at least C++17") != std::string::npos);
}

// No consumer project (CWD has no ezmk.toml) → check skipped, no warning.
TEST_CASE("std compat: no consumer project → skipped", "[pkg][1.3.1]") {
    CwdGuard empty_cwd;  // empty temp dir, no ezmk.toml anywhere above
    auto pkg = stdtest_source_pkg("noconsumer", "C++17");
    std::string out;
    try {
        out = capture_cerr([&] {
            auto r = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
            REQUIRE_FALSE(r.empty());
        });
    } catch (...) {
        fs::remove_all(pkg);
        throw;
    }
    fs::remove_all(pkg);
    REQUIRE(out.find("requires at least") == std::string::npos);
}

// Precompiled package over the consumer's standard → ABI-flavored warning.
TEST_CASE("std compat: precompiled pkg warns with ABI wording", "[pkg][1.3.1]") {
    ConsumerProject consumer("C++11");
    auto pkg = stdtest_precompiled_pkg("pc", "C++17");
    std::string out;
    try {
        out = capture_cerr([&] {
            auto r = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
            REQUIRE_FALSE(r.empty());
            REQUIRE(r.filename() == "libstdpkg.a");
        });
    } catch (...) {
        fs::remove_all(pkg);
        throw;
    }
    fs::remove_all(pkg);
    REQUIRE(out.find("requires at least C++17") != std::string::npos);
    REQUIRE(out.find("ABI mismatch") != std::string::npos);
}

// Precompiled package within the consumer's standard → silent.
TEST_CASE("std compat: precompiled pkg within consumer standard is silent", "[pkg][1.3.1]") {
    ConsumerProject consumer("C++17");
    auto pkg = stdtest_precompiled_pkg("pcok", "C++11");
    std::string out;
    try {
        out = capture_cerr([&] {
            auto r = compile_package(pkg, {}, ezmk::toolchain::Toolchain{});
            REQUIRE_FALSE(r.empty());
        });
    } catch (...) {
        fs::remove_all(pkg);
        throw;
    }
    fs::remove_all(pkg);
    REQUIRE(out.find("requires at least") == std::string::npos);
}
