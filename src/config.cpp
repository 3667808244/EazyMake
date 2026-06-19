#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include <map>
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

LanguageInfo parse_language(std::string_view language) {
    // Format: <Lang><Version>
    // e.g. "C++17" → C++, std=c++17
    //       "C11"  → C,   std=c11
    LanguageInfo info;

    bool is_cxx = false;
    std::string_view ver_str;

    if (language.size() >= 3 && language.substr(0, 3) == "C++") {
        is_cxx = true;
        ver_str = language.substr(3);
    } else if (!language.empty() && language[0] == 'C') {
        is_cxx = false;
        ver_str = language.substr(1);
    } else {
        throw std::runtime_error(
            std::string("invalid language format: '") + std::string(language) +
            "'. Expected format: <Lang><Version>, e.g. C++17, C11");
    }

    // Map version string to -std= flag
    static const std::map<std::string, std::string> ver_map = {
        {"89", "89"}, {"98", "98"}, {"99", "99"},
        {"03", "03"}, {"11", "11"}, {"14", "14"},
        {"17", "17"}, {"20", "20"}, {"23", "23"}, {"26", "26"},
    };

    auto it = ver_map.find(std::string(ver_str));
    if (it == ver_map.end()) {
        throw std::runtime_error(
            std::string("unknown language version: '") + std::string(ver_str) +
            "'. Supported: 89, 99, 11, 14, 17, 20, 23");
    }

    if (is_cxx) {
        info.compiler = "g++";
        info.std_flag = "-std=c++" + it->second;
    } else {
        info.compiler = "gcc";
        info.std_flag = "-std=c" + it->second;
    }

    return info;
}

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
        if (auto ver = (*proj)["version"].value<std::string>()) {
            cfg.project.version = *ver;
        }
        if (auto lang = (*proj)["language"].value<std::string>()) {
            cfg.project.language = *lang;
        }
    }

    // project.version is required
    if (cfg.project.version.empty()) {
        throw std::runtime_error(
            "ezmk.toml: [project] section missing required field 'version'. "
            "Add: version = \"0.1.0\"");
    }

    // [compile]
    if (auto comp = root["compile"].as_table()) {
        cfg.compile.flags = extract_string_array(comp->get("flags"));

        // Try new field name "include_dirs" first, fall back to old "include_dir"
        auto inc_dirs = comp->get("include_dirs");
        if (inc_dirs && inc_dirs->is_array()) {
            cfg.compile.include_dirs = extract_string_array(inc_dirs);
        } else {
            cfg.compile.include_dirs = extract_string_array(comp->get("include_dir"));
        }
    }

    // Apply default for include_dirs if empty
    if (cfg.compile.include_dirs.empty()) {
        cfg.compile.include_dirs = {"include"};
    }

    // [link]
    if (auto link = root["link"].as_table()) {
        cfg.link.flags = extract_string_array(link->get("flags"));
        cfg.link.link_dirs = extract_string_array(link->get("link_dirs"));
        cfg.link.system_targets = extract_string_array(link->get("system_target"));
    }

    // [depends]
    if (auto deps = root["depends"].as_table()) {
        cfg.depends.libs = extract_string_array(deps->get("lib"));
    }

    return cfg;
}

void write_default_config(const fs::path& toml_path, std::string_view project_name,
                          std::string_view project_type) {
    std::string content;
    content += "[project]\n";
    content += "name = \"";
    content += project_name;
    content += "\"\n";
    content += "type = \"";
    content += project_type;
    content += "\"\n";
    content += "version = \"0.1.0\"\n";
    content += "language = \"C++17\"\n";
    content += "\n";
    content += "[compile]\n";
    content += "flags = [\"-Wall\", \"-Wextra\", \"-O2\"]\n";
    content += "include_dirs = [\"include\"]\n";
    content += "\n";
    content += "[link]\n";
    content += "flags = []\n";
    content += "link_dirs = []\n";
    content += "system_target = []\n";
    content += "\n";
    content += "[depends]\n";
    content += "lib = []\n";

    util::file_write(toml_path, content);
}

} // namespace ezmk::config
