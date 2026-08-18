#pragma once

#include <string>
#include <filesystem>

namespace ezmk::project {
namespace fs = std::filesystem;

// Create a new EazyMake project scaffold at ./<name>/
// project_type: "executable" (default), "static", "shared", or "utils".
// disable_git_init: skip git init even if git is available.
// disable_gitignore: skip .gitignore generation.
// Throws if the target directory already exists.
void create_project(const std::string& name,
                    const std::string& project_type,
                    bool disable_git_init = false,
                    bool disable_gitignore = false);

// 1.2.1: convert a project name into a valid C++ namespace identifier by
// replacing '-', '.', and ' ' with '_' (e.g. "my-lib" → "my_lib"). File
// names keep the original project name — only the C++ identifier needs
// sanitizing, since the filesystem allows those characters.
std::string sanitize_namespace(const std::string& name);

} // namespace ezmk::project
