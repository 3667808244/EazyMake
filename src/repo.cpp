#include "ezmk/repo.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include "toml.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ezmk::repo {

// ===================================================================
// Path resolution
// ===================================================================

fs::path list_toml_path(cli::Scope scope) {
    switch (scope) {
    case cli::Scope::Project:
        return fs::current_path() / ".ezmk/repo/list.toml";
    case cli::Scope::User: {
#ifdef EZMK_WIN
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (appdata) return fs::path(appdata) / "ezmk/repo/list.toml";
        return util::get_home_dir() / "AppData/Local/ezmk/repo/list.toml";
#else
        return util::get_home_dir() / ".local/ezmk/repo/list.toml";
#endif
    }
    case cli::Scope::Global:
        return util::get_exe_dir() / "repo/list.toml";
    }
    return {};
}

fs::path cache_dir(cli::Scope scope, std::string_view repo_name) {
    switch (scope) {
    case cli::Scope::Project:
        return fs::current_path() / ".ezmk/repo/.cache" / repo_name;
    case cli::Scope::User: {
#ifdef EZMK_WIN
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (appdata) return fs::path(appdata) / "ezmk/repo/.cache" / repo_name;
        return util::get_home_dir() / "AppData/Local/ezmk/repo/.cache" / repo_name;
#else
        return util::get_home_dir() / ".local/ezmk/repo/.cache" / repo_name;
#endif
    }
    case cli::Scope::Global:
        return util::get_exe_dir() / "repo/.cache" / repo_name;
    }
    return {};
}

// ===================================================================
// list.toml read/write
// ===================================================================

std::vector<RepoEntry> load_repo_list(cli::Scope scope) {
    std::vector<RepoEntry> entries;
    auto path = list_toml_path(scope);
    if (!util::file_exists(path)) return entries;

    try {
        auto root = toml::parse_file(path.string());
        auto arr = root["repos"].as_array();
        if (!arr) return entries;

        for (size_t i = 0; i < arr->size(); ++i) {
            auto tbl = (*arr)[i].as_table();
            if (!tbl) continue;

            RepoEntry e;
            if (auto v = (*tbl)["name"].value<std::string>())  e.name = *v;
            if (auto v = (*tbl)["url"].value<std::string>())   e.url = *v;
            if (auto v = (*tbl)["type"].value<std::string>())  e.type = *v;
            if (auto v = (*tbl)["branch"].value<std::string>()) e.branch = *v;
            if (auto v = (*tbl)["last_update"].value<std::string>()) e.last_update = *v;

            if (e.type.empty()) e.type = "git";
            if (e.branch.empty()) e.branch = "main";

            if (!e.name.empty() && !e.url.empty()) {
                entries.push_back(std::move(e));
            }
        }
    } catch (const std::exception& e) {
        util::warn(std::string("failed to parse repo list: ") + e.what());
    }
    return entries;
}

void save_repo_list(cli::Scope scope, const std::vector<RepoEntry>& entries) {
    auto path = list_toml_path(scope);
    fs::create_directories(path.parent_path());

    // Escape special characters in TOML basic strings
    auto esc = [](const std::string& s) -> std::string {
        std::string r;
        for (char c : s) {
            if (c == '\\') r += "\\\\";
            else if (c == '"') r += "\\\"";
            else if (c == '\n') r += "\\n";
            else r += c;
        }
        return r;
    };

    std::ostringstream out;
    for (auto& e : entries) {
        out << "[[repos]]\n";
        out << "name = \"" << esc(e.name) << "\"\n";
        out << "url = \"" << esc(e.url) << "\"\n";
        out << "type = \"" << esc(e.type) << "\"\n";
        if (e.type == "git") {
            out << "branch = \"" << esc(e.branch) << "\"\n";
        }
        out << "last_update = \"" << esc(e.last_update) << "\"\n";
        out << "\n";
    }

    util::file_write(path, out.str());
}

// ===================================================================
// Helpers
// ===================================================================

