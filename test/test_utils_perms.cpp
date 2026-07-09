// Unit tests for 0.2.5 utils permission model (pure check functions).
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/lua_api.hpp"
#include "ezmk/config.hpp"

#include <filesystem>
#include <optional>

namespace fs = std::filesystem;
using ezmk::lua::PermResult;
using ezmk::lua::PermCategory;
using ezmk::config::UtilsPermissions;

// A stable fake project root for path-relative matching. The check functions
// do no I/O, so the directory need not exist.
static const fs::path ROOT =
#if defined(_WIN32)
    "C:/proj";
#else
    "/proj";
#endif
static const fs::path PKG = ROOT / ".ezmk/pkg/ezmk-cc";

static UtilsPermissions make_perms() {
    UtilsPermissions p;
    p.read       = {"src/", "include/", "ezmk.toml"};
    p.read_deny  = {"**/.ssh/", "*.key", ".env"};
    p.write      = {"build/", ".ezmk/cache/"};
    p.write_deny = {};
    p.run        = {"g++", "clang++", "git*"};
    p.run_deny   = {"rm", "curl"};
    return p;
}

// ===================================================================
// check_read_permission
// ===================================================================

TEST_CASE("perm read: nullopt is Allow (backward compat)", "[perms]") {
    std::optional<UtilsPermissions> none;
    REQUIRE(ezmk::lua::check_read_permission(ROOT / "anything.txt", none, ROOT, PKG)
            == PermResult::Allow);
}

TEST_CASE("perm read: allow-list hit → Allow", "[perms]") {
    auto p = std::make_optional(make_perms());
    REQUIRE(ezmk::lua::check_read_permission(ROOT / "src/main.cpp", p, ROOT, PKG)
            == PermResult::Allow);
}

TEST_CASE("perm read: deny-list hit → Deny even if under allow", "[perms]") {
    auto p = std::make_optional(make_perms());
    // .env is denied; also make sure a .ssh path is denied via glob.
    REQUIRE(ezmk::lua::check_read_permission(ROOT / ".env", p, ROOT, PKG)
            == PermResult::Deny);
    REQUIRE(ezmk::lua::check_read_permission(ROOT / "home/.ssh/id_rsa", p, ROOT, PKG)
            == PermResult::Deny);
}

TEST_CASE("perm read: deny wins when target is also in allow", "[perms]") {
    UtilsPermissions p;
    p.read      = {"src/"};
    p.read_deny = {"src/secret.txt"};
    auto op = std::make_optional(p);
    REQUIRE(ezmk::lua::check_read_permission(ROOT / "src/secret.txt", op, ROOT, PKG)
            == PermResult::Deny);
    REQUIRE(ezmk::lua::check_read_permission(ROOT / "src/main.cpp", op, ROOT, PKG)
            == PermResult::Allow);
}

TEST_CASE("perm read: neither list → Ask", "[perms]") {
    auto p = std::make_optional(make_perms());
    REQUIRE(ezmk::lua::check_read_permission(ROOT / "external/foo.txt", p, ROOT, PKG)
            == PermResult::Ask);
}

TEST_CASE("perm read: ezmk.toml implicitly allowed", "[perms]") {
    UtilsPermissions p; // empty lists
    auto op = std::make_optional(p);
    REQUIRE(ezmk::lua::check_read_permission(ROOT / "ezmk.toml", op, ROOT, PKG)
            == PermResult::Allow);
}

TEST_CASE("perm read: pkg_dir implicitly allowed", "[perms]") {
    UtilsPermissions p; // empty lists
    auto op = std::make_optional(p);
    REQUIRE(ezmk::lua::check_read_permission(PKG / "utils/cc.lua", op, ROOT, PKG)
            == PermResult::Allow);
}

TEST_CASE("perm read: implicit allow overridden by read_deny", "[perms]") {
    UtilsPermissions p;
    p.read_deny = {"ezmk.toml"};
    auto op = std::make_optional(p);
    REQUIRE(ezmk::lua::check_read_permission(ROOT / "ezmk.toml", op, ROOT, PKG)
            == PermResult::Deny);
}

// ===================================================================
// check_write_permission
// ===================================================================

TEST_CASE("perm write: nullopt is Allow", "[perms]") {
    std::optional<UtilsPermissions> none;
    REQUIRE(ezmk::lua::check_write_permission(ROOT / "build/x.o", none, ROOT)
            == PermResult::Allow);
}

TEST_CASE("perm write: allow / deny / ask", "[perms]") {
    auto p = std::make_optional(make_perms());
    REQUIRE(ezmk::lua::check_write_permission(ROOT / "build/x.o", p, ROOT)
            == PermResult::Allow);
    REQUIRE(ezmk::lua::check_write_permission(ROOT / "src/main.cpp", p, ROOT)
            == PermResult::Ask);
}

TEST_CASE("perm write: deny wins", "[perms]") {
    UtilsPermissions p;
    p.write      = {"build/"};
    p.write_deny = {"build/protected/"};
    auto op = std::make_optional(p);
    REQUIRE(ezmk::lua::check_write_permission(ROOT / "build/protected/x", op, ROOT)
            == PermResult::Deny);
}

// ===================================================================
// check_run_permission
// ===================================================================

