#pragma once

#include <string>
#include <filesystem>
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"

namespace ezmk::build {
namespace fs = std::filesystem;

// Run a full build: compile all sources, link according to project.type.
// Returns the path to the built artifact (executable, .a, or .dll/.so).
fs::path build_project(const config::EzConfig& cfg, const cli::BuildOptions& opts);

} // namespace ezmk::build