// Extract a repo name from a git URL (e.g. "git@github.com:user/repo.git" → "repo")
static std::string name_from_url(std::string_view url) {
    auto s = std::string(url);
    // Strip trailing .git
    if (s.size() > 4 && s.substr(s.size() - 4) == ".git") {
        s = s.substr(0, s.size() - 4);
    }
    // Take last component (after last '/' or ':')
    auto pos_slash = s.rfind('/');
    auto pos_colon = s.rfind(':');
    size_t pos = std::string::npos;
    if (pos_slash != std::string::npos && pos_colon != std::string::npos) {
        pos = std::max(pos_slash, pos_colon);
    } else if (pos_slash != std::string::npos) {
        pos = pos_slash;
    } else {
        pos = pos_colon;
    }
    if (pos != std::string::npos && pos + 1 < s.size()) {
        return s.substr(pos + 1);
    }
    return s;
}

// Guess if a string is a git URL (not a local filesystem path).
static bool is_git_url(std::string_view s) {
    // git@... style (SSH)
    if (s.substr(0, 4) == "git@") return true;
    // https://... or http://...
    if (s.find("://") != std::string::npos) return true;
    // Windows drive letter (e.g., C:/...) or absolute path → local
    if (s.size() >= 2 && s[1] == ':') return false;
    if (s.size() >= 1 && (s[0] == '/' || s[0] == '\\')) return false;
    // If it looks like a URL (contains dots in host), treat as git
    if (s.find('.') != std::string::npos && s.find('/') != std::string::npos) return true;
    return false;
}

// Verify that a local directory is a valid EazyMake repo.
// Enhanced in 0.2.5: validates each package's file existence and sha256 format.
static void validate_local_repo(const fs::path& dir) {
    if (!util::file_exists(dir)) {
        throw std::runtime_error("directory not found: " + dir.string());
    }
    auto index_path = dir / "index.toml";
    if (!util::file_exists(index_path)) {
        throw std::runtime_error("not a valid EazyMake repo (missing index.toml): " + dir.string());
    }
    // Try to parse it
    try {
        auto root = toml::parse_file(index_path.string());
        if (!root["repo"].as_table()) {
            throw std::runtime_error("index.toml missing [repo] section");
        }
        // 0.2.5+: validate each package entry
        auto pkgs = root["packages"].as_array();
        if (pkgs) {
            for (size_t i = 0; i < pkgs->size(); ++i) {
                auto tbl = (*pkgs)[i].as_table();
                if (!tbl) continue;
                // Validate file existence
                auto file = (*tbl)["file"].value<std::string>();
                if (file && !file->empty()) {
                    auto full_path = dir / *file;
                    if (!util::file_exists(full_path)) {
                        throw std::runtime_error(
                            ezmk::i18n::fmt(ezmk::i18n::I18nKey::repo_validate_missing_file,
                                            {{"file", full_path.string()}}));
                    }
                }
                // Validate sha256 format (non-empty must be 64-char hex)
                auto sha = (*tbl)["sha256"].value<std::string>();
                if (sha && !sha->empty()) {
                    if (sha->size() != 64) {
                        auto name = (*tbl)["name"].value<std::string>();
                        throw std::runtime_error(
                            ezmk::i18n::fmt(ezmk::i18n::I18nKey::repo_validate_bad_sha256,
                                            {{"name", name ? *name : "?"},
                                             {"hash", *sha}}));
                    }
                    for (char c : *sha) {
                        if (!std::isxdigit(static_cast<unsigned char>(c))) {
                            auto name = (*tbl)["name"].value<std::string>();
                            throw std::runtime_error(
                                ezmk::i18n::fmt(ezmk::i18n::I18nKey::repo_validate_bad_sha256,
                                                {{"name", name ? *name : "?"},
                                                 {"hash", *sha}}));
                        }
                    }
                }
            }
        }
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("invalid index.toml: ") + e.what());
    }
}

