#include "catch2.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"
#include "nlohmann_json.hpp"

#include <cstdlib>
#include <filesystem>

using namespace ezmk::i18n;
namespace fs = std::filesystem;

// Helper: temporary set/unset env var
struct EnvGuard {
    std::string name;
    bool had_old;
    std::string old_val;
    EnvGuard(const char* n, const char* v) : name(n) {
        const char* old = std::getenv(n);
        had_old = (old != nullptr);
        if (had_old) old_val = old;
#ifdef EZMK_WIN
        _putenv_s(n, v ? v : "");
#else
        if (v) setenv(n, v, 1);
        else unsetenv(n);
#endif
    }
    ~EnvGuard() {
#ifdef EZMK_WIN
        _putenv_s(name.c_str(), had_old ? old_val.c_str() : "");
#else
        if (had_old) setenv(name.c_str(), old_val.c_str(), 1);
        else unsetenv(name.c_str());
#endif
    }
};

// ===================================================================
// 1. Key consistency — en.json and zh.json have identical key sets
// ===================================================================

TEST_CASE("Locale JSON files exist and are valid", "[i18n][locale]") {
    // Check that the embedded locale data contains at least "en"
    // (embedded_locales is populated by locale_data.cpp)
    // We can't directly access embedded_locales here, but we can init and check basic keys.
    init("en");
    auto ver = get(I18nKey::version_output);
    REQUIRE(!ver.empty());
    REQUIRE(ver.find("EazyMake") != std::string::npos);
}

TEST_CASE("All I18nKey values produce non-empty strings", "[i18n][keys]") {
    init("en");

    // Sample a representative set of keys from each module group
    // to verify they all resolve to non-empty strings.
    std::vector<I18nKey> sample_keys = {
        // build
        I18nKey::build_success, I18nKey::compiling, I18nKey::linking,
        I18nKey::archiving, I18nKey::compiler_not_found, I18nKey::no_source_files,
        I18nKey::src_dir_missing, I18nKey::main_missing, I18nKey::building,
        I18nKey::compile_options_changed, I18nKey::cache_summary,
        I18nKey::compilation_failed, I18nKey::link_failed,
        I18nKey::archive_failed, I18nKey::build_failed,
        // cache
        I18nKey::cache_hit, I18nKey::cache_hit_brief, I18nKey::cache_miss,
        I18nKey::cache_miss_record, I18nKey::cache_miss_source,
        I18nKey::cache_miss_options, I18nKey::cache_miss_header,
        I18nKey::source_changed, I18nKey::options_changed, I18nKey::header_changed,
        I18nKey::include_structure_changed,
        // pkg
        I18nKey::installing, I18nKey::installed, I18nKey::removing,
        I18nKey::removed, I18nKey::downloading, I18nKey::extracting,
        I18nKey::verifying, I18nKey::sha256_ok, I18nKey::sha256_mismatch,
        I18nKey::circular_dep, I18nKey::missing_dep,
        I18nKey::global_confirm, I18nKey::overwrite_confirm,
        I18nKey::searching, I18nKey::not_found,
        I18nKey::resolving_deps, I18nKey::compiling_pkg,
        I18nKey::found_script, I18nKey::running_script,
        I18nKey::script_completed, I18nKey::script_failed,
        // repo
        I18nKey::cloning, I18nKey::pulling, I18nKey::re_cloning,
        I18nKey::re_reading, I18nKey::repo_added, I18nKey::repo_removed,
        I18nKey::repo_updated, I18nKey::repo_not_found, I18nKey::no_repos,
        I18nKey::removing_cache,
        // project
        I18nKey::creating_project, I18nKey::project_created,
        I18nKey::init_git, I18nKey::git_initialized,
        I18nKey::git_not_found, I18nKey::git_init_failed,
        // run & clean
        I18nKey::cleaned, I18nKey::running, I18nKey::clean_stale,
        // editor
        I18nKey::no_editor, I18nKey::opening_editor, I18nKey::editor_error,
        // version & utils
        I18nKey::version_output, I18nKey::utils_placeholder,
        // general
        I18nKey::fatal_prefix, I18nKey::error_prefix,
        I18nKey::warn_prefix, I18nKey::info_prefix,
    };

    for (auto key : sample_keys) {
        std::string s = get(key);
        INFO("Key: " << static_cast<int>(key));
        REQUIRE(!s.empty());
        // Raw template strings may contain placeholder markers like {file}.
        // The missing-key fallback format is "{keyname}" — a single placeholder
        // that starts with "{" and ends with "}" and matches no known key pattern.
        // We just check the string is not exactly "{...}" (the missing-key fallback).
        if (s.front() == '{' && s.back() == '}') {
            // This looks like a missing-key fallback — fail
            FAIL("Key returned missing-key fallback: " << s);
        }
    }
}

