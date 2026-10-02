#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "ezmk/cli.hpp"
#include "ezmk/config.hpp"

namespace ezmk::repo {
namespace fs = std::filesystem;

// ---- Data types ----

struct RepoEntry {
    std::string name;        // repo identifier
    std::string url;         // git clone URL or local path
    std::string type = "git"; // "git" or "local"
    std::string branch = "main";
    std::string last_update; // ISO 8601 timestamp
};

// ---- Path resolution ----

// 1.4.5: registry paths. `repo_list_path` is the JSON file ezmk writes
// (list.json); `legacy_repo_list_path` is the pre-1.4.5 TOML file (list.toml),
// still READ so existing registries keep working — the next save() migrates it.
fs::path repo_list_path(cli::Scope scope);
fs::path legacy_repo_list_path(cli::Scope scope);

// Deprecated alias of `legacy_repo_list_path` (the historical name). Kept so the
// 1.4.5 format change stays API-additive; removed in 2.0.0
// (see plans/2.0.x/REMOVALS.md R-04).
fs::path list_toml_path(cli::Scope scope);

// Get the cache directory for a repo of a given scope + name.
// For "git" repos this is where the clone lives; irrelevant for "local".
fs::path cache_dir(cli::Scope scope, std::string_view repo_name);

// ---- list.json read/write ----

// Load the registry (list.json, falling back to the pre-1.4.5 list.toml).
std::vector<RepoEntry> load_repo_list(cli::Scope scope);
// Write list.json (atomically); a legacy list.toml is removed once it is on disk.
void save_repo_list(cli::Scope scope, const std::vector<RepoEntry>& entries);

// ---- Operations ----

// Register a repository and clone it (if git). Throws on error.
void add(const cli::RepoOptions& opts);

// Unregister a repository and delete its cache. Throws on error.
void remove(std::string_view name, const std::vector<cli::Scope>& scopes);

// Update repo caches (git pull or re-read local index). Warns on failure.
void update(const std::string& name, const std::vector<cli::Scope>& scopes);

// List registered repos to stdout.
void list(const std::vector<cli::Scope>& scopes);

// Show detailed info for a single registered repo (0.2.5+).
void info(std::string_view name, const std::vector<cli::Scope>& scopes);

// ---- pkg integration ----

// Result of searching a package in registered repos.
struct PkgSearchResult {
    fs::path archive_path;   // path to the package archive
    std::string sha256;      // from index.toml, empty if not provided
    std::string version;     // 0.2.3+: version from index.toml
    std::string repo_name;   // 0.2.5+: source repo name
};

// Search registered repos (in scope order) for a package by name.
// Returns the archive path and optional sha256 from index.toml.
// If not found, archive_path is empty.
PkgSearchResult search_package(std::string_view pkg_name,
                               const std::vector<cli::Scope>& scopes);

// 0.9.6+ — Version-constrained search.
// Filters available versions by the given constraint and picks the highest match.
// If no version satisfies the constraint, archive_path is empty (caller reports error).
PkgSearchResult search_package(std::string_view pkg_name,
                               const std::vector<cli::Scope>& scopes,
                               const config::VersionConstraint& constraint);

} // namespace ezmk::repo