// Read a package's file path and sha256 from a repo's index.toml.
// Returns {file_path, sha256} where sha256 may be empty if not provided.
static PkgSearchResult read_pkg_from_index(const fs::path& repo_dir,
                                            std::string_view pkg_name) {
    auto index_path = repo_dir / "index.toml";
    if (!util::file_exists(index_path)) return {};

    try {
        auto root = toml::parse_file(index_path.string());
        auto pkgs = root["packages"].as_array();
        if (!pkgs) return {};

        std::string best_file;
        std::string best_version;
        std::string best_sha256;

        for (size_t i = 0; i < pkgs->size(); ++i) {
            auto tbl = (*pkgs)[i].as_table();
            if (!tbl) continue;

            auto name = (*tbl)["name"].value<std::string>();
            if (!name || *name != pkg_name) continue;

            auto ver = (*tbl)["version"].value<std::string>();
            auto file = (*tbl)["file"].value<std::string>();
            if (!file) continue;

            std::string ver_str = ver ? *ver : "0.0.0";
            if (best_file.empty() || util::compare_version(ver_str, best_version) > 0) {
                best_version = ver_str;
                best_file = *file;
                auto sha = (*tbl)["sha256"].value<std::string>();
                best_sha256 = sha ? *sha : "";
            }
        }

        if (best_file.empty()) return {};
        PkgSearchResult result;
        result.archive_path = repo_dir / best_file;
        result.sha256 = best_sha256;
        result.version = best_version;
        result.repo_name = repo_dir.filename().string();
        return result;
    } catch (const std::exception& e) {
        util::warn(std::string("failed to parse index.toml for repo '") +
                   repo_dir.filename().string() + "': " + e.what());
        return {};
    } catch (...) {
        util::warn(std::string("failed to parse index.toml for repo '") +
                   repo_dir.filename().string() + "' — unknown error");
        return {};
    }
}

// Current time as ISO 8601 string (simple version).
static std::string now_iso() {
    auto t = std::time(nullptr);
    auto* tm = std::localtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return buf;
}

// ===================================================================
// add
// ===================================================================

void add(const cli::RepoOptions& opts) {
    auto scope = opts.scopes[0]; // add only supports single scope

    // Determine repo name
    std::string repo_name = opts.name;
    if (repo_name.empty()) {
        repo_name = name_from_url(opts.url);
    }
    if (repo_name.empty()) {
        throw std::runtime_error("could not determine repo name; use --name");
    }

    // Load existing list
    auto entries = load_repo_list(scope);
    for (auto& e : entries) {
        if (e.name == repo_name) {
            throw std::runtime_error(
                "repo '" + repo_name + "' already registered. "
                "Use 'ezmk repo remove " + repo_name + "' first.");
        }
    }

    RepoEntry entry;
    entry.name = repo_name;

    if (is_git_url(opts.url)) {
        entry.type = "git";
        entry.url = opts.url;
        entry.branch = opts.branch;

        // Check git availability
        if (!util::git_available()) {
            throw std::runtime_error(
                "git is not available. Install git and ensure it is in PATH.");
        }

        // Clone
        auto dest = cache_dir(scope, repo_name);
        util::info(ezmk::i18n::I18nKey::cloning, {{"url", opts.url}});
        if (!util::git_clone(opts.url, dest, opts.branch)) {
            util::remove_all(dest);
            throw std::runtime_error("failed to clone repository");
        }

        // Verify index.toml in cloned repo
        try {
            validate_local_repo(dest);
        } catch (...) {
            util::remove_all(dest);
            throw;
        }

        entry.last_update = util::git_last_commit_time(dest);
        util::info(ezmk::i18n::I18nKey::repo_added, {{"name", repo_name}});
    } else {
        // Local directory
        entry.type = "local";
        entry.url = opts.url;

        validate_local_repo(fs::path(opts.url));
        entry.last_update = now_iso();
        util::info(ezmk::i18n::I18nKey::repo_added, {{"name", repo_name}});
    }

    entries.push_back(std::move(entry));
    save_repo_list(scope, entries);
}

// ===================================================================
// remove
// ===================================================================

void remove(std::string_view name, const std::vector<cli::Scope>& scopes) {
    for (auto scope : scopes) {
        auto entries = load_repo_list(scope);
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->name == name) {
                // Delete cache directory for git repos
                if (it->type == "git") {
                    auto cd = cache_dir(scope, name);
                    if (util::file_exists(cd)) {
                        util::info(ezmk::i18n::I18nKey::removing_cache,
                                   {{"path", cd.string()}});
                        util::remove_all(cd);
                    }
                }

                entries.erase(it);
                save_repo_list(scope, entries);
                util::info(ezmk::i18n::I18nKey::repo_removed, {{"name", std::string(name)}});
                return;
            }
        }
    }
    util::error(ezmk::i18n::I18nKey::repo_not_found, {{"name", std::string(name)}});
}