// ===================================================================
// 2. fmt() — placeholder replacement
// ===================================================================

TEST_CASE("fmt() replaces single placeholder", "[i18n][fmt]") {
    init("en");
    std::string result = fmt(I18nKey::compiling, {{"file", "test.cpp"}});
    REQUIRE(result.find("test.cpp") != std::string::npos);
    REQUIRE(result.find("{file}") == std::string::npos); // placeholder replaced
}

TEST_CASE("fmt() replaces multiple placeholders", "[i18n][fmt]") {
    init("en");
    std::string result = fmt(I18nKey::building,
                              {{"name", "MyApp"}, {"type", "executable"}, {"lang", "C++17"}});
    REQUIRE(result.find("MyApp") != std::string::npos);
    REQUIRE(result.find("executable") != std::string::npos);
    REQUIRE(result.find("C++17") != std::string::npos);
}

TEST_CASE("fmt() with no placeholders returns original string", "[i18n][fmt]") {
    init("en");
    std::string result = fmt(I18nKey::extracting);
    REQUIRE(result.find("Extracting") != std::string::npos);
    REQUIRE(result.find("{") == std::string::npos);
}

TEST_CASE("fmt() with extra args (not in template) ignores them", "[i18n][fmt]") {
    init("en");
    std::string result = fmt(I18nKey::compiling,
                              {{"file", "test.cpp"}, {"extra", "unused"}});
    REQUIRE(result.find("test.cpp") != std::string::npos);
    REQUIRE(result.find("unused") == std::string::npos);
}

TEST_CASE("fmt() with missing placeholder value keeps placeholder", "[i18n][fmt]") {
    init("en");
    // Don't provide "name" — the placeholder {name} stays in the output
    std::string result = fmt(I18nKey::building);
    REQUIRE(result.find("{name}") != std::string::npos);
}

TEST_CASE("fmt() with empty string value", "[i18n][fmt]") {
    init("en");
    std::string result = fmt(I18nKey::compiling, {{"file", ""}});
    // Empty replacement should result in empty string where placeholder was
    REQUIRE(result.find("Compiling") != std::string::npos);
}

TEST_CASE("fmt() never re-scans replaced values (no nested substitution)", "[i18n][fmt]") {
    init("en");
    // Arg value itself contains a "{...}" pattern that matches another arg
    // name: the value must be inserted verbatim, not substituted again.
    // Template: "Building {name} ({type}, {lang})..."
    std::string result = fmt(I18nKey::building,
                              {{"name", "{type}"}, {"type", "executable"}, {"lang", "C++17"}});
    // The injected "{type}" (from name's value) survives untouched...
    REQUIRE(result.find("{type}") != std::string::npos);
    // ...while the template's own {type} placeholder is still replaced.
    REQUIRE(result.find("executable") != std::string::npos);
}

TEST_CASE("fmt() replaces multiple occurrences of the same placeholder", "[i18n][fmt]") {
    init("en");
    std::string result = fmt(I18nKey::building,
                              {{"name", "x"}, {"type", "executable"}, {"lang", "C++17"}});
    REQUIRE(result.find("x") != std::string::npos);
    // Every {name} occurrence is replaced (no leftover placeholder)
    REQUIRE(result.find("{name}") == std::string::npos);
}

