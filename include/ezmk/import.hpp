#pragma once
// 1.2.0: project import — convert an existing CMake project into ezmk.toml.
// Experimental: single-direction snapshot; after import, ezmk.toml is the
// source of truth. Only the most standard CMake projects are supported;
// non-declarative constructs abort the conversion transactionally.
#include <filesystem>
#include "ezmk/cli.hpp"

namespace ezmk::import {

// Import the project at project_root (reads <root>/CMakeLists.txt) into
// <root>/ezmk.toml. Refuses to overwrite an existing ezmk.toml unless
// opts.overwrite. Throws (util::fatal) on missing CMakeLists.txt or an
// existing ezmk.toml without --overwrite.
int import_project(const cli::ProjectImportOptions& opts,
                   const std::filesystem::path& project_root);

} // namespace ezmk::import
