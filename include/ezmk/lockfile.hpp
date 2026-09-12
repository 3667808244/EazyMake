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

// 1.4.2 F-04: the ARTIFACT hash of a lockfile entry (lib_sha256 when present,
// else the legacy sha256 field). Used by verify's content check.
std::string artifact_hash(const config::LockedPackage& pkg);

// 1.4.2 F-04: the installation-source ARCHIVE hash (empty when unknown: git,
// directory and pre-1.4.2 entries). Used by `--locked` reinstall to verify the
// archive it is about to install from.
std::string archive_hash(const config::LockedPackage& pkg);

// Compare ezmk.toml [depends] with lockfile packages.
// Returns true if there are added/removed/modified dependencies.
bool depends_changed(const config::EzConfig& cfg,
                     const config::Lockfile& lf);

// 1.1.2 C3: format the root project's direct [depends] entries (lib + want) as
// spec strings ("name" or "name@^1.2"), sorted. Used both when generating
// ezmk.lock (pkg.cpp writes lf.direct_deps) and in depends_changed — the two
// sides must agree on the format.
std::vector<std::string> direct_dep_specs(const config::EzConfig& cfg);

} // namespace ezmk::lockfile
