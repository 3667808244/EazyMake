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
    std::string build_dir;            // 1.5.0-dev.2: per-project build dir (empty = global default)
    std::string output_name;          // 1.5.0-dev.5: artifact basename (empty = name)
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
    std::vector<bool> src_dirs_recursive;      // 1.4.10: parallel to src_dirs — entry ended in "/**"
    std::map<std::string, std::string> macros; // 0.2.2+ [compile.macros] key→value
    bool ezmk_macros = true;                   // 0.2.2+ inject EZMK_* standard macros
    bool deterministic = false;                // 1.1.0: reproducible builds
    uint64_t source_date_epoch = 0;            // 1.1.0: SOURCE_DATE_EPOCH override (0 = auto)
    bool compile_commands = false;             // 1.1.1: auto-generate compile_commands.json after build
    std::string default_profile;               // 1.2.0-dev.3: profile used when no --profile given (may be empty)
    int jobs = -1;                             // 1.5.0-dev.5: project parallelism (-1 = use global)
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
    // 1.4.9: profile-driven object export (opt-in). When true, a successful
    // compile of this profile also packs every project object file into an
    // archive; export_objs_path empty = default build/obj_files.zip, otherwise
    // the path suffix selects .zip / .tar.gz / .tgz.
    bool export_objs = false;
    std::string export_objs_path;
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
    std::vector<bool> dirs_recursive;               // 1.4.10: parallel to dirs — entry ended in "/**"
    std::string framework = "catch2";                // "catch2" | "ezmk"
    std::vector<std::string> flags = {};             // DEPRECATED (1.2.0-dev.12, removed in 2.0.0) — use default_profile / include_dirs / link_targets
    std::string default_profile = "";                // 1.2.0-dev.12: profile applied when no --profile given
    std::vector<std::string> include_dirs = {};      // 1.2.0-dev.12: test-only -I dirs (resolved relative to project root)
    std::vector<std::string> link_targets = {};      // 1.2.0-dev.12: test-only -l system targets
    int timeout = -1;                                // 1.5.0-dev.5: seconds (-1 = global; 0 = no timeout)
    std::string report_dir;                          // 1.5.0-dev.5: default report dir (empty = global)
};

// 1.4.0-dev.2: Package-management configuration section ([pkg] in ezmk.toml).
struct PkgSection {
    // Standard-compatibility check (1.3.1): warn → fatal when true. Default
    // false keeps the informative-warning behavior — a strict error would
    // break the existing package ecosystem, so it is opt-in (CI / strict
    // requirements only).
    bool strict_std_check = false;
    // 1.5.0-dev.5: [pkg].auto_install (false = fail on missing deps instead).
    bool has_auto_install = false;
    bool auto_install = true;
};

// 1.5.0-dev.5: [watch] — file-watcher defaults.
struct WatchSection {
    int debounce_ms = 0;                 // 0 = use [settings].watch_debounce_ms
    std::vector<std::string> ignore;     // extra prefixes (relative to project root)
};

// 1.5.0-dev.5: [pack] — package defaults.
struct PackSection {
    std::string format;                  // "tar.gz" | "zip" | "tgz" (empty = global)
    std::string output_dir;              // empty = global
    std::string name;                    // archive basename (empty = name-version)
    std::vector<std::string> exclude;    // extra dir names / extensions
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
    WatchSection watch;                                          // 1.5.0-dev.5 [watch]
    PackSection pack;                                            // 1.5.0-dev.5 [pack]
    std::map<std::string, ProfileConfig> compile_profiles;       // 0.2.3+ [compile.profile.*]
    std::map<std::string, ProfileLinkConfig> link_profiles;      // 0.2.3+ [link.profile.*]
};

// 1.1.0: Lockfile — dependency version & content pinning for reproducible builds.
// ===================================================================
// 1.5.0-dev.1: two-level runtime configuration (system + user config.toml).
// Loaded once before CLI parsing; consumers read runtime_config(). All
// defaults preserve pre-1.5.0 behavior.
// ===================================================================
struct BaseDirConfig {
    std::optional<fs::path> installed_packages;
    std::optional<fs::path> repo_cache;
    std::optional<fs::path> examples;
    std::optional<fs::path> logs;
};

