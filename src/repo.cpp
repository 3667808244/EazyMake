#include "ezmk/repo.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include "toml.hpp"

#include <algorithm>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
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
    case cli::Scope::User:
        return util::get_home_dir() / ".local/ezmk/repo/list.toml";
    case cli::Scope::Global:
        return util::get_exe_dir() / "repo/list.toml";
    }
    return {};
}

fs::path cache_dir(cli::Scope scope, std::string_view repo_name) {
    switch (scope) {
    case cli::Scope::Project:
        return fs::current_path() / ".ezmk/repo/.cache" / repo_name;
    case cli::Scope::User:
        return util::get_home_dir() / ".local/ezmk/repo/.cache" / repo_name;
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

// Verify that a local directory is a valid EazyMake repo (has index.toml).
static void validate_local_repo(const fs::path& dir) {
    if (!util::file_exists(dir)) {
        throw std::runtime_error("directory not found: " + dir.string());
    }
    if (!util::file_exists(dir / "index.toml")) {
        throw std::runtime_error("not a valid EazyMake repo (missing index.toml): " + dir.string());
    }
    // Try to parse it
    try {
        auto root = toml::parse_file((dir / "index.toml").string());
        if (!root["repo"].as_table()) {
            throw std::runtime_error("index.toml missing [repo] section");
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
            // Numeric version comparison — splits on '.' and compares each segment.
            // Non-numeric segments (e.g. "beta", "alpha") are treated as 0.
            auto num_cmp = [](std::string_view a, std::string_view b) -> int {
                auto parse_seg = [](std::string_view s) -> unsigned long {
                    try {
                        return std::stoul(std::string(s));
                    } catch (...) {
                        return 0; // non-numeric → 0
                    }
                };
                size_t pa = 0, pb = 0;
                while (pa < a.size() || pb < b.size()) {
                    unsigned long va = 0, vb = 0;
                    if (pa < a.size()) {
                        size_t dot = a.find('.', pa);
                        va = parse_seg(a.substr(pa, dot - pa));
                        pa = (dot == std::string::npos) ? a.size() : dot + 1;
                    }
                    if (pb < b.size()) {
                        size_t dot = b.find('.', pb);
                        vb = parse_seg(b.substr(pb, dot - pb));
                        pb = (dot == std::string::npos) ? b.size() : dot + 1;
                    }
                    if (va != vb) return va > vb ? 1 : -1;
                }
                return 0;
            };
            if (best_file.empty() || num_cmp(ver_str, best_version) > 0) {
                best_version = ver_str;
                best_file = *file;
                auto sha = (*tbl)["sha256"].value<std::string>();
                best_sha256 = sha ? *sha : "";
            }
        }

        return {repo_dir / best_file, best_sha256};
    } catch (...) {
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
        util::info("Cloning " + opts.url + " ...");
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
        util::info("Repository '" + repo_name + "' registered successfully");
    } else {
        // Local directory
        entry.type = "local";
        entry.url = opts.url;

        validate_local_repo(fs::path(opts.url));
        entry.last_update = now_iso();
        util::info("Local repository '" + repo_name + "' registered");
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
                        util::info("Removing cache: " + cd.string());
                        util::remove_all(cd);
                    }
                }

                entries.erase(it);
                save_repo_list(scope, entries);
                util::info("Repository '" + std::string(name) + "' removed");
                return;
            }
        }
    }
    util::error(std::string("repository not found: ") + std::string(name));
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
                    util::info("Re-cloning " + e.url + " ...");
                    if (!util::git_clone(e.url, cd, e.branch)) {
                        util::warn("failed to re-clone '" + e.name + "', skipping");
                        continue;
                    }
                } else {
                    util::info("Updating " + e.name + " ...");
                    if (!util::git_pull(cd, e.branch)) {
                        util::warn("failed to update '" + e.name + "', using cached version");
                        continue;
                    }
                }
                e.last_update = util::git_last_commit_time(cd);
            } else {
                // Local: re-validate
                util::info("Re-reading " + e.url + " ...");
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
            util::error("repository not found: " + name);
        } else {
            util::info("No repositories registered");
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
        std::cout << "Repositories (" << scope_label(scope) << "):\n";

        if (entries.empty()) {
            std::cout << "  (none)\n";
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
        util::info("No repositories registered. Use 'ezmk repo add' to register one.");
    }
}

// ===================================================================
// pkg integration: search for a package in registered repos
// ===================================================================

PkgSearchResult search_package(std::string_view pkg_name,
                               const std::vector<cli::Scope>& scopes) {
    for (auto scope : scopes) {
        auto entries = load_repo_list(scope);
        for (auto& e : entries) {
            fs::path repo_dir;
            if (e.type == "git") {
                repo_dir = cache_dir(scope, e.name);
            } else {
                repo_dir = fs::path(e.url);
            }

            if (!util::file_exists(repo_dir)) continue;

            auto result = read_pkg_from_index(repo_dir, pkg_name);
            if (!result.archive_path.empty() &&
                util::file_exists(result.archive_path)) {
                return result;
            }
        }
    }
    return {};
}

} // namespace ezmk::repo
