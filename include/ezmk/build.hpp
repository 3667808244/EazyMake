#pragma once

#include <string>
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

} // namespace ezmk::build
