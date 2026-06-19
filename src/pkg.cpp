#include "ezmk/pkg.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/config.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/repo.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <deque>
#include <iostream>
#include <map>
#include <random>
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

static bool confirm(std::string_view msg) {
    std::cerr << "[ezmk] " << msg << " [y/N] ";
    std::string line;
    std::getline(std::cin, line);
    return line == "y" || line == "Y" || line == "yes";
}

// Validate a directory is a proper EazyMake package
static void validate_pkg(const fs::path& dir) {
    if (!util::file_exists(dir / "include")) {
        throw std::runtime_error("package missing include/ directory: " + dir.string());
    }
    if (!util::file_exists(dir / "src")) {
        throw std::runtime_error("package missing src/ directory: " + dir.string());
    }
    if (!util::file_exists(dir / "ezmk.toml")) {
        throw std::runtime_error("package missing ezmk.toml: " + dir.string());
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

    std::vector<fs::path> objects;
    int cache_hits = 0;
    int cache_misses = 0;

    for (auto& src : sources) {
        fs::path obj = build_dir / src.filename();
        obj.replace_extension(EZMK_OBJ_SUFFIX);
        fs::path depfile = build_dir / src.filename();
        depfile.replace_extension(".d");

        // Check cache
        auto cached = cache::check_cache(src, cfg.compile, record, pkg_dir);
        if (cached && util::file_exists(obj)) {
            objects.push_back(obj);
            ++cache_hits;
            continue;
        }

        // Cache miss: compile
        ++cache_misses;
        auto lang = config::parse_language(cfg.project.language);
        std::ostringstream cmd;
        cmd << lang.compiler << " " << lang.std_flag << " -c ";
        for (auto& f : cfg.compile.flags) cmd << f << " ";
        cmd << "-I\"" << (pkg_dir / "include").string() << "\" ";
        for (auto& d : cfg.compile.include_dirs) {
            fs::path resolved = d;
            if (resolved.is_relative()) resolved = pkg_dir / resolved;
            cmd << "-I\"" << resolved.string() << "\" ";
        }
        for (auto& inc : dep_includes) {
            cmd << "-I\"" << inc.string() << "\" ";
        }
        cmd << "-MMD -MF \"" << depfile.string() << "\" ";
        cmd << "\"" << src.string() << "\" -o \"" << obj.string() << "\"";

        auto res = util::run_command(cmd.str());
        if (res.exit_code != 0) {
            util::error("compilation failed for " + src.string());
            util::error(res.err);
            throw std::runtime_error("failed to compile package: " + name);
        }
        objects.push_back(obj);

        // Update cache entry
        auto rel_src = fs::relative(src, pkg_dir).generic_string();
        auto& entry = record.files[rel_src];
        entry.source_hash = crypto::sha256_file(src);
        entry.object_file = fs::relative(obj, pkg_dir).generic_string();
        entry.compiler = "g++";
        entry.compile_opts = cfg.compile.flags;
        entry.dependencies = cache::parse_depfile_and_hash(depfile);
        // Normalize to pkg_dir-relative paths so cache survives package relocation
        for (auto& dep : entry.dependencies) {
            fs::path dp(dep.path);
            if (dp.is_absolute()) {
                auto rel = fs::relative(dp, pkg_dir);
                if (!rel.empty() && rel.string().find("..") == std::string::npos) {
                    dep.path = rel.generic_string();
                }
            }
        }
        entry.last_build_time = cache::iso_time();
    }

    // Save cache
    cache::save_record(record, cache_path);

    if (cache_hits > 0 || cache_misses > 0) {
        std::string summary = "    ";
        if (cache_hits > 0) summary += std::to_string(cache_hits) + " cached, ";
        summary += std::to_string(cache_misses) + " compiled";
        util::info(summary);
    }

    // Archive (skip if nothing changed and .a exists)
    if (cache_misses == 0 && util::file_exists(lib)) {
        return lib;
    }

    // Collect all objects (both cached and newly compiled)
    std::vector<fs::path> all_objs;
    for (auto& src : sources) {
        fs::path obj = build_dir / src.filename();
        obj.replace_extension(EZMK_OBJ_SUFFIX);
        all_objs.push_back(obj);
    }

    std::ostringstream ar_cmd;
    ar_cmd << "ar rcs \"" << lib.string() << "\"";
    for (auto& o : all_objs) {
        ar_cmd << " \"" << o.string() << "\"";
    }

    auto ar_res = util::run_command(ar_cmd.str());
    if (ar_res.exit_code != 0) {
        util::error(ar_res.err);
        throw std::runtime_error("failed to create archive for: " + name);
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
        throw std::runtime_error("circular dependency detected");
    }

    return sorted;
}

// ===================================================================
// Install
// ===================================================================

void install(const std::string& pkg_file, cli::Scope scope) {
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

        util::info("Downloading " + url + " ...");
        util::download(url, archive_path);
    } else {
        archive_path = input;
        if (!util::file_exists(archive_path)) {
            // Not a local file or URL — try searching registered repos
            util::info("Searching registered repos for '" + pkg_file + "'...");
            archive_path = repo::search_package(pkg_file, {
                cli::Scope::Project, cli::Scope::User, cli::Scope::Global});
            if (archive_path.empty() || !util::file_exists(archive_path)) {
                util::fatal("package not found: " + pkg_file);
            }
            util::info("Found in repo: " + archive_path.string());
        }
    }

    // Safety: global install confirmation
    if (scope == cli::Scope::Global) {
        if (!confirm("Global install requires confirmation. Continue?")) {
            util::info("Install cancelled");
            return;
        }
    }

    // Extract to temp staging area (use random suffix to avoid collisions)
    static std::mt19937 rng(static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count()));
    fs::path stage = fs::temp_directory_path() / ("ezmk_pkg_" + std::to_string(rng()));
    fs::create_directories(stage);

    try {
        util::info("Extracting...");
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

        // Check for existing install
        fs::path install_path = dest_dir / pkg_name;
        if (util::file_exists(install_path)) {
            if (!confirm("Package '" + pkg_name + "' already exists at " + install_path.string() + ". Overwrite?")) {
                util::info("Install cancelled");
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
                            "dependency not found: '" + cur + "'. "
                            "Install it first with: ezmk install <" + cur + ">");
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
                                "dependency not found: '" + dep + "'. "
                                "Install it first with: ezmk install <" + dep + ">");
                        }
                    }
                }
            }
        }

        // Dependency ordering + compilation
        util::info("Resolving dependencies...");
        auto order = resolve_dependency_order(all_pkgs);

        // Build name → dir map for resolving dependency include paths
        std::map<std::string, fs::path> name_to_dir;
        for (auto& d : all_pkgs) {
            name_to_dir[config::parse_config(d / "ezmk.toml").project.name] = d;
        }

        for (auto& dir : order) {
            auto cfg = config::parse_config(dir / "ezmk.toml");
            std::vector<fs::path> dep_includes;
            for (auto& dep : cfg.depends.libs) {
                auto it = name_to_dir.find(dep);
                if (it != name_to_dir.end()) {
                    dep_includes.push_back(it->second / "include");
                }
            }
            util::info("  Compiling " + cfg.project.name + "...");
            compile_package(dir, dep_includes);
        }

        // Copy to install directory
        fs::create_directories(dest_dir);
        util::info("Installing to " + install_path.string());
        util::copy_recursive(pkg_root, install_path);

        util::info("Package '" + pkg_name + "' installed successfully");

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
            util::info("Removing " + pkg_path.string());
            util::remove_all(pkg_path);
            return;
        }
    }
    util::error("package not found: " + pkg_name);
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
    const char* scope_name(cli::Scope s) {
        switch (s) {
        case cli::Scope::Project: return "project";
        case cli::Scope::User:    return "user";
        case cli::Scope::Global:  return "global";
        }
        return "unknown";
    }

    std::string format_time(const fs::path& p) {
        std::error_code ec;
        auto ftime = fs::last_write_time(p, ec);
        if (ec) return "(unknown)";

        // Convert to system_clock time
        auto sctp = std::chrono::time_point_cast<
            std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now()
            + std::chrono::system_clock::now());
        auto tt = std::chrono::system_clock::to_time_t(sctp);
        auto* tm = std::localtime(&tt);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        return buf;
    }
}

