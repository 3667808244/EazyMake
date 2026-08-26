#include "ezmk/config.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>

// toml++ header-only (exceptions enabled by default: parse_file returns table directly)
#include "toml.hpp"

// 1.1.3 Q1: .ezmk/links.json 改用 nlohmann/json（项目已依赖，见 cache.cpp）
#include "nlohmann_json.hpp"

namespace ezmk::config {

namespace {

// 1.2.0-dev.11: array fields must contain only strings — a type mismatch (e.g.
// `flags = ["-Wall", 42]`) used to be silently dropped, hiding config mistakes.
// `field` names the config field for the error message.
std::vector<std::string> extract_string_array(const toml::node* node,
                                              const char* field) {
    std::vector<std::string> result;
    if (!node || !node->is_array()) return result;
    auto& arr = *node->as_array();
    for (size_t i = 0; i < arr.size(); ++i) {
        if (auto val = arr[i].value<std::string>()) {
            result.push_back(*val);
        } else {
            throw std::runtime_error(
                i18n::fmt(i18n::I18nKey::config_err_array_type,
                          {{"field", field ? field : ""},
                           {"index", std::to_string(i)}}));
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
// 1.2.0-dev.11: the version part is validated (numeric dotted core) and a bare
// constraint (e.g. ">=1.0" with no package name) is rejected instead of being
// treated as a package named ">=1.0".
static bool is_valid_version_string(std::string_view v) {
    // Core: digits ( '.' digits )* — mirrors util::compare_version's parse.
    size_t i = 0;
    bool saw_digit = false;
    while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) {
        saw_digit = true;
        ++i;
    }
    if (!saw_digit) return false;
    while (i < v.size() && v[i] == '.') {
        ++i;
        bool seg = false;
        while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) {
            seg = true;
            ++i;
        }
        if (!seg) return false;
    }
    // Optional -prerelease / +build suffix: compare_version strips everything
    // after the first '-' or '+', so any suffix is accepted here.
    if (i < v.size() && (v[i] == '-' || v[i] == '+')) return true;
    return i == v.size();
}

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
    // 1.2.0-dev.11: pick the EARLIEST occurrence across all ops — the previous
    // ordered first-match scan mis-parsed compound constraints like "pkg@^1.0"
    // ("^" matched before "@" → name became "pkg@").
    struct { std::string_view op; VersionConstraint::Op kind; } const ops[] = {
        {">=", VersionConstraint::Gte},
        {">",  VersionConstraint::Gt},
        {"^",  VersionConstraint::Compatible},
        {"~",  VersionConstraint::Approx},
        {"@",  VersionConstraint::Exact},
    };

    size_t best_pos = std::string_view::npos;
    size_t best_idx = 0;
    for (size_t oi = 0; oi < sizeof(ops) / sizeof(ops[0]); ++oi) {
        auto pos = raw.find(ops[oi].op);
        if (pos != std::string_view::npos &&
            (best_pos == std::string_view::npos || pos < best_pos)) {
            best_pos = pos;
            best_idx = oi;
        }
    }

    if (best_pos != std::string_view::npos) {
        // 1.2.0-dev.11: a constraint at position 0 means the package name
        // is missing ("pkg>=1.0" is valid, ">=1.0" is not).
        if (best_pos == 0) {
            throw std::runtime_error(
                i18n::get(i18n::I18nKey::config_err_empty_depends_entry));
        }
        auto& o = ops[best_idx];
        std::string_view name_part = raw.substr(0, best_pos);
        std::string_view ver_part  = raw.substr(best_pos + o.op.size());

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

        // 1.2.0-dev.11: reject malformed versions at parse time instead of
        // failing cryptically later at install/compare time.
        if (!is_valid_version_string(ver_part)) {
            throw std::runtime_error(
                i18n::fmt(i18n::I18nKey::config_err_invalid_version,
                          {{"entry", std::string(raw)}}));
        }

        entry.name = std::string(name_part);
        entry.constraint.op = o.kind;
        entry.constraint.version = std::string(ver_part);
        return entry;
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
    // 1.3.1: range constraints are stripped BEFORE C++/CXX→CPP replacement and
    // GNU prefix detection, so ">=GNUCPP11" works naturally.
    // Format after normalization: [>=]<Lang>[..<Lang>], each <Lang> = [GNU]CPP<Ver> / [GNU]C<Ver>
    // e.g. "C++17" → "CPP17" → std=c++17
    //      "GNUCPP17" → "GNUCPP17" → std=gnu++17
    //      ">=C++11" → min=11, max=0 → std=c++11
    //      "C++11..C++17" → min=11, max=17 → std=c++11 (effective flag = min)
    LanguageInfo info;

    std::string normalized = normalize_lang(std::string(language));
    if (normalized.empty()) {
        throw std::runtime_error(
            ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang,
                            {{"lang", std::string(language)}}));
    }

    // ---- 1.3.1: strip range constraints (">=" single-sided, ".." double-sided) ----
    std::string min_part;
    std::string max_part;  // empty = no upper bound
    {
        if (normalized.rfind(">=", 0) == 0) {
            // ">=C++11" — single-sided lower bound, no upper bound allowed.
            min_part = normalized.substr(2);
            auto dd = min_part.find("..");
            if (dd != std::string::npos) {
                throw std::runtime_error(
                    ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang_range,
                                    {{"lang", std::string(language)},
                                     {"min", min_part.substr(0, dd)},
                                     {"max", min_part.substr(dd + 2)}}));
            }
        } else if (normalized[0] == '>') {
            // ">C++11" — only ">=" is supported.
            throw std::runtime_error(
                ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang,
                                {{"lang", std::string(language)}}));
        } else {
            auto dotdot = normalized.find("..");
            if (dotdot != std::string::npos) {
                min_part = normalized.substr(0, dotdot);
                max_part = normalized.substr(dotdot + 2);
                if (min_part.empty() || max_part.empty()) {
                    throw std::runtime_error(
                        ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang_range,
                                        {{"lang", std::string(language)},
                                         {"min", min_part},
                                         {"max", max_part}}));
                }
            } else {
                min_part = normalized;
            }
        }
        if (min_part.empty()) {
            // bare ">=" — nothing to parse.
            throw std::runtime_error(
                ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang,
                                {{"lang", std::string(language)}}));
        }
    }

    // Unify C++/CXX → CPP for language identification (both range ends).
    auto unify_cpp = [](std::string& s) {
        size_t pos = 0;
        while ((pos = s.find("C++", pos)) != std::string::npos) {
            s.replace(pos, 3, "CPP");
            pos += 3;
        }
        pos = 0;
        while ((pos = s.find("CXX", pos)) != std::string::npos) {
            s.replace(pos, 3, "CPP");
            pos += 3;
        }
    };
    unify_cpp(min_part);
    if (!max_part.empty()) unify_cpp(max_part);

    // Parse a single language part → family + GNU prefix + version string.
    struct LangPart {
        bool gnu = false;
        bool is_cxx = true;
        std::string ver;
    };
    auto parse_part = [&](const std::string& part) -> LangPart {
        LangPart out;
        std::string_view remainder(part);
        if (remainder.size() >= 3 && remainder.substr(0, 3) == "GNU") {
            out.gnu = true;
            remainder = remainder.substr(3);
        }
        if (remainder.size() >= 3 && remainder.substr(0, 3) == "CPP") {
            out.is_cxx = true;
            out.ver = std::string(remainder.substr(3));
        } else if (!remainder.empty() && remainder[0] == 'C') {
            out.is_cxx = false;
            out.ver = std::string(remainder.substr(1));
        } else if (out.gnu && !remainder.empty() &&
                   std::isdigit(static_cast<unsigned char>(remainder[0]))) {
            // GNU + version only (e.g., GNU11, GNU17) → C language with GNU extensions
            out.is_cxx = false;
            out.ver = std::string(remainder);
        } else {
            // Not recognized — provide a helpful error
            throw std::runtime_error(
                ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang,
                                {{"lang", std::string(language)}}));
        }
        // 1.3.1: "+" suffix ("C++11+") is not supported — reject explicitly.
        if (out.ver.find('+') != std::string::npos) {
            throw std::runtime_error(
                ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang,
                                {{"lang", std::string(language)}}));
        }
        return out;
    };

    LangPart lo = parse_part(min_part);
    LangPart hi;
    if (!max_part.empty()) hi = parse_part(max_part);

    // Default version when missing (C++17 or C11)
    if (lo.ver.empty()) lo.ver = lo.is_cxx ? "17" : "11";
    if (!max_part.empty() && hi.ver.empty()) hi.ver = hi.is_cxx ? "17" : "11";

    // 1.3.1: both ends of a range must be the same language family.
    if (!max_part.empty() && lo.is_cxx != hi.is_cxx) {
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
    // 1.3.6: derive the supported list from ver_map itself — the old hardcoded
    // string drifted from the map (missing 98/03/26) and misled users.
    static const std::string kSupportedVers = [] {
        std::string s;
        for (auto& [k, v] : ver_map) {
            if (!s.empty()) s += ", ";
            s += k;
        }
        return s;
    }();

    auto it = ver_map.find(lo.ver);
    if (it == ver_map.end()) {
        throw std::runtime_error(
            std::string("unknown language version: '") + lo.ver +
            "'. Supported: " + kSupportedVers);
    }
    if (!max_part.empty()) {
        if (ver_map.find(hi.ver) == ver_map.end()) {
            throw std::runtime_error(
                std::string("unknown language version: '") + hi.ver +
                "'. Supported: " + kSupportedVers);
        }
    }

    // 1.3.1: numeric bounds — the effective -std= flag comes from MIN.
    int min_ver = std::stoi(lo.ver);
    int max_ver = max_part.empty() ? 0 : std::stoi(hi.ver);
    if (max_ver != 0 && max_ver < min_ver) {
        throw std::runtime_error(
            ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_err_invalid_lang_range,
                            {{"lang", std::string(language)},
                             {"min", std::to_string(min_ver)},
                             {"max", std::to_string(max_ver)}}));
    }

    if (lo.is_cxx) {
        info.compiler = "g++";
        info.std_flag = lo.gnu ? ("-std=gnu++" + it->second) : ("-std=c++" + it->second);
    } else {
        info.compiler = "gcc";
        info.std_flag = lo.gnu ? ("-std=gnu" + it->second) : ("-std=c" + it->second);
    }

    info.gnu_extensions = lo.gnu;
    // 1.3.1: normalized_lang = the MIN canonical form (min_part after unify),
    // so EZMK_LANG stays identical for "C++11" and ">=C++11" / "C++11..C++17".
    info.normalized_lang = min_part;
    info.min_ver = min_ver;
    info.max_ver = max_ver;

    // Warn about non-ISO extensions
    if (lo.gnu) {
        std::string suggestion = lo.is_cxx ? "CPP" : "C";
        suggestion += lo.ver;
        util::warn(ezmk::i18n::fmt(ezmk::i18n::I18nKey::config_warn_gnu_extensions,
                                   {{"lang", std::string(language)},
                                    {"suggestion", suggestion}}));
    }

    return info;
}

