#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include "ezmk/config.hpp"

namespace ezmk::lockfile {
namespace fs = std::filesystem;

// Load ezmk.lock from project root. Returns std::nullopt if file doesn't exist.
std::optional<config::Lockfile> load(const fs::path& proj_root);

// Write ezmk.lock to project root.
void save(const fs::path& proj_root, const config::Lockfile& lf);

// Verify installed packages match lockfile entries.
// Returns list of mismatched package names (empty = all OK).
std::vector<std::string> verify(const fs::path& proj_root,
                                const config::Lockfile& lf);

// Compare ezmk.toml [depends] with lockfile packages.
// Returns true if there are added/removed/modified dependencies.
bool depends_changed(const config::EzConfig& cfg,
                     const config::Lockfile& lf);

} // namespace ezmk::lockfile