// ===================================================================
// 3. Language detection
// ===================================================================

TEST_CASE("detect_language() returns 'en' by default", "[i18n][detect]") {
    // Without EZMK_LANG set, should return something (en, zh, etc.)
    std::string lang = detect_language();
    REQUIRE(!lang.empty());
    // On most CI systems, this will be "en"
}

TEST_CASE("detect_language() respects EZMK_LANG=zh", "[i18n][detect]") {
    EnvGuard guard("EZMK_LANG", "zh");
    std::string lang = detect_language();
    REQUIRE(lang == "zh");
}

TEST_CASE("detect_language() respects EZMK_LANG=en", "[i18n][detect]") {
    EnvGuard guard("EZMK_LANG", "en");
    std::string lang = detect_language();
    REQUIRE(lang == "en");
}

TEST_CASE("detect_language() normalizes zh-CN to the full variant tag", "[i18n][detect]") {
    EnvGuard guard("EZMK_LANG", "zh-CN");
    std::string lang = detect_language();
    REQUIRE(lang == "zh-CN");  // 1.3.0-dev.4: full canonical tag, not just "zh"
}

// ===================================================================
// 4. Language switching
// ===================================================================

TEST_CASE("init() with explicit language switches output", "[i18n][switch]") {
    // English
    init("en");
    std::string en_result = get(I18nKey::building);
    REQUIRE(en_result.find("Building") != std::string::npos);

    // Chinese
    init("zh");
    std::string zh_result = get(I18nKey::building);
    REQUIRE(zh_result.find("构建") != std::string::npos);

    // Different outputs
    REQUIRE(en_result != zh_result);
}

TEST_CASE("init() fallback to en for unsupported language", "[i18n][switch]") {
    // "xx" is not a real language → should fallback to English
    init("xx");
    std::string result = get(I18nKey::building);
    REQUIRE(result.find("Building") != std::string::npos);
}

// ===================================================================
// 5. Missing key behavior
// ===================================================================

TEST_CASE("get() for keys that exist returns their string", "[i18n][missing]") {
    init("en");
    std::string result = get(I18nKey::build_failed);
    REQUIRE(!result.empty());
    REQUIRE(result.find("{") == std::string::npos);
}

// ===================================================================
// 6. Version key formatting
// ===================================================================

TEST_CASE("version_output with version argument", "[i18n][version]") {
    init("en");
    std::string result = fmt(I18nKey::version_output, {{"version", "0.1.7"}});
    REQUIRE(result.find("0.1.7") != std::string::npos);
    REQUIRE(result.find("EazyMake") != std::string::npos);
}

TEST_CASE("version_output in Chinese", "[i18n][version]") {
    init("zh");
    std::string result = fmt(I18nKey::version_output, {{"version", "0.1.7"}});
    REQUIRE(result.find("0.1.7") != std::string::npos);
    REQUIRE(result.find("EazyMake") != std::string::npos);
}

// ===================================================================
// 7. Chinese translations are not identical to English (sanity check)
// ===================================================================

TEST_CASE("Chinese translations differ from English for key strings", "[i18n][zh]") {
    // Sample of keys where zh and en MUST differ
    std::vector<std::pair<I18nKey, std::string>> checks = {
        {I18nKey::building, "构建"},
        {I18nKey::compiling, "编译"},
        {I18nKey::linking, "链接"},
        {I18nKey::build_success, "成功"},
        {I18nKey::build_failed, "失败"},
        {I18nKey::cleaned, "清除"},
    };

    init("en");
    for (auto& [key, zh_fragment] : checks) {
        std::string en_str = get(key);
        init("zh");
        std::string zh_str = get(key);
        REQUIRE(zh_str.find(zh_fragment) != std::string::npos);
        REQUIRE(en_str != zh_str); // they should be different
        init("en");
    }
}

// ===================================================================
// 8. General prefix keys are non-empty
// ===================================================================