struct SettingsConfig {
    std::string natural_language = "auto";
    std::string color_mode = "auto";
    std::string color_theme = "default";
    std::string log_level = "info";

    bool has_initial_commit = false;
    bool initial_commit_enabled = false;
    std::string initial_commit_message;
    bool auto_init_git = true;
    bool gen_gitignore = true;
    bool gen_readme = true;
    bool gen_agent_md = false;

    std::string default_project_type = "executable";
    std::string default_language = "C++17";
    std::string default_project_version = "0.1.0";
    std::vector<std::string> default_compile_flags = {"-Wall", "-Wextra"};

    std::string build_dir = "build";
    int default_jobs = 0;
    int test_timeout = 30;
    std::string test_report_dir = ".ezmk/test-results";
    int watch_debounce_ms = 300;
    std::vector<std::string> watch_ignore;

    std::string pack_format = "tar.gz";
    std::string pack_output_dir = ".";
    std::string pack_name;
    std::vector<std::string> pack_exclude;

    std::string default_pkg_scope = "project";
    bool auto_install_deps = true;
    std::vector<std::string> scan_skip_dirs;

    std::string git_path = "git";
    std::string editor;
    bool auto_pack = false;
};

struct RuntimeConfig {
    SettingsConfig settings;
    BaseDirConfig global_dirs;   // [base_dir] from the system-level file
    BaseDirConfig user_dirs;     // [base_dir] from the user-level file
    fs::path global_config_file; // empty = not present
    fs::path user_config_file;   // empty = not present
    bool loaded = false;
    std::string language_override; // 1.5.0-dev.3: --lang (highest priority)
};

// Merged runtime configuration. Before load_runtime_config() runs (e.g. in unit
// tests that call helpers directly) this returns the built-in defaults.
const RuntimeConfig& runtime_config();

// Load the system + user config.toml files. Idempotent. Scans --config <file>
// and --no-config, then reads the files. Throws std::runtime_error on malformed
// configuration. Must be called before cli::parse().
void load_runtime_config(int argc, const char* const* argv);

// Test-only: clear the cached configuration.
void reset_runtime_config_for_test();

// 1.5.0-dev.2: effective build directory — [project].build_dir > global
// [settings].build_dir > "build"; relative values resolve against proj_root.
fs::path effective_build_dir(const EzConfig& cfg, const fs::path& proj_root);

// 1.5.0-dev.5: artifact basename — [project].output_name or [project].name.
std::string project_output_name(const EzConfig& cfg);

// Parse <proj_root>/ezmk.toml (if present) and return its effective build dir;
// falls back to the global default when the config is absent/unparseable.
fs::path effective_build_dir_for(const fs::path& proj_root);

// User-level [base_dir] value overrides the system-level one.
std::optional<fs::path> effective_examples();
std::optional<fs::path> effective_logs();

struct LockedPackage {
    std::string name;
    std::string version;
    std::string source;            // repo name; "git" for git-URL installs (1.4.1)
    std::string source_url;
    std::string sha256;            // SHA-256 of the installed artifact (legacy alias of lib_sha256)
    // 1.4.2 F-04: the two hashes have DIFFERENT meanings and must never be
    // mixed:
    //   archive_sha256 — hash of the ARCHIVE the package was installed from
    //                    (re-verified when reinstalling with --locked);
    //   lib_sha256     — hash of the built/installed artifact
    //                    (lockfile::verify content check).
    // Lockfile version stays 1; a pre-1.4.2 file only carries `sha256`, which
    // has always meant the artifact hash — loaders treat it as the legacy alias
    // of lib_sha256.
    std::string archive_sha256;
    std::string lib_sha256;
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

// 1.4.9: Single source of truth for "which profile is active" — an explicit
// CLI --profile wins, otherwise the supplied default (compile.default_profile
// for builds/exports, test.default_profile for tests). Empty means no profile
// applies. Shared by apply_profile / project export / run_tests so the value
// seen by build hooks and the compiler can never drift.
std::string resolve_profile_name(const std::string& cli_profile,
                                 const std::string& default_profile);

} // namespace ezmk::config
