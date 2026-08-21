#pragma once

#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/cache.hpp"

namespace ezmk::build {
namespace fs = std::filesystem;

// Detect the best available C/C++ compiler. Respects $CXX/$CC env vars.
// language: "C++" or "C" (from config::LanguageInfo context).
// Returns the compiler executable name (e.g. "g++", "/usr/bin/clang++").
// Result is cached per process — only probes on first call per language.
// Throws fatal_error if no compiler is found.
std::string detect_compiler(const std::string& language);

// Run a full build: compile all sources, link according to project.type.
// Returns the path to the built artifact (executable, .a, or .dll/.so).
fs::path build_project(const config::EzConfig& cfg, const cli::BuildOptions& opts);

// 0.2.2+: Convert a macros map to -D flag vector.
// Empty value → -DKEY; non-empty → -DKEY=VALUE (quoted for strings).
std::vector<std::string> macros_to_flags(
    const std::map<std::string, std::string>& macros);

// 0.2.2+: Generate standard EZMK_* preprocessor macros from project config.
std::vector<std::string> generate_ezmk_macros(const config::EzConfig& cfg);

// 0.2.2+: Convert a package name to the EZMK_LIB_MISS_* macro name.
// Uppercase, replace -/. /space with _, drop other special chars.
std::string want_to_macro_name(const std::string& pkg_name);

// 0.2.2+: Collect source files from multiple src_dirs.
// Returns deduplicated list; warns on missing/empty directories.
// Throws if no source files found across all directories.
// 1.2.0-dev.9: `require_main` — only enforce the main.cpp requirement for
// executables when true. Package compilation passes false (packages are
// always static libraries, regardless of their [project].type).
std::vector<fs::path> collect_sources(
    const std::vector<std::string>& src_dirs,
    const fs::path& proj_root,
    const std::string& project_type,
    bool require_main = true);

// 0.2.3+: Merge a compile profile into the base compile section.
// Profile flags are appended to base flags; profile macros override base macros.
config::CompileSection merge_compile_profile(
    const config::CompileSection& base,
    const config::ProfileConfig& profile);

// 0.2.3+: Merge a link profile into the base link section.
// Profile flags are appended to base flags.
config::LinkSection merge_link_profile(
    const config::LinkSection& base,
    const config::ProfileLinkConfig& profile);

// 1.1.0: Install build artifacts to the configured prefix.
// Copies executables, libraries, and headers to their install destinations.
void install_project(const config::EzConfig& cfg,
                     const cli::ProjectInstallOptions& opts,
                     const fs::path& proj_root);

// 1.1.0-dev.2: Pack a static library project into a distributable .tar.gz.
// Builds the project first (if needed), then collects include/ + lib + ezmk.toml
// into a temporary directory and creates <name>-<version>.tar.gz.
// Prints the SHA-256 of the resulting archive.
void pack_project(const config::EzConfig& cfg,
                  const cli::ProjectPackOptions& opts,
                  const fs::path& proj_root);

// 1.1.0-dev.6: Run project tests (ezmk project test).
// Builds the project if needed, then compiles and runs tests according to
// the [test] configuration section. Supports Catch2 and ezmk built-in frameworks.
// test_framework_override: if non-empty, overrides cfg.test.framework (from --framework).
// test_filter: if non-empty, filters test names (Catch2: -c param; ezmk: filename glob).
// verbose: if true, shows detailed output for each test (even passing ones).
// 1.2.0-dev.12: test_profile_override — if non-empty, overrides cfg.test.default_profile
// (from --profile); both resolve through the shared apply_profile() helper.
void run_tests(const config::EzConfig& cfg,
               const std::string& test_framework_override,
               const std::string& test_filter,
               bool verbose,
               const std::string& test_profile_override = {});

// 1.1.1: Prepare a cache::CompileInput exactly as a real build would —
// include collection, profile merge, macro folding, dependency package
// extra_includes. Shared by compile_commands.json generation
// (the `ezmk utils cc` interception / `ezmk project cc` path).
cache::CompileInput prepare_compile_input(const config::EzConfig& cfg,
                                          const cli::BuildOptions& opts);

// 1.3.0-dev.2: sibling artifact injection = member self-discovery (dev.2 §3.4).
// A member declaring `[depends] workspace` injects its siblings' artifacts
// into its own build: `-I <ws>/<m>/include` (exists), `-L <ws>/<m>/build`
// + `-l<m>` (GCC) or the full `<m>.lib` path (MSVC), all existence-gated.
// Zero environment variables — the workspace file is the single source of
// truth, so injection length does not grow with workspace size.
struct WsInjection {
    std::vector<fs::path> include_dirs;   // -I dirs (exist)
    std::vector<fs::path> link_dirs;      // -L dirs (exist)
    std::vector<std::string> link_names;  // -l<name> (lib exists)
    std::vector<fs::path> msvc_archives;  // MSVC: full lib paths (no -L/-l)
    std::vector<std::string> missing;     // deps whose sibling artifacts are absent
    std::string error;                    // non-empty when the workspace could
                                          // not be loaded/resolved (caller warns)
};
WsInjection resolve_ws_injection(const fs::path& start_dir,
                                 const std::vector<std::string>& ws_deps,
                                 bool is_msvc);

} // namespace ezmk::build
