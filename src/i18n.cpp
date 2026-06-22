#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef EZMK_WIN
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

// nlohmann/json — single-header, already vendored
#include "nlohmann/json.hpp"
using json = nlohmann::json;

namespace ezmk::i18n {

// Forward declaration — definition in this file (weak) or locale_data.cpp (strong).
extern const std::map<std::string, std::string> embedded_locales;

namespace {

// ---- key_name mapping ----
const char* key_name(I18nKey key) {
    switch (key) {
    // build
    case I18nKey::archive_failed:            return "archive_failed";
    case I18nKey::build_failed:              return "build_failed";
    case I18nKey::build_success:             return "build_success";
    case I18nKey::building:                  return "building";
    case I18nKey::compiling:                 return "compiling";
    case I18nKey::compilation_failed:        return "compilation_failed";
    case I18nKey::compile_options_changed:   return "compile_options_changed";
    case I18nKey::compiler_not_found:        return "compiler_not_found";
    case I18nKey::linking:                   return "linking";
    case I18nKey::link_failed:               return "link_failed";
    case I18nKey::archiving:                 return "archiving";
    case I18nKey::main_missing:              return "main_missing";
    case I18nKey::no_source_files:           return "no_source_files";
    case I18nKey::src_dir_missing:            return "src_dir_missing";
    case I18nKey::cache_summary:             return "cache_summary";

    // cache
    case I18nKey::cache_hit:                 return "cache_hit";
    case I18nKey::cache_hit_brief:           return "cache_hit_brief";
    case I18nKey::cache_miss:                return "cache_miss";
    case I18nKey::cache_miss_record:         return "cache_miss_record";
    case I18nKey::cache_miss_source:         return "cache_miss_source";
    case I18nKey::cache_miss_options:        return "cache_miss_options";
    case I18nKey::cache_miss_header:         return "cache_miss_header";
    case I18nKey::source_changed:            return "source_changed";
    case I18nKey::options_changed:           return "options_changed";
    case I18nKey::header_changed:            return "header_changed";
    case I18nKey::include_structure_changed: return "include_structure_changed";

    // pkg
    case I18nKey::auto_yes:                  return "auto_yes";
    case I18nKey::circular_dep:              return "circular_dep";
    case I18nKey::compiling_pkg:             return "compiling_pkg";
    case I18nKey::confirm_continue:          return "confirm_continue";
    case I18nKey::confirm_script:            return "confirm_script";
    case I18nKey::downloading:               return "downloading";
    case I18nKey::extracting:                return "extracting";
    case I18nKey::exec_question:             return "exec_question";
    case I18nKey::found_in_repo:             return "found_in_repo";
    case I18nKey::found_script:              return "found_script";
    case I18nKey::global_confirm:            return "global_confirm";
    case I18nKey::install_cancelled:         return "install_cancelled";
    case I18nKey::install_cancelled_user:    return "install_cancelled_user";
    case I18nKey::installed:                 return "installed";
    case I18nKey::installing:                return "installing";
    case I18nKey::installing_to:             return "installing_to";
    case I18nKey::missing_dep:               return "missing_dep";
    case I18nKey::not_found:                 return "not_found";
    case I18nKey::overwrite_confirm:         return "overwrite_confirm";
    case I18nKey::removed:                   return "removed";
    case I18nKey::removing:                  return "removing";
    case I18nKey::resolving_deps:            return "resolving_deps";
    case I18nKey::running_script:            return "running_script";
    case I18nKey::script_completed:          return "script_completed";
    case I18nKey::script_failed:             return "script_failed";
    case I18nKey::searching:                 return "searching";
    case I18nKey::searching_repos:           return "searching_repos";
    case I18nKey::sha256_mismatch:           return "sha256_mismatch";
    case I18nKey::sha256_ok:                 return "sha256_ok";
    case I18nKey::skipping:                  return "skipping";
    case I18nKey::verifying:                 return "verifying";

    // repo
    case I18nKey::cloning:                   return "cloning";
    case I18nKey::no_repos:                  return "no_repos";
    case I18nKey::pulling:                   return "pulling";
    case I18nKey::re_cloning:                return "re_cloning";
    case I18nKey::re_reading:                return "re_reading";
    case I18nKey::repo_added:                return "repo_added";
    case I18nKey::repo_not_found:            return "repo_not_found";
    case I18nKey::repo_removed:              return "repo_removed";
    case I18nKey::repo_updated:              return "repo_updated";
    case I18nKey::removing_cache:            return "removing_cache";

    // project
    case I18nKey::creating_project:          return "creating_project";
    case I18nKey::project_created:           return "project_created";
    case I18nKey::init_git:                  return "init_git";
    case I18nKey::git_initialized:           return "git_initialized";
    case I18nKey::git_not_found:             return "git_not_found";
    case I18nKey::git_init_failed:           return "git_init_failed";

    // run & clean
    case I18nKey::cleaned:                   return "cleaned";
    case I18nKey::running:                   return "running";
    case I18nKey::clean_stale:                return "clean_stale";

    // editor
    case I18nKey::no_editor:                 return "no_editor";
    case I18nKey::opening_editor:            return "opening_editor";
    case I18nKey::editor_error:              return "editor_error";

    // version
    case I18nKey::version_output:            return "version_output";

    // utils
    case I18nKey::utils_placeholder:         return "utils_placeholder";

    // general
    case I18nKey::fatal_prefix:              return "fatal_prefix";
    case I18nKey::error_prefix:              return "error_prefix";
    case I18nKey::warn_prefix:               return "warn_prefix";
    case I18nKey::info_prefix:               return "info_prefix";
    }
    return "???"; // unreachable — all enum values covered
}

// ---- global state ----
std::string g_current_lang = "en";
std::map<std::string, std::string> g_strings; // key → translated string

// ---- helpers ----

// Get the directory where the ezmk executable lives (for runtime locale loading).
std::string exe_parent_dir() {
    return util::get_exe_dir().string();
}

// Try to load a locale JSON file from the runtime filesystem.
// Looks in: ../locale/ relative to exe dir (for installed layout).
std::string load_runtime_locale_file(const std::string& lang) {
    namespace fs = std::filesystem;
    fs::path exeDir = util::get_exe_dir();
    // Installed layout: <exe_dir>/../locale/<lang>.json
    fs::path candidate = exeDir / ".." / "locale" / (lang + ".json");
    std::error_code ec;
    if (fs::exists(candidate, ec)) {
        return util::file_read(candidate);
    }
    // Also try: <exe_dir>/locale/<lang>.json (same-dir layout)
    candidate = exeDir / "locale" / (lang + ".json");
    if (fs::exists(candidate, ec)) {
        return util::file_read(candidate);
    }
    return {};
}

// Parse a locale JSON string and populate g_strings.
// Returns true on success.
bool parse_locale_json(const std::string& json_text, const std::string& expected_lang) {
    try {
        auto root = json::parse(json_text);

        // Check meta version for compatibility (accepts both "1" and 1)
        if (root.contains("meta") && root["meta"].contains("version")) {
            int ver = 0;
            auto& v = root["meta"]["version"];
            if (v.is_number_integer()) {
                ver = v.get<int>();
            } else if (v.is_string()) {
                try { ver = std::stoi(v.get<std::string>()); }
                catch (...) { ver = -1; }
            }
            if (ver != 1) {
                util::warn(std::string("locale version mismatch for '") +
                           expected_lang + "' (expected 1, got " +
                           std::to_string(ver) + "). Falling back to English.");
                return false;
            }
        }

        // Load strings
        if (!root.contains("strings") || !root["strings"].is_object()) {
            util::warn(std::string("locale file for '") + expected_lang +
                       "' has no 'strings' object");
            return false;
        }

        g_strings.clear();
        for (auto& [key, val] : root["strings"].items()) {
            g_strings[key] = val.get<std::string>();
        }
        g_current_lang = expected_lang;
        return true;

    } catch (const json::exception& e) {
        util::warn(std::string("failed to parse locale '") + expected_lang +
                   "': " + e.what());
        return false;
    }
}

// Load English fallback from embedded data or hardcoded minimal set.
void load_en_fallback() {
    // Try embedded first
    auto it = embedded_locales.find("en");
    if (it != embedded_locales.end() && !it->second.empty()) {
        if (parse_locale_json(it->second, "en")) return;
    }
    // Runtime file as second fallback
    std::string file_json = load_runtime_locale_file("en");
    if (!file_json.empty()) {
        if (parse_locale_json(file_json, "en")) return;
    }
    // Absolute last resort: hardcoded English strings for critical keys
    // so the tool never crashes due to missing i18n.
    g_strings = {
        {"fatal_prefix", "fatal: "},
        {"error_prefix", "error: "},
        {"warn_prefix", "warning: "},
        {"info_prefix", "[ezmk] "},
        {"build_failed", "build failed"},
        {"compiler_not_found", "compiler not found: {compiler}"},
        {"building", "Building {name} ({type}, {lang})..."},
        {"build_success", "Build successful: {path}"},
        {"version_output", "EazyMake {version}"},
    };
    g_current_lang = "en";
}

} // anonymous namespace

// ---- embedded locale data ----
// Weak definition: when locale_data.cpp is linked, its strong definition wins.
// When it's not (development without running embed_locale.py), this empty map
// is used as fallback.  The 'extern' keyword ensures external linkage even with
// the initializer (required for __attribute__((weak)) to work on GCC).
#if defined(__GNUC__) || defined(__clang__)
extern const std::map<std::string, std::string> embedded_locales __attribute__((weak)) = {};
#else
extern const std::map<std::string, std::string> embedded_locales = {};
#endif

// ---- public API ----

void init(std::string_view lang) {
    std::string target_lang;
    if (lang.empty()) {
        target_lang = detect_language();
    } else {
        target_lang = std::string(lang);
    }

    // 1. Try runtime locale file first (allows user override)
    std::string file_json = load_runtime_locale_file(target_lang);
    if (!file_json.empty()) {
        if (parse_locale_json(file_json, target_lang)) return;
    }

    // 2. Try embedded data
    auto it = embedded_locales.find(target_lang);
    if (it != embedded_locales.end() && !it->second.empty()) {
        if (parse_locale_json(it->second, target_lang)) return;
    }

    // 3. Fallback to English
    load_en_fallback();
}

std::string get(I18nKey key) {
    const char* name = key_name(key);
    auto it = g_strings.find(name);
    if (it != g_strings.end()) return it->second;
    // Missing key — return the key name itself as a visible marker
    return std::string("{") + name + "}";
}

std::string fmt(I18nKey key, const std::map<std::string, std::string>& args) {
    std::string tmpl = get(key);

    // Replace {key} placeholders with args values.
    // Use simple find/replace to avoid depending on a formatting library.
    for (auto& [arg_name, arg_val] : args) {
        std::string placeholder = "{" + arg_name + "}";
        size_t pos = 0;
        while ((pos = tmpl.find(placeholder, pos)) != std::string::npos) {
            tmpl.replace(pos, placeholder.size(), arg_val);
            pos += arg_val.size();
        }
    }

    return tmpl;
}

std::string fmt(I18nKey key, std::string_view arg0) {
    return fmt(key, {{"0", std::string(arg0)}});
}

std::string fmt(I18nKey key, std::string_view arg0, std::string_view arg1) {
    return fmt(key, {{"0", std::string(arg0)}, {"1", std::string(arg1)}});
}

std::string fmt(I18nKey key, std::string_view arg0, std::string_view arg1, std::string_view arg2) {
    return fmt(key, {{"0", std::string(arg0)}, {"1", std::string(arg1)}, {"2", std::string(arg2)}});
}

std::string detect_language() {
    // 1. Check EZMK_LANG environment variable
    const char* env_lang = std::getenv("EZMK_LANG");
    if (env_lang && env_lang[0] != '\0') {
        std::string lang(env_lang);
        // Normalize: "zh-CN" → "zh", "en-US" → "en", etc.
        auto dash = lang.find('-');
        if (dash != std::string::npos) lang = lang.substr(0, dash);
        auto dot = lang.find('.');
        if (dot != std::string::npos) lang = lang.substr(0, dot);
        // Only return if we actually have data for this language
        if (embedded_locales.count(lang) || !load_runtime_locale_file(lang).empty()) {
            return lang;
        }
    }

    // 2. Platform-specific system language detection
#ifdef EZMK_WIN
    // Windows: GetUserDefaultLocaleName
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0) {
        std::wstring wlang(localeName);
        std::string lang(wlang.begin(), wlang.end());
        // e.g. "zh-CN" → "zh"
        auto dash = lang.find('-');
        if (dash != std::string::npos) lang = lang.substr(0, dash);
        if (embedded_locales.count(lang) || !load_runtime_locale_file(lang).empty()) {
            return lang;
        }
    }
#else
    // Linux/macOS: check $LANG, $LC_ALL
    for (const char* var : {"LANG", "LC_ALL"}) {
        const char* val = std::getenv(var);
        if (val && val[0] != '\0') {
            std::string lang(val);
            auto dot = lang.find('.');
            if (dot != std::string::npos) lang = lang.substr(0, dot);
            auto underscore = lang.find('_');
            if (underscore != std::string::npos) lang = lang.substr(0, underscore);
            auto dash = lang.find('-');
            if (dash != std::string::npos) lang = lang.substr(0, dash);
            if (embedded_locales.count(lang) || !load_runtime_locale_file(lang).empty()) {
                return lang;
            }
        }
    }
#endif

    // 3. Default to English
    return "en";
}

} // namespace ezmk::i18n
