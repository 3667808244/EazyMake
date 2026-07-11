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
#include "nlohmann_json.hpp"
using json = nlohmann::json;

namespace ezmk::i18n
{

    // Forward declaration — strong definition in locale_data.cpp.
    extern const std::map<std::string, std::string> embedded_locales;

    // 0.2.4+: Weak flag to detect if locale_data.cpp was linked (GCC 16+ compat).
    // The map itself is NOT weak to avoid double-destruction of std::map with
    // initializer_list under GCC 16+.
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((weak)) extern const bool g_has_embedded_locales;
#else
    extern const bool g_has_embedded_locales;
#endif

    const std::map<std::string, std::string>& get_embedded_locales() {
        if (&g_has_embedded_locales && g_has_embedded_locales) {
            return embedded_locales;
        }
        static const std::map<std::string, std::string> empty;
        return empty;
    }

    namespace
    {

        // ---- key_name mapping ----
        // Generated from include/ezmk/i18n_keys.def so it can never drift out
        // of sync with the I18nKey enum (single source of truth, 0.2.6+).
        const char *key_name(I18nKey key)
        {
            switch (key)
            {
#define EZMK_I18N_KEY(name) case I18nKey::name: return #name;
#include "ezmk/i18n_keys.def"
#undef EZMK_I18N_KEY
            }
            return "???"; // unreachable — all enum values are generated above
        }

        // ---- global state ----
        std::string g_current_lang = "en";
        std::map<std::string, std::string> g_strings; // key → translated string

        // ---- helpers ----

        // Get the directory where the ezmk executable lives (for runtime locale loading).
        std::string exe_parent_dir()
        {
            return util::get_exe_dir().string();
        }

        // Try to load a locale JSON file from the runtime filesystem.
        // Looks in: ../locale/ relative to exe dir (for installed layout).
        std::string load_runtime_locale_file(const std::string &lang)
        {
            namespace fs = std::filesystem;
            fs::path exeDir = util::get_exe_dir();
            // Installed layout: <exe_dir>/../locale/<lang>.json
            fs::path candidate = exeDir / ".." / "locale" / (lang + ".json");
            std::error_code ec;
            if (fs::exists(candidate, ec))
            {
                return util::file_read(candidate);
            }
            // Also try: <exe_dir>/locale/<lang>.json (same-dir layout)
            candidate = exeDir / "locale" / (lang + ".json");
            if (fs::exists(candidate, ec))
            {
                return util::file_read(candidate);
            }
            return {};
        }

        // Parse a locale JSON string and populate g_strings.
        // Returns true on success.
        bool parse_locale_json(const std::string &json_text, const std::string &expected_lang)
        {
            try
            {
                auto root = json::parse(json_text);

                // Check meta version for compatibility (accepts both "1" and 1)
                if (root.contains("meta") && root["meta"].contains("version"))
                {
                    int ver = 0;
                    auto &v = root["meta"]["version"];
                    if (v.is_number_integer())
                    {
                        ver = v.get<int>();
                    }
                    else if (v.is_string())
                    {
                        try
                        {
                            ver = std::stoi(v.get<std::string>());
                        }
                        catch (...)
                        {
                            ver = -1;
                        }
                    }
                    if (ver != 1)
                    {
                        util::warn(std::string("locale version mismatch for '") +
                                   expected_lang + "' (expected 1, got " +
                                   std::to_string(ver) + "). Falling back to English.");
                        return false;
                    }
                }

                // Load strings
                if (!root.contains("strings") || !root["strings"].is_object())
                {
                    util::warn(std::string("locale file for '") + expected_lang +
                               "' has no 'strings' object");
                    return false;
                }

                g_strings.clear();
                for (auto &[key, val] : root["strings"].items())
                {
                    g_strings[key] = val.get<std::string>();
                }
                g_current_lang = expected_lang;
                return true;
            }
            catch (const json::exception &e)
            {
                util::warn(std::string("failed to parse locale '") + expected_lang +
                           "': " + e.what());
                return false;
            }
        }

        // Load English fallback from embedded data or hardcoded minimal set.
        void load_en_fallback()
        {
            // Try embedded first
            auto it = get_embedded_locales().find("en");
            if (it != get_embedded_locales().end() && !it->second.empty())
            {
                if (parse_locale_json(it->second, "en"))
                    return;
            }
            // Runtime file as second fallback
            std::string file_json = load_runtime_locale_file("en");
            if (!file_json.empty())
            {
                if (parse_locale_json(file_json, "en"))
                    return;
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
                {"utils_not_found", "unknown utils command: '{name}'\n  Install a utils package with 'ezmk pkg install'."},
                {"lua_init_failed", "failed to initialize Lua runtime"},
                {"lua_error", "Lua error: {msg}"},
                {"lua_api_type_error", "bad argument #{n} to '{func}' ({expected}, got {got})"},
                {"lua_api_arg_count", "bad argument count to '{func}' (expected {expected}, got {got})"},
            };
            g_current_lang = "en";
        }

