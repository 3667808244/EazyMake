#pragma once

#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"

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
std::vector<fs::path> collect_sources(
    const std::vector<std::string>& src_dirs,
    const fs::path& proj_root,
    const std::string& project_type);

} // namespace ezmk::build
