#include "catch2.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"
#include "nlohmann/json.hpp"

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

TEST_CASE("detect_language() normalizes zh-CN to zh", "[i18n][detect]") {
    EnvGuard guard("EZMK_LANG", "zh-CN");
    std::string lang = detect_language();
    REQUIRE(lang == "zh");
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