        // Development-time aid (0.2.6+): warn once if any I18nKey has no entry
        // in the loaded locale (i.e. the JSON is missing a translation). This
        // is distinct from an enum/key_name mismatch, which the X-macro single
        // source of truth (i18n_keys.def) makes impossible. Silent in release
        // builds to avoid noise for end users.
        void audit_missing_keys()
        {
#ifndef NDEBUG
            static const char *const all_keys[] = {
#define EZMK_I18N_KEY(name) #name,
#include "ezmk/i18n_keys.def"
#undef EZMK_I18N_KEY
            };
            for (const char *k : all_keys)
            {
                if (g_strings.find(k) == g_strings.end())
                {
                    util::warn(std::string("i18n: locale '") + g_current_lang +
                               "' is missing a translation for key '" + k + "'");
                }
            }
#endif
        }

    } // anonymous namespace

    // ---- public API ----

    void init(std::string_view lang)
    {
        std::string target_lang;
        if (lang.empty())
        {
            target_lang = detect_language();
        }
        else
        {
            target_lang = std::string(lang);
        }

        // 1. Try runtime locale file first (allows user override)
        std::string file_json = load_runtime_locale_file(target_lang);
        if (!file_json.empty() && parse_locale_json(file_json, target_lang))
        {
            audit_missing_keys();
            return;
        }

        // 2. Try embedded data
        auto it = get_embedded_locales().find(target_lang);
        if (it != get_embedded_locales().end() && !it->second.empty() &&
            parse_locale_json(it->second, target_lang))
        {
            audit_missing_keys();
            return;
        }

        // 3. Fallback to English
        load_en_fallback();
        audit_missing_keys();
    }

    std::string get(I18nKey key)
    {
        const char *name = key_name(key);
        auto it = g_strings.find(name);
        if (it != g_strings.end())
            return it->second;
        // Missing key — return the key name itself as a visible marker
        return std::string("{") + name + "}";
    }

    std::string fmt(I18nKey key, const std::map<std::string, std::string> &args)
    {
        std::string tmpl = get(key);

        // Replace {key} placeholders with args values.
        // Use simple find/replace to avoid depending on a formatting library.
        for (auto &[arg_name, arg_val] : args)
        {
            std::string placeholder = "{" + arg_name + "}";
            size_t pos = 0;
            while ((pos = tmpl.find(placeholder, pos)) != std::string::npos)
            {
                tmpl.replace(pos, placeholder.size(), arg_val);
                pos += arg_val.size();
            }
        }

        return tmpl;
    }

    std::string fmt(I18nKey key, std::string_view arg0)
    {
        return fmt(key, {{"0", std::string(arg0)}});
    }

    std::string fmt(I18nKey key, std::string_view arg0, std::string_view arg1)
    {
        return fmt(key, {{"0", std::string(arg0)}, {"1", std::string(arg1)}});
    }

    std::string fmt(I18nKey key, std::string_view arg0, std::string_view arg1, std::string_view arg2)
    {
        return fmt(key, {{"0", std::string(arg0)}, {"1", std::string(arg1)}, {"2", std::string(arg2)}});
    }

    std::string detect_language()
    {
        // 1. Check EZMK_LANG environment variable
        const char *env_lang = std::getenv("EZMK_LANG");
        if (env_lang && env_lang[0] != '\0')
        {
            std::string lang(env_lang);
            // Normalize: "zh-CN" → "zh", "en-US" → "en", etc.
            auto dash = lang.find('-');
            if (dash != std::string::npos)
                lang = lang.substr(0, dash);
            auto dot = lang.find('.');
            if (dot != std::string::npos)
                lang = lang.substr(0, dot);
            // Only return if we actually have data for this language
            if (get_embedded_locales().count(lang) || !load_runtime_locale_file(lang).empty())
            {
                return lang;
            }
        }

        // 2. Platform-specific system language detection
#ifdef EZMK_WIN
        // Windows: GetUserDefaultLocaleName
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
        if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0)
        {
            std::wstring wlang(localeName);
            std::string lang(wlang.begin(), wlang.end());
            // e.g. "zh-CN" → "zh"
            auto dash = lang.find('-');
            if (dash != std::string::npos)
                lang = lang.substr(0, dash);
            if (get_embedded_locales().count(lang) || !load_runtime_locale_file(lang).empty())
            {
                return lang;
            }
        }
#else
        // Linux/macOS: check $LANG, $LC_ALL
        for (const char *var : {"LANG", "LC_ALL"})
        {
            const char *val = std::getenv(var);
            if (val && val[0] != '\0')
            {
                std::string lang(val);
                auto dot = lang.find('.');
                if (dot != std::string::npos)
                    lang = lang.substr(0, dot);
                auto underscore = lang.find('_');
                if (underscore != std::string::npos)
                    lang = lang.substr(0, underscore);
                auto dash = lang.find('-');
                if (dash != std::string::npos)
                    lang = lang.substr(0, dash);
                if (get_embedded_locales().count(lang) || !load_runtime_locale_file(lang).empty())
                {
                    return lang;
                }
            }
        }
#endif

        // 3. Default to English
        return "en";
    }

} // namespace ezmk::i18n
