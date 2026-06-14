#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace ezmk::config {

struct ProjectSection {
    std::string name;
    std::string type = "executable"; // "executable" | "static" | "shared" (only executable for now)
};

struct CompileSection {
    std::vector<std::string> flags;
    std::vector<std::string> include_dirs;
};

struct LinkSection {
    std::vector<std::string> flags;
    std::vector<std::string> system_targets;
};

struct DependsSection {
    std::vector<std::string> libs;
};

struct EzConfig {
    ProjectSection project;
    CompileSection compile;
    LinkSection link;
    DependsSection depends;
};

// Parse an ezmk.toml file. Throws on parse errors.
EzConfig parse_config(const fs::path& toml_path);

// Write a default ezmk.toml to the given path (used by `ezmk new`).
void write_default_config(const fs::path& toml_path, std::string_view project_name);

} // namespace ezmk::config
