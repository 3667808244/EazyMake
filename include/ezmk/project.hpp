#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace ezmk::project {

// Create a new EazyMake project scaffold at ./<name>/
// Throws if the target directory already exists.
void create_project(const std::string& name);

} // namespace ezmk::project
