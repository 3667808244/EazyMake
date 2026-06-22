#pragma once

#include <map>
#include <string>
#include <string_view>

namespace ezmk::i18n {

// All user-visible string keys (alphabetically sorted within each module group)
enum class I18nKey {
    // build
    archive_failed,
    build_failed,
    build_success,
    building,
    compiling,
    compilation_failed,
    compile_options_changed,
    compiler_not_found,
    linking,
    link_failed,
    archiving,
    main_missing,
    no_source_files,
    src_dir_missing,
    cache_summary,

    // cache (verbose)
    cache_hit,
    cache_hit_brief,
    cache_miss,
    cache_miss_record,
    cache_miss_source,
    cache_miss_options,
    cache_miss_header,
    source_changed,
    options_changed,
    header_changed,
    include_structure_changed,

    // pkg
    auto_yes,
    circular_dep,
    compiling_pkg,
    confirm_continue,
    confirm_script,
    downloading,
    extracting,
    exec_question,
    found_in_repo,
    found_script,
    global_confirm,
    install_cancelled,
    install_cancelled_user,
    installed,
    installing,
    installing_to,
    missing_dep,
    not_found,
    overwrite_confirm,
    removed,
    removing,
    resolving_deps,
    running_script,
    script_completed,
    script_failed,
    searching,
    searching_repos,
    sha256_mismatch,
    sha256_ok,
    skipping,
    verifying,

    // repo
    cloning,
    no_repos,
    pulling,
    re_cloning,
    re_reading,
    repo_added,
    repo_not_found,
    repo_removed,
    repo_updated,
    removing_cache,

    // project
    creating_project,
    project_created,
    init_git,
    git_initialized,
    git_not_found,
    git_init_failed,

    // run & clean
    cleaned,
    running,
    clean_stale,

    // editor
    no_editor,
    opening_editor,
    editor_error,

    // version
    version_output,

    // utils
    utils_placeholder,

    // general
    fatal_prefix,
    error_prefix,
    warn_prefix,
    info_prefix,
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
