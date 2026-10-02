#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include "ezmk/config.hpp"

namespace ezmk::lockfile {
namespace fs = std::filesystem;

// ---- 1.4.5: lockfile paths ----
// `lockfile_path`      — the JSON file ezmk writes (ezmk.lock.json).
// `legacy_lockfile_path` — the pre-1.4.5 TOML file (ezmk.lock): still READ so
//                        existing projects keep working; the next save() migrates
//                        it (writes the JSON file, removes this one).
// `active_path`        — what readers/verifiers must use: the JSON file when it
//                        exists, else the legacy file, else the JSON path (the
//                        write target). The deterministic-build cache signature
//                        hashes this file, so all three call sites must agree.
fs::path lockfile_path(const fs::path& proj_root);
fs::path legacy_lockfile_path(const fs::path& proj_root);
fs::path active_path(const fs::path& proj_root);

// Load the lockfile from project root (ezmk.lock.json, falling back to the
// pre-1.4.5 ezmk.lock). Returns std::nullopt if neither file exists.
std::optional<config::Lockfile> load(const fs::path& proj_root);

// Write ezmk.lock.json to project root (atomically). A legacy ezmk.lock left
// behind by an older ezmk is removed once the new file is on disk.
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

// 1.4.2 F-28: deterministic content hash of a package's installed payload — the
// sorted (relative path, file sha256) pairs of <pkg_dir>/include fed through one
// sha256. Header-only packages have no built archive, so this is what the
// lockfile pins as their artifact hash (and what verify() re-checks).
// Returns an empty string when the package has no include/ directory.
std::string payload_manifest_hash(const fs::path& pkg_dir);

// Compare ezmk.toml [depends] with lockfile packages.
// Returns true if there are added/removed/modified dependencies.
bool depends_changed(const config::EzConfig& cfg,
                     const config::Lockfile& lf);

// 1.1.2 C3: format the root project's direct [depends] entries (lib + want) as
// spec strings ("name" or "name@^1.2"), sorted. Used both when generating
// the lockfile (pkg.cpp writes lf.direct_deps) and in depends_changed — the two
// sides must agree on the format.
std::vector<std::string> direct_dep_specs(const config::EzConfig& cfg);

} // namespace ezmk::lockfile
