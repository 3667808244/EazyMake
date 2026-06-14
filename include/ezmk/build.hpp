#pragma once

#include <string>
#include <filesystem>
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"

namespace fs = std::filesystem;

namespace ezmk::build {

// Run a full build: compile all sources, link the executable.
// Returns the path to the built executable.
fs::path build_project(const config::EzConfig& cfg, const cli::BuildOptions& opts);

} // namespace ezmk::build
