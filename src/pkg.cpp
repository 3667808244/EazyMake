#include "ezmk/pkg.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/config.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/repo.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ezmk::pkg {

// ===================================================================
// Path resolution
// ===================================================================

fs::path pkg_install_dir(cli::Scope scope) {
    switch (scope) {
    case cli::Scope::Project:
        return fs::current_path() / ".ezmk/pkg";
    case cli::Scope::User: {
#ifdef EZMK_WIN
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (appdata) return fs::path(appdata) / "ezmk/pkg";
        return util::get_home_dir() / "AppData/Local/ezmk/pkg";
#else
        return util::get_home_dir() / ".local/ezmk/pkg";
#endif
    }
    case cli::Scope::Global:
        return util::get_exe_dir() / "pkg";
    }
    return {};
}

std::vector<fs::path> pkg_search_dirs(const std::vector<cli::Scope>& scopes) {
    std::vector<fs::path> dirs;
    for (auto s : scopes) {
        dirs.push_back(pkg_install_dir(s));
    }
    return dirs;
}

// ===================================================================
// Helpers
// ===================================================================

static bool confirm(std::string_view msg, bool assume_yes = false) {
    if (assume_yes) {
        util::info(std::string(msg) + ezmk::i18n::get(ezmk::i18n::I18nKey::auto_yes));
        return true;
    }
    std::cerr << "[ezmk] " << msg << " [y/N] ";
    std::string line;
    std::getline(std::cin, line);
    return line == "y" || line == "Y" || line == "yes";
}

// Detect the platform-appropriate install script in the script/ directory.
// Returns the path if found, empty path otherwise.
static fs::path detect_install_script(const fs::path& pkg_root,
                                       std::string_view basename) {
    auto script_dir = pkg_root / "script";
    if (!util::file_exists(script_dir)) return {};

#ifdef EZMK_WIN
    // Windows: .ps1 first, then .bat
    fs::path ps1 = script_dir / (std::string(basename) + ".ps1");
    if (util::file_exists(ps1)) return ps1;
    fs::path bat = script_dir / (std::string(basename) + ".bat");
    if (util::file_exists(bat)) return bat;
#else
    // Linux: .sh
    fs::path sh = script_dir / (std::string(basename) + ".sh");
    if (util::file_exists(sh)) return sh;
#endif
    return {};
}

// Run an install script with user review + confirmation.
// Returns true if execution succeeded or was skipped (continue install).
// Returns false if user chose to abort.
static bool run_install_script(const fs::path& script, const fs::path& cwd,
                                bool assume_yes, std::string_view label) {
    std::string desc = std::string(label) + " " + script.filename().string();

    util::info(ezmk::i18n::I18nKey::found_script, {{"label", desc}});
    util::open_in_editor(script);

    if (!confirm(ezmk::i18n::fmt(ezmk::i18n::I18nKey::exec_question, {{"label", desc}}), assume_yes)) {
        util::info(ezmk::i18n::I18nKey::skipping, {{"label", desc}});
        return true; // skip but continue
    }

    util::info(ezmk::i18n::I18nKey::running_script, {{"label", desc}});
    auto res = util::run_script(script, cwd);
    if (res.exit_code != 0) {
        std::string code_str = std::to_string(res.exit_code);
        std::string err_msg = ezmk::i18n::fmt(ezmk::i18n::I18nKey::script_failed,
                                               {{"label", std::string(label)}, {"code", code_str}});
        if (!res.err.empty()) util::error(res.err);
        if (!confirm(ezmk::i18n::fmt(ezmk::i18n::I18nKey::confirm_continue, {{"msg", err_msg}}), assume_yes)) {
            return false; // abort
        }
    } else {
        util::info(ezmk::i18n::I18nKey::script_completed, {{"label", std::string(label)}});
    }
    return true;
}