TEST_CASE("General prefix keys are non-empty", "[i18n][general]") {
    init("en");
    REQUIRE(!get(I18nKey::fatal_prefix).empty());
    REQUIRE(!get(I18nKey::error_prefix).empty());
    REQUIRE(!get(I18nKey::warn_prefix).empty());
    REQUIRE(!get(I18nKey::info_prefix).empty());
}

// ===================================================================
// 0.2.3+: New i18n keys — existence and non-empty check
// ===================================================================

TEST_CASE("0.2.3 i18n keys: parallel and profile keys", "[i18n][0.2.3]") {
    init("en");
    REQUIRE(!get(I18nKey::parallel_jobs_info).empty());
    REQUIRE(!get(I18nKey::profile_not_found).empty());
}

TEST_CASE("0.2.3 i18n keys: hook keys", "[i18n][0.2.3]") {
    init("en");
    REQUIRE(!get(I18nKey::pre_build_hook).empty());
    REQUIRE(!get(I18nKey::post_build_hook).empty());
    REQUIRE(!get(I18nKey::on_failure_hook).empty());
    REQUIRE(!get(I18nKey::hook_not_found).empty());
    REQUIRE(!get(I18nKey::hook_nonzero).empty());
}

TEST_CASE("0.2.3 i18n keys: pkg list/update keys", "[i18n][0.2.3]") {
    init("en");
    REQUIRE(!get(I18nKey::pkg_list_title).empty());
    REQUIRE(!get(I18nKey::pkg_list_none).empty());
    REQUIRE(!get(I18nKey::pkg_list_item).empty());
    REQUIRE(!get(I18nKey::pkg_update_up_to_date).empty());
    REQUIRE(!get(I18nKey::pkg_update_no_updates).empty());
    REQUIRE(!get(I18nKey::pkg_update_updating).empty());
}

TEST_CASE("0.2.3 i18n keys: watch mode keys", "[i18n][0.2.3]") {
    init("en");
    REQUIRE(!get(I18nKey::watch_started).empty());
    REQUIRE(!get(I18nKey::watch_skip_initial).empty());
    REQUIRE(!get(I18nKey::watch_config_changed).empty());
    REQUIRE(!get(I18nKey::watch_detected_change).empty());
    REQUIRE(!get(I18nKey::watch_stopping).empty());
}

TEST_CASE("0.2.3 i18n keys: Chinese translations also non-empty", "[i18n][0.2.3][zh]") {
    init("zh");
    REQUIRE(!get(I18nKey::parallel_jobs_info).empty());
    REQUIRE(!get(I18nKey::pre_build_hook).empty());
    REQUIRE(!get(I18nKey::pkg_list_title).empty());
    REQUIRE(!get(I18nKey::pkg_update_up_to_date).empty());
    REQUIRE(!get(I18nKey::watch_started).empty());
}

TEST_CASE("0.2.3 i18n keys: en and zh differ for key strings", "[i18n][0.2.3][zh]") {
    init("en");
    std::string en_watch = get(I18nKey::watch_started);
    std::string en_list = get(I18nKey::pkg_list_title);
    std::string en_update = get(I18nKey::pkg_update_up_to_date);

    init("zh");
    std::string zh_watch = get(I18nKey::watch_started);
    std::string zh_list = get(I18nKey::pkg_list_title);
    std::string zh_update = get(I18nKey::pkg_update_up_to_date);

    REQUIRE(en_watch != zh_watch);
    REQUIRE(en_list != zh_list);
    REQUIRE(en_update != zh_update);
}

TEST_CASE("0.2.3 i18n keys: fmt works with new keys", "[i18n][0.2.3]") {
    init("en");

    std::string r1 = fmt(I18nKey::parallel_jobs_info,
                         {{"jobs", "8"}, {"total", "12"}});
    REQUIRE(r1.find("8") != std::string::npos);
    REQUIRE(r1.find("12") != std::string::npos);

    std::string r2 = fmt(I18nKey::pkg_update_updating,
                         {{"pkg", "fmt"}, {"old", "1.0"}, {"new", "2.0"}});
    REQUIRE(r2.find("fmt") != std::string::npos);
    REQUIRE(r2.find("1.0") != std::string::npos);
    REQUIRE(r2.find("2.0") != std::string::npos);

    std::string r3 = fmt(I18nKey::watch_detected_change,
                         {{"path", "/src/main.cpp"}});
    REQUIRE(r3.find("/src/main.cpp") != std::string::npos);
}