// ===================================================================
// 1.1.0-dev.5: .ezmk/links.json — link mechanism
// ===================================================================

std::map<std::string, std::string> load_links_json(const fs::path& project_root) {
    std::map<std::string, std::string> links;
    auto links_file = project_root / ".ezmk" / "links.json";
    if (!util::file_exists(links_file)) {
        return links; // empty — no links defined
    }

    std::string content = util::file_read(links_file);
    if (content.empty()) return links;

    // 1.1.3 Q1: 改用项目已依赖的 nlohmann/json。原手写解析器不支持 Unicode 与
    // 标准反斜杠转义（\uXXXX / \n 等）、无递归，且遇畸形输入易误判。
    // 格式：{ "name": "path", ... }；返回结构与调用点签名不变。
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(content);
    } catch (const std::exception&) {
        throw std::runtime_error(".ezmk/links.json: invalid JSON");
    }
    if (!j.is_object()) {
        throw std::runtime_error(".ezmk/links.json: expected JSON object");
    }

    for (auto& [key, val] : j.items()) {
        if (!val.is_string()) {
            throw std::runtime_error(".ezmk/links.json: expected string value for key \"" + key + "\"");
        }
        std::string value = val.get<std::string>();

        // Validate key: [A-Za-z0-9_-]+
        if (key.empty()) {
            throw std::runtime_error(".ezmk/links.json: empty link name");
        }
        for (char ch : key) {
            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-') {
                throw std::runtime_error(".ezmk/links.json: invalid link name '" + key + "'");
            }
        }

        // Validate value: no absolute paths, no traversal attempts
        if (!value.empty() && (value[0] == '/' || value[0] == '\\')) {
            throw std::runtime_error(".ezmk/links.json: absolute paths not allowed for link '" + key + "'");
        }

        links[key] = value;
    }

    return links;
}

