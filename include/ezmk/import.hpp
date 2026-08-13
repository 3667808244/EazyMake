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
// opts.overwrite. Throws (util::fatal) on missing CMakeLists.txt, an
// existing ezmk.toml without --overwrite, or an unsupported non-declarative
// CMake construct (transactional abort).
int import_project(const cli::ProjectImportOptions& opts,
                   const std::filesystem::path& project_root);

// Parse `cmakelists_src` and return the generated ezmk.toml text without
// writing any file. Throws (util::fatal) on unsupported constructs. Exposed
// for unit tests and tooling; `import_project` is the filesystem entry point.
std::string import_cmake_text(const std::string& cmakelists_src,
                              const std::filesystem::path& project_root);

} // namespace ezmk::import