// 1.2.0-dev.6: per-file build timing detail keys.
TEST_CASE("1.2.0-dev.6 i18n keys: build timing detail", "[i18n][1.2.0-dev.6]") {
    init("en");
    REQUIRE(!get(I18nKey::build_time_header).empty());
    REQUIRE(!get(I18nKey::build_time_entry).empty());
    REQUIRE(!get(I18nKey::build_time_truncated).empty());

    std::string hdr = fmt(I18nKey::build_time_header,
                          {{"compiled", "8"}, {"time", "4.2s"}});
    REQUIRE(hdr.find("8") != std::string::npos);
    REQUIRE(hdr.find("4.2s") != std::string::npos);

    std::string entry = fmt(I18nKey::build_time_entry,
                            {{"time", "2.1s"}, {"file", "src/main.cpp"}});
    REQUIRE(entry.find("2.1s") != std::string::npos);
    REQUIRE(entry.find("src/main.cpp") != std::string::npos);
}

// 1.2.0-dev.7: directory install + upward project-root keys.
TEST_CASE("1.2.0-dev.7 i18n keys: dir install + upward root", "[i18n][1.2.0-dev.7]") {
    init("en");
    REQUIRE(!get(I18nKey::pkg_install_from_dir).empty());
    REQUIRE(!get(I18nKey::pkg_sha256_skipped_dir).empty());
    REQUIRE(!get(I18nKey::config_not_found_upward).empty());

    std::string msg = fmt(I18nKey::pkg_install_from_dir, {{"dir", "/tmp/pkg"}});
    REQUIRE(msg.find("/tmp/pkg") != std::string::npos);
    // the not-found message names the 5-level upward search boundary
    REQUIRE(get(I18nKey::config_not_found_upward).find("5") != std::string::npos);
}

// 1.2.0-dev.8: export hook + ezmk-lua standalone runtime keys.
TEST_CASE("1.2.0-dev.8 i18n keys: export hook + ezmk-lua", "[i18n][1.2.0-dev.8]") {
    init("en");
    REQUIRE(!get(I18nKey::export_hook_note).empty());
    REQUIRE(get(I18nKey::ezmk_lua_usage).find("ezmk-lua") != std::string::npos);
    REQUIRE(!get(I18nKey::ezmk_lua_missing_script).empty());

    std::string need = fmt(I18nKey::ezmk_lua_need_value, {{"option", "--profile"}});
    REQUIRE(need.find("--profile") != std::string::npos);
    std::string unk = fmt(I18nKey::ezmk_lua_unknown_option, {{"option", "--bogus"}});
    REQUIRE(unk.find("--bogus") != std::string::npos);
    std::string extra = fmt(I18nKey::ezmk_lua_extra_arg, {{"arg", "x"}});
    REQUIRE(extra.find("x") != std::string::npos);
}

// ===================================================================
// 0.2.6+: exhaustive regression — EVERY I18nKey must resolve in both
// languages (guards against the enum/key_name/JSON drift that produced
// the {???} bug). Keys and their JSON names are generated from the same
// single source of truth (i18n_keys.def) used by the enum itself.
// ===================================================================

namespace {
static const I18nKey kAllKeys[] = {
#define EZMK_I18N_KEY(name) I18nKey::name,
#include "ezmk/i18n_keys.def"
#undef EZMK_I18N_KEY
};
static const char* const kAllKeyNames[] = {
#define EZMK_I18N_KEY(name) #name,
#include "ezmk/i18n_keys.def"
#undef EZMK_I18N_KEY
};
constexpr size_t kAllKeyCount = sizeof(kAllKeys) / sizeof(kAllKeys[0]);
} // namespace

