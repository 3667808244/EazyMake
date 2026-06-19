#pragma once

#include <string>
#include <filesystem>

namespace ezmk::project {
namespace fs = std::filesystem;

// Create a new EazyMake project scaffold at ./<name>/
// project_type: "executable" (default), "static", or "shared".
// Throws if the target directory already exists.
void create_project(const std::string& name,
                    const std::string& project_type);

} // namespace ezmk::project
