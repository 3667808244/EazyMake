#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include <stdexcept>
#include <string>

// toml++ header-only (exceptions enabled by default: parse_file returns table directly)
#include "toml.hpp"

namespace ezmk::config {

namespace {

std::vector<std::string> extract_string_array(const toml::node* node) {
    std::vector<std::string> result;
    if (!node || !node->is_array()) return result;
    auto& arr = *node->as_array();
    for (size_t i = 0; i < arr.size(); ++i) {
        if (auto val = arr[i].value<std::string>()) {
            result.push_back(*val);
        }
    }
    return result;
}

} // anonymous namespace

EzConfig parse_config(const fs::path& toml_path) {
    EzConfig cfg;

    if (!util::file_exists(toml_path)) {
        throw std::runtime_error("config file not found: " + toml_path.string());
    }

    // toml++ with exceptions: throws on parse error, returns table on success
    auto root = toml::parse_file(toml_path.string());

    // [project]
    if (auto proj = root["project"].as_table()) {
        if (auto name = (*proj)["name"].value<std::string>()) {
            cfg.project.name = *name;
        }
        if (auto type = (*proj)["type"].value<std::string>()) {
            cfg.project.type = *type;
        }
    }

    // [compile]
    if (auto comp = root["compile"].as_table()) {
        cfg.compile.flags = extract_string_array(comp->get("flags"));
        cfg.compile.include_dirs = extract_string_array(comp->get("include_dir"));
    }

    // [link]
    if (auto link = root["link"].as_table()) {
        cfg.link.flags = extract_string_array(link->get("flags"));
        cfg.link.system_targets = extract_string_array(link->get("system_target"));
    }

    // [depends]
    if (auto deps = root["depends"].as_table()) {
        cfg.depends.libs = extract_string_array(deps->get("lib"));
    }

    return cfg;
}

void write_default_config(const fs::path& toml_path, std::string_view project_name) {
    std::string content;
    content += "[project]\n";
    content += "name = \"";
    content += project_name;
    content += "\"\n";
    content += "type = \"executable\"\n";
    content += "\n";
    content += "[compile]\n";
    content += "flags = [\"-Wall\", \"-Wextra\", \"-O2\"]\n";
    content += "include_dir = []\n";
    content += "\n";
    content += "[link]\n";
    content += "flags = []\n";
    content += "system_target = []\n";
    content += "\n";
    content += "[depends]\n";
    content += "lib = []\n";

    util::file_write(toml_path, content);
}

} // namespace ezmk::config