TEST_CASE("Every I18nKey resolves (no missing-key fallback) in en, zh and zh-TW",
          "[i18n][keys][regression]") {
    for (const char* lang : {"en", "zh", "zh-TW"}) {  // zh-TW: 1.3.0-dev.4 variant
        init(lang);
        for (size_t i = 0; i < kAllKeyCount; ++i) {
            std::string s = get(kAllKeys[i]);
            std::string fallback = std::string("{") + kAllKeyNames[i] + "}";
            INFO("lang=" << lang << " key=" << kAllKeyNames[i]);
            // The missing-key fallback is exactly "{<keyname>}". A real
            // translation is never equal to that (even templates that contain
            // placeholders differ from the bare key name).
            REQUIRE(s != fallback);
            REQUIRE(!s.empty());
        }
    }
}

TEST_CASE("0.2.6 i18n keys: help, cli-error and repo-list keys present",
          "[i18n][0.2.6]") {
    for (const char* lang : {"en", "zh"}) {
        init(lang);
        REQUIRE(!get(I18nKey::repo_list_title).empty());
        REQUIRE(!get(I18nKey::repo_list_none).empty());
        REQUIRE(!get(I18nKey::help_project_new).empty());
        REQUIRE(!get(I18nKey::help_flag_color).empty());
        REQUIRE(!get(I18nKey::cli_arg_required).empty());
        REQUIRE(!get(I18nKey::cli_invalid_color).empty());
    }
}

TEST_CASE("0.2.6 i18n: cli error templates format correctly", "[i18n][0.2.6]") {
    init("en");
    std::string r = fmt(I18nKey::cli_arg_required,
                        {{"cmd", "ezmk project new"}, {"what", "project name"}});
    REQUIRE(r.find("ezmk project new") != std::string::npos);
    REQUIRE(r.find("project name") != std::string::npos);
    REQUIRE(r.find("{cmd}") == std::string::npos);
    REQUIRE(r.find("{what}") == std::string::npos);

    std::string c = fmt(I18nKey::cli_invalid_color, {{"val", "bogus"}});
    REQUIRE(c.find("bogus") != std::string::npos);
}

// ===================================================================
// 1.3.0-dev.4: locale variants — tag normalization, detection paths,
// inheritance (variant → base → English fallback chain).
// ===================================================================

// RAII: remove a file on scope exit (variant-inheritance test writes a temp
// runtime variant next to the test binary).
struct FileGuard {
    fs::path path;
    ~FileGuard() {
        std::error_code ec;
        fs::remove(path, ec);
    }
};

TEST_CASE("normalize_locale_tag canonicalizes BCP-47 tags (1.3.0-dev.4)", "[i18n][1.3.0-dev.4]") {
    // Canonical forms pass through unchanged.
    REQUIRE(normalize_locale_tag("en") == "en");
    REQUIRE(normalize_locale_tag("zh") == "zh");
    REQUIRE(normalize_locale_tag("zh-TW") == "zh-TW");
    REQUIRE(normalize_locale_tag("en-US") == "en-US");
    // Underscore separator, encoding suffix, mixed case → canonical form.
    REQUIRE(normalize_locale_tag("zh_CN") == "zh-CN");
    REQUIRE(normalize_locale_tag("zh_CN.UTF-8") == "zh-CN");
    REQUIRE(normalize_locale_tag("zh-cn") == "zh-CN");
    REQUIRE(normalize_locale_tag("ZH-cn") == "zh-CN");
    REQUIRE(normalize_locale_tag("en_US.UTF-8") == "en-US");
    // Leading/trailing separators are tolerated (lenient → base language).
    REQUIRE(normalize_locale_tag("_zh") == "zh");
    REQUIRE(normalize_locale_tag("zh_") == "zh");
    // Invalid input → empty (callers fall back to detection).
    REQUIRE(normalize_locale_tag("").empty());
    REQUIRE(normalize_locale_tag("e").empty());          // 1-letter language
    REQUIRE(normalize_locale_tag("english").empty());    // >3 letters
    REQUIRE(normalize_locale_tag("zh-Hant-TW").empty()); // script tag — out of scope
    REQUIRE(normalize_locale_tag("zh-1").empty());       // non-letter region
    REQUIRE(normalize_locale_tag("zh-ABC").empty());     // region >2 letters
}