// Validate a directory is a proper EazyMake package
static void validate_pkg(const fs::path& dir) {
    // ezmk.toml is always required
    if (!util::file_exists(dir / "ezmk.toml")) {
        throw std::runtime_error("package missing ezmk.toml: " + dir.string());
    }

    // Read config to determine package type
    auto cfg = config::parse_config(dir / "ezmk.toml");
    bool is_utils = (cfg.project.type == "utils");

    // include/ and src/ are optional for utils packages (they may have only utils/ scripts)
    if (!is_utils) {
        if (!util::file_exists(dir / "include")) {
            throw std::runtime_error("package missing include/ directory: " + dir.string());
        }
        if (!util::file_exists(dir / "src")) {
            throw std::runtime_error("package missing src/ directory: " + dir.string());
        }
    }
}

// Extract the package name from its ezmk.toml
static std::string pkg_name_from_dir(const fs::path& dir) {
    auto cfg = config::parse_config(dir / "ezmk.toml");
    return cfg.project.name;
}

// ===================================================================
// Compile a package to .a static library
// ===================================================================

fs::path compile_package(const fs::path& pkg_dir,
                         const std::vector<fs::path>& dep_includes) {
    auto cfg = config::parse_config(pkg_dir / "ezmk.toml");
    std::string name = cfg.project.name;
    fs::path build_dir = pkg_dir / "build";
    fs::create_directories(build_dir);

    auto sources = util::list_files(pkg_dir / "src", {".c", ".cc", ".cpp", ".cxx"});
    if (sources.empty()) {
        util::warn("package has no source files: " + name);
        return {};
    }

    fs::path lib = build_dir / ("lib" + name + ".a");

    // Clean stale temps from previous crashed builds
    {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(build_dir, ec)) {
            auto& p = e.path();
            if (p.extension() == ".tmp") {
                fs::remove(p, ec);
            }
        }
    }

    // Load package cache
    fs::path cache_path = build_dir / ".pkg_cache.json";
    auto record = cache::load_record(cache_path);

    // Check global compile options signature
    auto cur_sig = cache::compile_options_signature(cfg.compile);
    if (record.compile_options_signature != cur_sig) {
        if (!record.compile_options_signature.empty()) {
            util::info("    compile options changed, invalidating package cache");
        }
        record.compile_options_signature = cur_sig;
        record.files.clear();
    }

    // ---- Unified compile phase ----
    auto lang = config::parse_language(cfg.project.language);

    cache::CompileInput cin;
    cin.sources = std::move(sources);
    cin.obj_dir = build_dir;
    cin.dep_dir = build_dir;
    cin.proj_root = pkg_dir;
    cin.compile = cfg.compile;
    cin.lang = lang;
    cin.extra_includes = dep_includes;
    cin.cache_obj_dir = build_dir;   // package: obj_dir == cache_obj_dir
    cin.disable_cache = false;       // packages always use cache
    cin.use_pic = false;             // packages are always static libs

    auto comp_result = cache::compile_sources(cin, record);

    // Save cache
    cache::save_record(record, cache_path);

    if (comp_result.cache_hits > 0 || comp_result.cache_misses > 0) {
        std::string summary = "    ";
        if (comp_result.cache_hits > 0) summary += std::to_string(comp_result.cache_hits) + " cached, ";
        summary += std::to_string(comp_result.cache_misses) + " compiled";
        util::info(summary);
    }

    // Archive (skip if nothing changed and .a exists)
    if (comp_result.cache_misses == 0 && util::file_exists(lib)) {
        return lib;
    }

    // Use the actual object paths returned by compile_sources
    // (handles nested source directories correctly, unlike filename-based reconstruction)

    // Archive to temp file, then atomic rename
    fs::path lib_tmp = build_dir / ("lib" + name + ".a.tmp");
    {
        std::error_code ec;
        fs::remove(lib_tmp, ec);
    }

    std::ostringstream ar_cmd;
    ar_cmd << "ar rcs \"" << lib_tmp.string() << "\"";
    for (auto& o : comp_result.objects) {
        ar_cmd << " \"" << o.string() << "\"";
    }

    auto ar_res = util::run_command(ar_cmd.str());
    if (ar_res.exit_code != 0) {
        std::error_code ec;
        fs::remove(lib_tmp, ec);
        util::error(ar_res.err);
        throw std::runtime_error("failed to create archive for: " + name);
    }

    {
        std::error_code ec;
        fs::rename(lib_tmp, lib, ec);
        if (ec) {
            fs::copy_file(lib_tmp, lib, fs::copy_options::overwrite_existing, ec);
            fs::remove(lib_tmp, ec);
        }
    }

    return lib;
}