fs::path resolve_link_path(std::string_view name,
                            std::string_view sub_path,
                            const std::map<std::string, std::string>& links,
                            int max_depth) {
    if (max_depth <= 0) {
        throw std::runtime_error(
            "link resolution depth exceeded (>10) for '" + std::string(name) +
            "': possible infinite chain or too many indirections");
    }

    auto it = links.find(std::string(name));
    if (it == links.end()) {
        throw std::runtime_error(
            "link '" + std::string(name) + "' not found in .ezmk/links.json");
    }

    fs::path resolved = it->second;
    if (!sub_path.empty()) {
        resolved = resolved / std::string(sub_path);
    }

    return resolved;
}

// 1.1.3 Q2: parse_config 按 TOML 节拆分为私有 helper（纯提取，不改行为）。
// 每个 helper 只读 root 对应子表并写入 cfg；parse_config 只做编排。

static void parse_project(const toml::table& root, EzConfig& cfg) {
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
        // 1.2.0-dev.10: precompiled_strict — refuse ABI-unsafe toolchain fallback
        if (auto ps = (*proj)["precompiled_strict"].as_boolean()) {
            cfg.project.precompiled_strict = ps->get();
        }
    }
}

static void parse_compile(const toml::table& root, EzConfig& cfg) {
    // [compile]
    if (auto comp = root["compile"].as_table()) {
        cfg.compile.flags = extract_string_array(comp->get("flags"), "compile.flags");
        cfg.compile.msvc_flags = extract_string_array(comp->get("msvc_flags"), "compile.msvc_flags");

        // Try new field name "include_dirs" first, fall back to old "include_dir"
        auto inc_dirs = comp->get("include_dirs");
        if (inc_dirs && inc_dirs->is_array()) {
            cfg.compile.include_dirs = extract_string_array(inc_dirs, "compile.include_dirs");
        } else {
            cfg.compile.include_dirs = extract_string_array(comp->get("include_dir"), "compile.include_dir");
        }

        // 0.2.2+: src_dirs — multiple source directories
        cfg.compile.src_dirs = extract_string_array(comp->get("src_dirs"), "compile.src_dirs");

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
        // 1.2.0-dev.11: negative values were silently ignored — reject them.
        if (auto sde = (*comp)["source_date_epoch"].value<int64_t>()) {
            if (*sde < 0) {
                throw std::runtime_error(
                    i18n::get(i18n::I18nKey::config_err_invalid_source_date_epoch));
            }
            cfg.compile.source_date_epoch = static_cast<uint64_t>(*sde);
        }

        // 1.1.1: compile_commands — auto-generate compile_commands.json after build
        if (auto cc = (*comp)["compile_commands"].as_boolean()) {
            cfg.compile.compile_commands = cc->get();
        }

        // 1.2.0-dev.3: default_profile — default profile when no --profile given
        if (auto dp = (*comp)["default_profile"].value<std::string>()) {
            cfg.compile.default_profile = *dp;
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
}

static void parse_link(const toml::table& root, EzConfig& cfg) {
    // [link]
    if (auto link = root["link"].as_table()) {
        cfg.link.flags = extract_string_array(link->get("flags"), "link.flags");
        cfg.link.msvc_flags = extract_string_array(link->get("msvc_flags"), "link.msvc_flags");
        cfg.link.link_dirs = extract_string_array(link->get("link_dirs"), "link.link_dirs");
        cfg.link.system_targets = extract_string_array(link->get("system_target"), "link.system_targets");
    }
}

static void parse_depends(const toml::table& root, EzConfig& cfg) {
    // [depends]
    if (auto deps = root["depends"].as_table()) {
        cfg.depends.libs = extract_depends_array(deps->get("lib"));
        // 0.2.2+: optional dependencies
        cfg.depends.want = extract_depends_array(deps->get("want"));
        // 1.3.0-dev.1: workspace sibling dependencies — plain member refs
        // (basename or relative path), validated by workspace::validate_ws_deps.
        cfg.depends.workspace =
            extract_string_array(deps->get("workspace"), "depends.workspace");
    }
}

static void parse_profiles(const toml::table& root, EzConfig& cfg) {
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
                    pc.flags = extract_string_array(prof_table->get("flags"), "profile.flags");
                    pc.msvc_flags = extract_string_array(prof_table->get("msvc_flags"), "profile.msvc_flags");

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
                    plc.flags = extract_string_array(prof_table->get("flags"), "profile.flags");
                    plc.msvc_flags = extract_string_array(prof_table->get("msvc_flags"), "profile.msvc_flags");
                }
                cfg.link_profiles[profile_name] = std::move(plc);
            }
        }
    }
}

