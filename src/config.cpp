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

// Validate that a profile name contains only [a-zA-Z0-9_-].
static bool is_valid_profile_name(std::string_view name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
            return false;
    }
    return true;
}

// 0.9.6+ — Parse a single dependency entry string into name + version constraint.
// Syntax: "pkg" (no constraint), "pkg@1.2.3" (exact), "pkg^1.0" (compatible),
//         "pkg~1.2" (approx), "pkg>=1.0" (gte), "pkg>1.0" (gt).
static DependsEntry parse_depends_entry(std::string_view raw) {
    DependsEntry entry;

    // Trim leading/trailing whitespace
    auto start = raw.find_first_not_of(" \t");
    auto end   = raw.find_last_not_of(" \t");
    if (start == std::string_view::npos) {
        throw std::runtime_error(
            i18n::get(i18n::I18nKey::config_err_empty_depends_entry));
    }
    raw = raw.substr(start, end - start + 1);

    // Scan for constraint operators. Order: longest match first (>= before >).
    struct { std::string_view op; VersionConstraint::Op kind; } const ops[] = {
        {">=", VersionConstraint::Gte},
        {">",  VersionConstraint::Gt},
        {"^",  VersionConstraint::Compatible},
        {"~",  VersionConstraint::Approx},
        {"@",  VersionConstraint::Exact},
    };

    for (auto& o : ops) {
        auto pos = raw.find(o.op);
        if (pos != std::string_view::npos && pos > 0) {
            std::string_view name_part = raw.substr(0, pos);
            std::string_view ver_part  = raw.substr(pos + o.op.size());

            // Trim trailing whitespace from name
            auto name_end = name_part.find_last_not_of(" \t");
            if (name_end != std::string_view::npos)
                name_part = name_part.substr(0, name_end + 1);

            // Trim leading whitespace from version
            auto ver_start = ver_part.find_first_not_of(" \t");
            if (ver_start == std::string_view::npos) {
                throw std::runtime_error(
                    i18n::fmt(i18n::I18nKey::config_err_version_missing,
                              {{"entry", std::string(raw)}}));
            }
            ver_part = ver_part.substr(ver_start);

            if (name_part.empty()) {
                throw std::runtime_error(
                    i18n::get(i18n::I18nKey::config_err_empty_depends_entry));
            }

            entry.name = std::string(name_part);
            entry.constraint.op = o.kind;
            entry.constraint.version = std::string(ver_part);
            return entry;
        }
    }

    // No operator found — plain package name, no constraint
    entry.name = std::string(raw);
    return entry;
}

// 0.9.6+ — Extract an array of DependsEntry from a TOML node.
static std::vector<DependsEntry> extract_depends_array(const toml::node* node) {
    std::vector<DependsEntry> result;
    if (!node || !node->is_array()) return result;
    auto& arr = *node->as_array();
    for (size_t i = 0; i < arr.size(); ++i) {
        if (auto val = arr[i].value<std::string>()) {
            result.push_back(parse_depends_entry(*val));
        }
    }
    return result;
}

} // anonymous namespace

// 1.1.0-dev.4: Normalize a language/stdlib string (upper-case, trim).
// Used as a shared helper — language-specific C++/CXX → CPP is done in parse_language().
std::string normalize_lang(const std::string& input) {
    std::string result;

    // Step 1: to upper
    for (char c : input) {
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Step 2: trim whitespace
    auto start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";  // all whitespace → empty
    auto end = result.find_last_not_of(" \t\r\n");
    result = result.substr(start, end - start + 1);

    return result;
}

// 1.1.0-dev.4: Normalize a stdlib value to canonical form.
// Supports aliases: glibcxx/gnu → libstdc++, llvm → libc++.
// Returns "libstdc++" or "libc++"; throws on unrecognized input.
static std::string normalize_stdlib(const std::string& input) {
    std::string s = normalize_lang(input);
    if (s.empty()) {
        throw std::runtime_error(
            ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_stdlib,
                            {{"stdlib", input}}));
    }
    // Map aliases to canonical forms
    if (s == "GLIBCXX" || s == "GNU") return "libstdc++";
    if (s == "LIBSTDC++" || s == "LIBSTDCPP") return "libstdc++";
    if (s == "LLVM") return "libc++";
    if (s == "LIBC++" || s == "LIBCPP") return "libc++";
    throw std::runtime_error(
        ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_stdlib,
                        {{"stdlib", input}}));
}