// ===================================================================
// update
// ===================================================================

void update(const std::string& name, const std::vector<cli::Scope>& scopes) {
    bool updated_any = false;

    for (auto scope : scopes) {
        auto entries = load_repo_list(scope);

        for (auto& e : entries) {
            if (!name.empty() && e.name != name) continue;
            updated_any = true;

            if (e.type == "git") {
                auto cd = cache_dir(scope, e.name);
                if (!util::file_exists(cd)) {
                    // Cache missing — re-clone
                    util::info(ezmk::i18n::I18nKey::re_cloning, {{"url", e.url}});
                    if (!util::git_clone(e.url, cd, e.branch)) {
                        util::warn("failed to re-clone '" + e.name + "', skipping");
                        continue;
                    }
                } else {
                    util::info(ezmk::i18n::I18nKey::pulling, {{"name", e.name}});
                    if (!util::git_pull(cd, e.branch)) {
                        util::warn("failed to update '" + e.name + "', using cached version");
                        continue;
                    }
                }
                e.last_update = util::git_last_commit_time(cd);
            } else {
                // Local: re-validate
                util::info(ezmk::i18n::I18nKey::re_reading, {{"url", e.url}});
                try {
                    validate_local_repo(fs::path(e.url));
                } catch (const std::exception& ex) {
                    util::warn(std::string("local repo '") + e.name + "': " + ex.what());
                    continue;
                }
                e.last_update = now_iso();
            }
        }

        save_repo_list(scope, entries);
    }

    if (!updated_any) {
        if (!name.empty()) {
            util::error(ezmk::i18n::I18nKey::repo_not_found, {{"name", name}});
        } else {
            util::info(ezmk::i18n::I18nKey::no_repos);
        }
    }
}

// ===================================================================
// list
// ===================================================================

namespace {

const char* scope_label(cli::Scope s) {
    switch (s) {
    case cli::Scope::Project: return "project scope";
    case cli::Scope::User:    return "user scope";
    case cli::Scope::Global:  return "global scope";
    }
    return "unknown";
}

} // anonymous namespace

void list(const std::vector<cli::Scope>& scopes) {
    bool found_any = false;

    for (auto scope : scopes) {
        auto entries = load_repo_list(scope);
        std::cout << ezmk::i18n::fmt(ezmk::i18n::I18nKey::repo_list_title,
                                      {{"scope", scope_label(scope)}}) << "\n";

        if (entries.empty()) {
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_list_none) << "\n";
        } else {
            for (auto& e : entries) {
                std::cout << "  " << std::left << std::setw(16) << e.name
                          << " " << std::setw(48) << e.url;
                if (e.type == "git") {
                    std::cout << " (" << e.branch << ")";
                } else {
                    std::cout << " (local)";
                }
                if (!e.last_update.empty()) {
                    std::cout << "  " << e.last_update;
                }
                std::cout << "\n";
            }
            found_any = true;
        }
        std::cout << "\n";
    }

    if (!found_any) {
        util::info(ezmk::i18n::I18nKey::no_repos);
    }
}

// ===================================================================
// info — show detailed info for a single registered repo (0.2.5+)
// ===================================================================