static void parse_hooks(const toml::table& root, EzConfig& cfg) {
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
}

static void parse_install(const toml::table& root, EzConfig& cfg) {
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
    // 1.1.3 C2: 仅 `~/`（或 `~\`）与单独 `~` 展开；`"~abc"` 这类非 `~/` 前缀
    // 不再被误截断（原逻辑把 `"~abc"` 截成 `"c"`）。
    const std::string& prefix = cfg.install.prefix;
    if (prefix.size() > 1 && prefix[0] == '~' && (prefix[1] == '/' || prefix[1] == '\\')) {
        cfg.install.prefix = (util::get_home_dir() / prefix.substr(2)).string();
    } else if (prefix == "~") {
        cfg.install.prefix = util::get_home_dir().string();
    }
}

static void parse_test(const toml::table& root, EzConfig& cfg) {
    // 1.1.0-dev.6: [test] — test configuration
    if (auto test = root["test"].as_table()) {
        cfg.test.dirs = extract_string_array(test->get("dirs"), "test.dirs");
        if (auto fw = (*test)["framework"].value<std::string>()) {
            // Normalize framework name (case-insensitive, reuse normalize_lang)
            cfg.test.framework = normalize_lang(*fw);
        }
        cfg.test.flags = extract_string_array(test->get("flags"), "test.flags");
        // 1.2.0-dev.12: default_profile — default profile when no --profile given
        if (auto dp = (*test)["default_profile"].value<std::string>()) {
            cfg.test.default_profile = *dp;
        }
        // 1.2.0-dev.12: test-only include dirs (-I, resolved relative to project root)
        cfg.test.include_dirs = extract_string_array(test->get("include_dirs"), "test.include_dirs");
        // 1.2.0-dev.12: test-only link targets (-l system libraries)
        cfg.test.link_targets = extract_string_array(test->get("link_targets"), "test.link_targets");
    }
    // Apply defaults for test section
    if (cfg.test.dirs.empty()) cfg.test.dirs = {"test"};
}

