#pragma once

#include <string>
#include <filesystem>

namespace ezmk::project {
namespace fs = std::filesystem;

// Create a new EazyMake project scaffold at ./<name>/
// project_type: "executable" (default), "static", or "shared".
// disable_git_init: skip git init even if git is available.
// disable_gitignore: skip .gitignore generation.
// Throws if the target directory already exists.
void create_project(const std::string& name,
                    const std::string& project_type,
                    bool disable_git_init = false,
                    bool disable_gitignore = false);

} // namespace ezmk::project
