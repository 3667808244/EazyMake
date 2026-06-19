#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "ezmk/cli.hpp"

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

// Get the path to list.toml for a given scope.
fs::path list_toml_path(cli::Scope scope);

// Get the cache directory for a repo of a given scope + name.
// For "git" repos this is where the clone lives; irrelevant for "local".
fs::path cache_dir(cli::Scope scope, std::string_view repo_name);

// ---- list.toml read/write ----

std::vector<RepoEntry> load_repo_list(cli::Scope scope);
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

// ---- pkg integration ----

// Result of searching a package in registered repos.
struct PkgSearchResult {
    fs::path archive_path;   // path to the package archive
    std::string sha256;      // from index.toml, empty if not provided
};

// Search registered repos (in scope order) for a package by name.
// Returns the archive path and optional sha256 from index.toml.
// If not found, archive_path is empty.
PkgSearchResult search_package(std::string_view pkg_name,
                               const std::vector<cli::Scope>& scopes);

} // namespace ezmk::repo