static void parse_utils(const toml::table& root, EzConfig& cfg) {
    // [utils] (only relevant for type = "utils")
    if (auto utils = root["utils"].as_table()) {
        cfg.utils.tools = extract_string_array(utils->get("tools"), "utils.tools");

        // 0.2.5+: [utils.permissions] — fine-grained read/write/run control.
        // Presence of the sub-table switches on the deny/allow/ask model;
        // absence keeps the legacy unrestricted behavior.
        if (auto perms = (*utils)["permissions"].as_table()) {
            UtilsPermissions up;
            up.read       = extract_string_array(perms->get("read"), "permissions.read");
            up.read_deny  = extract_string_array(perms->get("read_deny"), "permissions.read_deny");
            up.write      = extract_string_array(perms->get("write"), "permissions.write");
            up.write_deny = extract_string_array(perms->get("write_deny"), "permissions.write_deny");
            up.run        = extract_string_array(perms->get("run"), "permissions.run");
            up.run_deny   = extract_string_array(perms->get("run_deny"), "permissions.run_deny");
            if (auto net = perms->get("network")) {
                if (net->is_boolean()) {
                    up.network = net->as_boolean()->get();
                }
            }
            cfg.utils.permissions = std::move(up);
        }
    }
}

// 1.4.0-dev.2: [pkg] — package-management configuration.
static void parse_pkg(const toml::table& root, EzConfig& cfg) {
    if (auto pkg = root["pkg"].as_table()) {
        if (auto ssc = (*pkg)["strict_std_check"].as_boolean()) {
            cfg.pkg.strict_std_check = ssc->get();
        }
    }
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

    // [project] + required version
    parse_project(root, cfg);
    if (cfg.project.version.empty()) {
        throw std::runtime_error(
            ezmk::i18n::get(ezmk::i18n::I18nKey::config_err_missing_ver));
    }

    // [compile] / [link] / [depends]
    parse_compile(root, cfg);
    parse_link(root, cfg);
    parse_depends(root, cfg);

    // [compile.profile.*] / [link.profile.*]
    parse_profiles(root, cfg);

    // [hooks]
    parse_hooks(root, cfg);

    // [install]
    parse_install(root, cfg);

    // [test]
    parse_test(root, cfg);

    // [utils]
    parse_utils(root, cfg);

    // [pkg] (1.4.0-dev.2)
    parse_pkg(root, cfg);

    // 1.1.0-dev.5: Resolve @link: references in path arrays
    {
        auto project_root = toml_path.parent_path();
        auto links = load_links_json(project_root);
        if (!links.empty()) {
            // Helper to resolve a single path entry
            auto resolve = [&](std::string& entry) {
                auto ref = util::parse_link_syntax(entry);
                if (ref.name.empty()) return; // not a @link: reference

                fs::path resolved = resolve_link_path(ref.name, ref.sub_path, links);
                entry = resolved.string();
            };

            // Resolve @link: in src_dirs
            for (auto& d : cfg.compile.src_dirs) {
                resolve(d);
            }
            // Resolve @link: in include_dirs
            for (auto& d : cfg.compile.include_dirs) {
                resolve(d);
            }
            // Resolve @link: in link_dirs
            for (auto& d : cfg.link.link_dirs) {
                resolve(d);
            }
        }
    }

    // 1.2.0-dev.11: validate default_profile references at parse time — a typo
    // like "relese" used to surface only deep in build/test with a confusing
    // error. A profile is valid if it exists in compile OR link profiles (the
    // shared apply_profile accepts both).
    {
        auto check_default = [&](const std::string& profile, const char* field) {
            if (profile.empty()) return;
            if (cfg.compile_profiles.find(profile) == cfg.compile_profiles.end() &&
                cfg.link_profiles.find(profile) == cfg.link_profiles.end()) {
                throw std::runtime_error(
                    i18n::fmt(i18n::I18nKey::config_err_unknown_profile,
                              {{"field", field}, {"profile", profile}}));
            }
        };
        check_default(cfg.compile.default_profile, "compile.default_profile");
        check_default(cfg.test.default_profile, "test.default_profile");
    }

    return cfg;
}

