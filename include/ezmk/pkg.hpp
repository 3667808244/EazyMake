#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "ezmk/cli.hpp"

namespace fs = std::filesystem;

namespace ezmk::pkg {

// ---- Path resolution ----
// Get the install directory for a given scope.
fs::path pkg_install_dir(cli::Scope scope);

// Resolve search paths for multiple scopes, in order.
std::vector<fs::path> pkg_search_dirs(const std::vector<cli::Scope>& scopes);

// ---- Package operations ----
// Install a package from a local file or URL into the given scope.
void install(const std::string& pkg_file, cli::Scope scope);

// Remove a package: search scopes in order, delete the first match.
void remove(const std::string& pkg_name, const std::vector<cli::Scope>& scopes);

// Search for a package across scopes, returning paths where found.
std::vector<fs::path> search(const std::string& pkg_name,
                             const std::vector<cli::Scope>& scopes);

// Show information about a package (its ezmk.toml contents).
void info(const std::string& pkg_name, const std::vector<cli::Scope>& scopes);

// ---- Dependency resolution ----
// Topologically sort a list of package directories by their [depends].lib order.
// Throws if a cycle is detected or a dependency is missing.
std::vector<fs::path> resolve_dependency_order(const std::vector<fs::path>& pkg_dirs);

// ---- Compile a package to a .a static library ----
// Returns the path to the compiled .a file.
// dep_includes: extra -I paths for dependencies' include/ directories.
fs::path compile_package(const fs::path& pkg_dir,
                         const std::vector<fs::path>& dep_includes = {});

} // namespace ezmk::pkg