void info(std::string_view name, const std::vector<cli::Scope>& scopes) {
    for (auto scope : scopes) {
        auto entries = load_repo_list(scope);
        for (auto& e : entries) {
            if (e.name != name) continue;

            fs::path index_path;
            fs::path cache_path;
            if (e.type == "git") {
                cache_path = cache_dir(scope, e.name);
                index_path = cache_path / "index.toml";
            } else {
                cache_path = fs::path(e.url);
                index_path = cache_path / "index.toml";
            }

            // Print repo header
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_name) << ": " << name << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_scope) << ": "
                      << scope_label(scope) << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_url) << ": " << e.url << "\n";
            std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_type) << ": " << e.type << "\n";
            if (e.type == "git") {
                std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_branch) << ": "
                          << e.branch << "\n";
            }
            if (!e.last_update.empty()) {
                std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_updated) << ": "
                          << e.last_update << "\n";
            }
            if (e.type == "git") {
                std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_cache) << ": "
                          << cache_path.string() << "\n";
            }

            // Parse index.toml for package stats
            if (util::file_exists(index_path)) {
                try {
                    auto root = toml::parse_file(index_path.string());
                    auto pkgs = root["packages"].as_array();
                    if (pkgs) {
                        std::map<std::string, std::vector<std::string>> pkg_versions;
                        for (size_t i = 0; i < pkgs->size(); ++i) {
                            auto tbl = (*pkgs)[i].as_table();
                            if (!tbl) continue;
                            auto pkg_name = (*tbl)["name"].value<std::string>();
                            auto ver = (*tbl)["version"].value<std::string>();
                            if (pkg_name) {
                                pkg_versions[*pkg_name].push_back(ver ? *ver : "0.0.0");
                            }
                        }
                        std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_packages) << ": "
                                  << pkg_versions.size() << "\n";
                        for (auto& [pkg, vers] : pkg_versions) {
                            // Sort versions descending
                            std::sort(vers.begin(), vers.end(),
                                      [](const std::string& a, const std::string& b) {
                                          return util::compare_version(a, b) > 0;
                                      });
                            std::string version_list;
                            for (size_t i = 0; i < vers.size(); ++i) {
                                if (i > 0) version_list += ", ";
                                version_list += vers[i];
                            }
                            std::cout << ezmk::i18n::fmt(ezmk::i18n::I18nKey::repo_info_version_list,
                                                          {{"name", pkg},
                                                           {"versions", version_list}})
                                      << "\n";
                        }
                    } else {
                        std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_packages) << ": 0\n";
                    }
                } catch (const std::exception& e) {
                    std::cout << "  (parse error: " << e.what() << ")\n";
                }
            } else {
                std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::repo_info_packages)
                          << ": (index.toml not found)\n";
            }
            return;
        }
    }
    // Not found
    util::error(ezmk::i18n::I18nKey::repo_not_found, {{"name", std::string(name)}});
}

// ===================================================================
// pkg integration: search for a package in registered repos
// 0.2.5+: cross-repo highest-version selection
// ===================================================================

PkgSearchResult search_package(std::string_view pkg_name,
                               const std::vector<cli::Scope>& scopes) {
    // 0.2.5+: collect all matches across all repos, pick highest version.
    struct Match {
        std::string version;
        fs::path archive_path;
        std::string sha256;
        std::string repo_name;
        int scope_index; // 0=project, 1=user, 2=global
    };
    std::vector<Match> matches;
    int repos_searched = 0;

    for (size_t si = 0; si < scopes.size(); ++si) {
        auto scope = scopes[si];
        auto entries = load_repo_list(scope);
        for (auto& e : entries) {
            fs::path repo_dir;
            if (e.type == "git") {
                repo_dir = cache_dir(scope, e.name);
            } else {
                repo_dir = fs::path(e.url);
            }

            if (!util::file_exists(repo_dir)) continue;
            repos_searched++;

            auto result = read_pkg_from_index(repo_dir, pkg_name);
            if (!result.archive_path.empty() &&
                util::file_exists(result.archive_path)) {
                matches.push_back({result.version, result.archive_path,
                                   result.sha256, result.repo_name,
                                   static_cast<int>(si)});
            }
        }
    }

    if (matches.empty()) return {};

    // Find the highest version; tie-break by scope priority
    const Match* best = &matches[0];
    for (size_t i = 1; i < matches.size(); ++i) {
        int cmp = util::compare_version(matches[i].version, best->version);
        if (cmp > 0 || (cmp == 0 && matches[i].scope_index < best->scope_index)) {
            best = &matches[i];
        }
    }

    if (matches.size() > 1 || repos_searched > 1) {
        util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::repo_search_resolved,
                   {{"pkg", std::string(pkg_name)},
                    {"version", best->version},
                    {"repo", best->repo_name},
                    {"count", std::to_string(repos_searched)}}));
    }

    return {best->archive_path, best->sha256, best->version, best->repo_name};
}