void write_default_config(const fs::path& toml_path, std::string_view project_name,
                          std::string_view project_type) {
    std::string content;
    // 1.1.2 C5: project name/type are user-controlled — escape for TOML so a
    // name containing `"` or a newline cannot produce invalid/injected config.
    content += "[project]\n";
    content += "name = ";
    content += util::toml_quote(project_name);
    content += "\n";
    content += "type = ";
    content += util::toml_quote(project_type);
    content += "\n";
    content += "version = \"0.1.0\"\n";
    content += "language = \"C++17\"\n";
    content += "\n";
    content += "[compile]\n";
    content += "flags = [\"-Wall\", \"-Wextra\"]\n";
    content += "default_profile = \"debug\"\n";
    content += "include_dirs = [\"include\"]\n";
    content += "\n";
    content += "[compile.profile.debug]\n";
    content += "flags = [\"-g\", \"-O0\"]\n";
    content += "msvc_flags = [\"/Zi\", \"/Od\"]\n";
    content += "\n";
    content += "[compile.profile.release]\n";
    content += "flags = [\"-O2\", \"-DNDEBUG\"]\n";
    content += "msvc_flags = [\"/O2\", \"/DNDEBUG\"]\n";
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

    // 1.2.1: commented-out [test] example — uncomment to enable `ezmk test`.
    // Pure TOML comments (zero parse impact); fields match TestConfig exactly,
    // including the 1.2.0-dev.12 additions (default_profile / include_dirs /
    // link_targets). Deliberately omits the deprecated [test].flags (removed
    // in 2.0.0) — the example only teaches the modern usage.
    content += "\n";
    content += "# [test]                     # 启用项目测试：取消注释后运行 `ezmk test`\n";
    content += "# framework = \"catch2\"       # \"catch2\" | \"ezmk\"（内置框架）\n";
    content += "# dirs = [\"test\"]\n";
    content += "# default_profile = \"debug\"  # 1.2.0-dev.12+：测试默认 profile\n";
    content += "# include_dirs = [\"test/helpers\"]   # 测试专属 -I（1.2.0-dev.12+）\n";
    content += "# link_targets = [\"pthread\"]        # 测试专属 -l（1.2.0-dev.12+）\n";

    if (!util::file_write(toml_path, content)) {
        throw std::runtime_error("failed to write config file: " + toml_path.string());
    }
}

} // namespace ezmk::config
