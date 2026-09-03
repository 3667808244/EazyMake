#pragma once

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace ezmk::config {
namespace fs = std::filesystem;

struct ProjectSection {
    std::string name;
    std::string type = "executable"; // "executable" | "static" | "shared" | "utils"
    std::string version;             // required, e.g. "0.1.0"
    std::string language = "C++17";  // <Lang><Ver>, e.g. "C++17", "C11"
    std::string stdlib = "libstdc++";// 1.1.0-dev.4: standard library ("libstdc++" | "libc++")
    bool header_only = false;        // 0.9.7+ header-only package (no compilation needed)
    bool precompiled = false;        // 0.9.7+ precompiled package (use lib/*.a, skip compilation)
    bool precompiled_strict = false; // 1.2.0-dev.10: precompiled packages refuse ABI-unsafe toolchain fallback (fail-fast)
};

// 0.2.5+ — Fine-grained utils permission declaration ([utils.permissions]).
// Judged in order deny > allow > ask (see lua_api.hpp check_*_permission).
struct UtilsPermissions {
    std::vector<std::string> read;         // allow read paths (relative to project root)
    std::vector<std::string> read_deny;    // deny read paths (takes precedence over allow)
    std::vector<std::string> write;        // allow write paths
    std::vector<std::string> write_deny;   // deny write paths
    std::vector<std::string> run;          // allow commands
    std::vector<std::string> run_deny;     // deny commands
    bool network = false;                  // network access (declarative, not yet enforced)
};

struct UtilsSection {
    std::vector<std::string> tools;                    // only for type = "utils"
    std::optional<UtilsPermissions> permissions;       // 0.2.5+ — nullopt when [utils.permissions] absent
};

struct CompileSection {
    std::vector<std::string> flags;
    std::vector<std::string> msvc_flags;       // 0.2.1+ MSVC-only compile flags
    std::vector<std::string> include_dirs;     // -I paths (default ["include"])
    std::vector<std::string> src_dirs;         // 0.2.2+ source dirs (default ["src"])
    std::map<std::string, std::string> macros; // 0.2.2+ [compile.macros] key→value
    bool ezmk_macros = true;                   // 0.2.2+ inject EZMK_* standard macros
    bool deterministic = false;                // 1.1.0: reproducible builds
    uint64_t source_date_epoch = 0;            // 1.1.0: SOURCE_DATE_EPOCH override (0 = auto)
    bool compile_commands = false;             // 1.1.1: auto-generate compile_commands.json after build
    std::string default_profile;               // 1.2.0-dev.3: profile used when no --profile given (may be empty)
};

struct LinkSection {
    std::vector<std::string> flags;
    std::vector<std::string> msvc_flags;       // 0.2.1+ MSVC-only link flags
    std::vector<std::string> link_dirs;       // -L paths (default [])
    std::vector<std::string> system_targets;  // -l libs (e.g. "pthread")
};

// 0.9.6+ — Version constraint for dependency entries
struct VersionConstraint {
    enum Op { None, Exact, Compatible, Approx, Gte, Gt };
    Op op = None;
    std::string version;  // e.g. "1.2.3"
};

// 0.9.6+ — A single dependency entry with optional version constraint
struct DependsEntry {
    std::string name;
    VersionConstraint constraint;
};

struct DependsSection {
    std::vector<DependsEntry> libs;   // 0.9.6+: changed from std::vector<std::string>
    std::vector<DependsEntry> want;   // 0.2.2+ optional dependencies
    // 1.3.0-dev.1: workspace sibling dependencies — `[depends] workspace = [...]`
    // values are member basenames or full relative paths within the enclosing
    // workspace. Empty when not declared; workspace validation lives in
    // workspace.cpp (reference resolution / cycle detection / static constraint).
    std::vector<std::string> workspace;
};

// 0.2.3+ — Build profile configuration for debug/release/etc.
struct ProfileConfig {
    std::vector<std::string> flags;
    std::vector<std::string> msvc_flags;
    std::map<std::string, std::string> macros;
};

// 0.2.3+ — Link profile configuration
struct ProfileLinkConfig {
    std::vector<std::string> flags;
    std::vector<std::string> msvc_flags;
};

// 0.2.3+ — Build hook scripts
struct HooksSection {
    std::string pre_build;    // Lua script path (relative to project root)
    std::string post_build;   // executed after successful link
    std::string on_failure;   // executed on build/link failure
};

// 1.1.0: Install section for ezmk project install
struct InstallSection {
    std::string prefix;       // install root (default: Unix ~/.local, Windows %LOCALAPPDATA%\ezmk)
    std::string bindir;       // executable subdir (default: "bin")
    std::string libdir;       // library subdir (default: "lib")
    std::string includedir;   // header subdir (default: "include")
    std::string sharedir;     // data subdir (default: "share")
};

// 1.1.0-dev.6: Test configuration section ([test] in ezmk.toml)
struct TestConfig {
    std::vector<std::string> dirs = {"test"};       // test source directories
    std::string framework = "catch2";                // "catch2" | "ezmk"
    std::vector<std::string> flags = {};             // DEPRECATED (1.2.0-dev.12, removed in 2.0.0) — use default_profile / include_dirs / link_targets
    std::string default_profile = "";                // 1.2.0-dev.12: profile applied when no --profile given
    std::vector<std::string> include_dirs = {};      // 1.2.0-dev.12: test-only -I dirs (resolved relative to project root)
    std::vector<std::string> link_targets = {};      // 1.2.0-dev.12: test-only -l system targets
};

// 1.4.0-dev.2: Package-management configuration section ([pkg] in ezmk.toml).
struct PkgSection {
    // Standard-compatibility check (1.3.1): warn → fatal when true. Default
    // false keeps the informative-warning behavior — a strict error would
    // break the existing package ecosystem, so it is opt-in (CI / strict
    // requirements only).
    bool strict_std_check = false;
};

struct EzConfig {
    ProjectSection project;
    CompileSection compile;
    LinkSection link;
    DependsSection depends;
    UtilsSection utils;
    HooksSection hooks;                                          // 0.2.3+
    InstallSection install;                                      // 1.1.0 [install]
    TestConfig test;                                             // 1.1.0-dev.6 [test]
    PkgSection pkg;                                              // 1.4.0-dev.2 [pkg]
    std::map<std::string, ProfileConfig> compile_profiles;       // 0.2.3+ [compile.profile.*]
    std::map<std::string, ProfileLinkConfig> link_profiles;      // 0.2.3+ [link.profile.*]
};

// 1.1.0: Lockfile — dependency version & content pinning for reproducible builds.
struct LockedPackage {
    std::string name;
    std::string version;
    std::string source;            // repo name; "git" for git-URL installs (1.4.1)
    std::string source_url;
    std::string sha256;            // SHA-256 of the installed artifact
    std::string commit;            // 1.4.1: pinned commit SHA for git sources (optional)
    std::string type;              // "static" / "shared" / "header-only"
    std::string scope;             // "project" / "user" / "global"
    std::string platform;          // e.g. "windows_x86_64_msvc"
    std::vector<std::string> dependencies;
};

struct Lockfile {
    int version = 1;
    std::string generated_by;
    std::string generated_at;
    std::string toolchain;         // "gcc" / "clang" / "msvc"
    std::string toolchain_version;
    // 1.1.2 C3: root project's DIRECT [depends] entries as spec strings
    // ("name" or "name@^1.2"). packages[] also includes transitive/auto-installed
    // deps, so depends_changed must compare direct-vs-direct, not direct-vs-all.
    std::vector<std::string> direct_deps;
    std::vector<LockedPackage> packages;
};

// Parse an ezmk.toml file. Throws on parse errors.
EzConfig parse_config(const fs::path& toml_path);

// Write a default ezmk.toml to the given path (used by `ezmk project new`).
// project_type: "executable" (default), "static", or "shared".
void write_default_config(const fs::path& toml_path, std::string_view project_name,
                          std::string_view project_type = "executable");

// 1.1.0-dev.5: Load .ezmk/links.json and return the key→value map.
// Returns empty map if the file does not exist.
// Throws on parse error (invalid JSON).
std::map<std::string, std::string> load_links_json(const fs::path& project_root);

// 1.1.0-dev.5: Resolve a "@link:<name>" reference to a concrete path.
// links: the parsed .ezmk/links.json map (name → target_path).
// sub_path: the part after the link name in "@link:<name>/sub/path" (empty if none).
// max_depth: remaining chain resolution depth (starts at 10, decrements per hop).
// Throws on: link not found, cycle detected, or depth exceeded.
fs::path resolve_link_path(std::string_view name,
                           std::string_view sub_path,
                           const std::map<std::string, std::string>& links,
                           int max_depth = 10);

// 1.1.0-dev.4: Normalize a language/stdlib string (upper-case, unify variants).
// "c++17" / "CXX17" / "CPP17" → "CPP17"; "libstdc++" / "glibcxx" → "LIBSTDCXX".
std::string normalize_lang(const std::string& input);

// Parse a language string like "C++17" into compiler name and -std= flag.
// Returns {"g++", "-std=c++17"} for C++; {"gcc", "-std=c11"} for C.
struct LanguageInfo {
    std::string compiler;            // default compiler from config ("g++" or "gcc")
    std::string std_flag;            // 1.3.1: effective flag — a range takes min ("-std=c++17" / "-std=gnu++17")
    std::string detected_compiler;   // runtime-detected compiler, empty if not yet probed
    bool gnu_extensions = false;     // 1.1.0-dev.4: true if GNU prefix detected
    std::string normalized_lang;     // 1.1.0-dev.4: e.g. "CPP17" (for EZMK_LANG macro); a range yields the min canonical form
    int min_ver = 0;                 // 1.3.1: parsed lower bound (0 = not declared); exact values fill this too
    int max_ver = 0;                 // 1.3.1: upper bound (0 = no upper bound / not declared)
};
LanguageInfo parse_language(std::string_view language);

} // namespace ezmk::config