// ===================================================================
// Dependency resolution
// ===================================================================

std::vector<fs::path> resolve_dependency_order(const std::vector<fs::path>& pkg_dirs) {
    if (pkg_dirs.empty()) return {};

    // Map: package name → directory path
    std::map<std::string, fs::path> name_to_dir;
    for (auto& d : pkg_dirs) {
        name_to_dir[pkg_name_from_dir(d)] = d;
    }

    // Build adjacency list and in-degree map
    std::map<std::string, std::vector<std::string>> adj;
    std::map<std::string, int> in_degree;
    for (auto& d : pkg_dirs) {
        auto cfg = config::parse_config(d / "ezmk.toml");
        std::string name = cfg.project.name;
        if (in_degree.find(name) == in_degree.end()) in_degree[name] = 0;
        for (auto& dep : cfg.depends.libs) {
            adj[dep].push_back(name);
            in_degree[name]++;
        }
        // 0.2.2+: want dependencies are included if the package is installed
        for (auto& dep : cfg.depends.want) {
            if (name_to_dir.find(dep) != name_to_dir.end()) {
                adj[dep].push_back(name);
                in_degree[name]++;
            }
        }
    }

    // Check that all dependencies are satisfied
    for (auto& [name, _] : adj) {
        if (name_to_dir.find(name) == name_to_dir.end()) {
            throw std::runtime_error("missing dependency: " + name);
        }
    }

    // Kahn's algorithm
    std::deque<std::string> queue;
    for (auto& [name, deg] : in_degree) {
        if (deg == 0) queue.push_back(name);
    }

    std::vector<fs::path> sorted;
    while (!queue.empty()) {
        auto name = queue.front();
        queue.pop_front();
        sorted.push_back(name_to_dir[name]);

        for (auto& next : adj[name]) {
            if (--in_degree[next] == 0) {
                queue.push_back(next);
            }
        }
    }

    if (sorted.size() != pkg_dirs.size()) {
        throw std::runtime_error(ezmk::i18n::get(ezmk::i18n::I18nKey::circular_dep));
    }

    return sorted;
}

// ===================================================================
// Install
// ===================================================================