LanguageInfo parse_language(std::string_view language) {
    // 1.1.0-dev.4: Normalize first, then parse.
    // Format after normalization: [GNU]CPP<Ver> or [GNU]C<Ver>
    // e.g. "C++17" → "CPP17" → std=c++17
    //      "GNUCPP17" → "GNUCPP17" → std=gnu++17
    LanguageInfo info;

    std::string normalized = normalize_lang(std::string(language));
    if (normalized.empty()) {
        throw std::runtime_error(
            ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang,
                            {{"lang", std::string(language)}}));
    }

    // Unify C++/CXX → CPP for language identification
    {
        size_t pos = 0;
        while ((pos = normalized.find("C++", pos)) != std::string::npos) {
            normalized.replace(pos, 3, "CPP");
            pos += 3;
        }
        pos = 0;
        while ((pos = normalized.find("CXX", pos)) != std::string::npos) {
            normalized.replace(pos, 3, "CPP");
            pos += 3;
        }
    }

    // Detect GNU prefix
    bool gnu = false;
    std::string_view remainder(normalized);
    if (remainder.size() >= 3 && remainder.substr(0, 3) == "GNU") {
        gnu = true;
        remainder = remainder.substr(3);
    }

    // Parse CPP<Ver> or C<Ver> (or plain version for GNU C, e.g. GNU11)
    bool is_cxx = false;
    std::string_view ver_str;
    if (remainder.size() >= 3 && remainder.substr(0, 3) == "CPP") {
        is_cxx = true;
        ver_str = remainder.substr(3);
    } else if (!remainder.empty() && remainder[0] == 'C') {
        is_cxx = false;
        ver_str = remainder.substr(1);
    } else if (gnu && !remainder.empty() && std::isdigit(static_cast<unsigned char>(remainder[0]))) {
        // GNU + version only (e.g., GNU11, GNU17) → C language with GNU extensions
        is_cxx = false;
        ver_str = remainder;
    } else {
        // Not recognized — provide a helpful error
        throw std::runtime_error(
            ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang,
                            {{"lang", std::string(language)}}));
    }

    // Default version when missing
    if (ver_str.empty()) {
        // Default: C++17 or C11
        ver_str = is_cxx ? "17" : "11";
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
        info.std_flag = gnu ? ("-std=gnu++" + it->second) : ("-std=c++" + it->second);
    } else {
        info.compiler = "gcc";
        info.std_flag = gnu ? ("-std=gnu" + it->second) : ("-std=c" + it->second);
    }

    info.gnu_extensions = gnu;
    info.normalized_lang = gnu ? ("GNU" + std::string(remainder)) : normalized;

    // Warn about non-ISO extensions
    if (gnu) {
        std::string suggestion = is_cxx ? "CPP" : "C";
        suggestion += std::string(ver_str);
        util::warn(ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_warn_gnu_extensions,
                                   {{"lang", std::string(language)},
                                    {"suggestion", suggestion}}));
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
        // 1.1.0-dev.4: stdlib (optional, default "libstdc++")
        if (auto sl = (*proj)["stdlib"].value<std::string>()) {
            cfg.project.stdlib = normalize_stdlib(*sl);
        }
        // 0.9.7+: header_only — skip compilation for header-only packages
        if (auto ho = (*proj)["header_only"].as_boolean()) {
            cfg.project.header_only = ho->get();
        }
        // 0.9.7+: precompiled — use lib/*.a directly, skip compilation
        if (auto pc = (*proj)["precompiled"].as_boolean()) {
            cfg.project.precompiled = pc->get();
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

        // 1.1.0: deterministic — reproducible builds
        if (auto det = (*comp)["deterministic"].as_boolean()) {
            cfg.compile.deterministic = det->get();
        }

        // 1.1.0: source_date_epoch — SOURCE_DATE_EPOCH override
        if (auto sde = (*comp)["source_date_epoch"].value<int64_t>()) {
            if (*sde >= 0) cfg.compile.source_date_epoch = static_cast<uint64_t>(*sde);
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
        cfg.depends.libs = extract_depends_array(deps->get("lib"));
        // 0.2.2+: optional dependencies
        cfg.depends.want = extract_depends_array(deps->get("want"));
    }

    // 0.2.3+: [compile.profile.<name>] — build configuration profiles
    if (auto comp = root["compile"].as_table()) {
        if (auto profiles = (*comp)["profile"].as_table()) {
            for (auto& [key, val] : *profiles) {
                std::string profile_name(key.str());
                if (!is_valid_profile_name(profile_name)) {
                    throw std::runtime_error(
                        profile_name.empty()
                            ? ezmk::i18n::get(ezmk::i18n::I18nKey::config_err_empty_profile)
                            : ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_profile,
                                              {{"name", profile_name}}));
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
                if (!is_valid_profile_name(profile_name)) {
                    throw std::runtime_error(
                        profile_name.empty()
                            ? ezmk::i18n::get(ezmk::i18n::I18nKey::config_err_empty_profile)
                            : ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_profile,
                                              {{"name", profile_name}}));
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

    // 1.1.0: [install] — project install configuration
    if (auto inst = root["install"].as_table()) {
        if (auto v = (*inst)["prefix"].value<std::string>())
            cfg.install.prefix = *v;
        if (auto v = (*inst)["bindir"].value<std::string>())
            cfg.install.bindir = *v;
        if (auto v = (*inst)["libdir"].value<std::string>())
            cfg.install.libdir = *v;
        if (auto v = (*inst)["includedir"].value<std::string>())
            cfg.install.includedir = *v;
        if (auto v = (*inst)["sharedir"].value<std::string>())
            cfg.install.sharedir = *v;
    }
    // Apply defaults for install section
    if (cfg.install.bindir.empty()) cfg.install.bindir = "bin";
    if (cfg.install.libdir.empty()) cfg.install.libdir = "lib";
    if (cfg.install.includedir.empty()) cfg.install.includedir = "include";
    if (cfg.install.sharedir.empty()) cfg.install.sharedir = "share";
    if (cfg.install.prefix.empty()) {
#ifdef EZMK_WIN
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (appdata) cfg.install.prefix = std::string(appdata) + "\\ezmk";
        else cfg.install.prefix = (util::get_home_dir() / "AppData/Local/ezmk").string();
#else
        cfg.install.prefix = (util::get_home_dir() / ".local").string();
#endif
    }
    // Expand ~ in prefix
    if (!cfg.install.prefix.empty() && cfg.install.prefix[0] == '~') {
        cfg.install.prefix = (util::get_home_dir() / cfg.install.prefix.substr(2)).string();
    }

    // [utils] (only relevant for type = "utils")
    if (auto utils = root["utils"].as_table()) {
        cfg.utils.tools = extract_string_array(utils->get("tools"));

        // 0.2.5+: [utils.permissions] — fine-grained read/write/run control.
        // Presence of the sub-table switches on the deny/allow/ask model;
        // absence keeps the legacy unrestricted behavior.
        if (auto perms = (*utils)["permissions"].as_table()) {
            UtilsPermissions up;
            up.read       = extract_string_array(perms->get("read"));
            up.read_deny  = extract_string_array(perms->get("read_deny"));
            up.write      = extract_string_array(perms->get("write"));
            up.write_deny = extract_string_array(perms->get("write_deny"));
            up.run        = extract_string_array(perms->get("run"));
            up.run_deny   = extract_string_array(perms->get("run_deny"));
            if (auto net = perms->get("network")) {
                if (net->is_boolean()) {
                    up.network = net->as_boolean()->get();
                }
            }
            cfg.utils.permissions = std::move(up);
        }
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