void info(const std::string& pkg_name, const std::vector<cli::Scope>& scopes) {
    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (util::file_exists(pkg_path)) {
            auto cfg = config::parse_config(pkg_path / "ezmk.toml");
            std::cout << "Package: " << cfg.project.name << "\n";
            std::cout << "  Version: " << cfg.project.version << "\n";
            std::cout << "  Type: " << cfg.project.type << "\n";
            std::cout << "  Language: " << cfg.project.language << "\n";
            std::cout << "  Scope: " << scope_name(scope) << "\n";
            std::cout << "  Location: " << pkg_path.string() << "\n";
            std::cout << "  Installed: " << format_time(pkg_path) << "\n";
            std::cout << "  Compile flags:";
            for (auto& f : cfg.compile.flags) std::cout << " " << f;
            if (cfg.compile.flags.empty()) std::cout << " (none)";
            std::cout << "\n";
            std::cout << "  Include dirs:";
            for (auto& d : cfg.compile.include_dirs) std::cout << " " << d;
            if (cfg.compile.include_dirs.empty()) std::cout << " (none)";
            std::cout << "\n";
            std::cout << "  Dependencies:";
            if (cfg.depends.libs.empty()) std::cout << " (none)";
            for (auto& d : cfg.depends.libs) std::cout << " " << d;
            std::cout << "\n";

            // Show built artifacts
            fs::path build_dir = pkg_path / "build";
            if (util::file_exists(build_dir)) {
                std::cout << "  Artifacts:";
                bool found = false;
                for (auto& f : fs::directory_iterator(build_dir)) {
                    auto ext = f.path().extension().string();
                    if (ext == ".a" || ext == ".dll" || ext == ".so") {
                        std::cout << " " << f.path().filename().string();
                        found = true;
                    }
                }
                if (!found) std::cout << " (none)";
                std::cout << "\n";
            }
            return;
        }
    }
    util::error("package not found: " + pkg_name);
}

} // namespace ezmk::pkg