void install(const std::string& pkg_file, cli::Scope scope,
             std::string_view expected_sha256,
             bool assume_yes) {
    fs::path dest_dir = pkg_install_dir(scope);

    // Determine if it's a URL or local file
    fs::path input(pkg_file);
    bool is_url = pkg_file.find("://") != std::string::npos
               || (pkg_file.find('.') != std::string::npos
                   && pkg_file.find('/') != std::string::npos
                   && !util::file_exists(fs::path(pkg_file)));

    // If no protocol, prepend https://
    std::string url;
    if (is_url && pkg_file.find("://") == std::string::npos) {
        url = "https://" + pkg_file;
    } else {
        url = pkg_file;
    }

    fs::path archive_path;

    if (is_url) {
        // Download to temp
        fs::path tmp_dir = fs::temp_directory_path();
        // Extract filename from URL
        std::string fname = url;
        size_t last_slash = fname.rfind('/');
        if (last_slash != std::string::npos) fname = fname.substr(last_slash + 1);
        if (fname.empty()) fname = "package.tar.gz";
        archive_path = tmp_dir / fname;

        util::info(ezmk::i18n::I18nKey::downloading, {{"url", url}});
        util::download(url, archive_path);
    } else {
        archive_path = input;
        if (!util::file_exists(archive_path)) {
            // Not a local file or URL — try searching registered repos
            util::info(ezmk::i18n::I18nKey::searching_repos, {{"pkg", pkg_file}});
            auto search_result = repo::search_package(pkg_file, {
                cli::Scope::Project, cli::Scope::User, cli::Scope::Global});
            if (search_result.archive_path.empty() ||
                !util::file_exists(search_result.archive_path)) {
                util::fatal(ezmk::i18n::I18nKey::not_found, {{"pkg", pkg_file}});
            }
            archive_path = search_result.archive_path;
            // Use sha256 from index.toml if user didn't provide one explicitly
            if (expected_sha256.empty() && !search_result.sha256.empty()) {
                expected_sha256 = search_result.sha256;
            }
            util::info(ezmk::i18n::I18nKey::found_in_repo, {{"path", archive_path.string()}});
        }
    }

    // SHA-256 verification
    if (!expected_sha256.empty()) {
        util::info(ezmk::i18n::I18nKey::verifying);
        std::string actual = crypto::sha256_file(archive_path);
        // Case-insensitive comparison
        std::string expected(expected_sha256);
        std::string actual_lower = actual;
        for (auto& c : expected) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (auto& c : actual_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (actual_lower != expected) {
            // Technical details first (untranslated), then fatal with translated message
            util::error("  expected: " + std::string(expected_sha256));
            util::error("  actual:   " + actual);
            util::fatal(ezmk::i18n::I18nKey::sha256_mismatch, {{"pkg", pkg_file}});
        }
        util::info(ezmk::i18n::I18nKey::sha256_ok);
    }

    // Safety: global install confirmation
    if (scope == cli::Scope::Global) {
        if (!confirm(ezmk::i18n::get(ezmk::i18n::I18nKey::global_confirm), assume_yes)) {
            util::info(ezmk::i18n::I18nKey::install_cancelled);
            return;
        }
    }

    // Extract to temp staging area — use PID + high-res counter to avoid collisions
    static std::atomic<uint64_t> counter{0};
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    fs::path stage = fs::temp_directory_path() /
        ("ezmk_pkg_" + std::to_string(now_us) + "_" +
         std::to_string(counter.fetch_add(1, std::memory_order_relaxed)));
    fs::create_directories(stage);

    try {
        util::info(ezmk::i18n::I18nKey::extracting);
        util::extract_archive(archive_path, stage);

        // The archive might have a top-level directory; find the actual package root
        fs::path pkg_root = stage;
        {
            std::vector<fs::path> subdirs;
            for (auto& e : fs::directory_iterator(stage)) {
                if (e.is_directory()) subdirs.push_back(e.path());
            }
            if (subdirs.size() == 1) {
                if (util::file_exists(subdirs[0] / "ezmk.toml")) {
                    pkg_root = subdirs[0];
                }
            }
        }

        validate_pkg(pkg_root);

        auto pkg_cfg = config::parse_config(pkg_root / "ezmk.toml");
        std::string pkg_name = pkg_cfg.project.name;

        // Preinstall hook
        fs::path preinstall_script = detect_install_script(pkg_root, "preinstall");
        if (!preinstall_script.empty()) {
            if (!run_install_script(preinstall_script, dest_dir / pkg_name,
                                    assume_yes, "preinstall")) {
                util::info(ezmk::i18n::I18nKey::install_cancelled_user,
                           {{"hook", "preinstall"}});
                util::remove_all(stage);
                return;
            }
        }

        // Check for existing install
        fs::path install_path = dest_dir / pkg_name;
        if (util::file_exists(install_path)) {
            if (!confirm(ezmk::i18n::fmt(ezmk::i18n::I18nKey::overwrite_confirm,
                         {{"pkg", pkg_name}, {"path", install_path.string()}}), assume_yes)) {
                util::info(ezmk::i18n::I18nKey::install_cancelled);
                util::remove_all(stage);
                return;
            }
            util::remove_all(install_path);
        }

        // Collect all involved packages for dependency resolution
        std::vector<fs::path> all_pkgs = { pkg_root };

        // Check and resolve dependencies
        {
            std::set<std::string> seen = { pkg_name };
            std::deque<std::string> to_check = { pkg_name };
            while (!to_check.empty()) {
                auto cur = to_check.front();
                to_check.pop_front();

                fs::path cur_dir = pkg_root;
                if (cur != pkg_name) {
                    cur_dir = dest_dir / cur;
                    if (!util::file_exists(cur_dir)) {
                        throw std::runtime_error(
                            ezmk::i18n::fmt(ezmk::i18n::I18nKey::missing_dep,
                                            {{"dep", cur}}));
                    }
                }

                auto cur_cfg = config::parse_config(cur_dir / "ezmk.toml");
                for (auto& dep : cur_cfg.depends.libs) {
                    if (seen.insert(dep).second) {
                        to_check.push_back(dep);
                        fs::path dep_path = dest_dir / dep;
                        if (util::file_exists(dep_path)) {
                            all_pkgs.push_back(dep_path);
                        } else {
                            throw std::runtime_error(
                                ezmk::i18n::fmt(ezmk::i18n::I18nKey::missing_dep,
                                                {{"dep", dep}}));
                        }
                    }
                }
                // 0.2.2+: want dependencies are optional — include if installed, skip if missing
                for (auto& dep : cur_cfg.depends.want) {
                    if (seen.insert(dep).second) {
                        fs::path dep_path = dest_dir / dep;
                        if (util::file_exists(dep_path)) {
                            to_check.push_back(dep);
                            all_pkgs.push_back(dep_path);
                        }
                        // If not installed: silently skip (optional dependency)
                    }
                }
            }
        }

        // Dependency ordering + compilation
        util::info(ezmk::i18n::I18nKey::resolving_deps);
        auto order = resolve_dependency_order(all_pkgs);

        // Build name → dir map for resolving dependency include paths
        std::map<std::string, fs::path> name_to_dir;
        for (auto& d : all_pkgs) {
            name_to_dir[config::parse_config(d / "ezmk.toml").project.name] = d;
        }

        for (auto& dir : order) {
            auto cfg = config::parse_config(dir / "ezmk.toml");
            // Skip compilation for utils packages without source files
            if (cfg.project.type == "utils" && !util::file_exists(dir / "src")) {
                continue;
            }
            std::vector<fs::path> dep_includes;
            for (auto& dep : cfg.depends.libs) {
                auto it = name_to_dir.find(dep);
                if (it != name_to_dir.end()) {
                    dep_includes.push_back(it->second / "include");
                }
            }
            // 0.2.2+: want deps also contribute include paths when installed
            for (auto& dep : cfg.depends.want) {
                auto it = name_to_dir.find(dep);
                if (it != name_to_dir.end()) {
                    dep_includes.push_back(it->second / "include");
                }
            }
            util::info(ezmk::i18n::I18nKey::compiling_pkg,
                       {{"name", cfg.project.name}});
            compile_package(dir, dep_includes);
        }

        // Copy to install directory
        fs::create_directories(dest_dir);
        util::info(ezmk::i18n::I18nKey::installing_to, {{"path", install_path.string()}});
        util::copy_recursive(pkg_root, install_path);

        // Postinstall hook
        fs::path postinstall_script = detect_install_script(install_path, "postinstall");
        if (!postinstall_script.empty()) {
            if (!run_install_script(postinstall_script, install_path,
                                    assume_yes, "postinstall")) {
                util::info(ezmk::i18n::I18nKey::install_cancelled_user,
                           {{"hook", "postinstall"}});
                // Installation files are already in place; leave them
            }
        }

        util::info(ezmk::i18n::I18nKey::installed, {{"pkg", pkg_name}});

    } catch (...) {
        // Clean up staging on error
        util::remove_all(stage);
        throw;
    }

    // Cleanup temp
    util::remove_all(stage);
}

// ===================================================================
// Remove
// ===================================================================

void remove(const std::string& pkg_name, const std::vector<cli::Scope>& scopes) {
    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (util::file_exists(pkg_path)) {
            util::info(ezmk::i18n::I18nKey::removing, {{"pkg", pkg_path.string()}});
            util::remove_all(pkg_path);
            return;
        }
    }
    util::error(ezmk::i18n::I18nKey::not_found, {{"pkg", pkg_name}});
}

