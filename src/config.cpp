#include "ezmk/config.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <cctype>
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

// Validate that a macro name is a legal C identifier: [A-Za-z_][A-Za-z0-9_]*
static bool is_valid_macro_name(std::string_view name) {
    if (name.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_') return false;
    for (size_t i = 1; i < name.size(); ++i) {
        char c = name[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
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
            ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang,
                            {{"lang", std::string(language)}}));
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
    toml::table root;
    try {
        root = toml::parse_file(toml_path.string());
    } catch (const toml::parse_error& e) {
        // e.what() includes file path, line, and column info
        throw std::runtime_error(
            std::string("failed to parse ") + toml_path.string() + ":\n  " + e.what());
    }

    // [project]
    if (auto proj = root["project"].as_table()) {
        if (auto name = (*proj)["name"].value<std::string>()) {
            cfg.project.name = *name;
        }
        if (auto type = (*proj)["type"].value<std::string>()) {
            // Validate type
            if (*type != "executable" && *type != "static" &&
                *type != "shared" && *type != "utils") {
                throw std::runtime_error(
                    ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_type,
                                    {{"type", *type}}));
            }
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
            ezmk::i18n::get(ezmk::i18n::I18nKey::config_err_missing_ver));
    }

    // [compile]
    if (auto comp = root["compile"].as_table()) {
        cfg.compile.flags = extract_string_array(comp->get("flags"));
        cfg.compile.msvc_flags = extract_string_array(comp->get("msvc_flags"));

        // Try new field name "include_dirs" first, fall back to old "include_dir"
        auto inc_dirs = comp->get("include_dirs");
        if (inc_dirs && inc_dirs->is_array()) {
            cfg.compile.include_dirs = extract_string_array(inc_dirs);
        } else {
            cfg.compile.include_dirs = extract_string_array(comp->get("include_dir"));
        }

        // 0.2.2+: src_dirs — multiple source directories
        cfg.compile.src_dirs = extract_string_array(comp->get("src_dirs"));

        // 0.2.2+: ezmk_macros — inject EZMK_* standard macros (default true)
        if (auto ezm = comp->get("ezmk_macros")) {
            if (ezm->is_boolean()) {
                cfg.compile.ezmk_macros = ezm->as_boolean()->get();
            } else {
                throw std::runtime_error(
                    ezmk::i18n::get(ezmk::i18n::I18nKey::config_err_ezmk_macros_type));
            }
        }
    }

    // Apply default for include_dirs if empty
    if (cfg.compile.include_dirs.empty()) {
        cfg.compile.include_dirs = {"include"};
    }

    // Apply default for src_dirs if empty (0.2.2+)
    if (cfg.compile.src_dirs.empty()) {
        cfg.compile.src_dirs = {"src"};
    }

    // 0.2.2+: validate src_dirs is not explicitly set to empty
    // (check the raw TOML to distinguish "not set" from "set to []")
    if (auto comp = root["compile"].as_table()) {
        auto raw_src_dirs = comp->get("src_dirs");
        if (raw_src_dirs && raw_src_dirs->is_array() &&
            raw_src_dirs->as_array()->size() == 0) {
            throw std::runtime_error(
                ezmk::i18n::get(ezmk::i18n::I18nKey::config_err_empty_src_dirs));
        }
    }

    // 0.2.2+: [compile.macros] — semantic macro definitions
    if (auto macros_node = root["compile"]["macros"].as_table()) {
        for (auto& [key, val] : *macros_node) {
            std::string macro_key(key.str());
            if (!is_valid_macro_name(macro_key)) {
                throw std::runtime_error(
                    ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_macro,
                                    {{"name", macro_key}}));
            }
            std::string macro_val;
            if (val.is_string()) {
                macro_val = val.as_string()->get();
            } else if (val.is_integer()) {
                macro_val = std::to_string(val.as_integer()->get());
            } else if (val.is_boolean()) {
                if (!val.as_boolean()->get()) continue; // false → skip
                macro_val = "1";
            } else {
                throw std::runtime_error(
                    ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_macros_val_type,
                                    {{"key", macro_key}}));
            }
            cfg.compile.macros[macro_key] = macro_val;
        }
    }

    // [link]
    if (auto link = root["link"].as_table()) {
        cfg.link.flags = extract_string_array(link->get("flags"));
        cfg.link.msvc_flags = extract_string_array(link->get("msvc_flags"));
        cfg.link.link_dirs = extract_string_array(link->get("link_dirs"));
        cfg.link.system_targets = extract_string_array(link->get("system_target"));
    }

    // [depends]
    if (auto deps = root["depends"].as_table()) {
        cfg.depends.libs = extract_string_array(deps->get("lib"));
        // 0.2.2+: optional dependencies
        cfg.depends.want = extract_string_array(deps->get("want"));
    }

    // 0.2.3+: [compile.profile.<name>] — build configuration profiles
    if (auto comp = root["compile"].as_table()) {
        if (auto profiles = (*comp)["profile"].as_table()) {
            for (auto& [key, val] : *profiles) {
                std::string profile_name(key.str());
                // Validate profile name: [a-zA-Z0-9_-]+
                if (profile_name.empty()) {
                    throw std::runtime_error(
                        ezmk::i18n::get(ezmk::i18n::I18nKey::config_err_empty_profile));
                }
                for (char c : profile_name) {
                    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
                        throw std::runtime_error(
                            ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_profile,
                                            {{"name", profile_name}}));
                    }
                }

                ProfileConfig pc;
                if (auto prof_table = val.as_table()) {
                    pc.flags = extract_string_array(prof_table->get("flags"));
                    pc.msvc_flags = extract_string_array(prof_table->get("msvc_flags"));

                    // Parse macros sub-table within profile
                    if (auto macros_node = (*prof_table)["macros"].as_table()) {
                        for (auto& [mk, mv] : *macros_node) {
                            std::string macro_key(mk.str());
                            if (!is_valid_macro_name(macro_key)) {
                                throw std::runtime_error(
                                    ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_macro,
                                                    {{"name", macro_key}}));
                            }
                            std::string macro_val;
                            if (mv.is_string()) {
                                macro_val = mv.as_string()->get();
                            } else if (mv.is_integer()) {
                                macro_val = std::to_string(mv.as_integer()->get());
                            } else if (mv.is_boolean()) {
                                if (!mv.as_boolean()->get()) continue;
                                macro_val = "1";
                            } else {
                                throw std::runtime_error(
                                    ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_macros_val_type,
                                                    {{"key", macro_key}}));
                            }
                            pc.macros[macro_key] = macro_val;
                        }
                    }
                }
                cfg.compile_profiles[profile_name] = std::move(pc);
            }
        }
    }

    // 0.2.3+: [link.profile.<name>] — link configuration profiles
    if (auto link = root["link"].as_table()) {
        if (auto profiles = (*link)["profile"].as_table()) {
            for (auto& [key, val] : *profiles) {
                std::string profile_name(key.str());
                if (profile_name.empty()) {
                    throw std::runtime_error(
                        ezmk::i18n::get(ezmk::i18n::I18nKey::config_err_empty_profile));
                }
                for (char c : profile_name) {
                    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
                        throw std::runtime_error(
                            ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_profile,
                                            {{"name", profile_name}}));
                    }
                }

                ProfileLinkConfig plc;
                if (auto prof_table = val.as_table()) {
                    plc.flags = extract_string_array(prof_table->get("flags"));
                    plc.msvc_flags = extract_string_array(prof_table->get("msvc_flags"));
                }
                cfg.link_profiles[profile_name] = std::move(plc);
            }
        }
    }

    // 0.2.3+: [hooks] — pre/post-build Lua hook scripts
    if (auto hooks = root["hooks"].as_table()) {
        if (auto pre = (*hooks)["pre_build"].value<std::string>()) {
            cfg.hooks.pre_build = *pre;
        }
        if (auto post = (*hooks)["post_build"].value<std::string>()) {
            cfg.hooks.post_build = *post;
        }
        if (auto fail = (*hooks)["on_failure"].value<std::string>()) {
            cfg.hooks.on_failure = *fail;
        }
    }

    // [utils] (only relevant for type = "utils")
    if (auto utils = root["utils"].as_table()) {
        cfg.utils.tools = extract_string_array(utils->get("tools"));
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

    if (project_type == "utils") {
        content += "\n";
        content += "[utils]\n";
        content += "tools = []\n";
    }

    if (!util::file_write(toml_path, content)) {
        throw std::runtime_error("failed to write config file: " + toml_path.string());
    }
}

} // namespace ezmk::config