TEST_CASE("perm run: nullopt is Allow", "[perms]") {
    std::optional<UtilsPermissions> none;
    REQUIRE(ezmk::lua::check_run_permission("anything --x", none)
            == PermResult::Allow);
}

TEST_CASE("perm run: deny hit → Deny (priority over allow)", "[perms]") {
    UtilsPermissions p;
    p.run      = {"rm"};
    p.run_deny = {"rm"};
    auto op = std::make_optional(p);
    REQUIRE(ezmk::lua::check_run_permission("rm -rf /", op) == PermResult::Deny);
}

TEST_CASE("perm run: exact match g++ does not match g++-13", "[perms]") {
    auto p = std::make_optional(make_perms());
    REQUIRE(ezmk::lua::check_run_permission("g++ -c a.cpp", p) == PermResult::Allow);
    REQUIRE(ezmk::lua::check_run_permission("g++-13 -c a.cpp", p) == PermResult::Ask);
}

TEST_CASE("perm run: trailing-* prefix wildcard", "[perms]") {
    auto p = std::make_optional(make_perms());
    REQUIRE(ezmk::lua::check_run_permission("git status", p) == PermResult::Allow);
    REQUIRE(ezmk::lua::check_run_permission("git.exe status", p) == PermResult::Allow);
}

TEST_CASE("perm run: unlisted command → Ask", "[perms]") {
    auto p = std::make_optional(make_perms());
    REQUIRE(ezmk::lua::check_run_permission("python foo.py", p) == PermResult::Ask);
}

TEST_CASE("perm run: quoted first token", "[perms]") {
    UtilsPermissions p;
    p.run = {"my tool"};
    auto op = std::make_optional(p);
    REQUIRE(ezmk::lua::check_run_permission("\"my tool\" --flag", op) == PermResult::Allow);
}

// ===================================================================
// resolve_ask (non-interactive fail-safe + session cache)
// ===================================================================

TEST_CASE("resolve_ask: non-interactive denies", "[perms]") {
    ezmk::lua::clear_ask_cache();
    ezmk::lua::set_noninteractive(true);
    REQUIRE(ezmk::lua::resolve_ask(PermCategory::Read, "/proj/x") == false);
    ezmk::lua::set_noninteractive(false);
}

// ===================================================================
// parse_config: [utils.permissions]
// ===================================================================

#include "ezmk/util.hpp"
#include <fstream>

TEST_CASE("parse_config: [utils.permissions] fields", "[perms][config]") {
    fs::path dir = fs::temp_directory_path() /
                   ("ezmk_perm_cfg_" + std::to_string(std::rand()));
    fs::create_directories(dir);
    {
        std::ofstream of(dir / "ezmk.toml");
        of << "[project]\nname=\"p\"\ntype=\"utils\"\nversion=\"0.1.0\"\n\n"
           << "[utils]\ntools=[\"cc\"]\n\n"
           << "[utils.permissions]\n"
           << "read = [\"src/\", \"include/\"]\n"
           << "read_deny = [\".env\"]\n"
           << "write = [\"build/\"]\n"
           << "run = [\"g++\"]\n"
           << "run_deny = [\"rm\"]\n"
           << "network = true\n";
    }
    auto cfg = ezmk::config::parse_config(dir / "ezmk.toml");
    REQUIRE(cfg.utils.permissions.has_value());
    const auto& pm = *cfg.utils.permissions;
    REQUIRE(pm.read == std::vector<std::string>{"src/", "include/"});
    REQUIRE(pm.read_deny == std::vector<std::string>{".env"});
    REQUIRE(pm.write == std::vector<std::string>{"build/"});
    REQUIRE(pm.run == std::vector<std::string>{"g++"});
    REQUIRE(pm.run_deny == std::vector<std::string>{"rm"});
    REQUIRE(pm.network == true);
    fs::remove_all(dir);
}

TEST_CASE("parse_config: no [utils.permissions] → nullopt", "[perms][config]") {
    fs::path dir = fs::temp_directory_path() /
                   ("ezmk_perm_cfg2_" + std::to_string(std::rand()));
    fs::create_directories(dir);
    {
        std::ofstream of(dir / "ezmk.toml");
        of << "[project]\nname=\"p\"\ntype=\"utils\"\nversion=\"0.1.0\"\n\n"
           << "[utils]\ntools=[\"cc\"]\n";
    }
    auto cfg = ezmk::config::parse_config(dir / "ezmk.toml");
    REQUIRE_FALSE(cfg.utils.permissions.has_value());
    REQUIRE(cfg.utils.tools == std::vector<std::string>{"cc"});
    fs::remove_all(dir);
}

TEST_CASE("parse_config: [utils.permissions] network defaults false", "[perms][config]") {
    fs::path dir = fs::temp_directory_path() /
                   ("ezmk_perm_cfg3_" + std::to_string(std::rand()));
    fs::create_directories(dir);
    {
        std::ofstream of(dir / "ezmk.toml");
        of << "[project]\nname=\"p\"\ntype=\"utils\"\nversion=\"0.1.0\"\n\n"
           << "[utils.permissions]\nread = [\"src/\"]\n";
    }
    auto cfg = ezmk::config::parse_config(dir / "ezmk.toml");
    REQUIRE(cfg.utils.permissions.has_value());
    REQUIRE(cfg.utils.permissions->network == false);
    fs::remove_all(dir);
}
