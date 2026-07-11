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

} // namespace ezmk::i18n