// ===================================================================
// Search
// ===================================================================

std::vector<fs::path> search(const std::string& pkg_name,
                             const std::vector<cli::Scope>& scopes) {
    std::vector<fs::path> results;
    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (util::file_exists(pkg_path)) {
            results.push_back(pkg_path);
        }
    }
    return results;
}

// ===================================================================
// Info
// ===================================================================

namespace {
    std::string scope_name(cli::Scope s) {
        switch (s) {
        case cli::Scope::Project: return ezmk::i18n::get(ezmk::i18n::I18nKey::scope_project);
        case cli::Scope::User:    return ezmk::i18n::get(ezmk::i18n::I18nKey::scope_user);
        case cli::Scope::Global:  return ezmk::i18n::get(ezmk::i18n::I18nKey::scope_global);
        }
        return "unknown";
    }

    std::string format_time(const fs::path& p) {
        std::error_code ec;
        auto ftime = fs::last_write_time(p, ec);
        if (ec) return "(unknown)";

        // C++20: use clock_cast; pre-C++20 fallback
#if __cplusplus >= 202002L
        auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
#else
        // Convert file_time to system_clock by going through time_t
        auto sctp = std::chrono::system_clock::from_time_t(
            std::chrono::system_clock::to_time_t(
                std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now()
                    + std::chrono::system_clock::now())));
#endif
        auto tt = std::chrono::system_clock::to_time_t(sctp);
        auto* tm = std::localtime(&tt);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        return buf;
    }
}