// 0.9.6+ — Version-constrained variant.
PkgSearchResult search_package(std::string_view pkg_name,
                               const std::vector<cli::Scope>& scopes,
                               const config::VersionConstraint& constraint) {
    // Unconstrained? Delegate to the base overload.
    if (constraint.op == config::VersionConstraint::None) {
        return search_package(pkg_name, scopes);
    }

    struct Match {
        std::string version;
        fs::path archive_path;
        std::string sha256;
        std::string repo_name;
        int scope_index;
    };
    std::vector<Match> matches;
    int repos_searched = 0;

    for (size_t si = 0; si < scopes.size(); ++si) {
        auto scope = scopes[si];
        auto entries = load_repo_list(scope);
        for (auto& e : entries) {
            fs::path repo_dir;
            if (e.type == "git") {
                repo_dir = cache_dir(scope, e.name);
            } else {
                repo_dir = fs::path(e.url);
            }

            if (!util::file_exists(repo_dir)) continue;
            repos_searched++;

            auto result = read_pkg_from_index(repo_dir, pkg_name);
            if (!result.archive_path.empty() &&
                util::file_exists(result.archive_path)) {
                matches.push_back({result.version, result.archive_path,
                                   result.sha256, result.repo_name,
                                   static_cast<int>(si)});
            }
        }
    }

    // Filter by version constraint
    std::vector<Match> filtered;
    for (auto& m : matches) {
        // Reuse the same logic as pkg.cpp's satisfies_constraint
        int cmp = util::compare_version(m.version, constraint.version);
        bool ok = false;
        switch (constraint.op) {
        case config::VersionConstraint::Exact:
            ok = (cmp == 0); break;
        case config::VersionConstraint::Gte:
            ok = (cmp >= 0); break;
        case config::VersionConstraint::Gt:
            ok = (cmp > 0); break;
        case config::VersionConstraint::Compatible: {
            if (cmp >= 0) {
                auto dot = constraint.version.find('.');
                unsigned long major = dot == std::string::npos
                    ? std::stoul(std::string(constraint.version))
                    : std::stoul(std::string(constraint.version.substr(0, dot)));
                ok = util::compare_version(m.version,
                       std::to_string(major + 1) + ".0.0") < 0;
            }
            break;
        }
        case config::VersionConstraint::Approx: {
            if (cmp >= 0) {
                auto dot1 = constraint.version.find('.');
                if (dot1 == std::string::npos) { ok = true; break; }
                auto dot2 = constraint.version.find('.', dot1 + 1);
                unsigned long major = std::stoul(
                    std::string(constraint.version.substr(0, dot1)));
                unsigned long minor = std::stoul(
                    std::string(constraint.version.substr(dot1 + 1,
                        dot2 - dot1 - 1)));
                ok = util::compare_version(m.version,
                       std::to_string(major) + "." + std::to_string(minor + 1) + ".0") < 0;
            }
            break;
        }
        default: ok = true; break;
        }
        if (ok) filtered.push_back(m);
    }

    if (filtered.empty()) {
        // Build list of available versions for error message
        std::string available;
        for (auto& m : matches) {
            if (!available.empty()) available += ", ";
            available += m.version;
        }
        util::error(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_constraint_unsatisfied,
                    {{"pkg", std::string(pkg_name)},
                     {"constraint", std::string(constraint.version)},
                     {"available", available}}));
        return {};
    }

    // Find the highest version among filtered; tie-break by scope priority
    const Match* best = &filtered[0];
    for (size_t i = 1; i < filtered.size(); ++i) {
        int cmp = util::compare_version(filtered[i].version, best->version);
        if (cmp > 0 || (cmp == 0 && filtered[i].scope_index < best->scope_index)) {
            best = &filtered[i];
        }
    }

    if (filtered.size() > 1 || repos_searched > 1) {
        util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::repo_search_resolved,
                   {{"pkg", std::string(pkg_name)},
                    {"version", best->version},
                    {"repo", best->repo_name},
                    {"count", std::to_string(repos_searched)}}));
    }

    return {best->archive_path, best->sha256, best->version, best->repo_name};
}

} // namespace ezmk::repo
