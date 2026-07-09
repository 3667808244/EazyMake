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
    parallel_jobs_info,       // 0.2.3+
    profile_not_found,        // 0.2.3+
    pre_build_hook,           // 0.2.3+
    post_build_hook,          // 0.2.3+
    on_failure_hook,          // 0.2.3+
    hook_not_found,           // 0.2.3+
    hook_nonzero,             // 0.2.3+

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
    pkg_list_title,           // 0.2.3+
    pkg_list_none,            // 0.2.3+
    pkg_list_item,            // 0.2.3+
    pkg_list_parse_error,     // 0.2.3+
    pkg_list_no_toml,         // 0.2.3+
    pkg_update_up_to_date,    // 0.2.3+
    pkg_update_no_updates,    // 0.2.3+
    pkg_update_updating,      // 0.2.3+
    pkg_info_name,            // 0.2.3+
    pkg_info_version,         // 0.2.3+
    pkg_info_type,            // 0.2.3+
    pkg_info_language,        // 0.2.3+
    pkg_info_scope,           // 0.2.3+
    pkg_info_location,        // 0.2.3+
    pkg_info_installed,       // 0.2.3+
    pkg_info_compile_flags,   // 0.2.3+
    pkg_info_include_dirs,    // 0.2.3+
    pkg_info_hard_deps,       // 0.2.3+
    pkg_info_optional_deps,   // 0.2.3+
    pkg_info_link_flags,      // 0.2.3+
    pkg_info_link_dirs,       // 0.2.3+
    pkg_info_system_targets,  // 0.2.3+
    pkg_info_tools,           // 0.2.3+
    pkg_info_artifacts,       // 0.2.3+
    pkg_info_none,            // 0.2.3+
    pkg_info_permissions,     // 0.2.5+

    // config errors (0.2.3+)
    config_err_invalid_type,
    config_err_missing_ver,   // 0.2.3+
    config_err_invalid_lang,  // 0.2.3+
    config_err_invalid_macro, // 0.2.3+
    config_err_empty_src_dirs,// 0.2.3+
    config_err_ezmk_macros_type, // 0.2.3+
    config_err_macros_val_type,  // 0.2.3+
    config_err_empty_profile,    // 0.2.3+
    config_err_invalid_profile,  // 0.2.3+

    // cli usage (0.2.3+)
    cli_usage_header,
    cli_usage_project,        // 0.2.3+
    cli_usage_pkg,            // 0.2.3+
    cli_usage_repo,           // 0.2.3+
    cli_usage_utils,          // 0.2.3+
    cli_usage_scopes,         // 0.2.3+
    cli_usage_build_flags,    // 0.2.3+
    cli_usage_install_flags,  // 0.2.3+
    cli_usage_usage,           // 0.2.3+
    cli_unknown_option,        // 0.2.5+
    cli_missing_value,         // 0.2.5+
    scope_project,             // 0.2.3+
    scope_user,                // 0.2.3+
    scope_global,              // 0.2.3+

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

    // repo info (0.2.5+)
    repo_info_name,
    repo_info_scope,
    repo_info_url,
    repo_info_type,
    repo_info_branch,
    repo_info_updated,
    repo_info_cache,
    repo_info_packages,
    repo_info_version_list,
    repo_search_resolved,
    repo_validate_missing_file,
    repo_validate_bad_sha256,
    auto_updating_repos,

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

    // utils / Lua
    utils_placeholder,
    utils_not_found,
    lua_init_failed,
    lua_error,
    lua_api_type_error,
    lua_api_arg_count,

    // watch (0.2.3+)
    watch_started,
    watch_skip_initial,
    watch_config_changed,
    watch_detected_change,
    watch_stopping,

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