void info(const std::string& pkg_name, const std::vector<cli::Scope>& scopes) {
    auto none_str = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_none);
    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (util::file_exists(pkg_path)) {
            auto cfg = config::parse_config(pkg_path / "ezmk.toml");
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_name)
                      << ": " << cfg.project.name << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_version)
                      << ": " << cfg.project.version << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_type)
                      << ": " << cfg.project.type << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_language)
                      << ": " << cfg.project.language << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_scope)
                      << ": " << scope_name(scope) << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_location)
                      << ": " << pkg_path.string() << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_installed)
                      << ": " << format_time(pkg_path) << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_compile_flags)
                      << ":";
            for (auto& f : cfg.compile.flags) std::cout << " " << f;
            if (cfg.compile.flags.empty()) std::cout << none_str;
            std::cout << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_include_dirs)
                      << ":";
            for (auto& d : cfg.compile.include_dirs) std::cout << " " << d;
            if (cfg.compile.include_dirs.empty()) std::cout << none_str;
            std::cout << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_hard_deps)
                      << ":";
            if (cfg.depends.libs.empty()) std::cout << none_str;
            for (auto& d : cfg.depends.libs) std::cout << " " << d;
            std::cout << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_optional_deps)
                      << ":";
            if (cfg.depends.want.empty()) std::cout << none_str;
            for (auto& d : cfg.depends.want) std::cout << " " << d;
            std::cout << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_link_flags)
                      << ":";
            for (auto& f : cfg.link.flags) std::cout << " " << f;
            if (cfg.link.flags.empty()) std::cout << none_str;
            std::cout << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_link_dirs)
                      << ":";
            for (auto& d : cfg.link.link_dirs) std::cout << " " << d;
            if (cfg.link.link_dirs.empty()) std::cout << none_str;
            std::cout << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_system_targets)
                      << ":";
            for (auto& t : cfg.link.system_targets) std::cout << " " << t;
            if (cfg.link.system_targets.empty()) std::cout << none_str;
            std::cout << "\n";

            // Show tools for utils packages
            if (cfg.project.type == "utils") {
                std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_tools)
                          << ":";
                if (cfg.utils.tools.empty()) {
                    std::cout << none_str;
                } else {
                    for (size_t i = 0; i < cfg.utils.tools.size(); ++i) {
                        if (i > 0) std::cout << ",";
                        std::cout << " " << cfg.utils.tools[i];
                    }
                }
                std::cout << "\n";
            }

            // 0.2.5+: Show declared utils permissions, if any.
            if (cfg.utils.permissions.has_value()) {
                const auto& pm = *cfg.utils.permissions;
                std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_permissions)
                          << ":\n";
                auto print_list = [&](const char* label,
                                      const std::vector<std::string>& v) {
                    if (v.empty()) return;
                    std::cout << "    " << label << ":";
                    for (auto& e : v) std::cout << " " << e;
                    std::cout << "\n";
                };
                print_list("read", pm.read);
                print_list("read-deny", pm.read_deny);
                print_list("write", pm.write);
                print_list("write-deny", pm.write_deny);
                print_list("run", pm.run);
                print_list("run-deny", pm.run_deny);
                std::cout << "    (unlisted access will prompt at runtime)\n";
            }
            fs::path build_dir = pkg_path / "build";
            if (util::file_exists(build_dir)) {
                std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_artifacts)
                          << ":";
                bool found = false;
                for (auto& f : fs::directory_iterator(build_dir)) {
                    auto ext = f.path().extension().string();
                    if (ext == ".a" || ext == ".dll" || ext == ".so") {
                        std::cout << " " << f.path().filename().string();
                        found = true;
                    }
                }
                if (!found) std::cout << none_str;
                std::cout << "\n";
            }
            return;
        }
    }
    util::error(ezmk::i18n::I18nKey::not_found, {{"pkg", pkg_name}});
}

