#pragma once

#include <map>
#include <string>
#include <string_view>

namespace ezmk::i18n {

// All user-visible string keys.
//
// The enum is generated from include/ezmk/i18n_keys.def (single source of
// truth, 0.2.6+) so it can never drift out of sync with key_name() in
// i18n.cpp. To add a key: add a line to i18n_keys.def + a string to both
// locale/*.json, then rebuild.
enum class I18nKey {
#define EZMK_I18N_KEY(name) name,
#include "ezmk/i18n_keys.def"
#undef EZMK_I18N_KEY
};

// Initialize i18n subsystem. Call once at startup (in main()).
// lang: "en", "zh", etc. Empty string → detect from EZMK_LANG env or system.
void init(std::string_view lang = "");

// Get a localized string by key (without formatting).
std::string get(I18nKey key);

// Get a localized string and replace named placeholders ({key} format).
// Extra args beyond placeholder count are ignored.
std::string fmt(I18nKey key, const std::map<std::string, std::string>& args = {});

// Shorthand: fmt with positional args {0}, {1}, {2}
std::string fmt(I18nKey key, std::string_view arg0);
std::string fmt(I18nKey key, std::string_view arg0, std::string_view arg1);
std::string fmt(I18nKey key, std::string_view arg0, std::string_view arg1, std::string_view arg2);

// Detect language from environment/system. Returns "en" on failure.
std::string detect_language();

// 1.3.0-dev.4: normalize a BCP-47-style locale tag to its canonical form —
//   "zh-CN" / "zh_CN" / "zh_CN.UTF-8" / "zh-cn" / "ZH-cn" → "zh-CN"
//   "zh" / "en" → unchanged
// Rules: strip any ".encoding" suffix; treat '_' and '-' as separators;
// lowercase the language segment (2-3 letters), uppercase the region segment
// (exactly 2 letters). Invalid input (empty / non-letter / >2 segments, e.g.
// script tags like "zh-Hant-TW" which are out of scope) → "" so callers fall
// back to their detection logic. The canonical form is used for file names,
// JSON keys and internal comparisons.
std::string normalize_locale_tag(std::string_view raw);

} // namespace ezmk::i18n
