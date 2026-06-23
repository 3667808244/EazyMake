#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace ezmk::config {
namespace fs = std::filesystem;

struct ProjectSection {
    std::string name;
    std::string type = "executable"; // "executable" | "static" | "shared"
    std::string version;             // required, e.g. "0.1.0"
    std::string language = "C++17";  // <Lang><Ver>, e.g. "C++17", "C11"
};

struct CompileSection {
    std::vector<std::string> flags;
    std::vector<std::string> include_dirs;  // -I paths (default ["include"])
};

struct LinkSection {
    std::vector<std::string> flags;
    std::vector<std::string> link_dirs;       // -L paths (default [])
    std::vector<std::string> system_targets;  // -l libs (e.g. "pthread")
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

// Write a default ezmk.toml to the given path (used by `ezmk project new`).
// project_type: "executable" (default), "static", or "shared".
void write_default_config(const fs::path& toml_path, std::string_view project_name,
                          std::string_view project_type = "executable");

// Parse a language string like "C++17" into compiler name and -std= flag.
// Returns {"g++", "-std=c++17"} for C++; {"gcc", "-std=c11"} for C.
struct LanguageInfo {
    std::string compiler;            // default compiler from config ("g++" or "gcc")
    std::string std_flag;            // e.g. "-std=c++17"
    std::string detected_compiler;   // runtime-detected compiler, empty if not yet probed
};
LanguageInfo parse_language(std::string_view language);

} // namespace ezmk::config