// 0.2.3+
void list(const std::vector<cli::Scope>& scopes) {
    for (auto scope : scopes) {
        std::string scope_label = (scope == cli::Scope::Project) ? "project" :
                                  (scope == cli::Scope::User) ? "user" : "global";
        util::info(ezmk::i18n::I18nKey::pkg_list_title, {{"scope", scope_label}});

        fs::path dir = pkg_install_dir(scope);
        if (!util::file_exists(dir)) {
            util::info(ezmk::i18n::I18nKey::pkg_list_none);
            continue;
        }

        std::vector<std::string> names;
        for (auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                names.push_back(entry.path().filename().string());
            }
        }
        std::sort(names.begin(), names.end());

        if (names.empty()) {
            util::info(ezmk::i18n::I18nKey::pkg_list_none);
            continue;
        }

        for (auto& name : names) {
            fs::path pkg_path = dir / name;
            auto toml = pkg_path / "ezmk.toml";
            if (util::file_exists(toml)) {
                try {
                    auto cfg = config::parse_config(toml);
                    std::string line = ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_list_item,
                        {{"name", cfg.project.name},
                         {"version", cfg.project.version},
                         {"type", cfg.project.type}});
                    if (cfg.project.type == "utils" && !cfg.utils.tools.empty()) {
                        line += " (tools:";
                        for (size_t i = 0; i < cfg.utils.tools.size(); ++i) {
                            if (i > 0) line += ",";
                            line += " " + cfg.utils.tools[i];
                        }
                        line += ")";
                    } else if (!cfg.depends.libs.empty()) {
                        line += " (depends:";
                        for (size_t i = 0; i < cfg.depends.libs.size(); ++i) {
                            if (i > 0) line += ",";
                            line += " " + cfg.depends.libs[i];
                        }
                        line += ")";
                    }
                    util::info(line);
                } catch (...) {
                    util::warn(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_list_parse_error,
                                                {{"name", name}}));
                }
            } else {
                util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_list_no_toml,
                                            {{"name", name}}));
            }
        }
    }
}