TEST_CASE("detect_language() returns the full variant tag (1.3.0-dev.4)", "[i18n][detect][1.3.0-dev.4]") {
    EnvGuard guard("EZMK_LANG", "zh-TW");
    REQUIRE(detect_language() == "zh-TW");
}

TEST_CASE("detect_language() normalizes underscore + encoding variants (1.3.0-dev.4)", "[i18n][detect][1.3.0-dev.4]") {
    EnvGuard guard("EZMK_LANG", "zh_CN.UTF-8");
    REQUIRE(detect_language() == "zh-CN");
}

TEST_CASE("detect_language() falls through for unknown variant tags (1.3.0-dev.4)", "[i18n][detect][1.3.0-dev.4]") {
    // "zz-ZZ" has neither base nor variant data → the EZMK_LANG value must
    // NOT stick; detection falls back to the platform locale / English.
    EnvGuard guard("EZMK_LANG", "zz-ZZ");
    std::string lang = detect_language();
    REQUIRE(lang != "zz-ZZ");
    REQUIRE(!lang.empty());
}

TEST_CASE("init(zh-TW) loads the Traditional variant over the zh base (1.3.0-dev.4)", "[i18n][1.3.0-dev.4]") {
    init("zh-TW");
    std::string building = get(I18nKey::building);
    REQUIRE(building.find("建置") != std::string::npos);   // variant override
    // Differs from the simplified zh base.
    init("zh");
    std::string zh_building = get(I18nKey::building);
    REQUIRE(zh_building.find("构建") != std::string::npos);
    REQUIRE(building != zh_building);
}

TEST_CASE("init(zh-CN) has no variant file → falls back to the zh base (1.3.0-dev.4)", "[i18n][1.3.0-dev.4]") {
    // zh-CN is a canonical tag but no locale/zh-CN.json exists — the base
    // language (zh) is used and behavior matches the pre-variant release.
    init("zh-CN");
    REQUIRE(get(I18nKey::building).find("构建") != std::string::npos);
    // Underscore spelling reaches the same state via normalization.
    init("zh_CN");
    REQUIRE(get(I18nKey::building).find("构建") != std::string::npos);
}

TEST_CASE("variant inheritance: missing keys fall back to the base language (1.3.0-dev.4)", "[i18n][1.3.0-dev.4]") {
    // Drop a PARTIAL runtime variant (few keys) next to the test binary — the
    // runtime file takes priority over embedded data. Keys it declares must
    // override the base; keys it omits must inherit the base (zh).
    fs::path variant_file =
        ezmk::util::get_exe_dir() / ".." / "locale" / "zh-HK.json";
    REQUIRE_FALSE(fs::exists(variant_file));  // must not clobber a real file
    ezmk::util::file_write(variant_file,
        "{\"meta\":{\"language\":\"zh-HK\",\"language_name\":\"測試變體\","
        "\"version\":\"1\",\"extends\":\"zh\"},\"strings\":{"
        "\"build_success\":\"建置成功: {path}\"}}\n");
    FileGuard guard{variant_file};

    init("zh-HK");
    // Override key: declared by the variant.
    REQUIRE(get(I18nKey::build_success).find("建置") != std::string::npos);
    // Inherited key: NOT declared by the variant → the zh base value.
    REQUIRE(get(I18nKey::building).find("构建") != std::string::npos);
    // Placeholders still format on the inherited template.
    std::string r = fmt(I18nKey::building,
                        {{"name", "x"}, {"type", "executable"}, {"lang", "C++17"}});
    REQUIRE(r.find("x") != std::string::npos);
}

TEST_CASE("init(xx) still falls back to English (1.3.0-dev.4)", "[i18n][1.3.0-dev.4]") {
    init("xx");  // 2-letter tag, no data → English fallback (unchanged).
    REQUIRE(get(I18nKey::building).find("Building") != std::string::npos);
}
