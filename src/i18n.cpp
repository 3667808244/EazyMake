#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

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
        // overlay=false (default): replaces g_strings entirely (base language).
        // overlay=true: keeps existing entries and overrides only the keys the
        // file declares — used to layer a variant file over its base language
        // (inheritance: variant files only list their differences).
        // Returns true on success.
        bool parse_locale_json(const std::string &json_text,
                               const std::string &expected_lang,
                               bool overlay = false)
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

                // 1.3.0-dev.4: meta.language must match the tag being loaded —
                // a variant file must declare its own tag; a base file the base.
                if (root.contains("meta") && root["meta"].contains("language"))
                {
                    std::string declared = root["meta"].value("language", std::string(""));
                    if (!declared.empty() && declared != expected_lang)
                    {
                        util::warn(std::string("locale file for '") + expected_lang +
                                   "' declares meta.language '" + declared + "' — mismatch");
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

                if (!overlay)
                {
                    g_strings.clear();
                }
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
        // 1.3.0-dev.4: target is a canonical locale tag (e.g. "zh-TW").
        // Explicit lang args are normalized too ("zh_CN" → "zh-CN") so the
        // inheritance split below is always well-formed.
        std::string target_lang =
            lang.empty() ? detect_language()
                         : normalize_locale_tag(std::string(lang));

        // Split into base language + optional variant:
        //   "zh-TW" → base "zh", variant "zh-TW"
        // The variant overlays the base (inheritance); a missing variant file
        // is a pure fallback to the base language (never an error).
        std::string base = target_lang;
        std::string variant;
        auto dash = target_lang.find('-');
        if (dash != std::string::npos)
        {
            base = target_lang.substr(0, dash);
            variant = target_lang;
        }

        // 1. Base language: runtime locale file first (user override), then
        //    embedded data (unchanged priority).
        bool base_loaded = false;
        std::string file_json = load_runtime_locale_file(base);
        if (!file_json.empty() && parse_locale_json(file_json, base))
        {
            base_loaded = true;
        }
        if (!base_loaded)
        {
            auto it = get_embedded_locales().find(base);
            if (it != get_embedded_locales().end() && !it->second.empty() &&
                parse_locale_json(it->second, base))
            {
                base_loaded = true;
            }
        }

        // 2. Variant overlay: locale/<tag>.json overrides only the keys it
        //    declares; when absent (or unparseable) the base language stays.
        if (base_loaded && !variant.empty())
        {
            std::string v_json = load_runtime_locale_file(variant);
            if (v_json.empty())
            {
                auto it = get_embedded_locales().find(variant);
                if (it != get_embedded_locales().end() && !it->second.empty())
                {
                    v_json = it->second;
                }
            }
            if (!v_json.empty())
            {
                parse_locale_json(v_json, variant, /*overlay=*/true);
            }
        }

        // 3. Unknown base language → English fallback.
        if (!base_loaded)
        {
            load_en_fallback();
        }

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

        // Single-pass scan: walk the template once and replace each {name}
        // placeholder with the matching arg value. Replacement values are never
        // re-scanned, so an arg value that itself contains "{...}" cannot
        // trigger nested substitution.
        std::string out;
        out.reserve(tmpl.size());
        size_t pos = 0;
        while (pos < tmpl.size())
        {
            size_t open = tmpl.find('{', pos);
            if (open == std::string::npos)
            {
                out.append(tmpl, pos, std::string::npos);
                break;
            }
            out.append(tmpl, pos, open - pos);
            size_t close = tmpl.find('}', open + 1);
            if (close == std::string::npos)
            {
                // Unmatched '{' — keep the rest verbatim.
                out.append(tmpl, open, std::string::npos);
                break;
            }
            std::string name = tmpl.substr(open + 1, close - open - 1);
            auto it = args.find(name);
            if (it != args.end())
            {
                out += it->second;
            }
            else
            {
                // Unknown placeholder — keep it verbatim as a visible marker.
                out.append(tmpl, open, close - open + 1);
            }
            pos = close + 1;
        }

        return out;
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
        // 1.3.0-dev.4: true when a normalized tag OR its base language has
        // locale data (embedded or a runtime file). The guard is deliberately
        // permissive about variants: "zh-TW" is accepted when embedded/runtime
        // data exists for "zh-TW" itself OR for "zh" (the variant inherits the
        // base — init() layers it, and a missing variant file falls back).
        auto has_data = [](const std::string &tag) -> bool {
            if (tag.empty())
                return false;
            if (get_embedded_locales().count(tag) ||
                !load_runtime_locale_file(tag).empty())
            {
                return true;
            }
            auto dash = tag.find('-');
            if (dash == std::string::npos)
                return false;
            std::string base = tag.substr(0, dash);
            return get_embedded_locales().count(base) ||
                   !load_runtime_locale_file(base).empty();
        };

        // 1. Check EZMK_LANG environment variable (normalized tag).
        const char *env_lang = std::getenv("EZMK_LANG");
        if (env_lang && env_lang[0] != '\0')
        {
            std::string tag = normalize_locale_tag(env_lang);
            if (has_data(tag))
            {
                return tag;
            }
        }

        // 2. Platform-specific system language detection (also normalized).
#ifdef EZMK_WIN
        // Windows: GetUserDefaultLocaleName (already BCP 47, e.g. "zh-TW").
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
        if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0)
        {
            std::wstring wlang(localeName);
            std::string lang(wlang.begin(), wlang.end());
            std::string tag = normalize_locale_tag(lang);
            if (has_data(tag))
            {
                return tag;
            }
        }
#else
        // Linux/macOS: check $LANG, $LC_ALL (e.g. "zh_TW.UTF-8").
        for (const char *var : {"LANG", "LC_ALL"})
        {
            const char *val = std::getenv(var);
            if (val && val[0] != '\0')
            {
                std::string tag = normalize_locale_tag(val);
                if (has_data(tag))
                {
                    return tag;
                }
            }
        }
#endif

        // 3. Default to English
        return "en";
    }

    std::string normalize_locale_tag(std::string_view raw)
    {
        // Strip an encoding suffix: "zh_CN.UTF-8" → "zh_CN".
        auto dot = raw.find('.');
        if (dot != std::string_view::npos)
            raw = raw.substr(0, dot);

        // Split on '_' or '-' (both are valid BCP-47 separators).
        std::vector<std::string> parts;
        std::string cur;
        for (char c : raw)
        {
            if (c == '_' || c == '-')
            {
                if (!cur.empty())
                {
                    parts.push_back(std::move(cur));
                    cur.clear();
                }
            }
            else
            {
                cur.push_back(c);
            }
        }
        if (!cur.empty())
            parts.push_back(std::move(cur));

        auto is_alpha = [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        };

        // Language segment: 2-3 ASCII letters, lowercased.
        if (parts.empty() || parts[0].size() < 2 || parts[0].size() > 3)
            return {};
        for (char c : parts[0])
        {
            if (!is_alpha(c))
                return {};
        }
        std::string out;
        for (char c : parts[0])
        {
            out.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))));
        }

        // Optional region segment: exactly 2 letters, uppercased.
        if (parts.size() == 2)
        {
            if (parts[1].size() != 2)
                return {};
            for (char c : parts[1])
            {
                if (!is_alpha(c))
                    return {};
            }
            out += '-';
            for (char c : parts[1])
            {
                out.push_back(static_cast<char>(
                    std::toupper(static_cast<unsigned char>(c))));
            }
        }
        else if (parts.size() > 2)
        {
            // Script/extension tags ("zh-Hant-TW") are out of scope — invalid.
            return {};
        }

        return out;
    }

} // namespace ezmk::i18n