// 0.2.3+
void update(const std::string& pkg_name, const std::vector<cli::Scope>& scopes) {
    // Find installed package in specified scopes (first match wins)
    cli::Scope found_scope = cli::Scope::Project;
    fs::path found_pkg_path;
    std::string installed_version;
    bool found = false;

    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (!util::file_exists(pkg_path)) continue;

        auto cfg = config::parse_config(pkg_path / "ezmk.toml");
        found_scope = scope;
        found_pkg_path = pkg_path;
        installed_version = cfg.project.version;
        found = true;
        break;
    }

    if (!found) {
        util::error(ezmk::i18n::I18nKey::not_found, {{"pkg", pkg_name}});
        return;
    }

    // Search registered repos for the package (all scopes)
    auto search_result = repo::search_package(pkg_name, {
        cli::Scope::Project, cli::Scope::User, cli::Scope::Global});

    if (search_result.archive_path.empty() ||
        !util::file_exists(search_result.archive_path)) {
        util::info(ezmk::i18n::I18nKey::pkg_update_no_updates, {{"pkg", pkg_name}});
        return;
    }

    std::string repo_version = search_result.version;

    // Version comparison — semantic numeric comparison
    if (util::compare_version(repo_version, installed_version) == 0) {
        util::info(ezmk::i18n::I18nKey::pkg_update_up_to_date,
                   {{"pkg", pkg_name}, {"version", installed_version}});
        return;
    }

    util::info(ezmk::i18n::I18nKey::pkg_update_updating,
               {{"pkg", pkg_name}, {"old", installed_version}, {"new", repo_version}});

    // Use the install flow to handle download, verify, compile, and replace
    std::string sha256_hint;
    if (!search_result.sha256.empty()) {
        sha256_hint = search_result.sha256;
    }

    // Call install with the archive path found in the repo
    // Since install() accepts local paths, we pass the archive_path directly
    install(search_result.archive_path.string(), found_scope, sha256_hint, false);
}

// 0.2.4+
void update_all(const std::vector<cli::Scope>& scopes) {
    // Note: users should run 'ezmk repo update' first to refresh repo indices.
    int updated = 0;
    int up_to_date = 0;
    int failed = 0;

    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        if (!util::file_exists(dir)) continue;

        for (auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;
            std::string pkg_name = entry.path().filename().string();

            // Read installed version
            auto toml = entry.path() / "ezmk.toml";
            if (!util::file_exists(toml)) continue;

            std::string installed_version;
            try {
                auto cfg = config::parse_config(toml);
                installed_version = cfg.project.version;
            } catch (...) {
                util::warn(std::string("failed to parse config for package: ") + pkg_name + " — skipping");
                ++failed;
                continue;
            }

            // Search repos for latest version
            auto result = repo::search_package(pkg_name, {
                cli::Scope::Project, cli::Scope::User, cli::Scope::Global});

            if (result.archive_path.empty() ||
                !util::file_exists(result.archive_path)) {
                ++up_to_date;
                continue;
            }

            // Compare versions
            if (util::compare_version(result.version, installed_version) <= 0) {
                ++up_to_date;
                continue;
            }

            // Update available — install latest
            util::info(ezmk::i18n::I18nKey::pkg_update_updating,
                       {{"pkg", pkg_name},
                        {"old", installed_version},
                        {"new", result.version}});

            std::string sha256_hint;
            if (!result.sha256.empty()) sha256_hint = result.sha256;

            try {
                install(result.archive_path.string(), scope, sha256_hint, false);
                ++updated;
            } catch (...) {
                util::warn(std::string("failed to update package: ") + pkg_name);
                ++failed;
            }
        }
    }

    // Summary
    if (updated > 0 || up_to_date > 0 || failed > 0) {
        std::string summary = std::to_string(updated) + " updated";
        if (up_to_date > 0) summary += ", " + std::to_string(up_to_date) + " already up-to-date";
        if (failed > 0) summary += ", " + std::to_string(failed) + " failed";
        util::info(summary);
    }
}

} // namespace ezmk::pkg
