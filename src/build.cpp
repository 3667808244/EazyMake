#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/util.hpp"

#include <cstdlib>
#include <ctime>
#include <sstream>

namespace ezmk::build {

// ---- helpers ----

static fs::path find_compiler(const config::LanguageInfo& lang) {
#ifdef EZMK_WIN
    return fs::path(lang.compiler + ".exe");
#else
    return fs::path(lang.compiler);
#endif
}

static std::string make_compile_cmd(const fs::path& src,
                                    const fs::path& obj,
                                    const fs::path& depfile,
                                    const config::CompileSection& compile,
                                    const config::LanguageInfo& lang,
                                    const std::vector<fs::path>& extra_includes,
                                    const fs::path& proj_root,
                                    bool use_pic = false) {
    std::ostringstream cmd;
    cmd << lang.compiler << " " << lang.std_flag << " -c ";
    cmd << "\"" << src.string() << "\" -o \"" << obj.string() << "\"";

    for (auto& f : compile.flags) {
        cmd << " " << f;
    }

    if (use_pic) {
        cmd << " -fPIC";
    }

    // Default include: project include/
    auto def_inc = fs::current_path() / "include";
    if (util::file_exists(def_inc)) {
        cmd << " -I\"" << def_inc.string() << "\"";
    }

    for (auto& d : compile.include_dirs) {
        fs::path resolved = d;
        if (resolved.is_relative()) resolved = proj_root / resolved;
        cmd << " -I\"" << resolved.string() << "\"";
    }
    for (auto& d : extra_includes) {
        cmd << " -I\"" << d.string() << "\"";
    }

    cmd << " -MMD -MF \"" << depfile.string() << "\"";

    return cmd.str();
}

static std::string make_link_cmd(const std::vector<fs::path>& objs,
                                 const std::vector<fs::path>& archives,
                                 const fs::path& output,
                                 const config::LinkSection& link,
                                 const config::LanguageInfo& lang,
                                 bool shared = false) {
    std::ostringstream cmd;
    cmd << lang.compiler;

    for (auto& o : objs) {
        cmd << " \"" << o.string() << "\"";
    }
    for (auto& a : archives) {
        cmd << " \"" << a.string() << "\"";
    }

    cmd << " -o \"" << output.string() << "\"";

    if (shared) {
        cmd << " -shared";
    }

    for (auto& f : link.flags) {
        cmd << " " << f;
    }
    for (auto& d : link.link_dirs) {
        cmd << " -L\"" << d << "\"";
    }
    for (auto& t : link.system_targets) {
        cmd << " -l" << t;
    }

    return cmd.str();
}

// ===================================================================
// Build
// ===================================================================

fs::path build_project(const config::EzConfig& cfg, const cli::BuildOptions& opts) {
    // Parse language → compiler + std flag
    auto lang = config::parse_language(cfg.project.language);

    // Verify compiler
    auto gxx = find_compiler(lang);
    auto ver = util::run_command(gxx.string() + " --version 2>&1");
    if (ver.exit_code != 0) {
        util::fatal("compiler not found: " + gxx.string() +
                    "\n  Install g++ (MSYS2: pacman -S mingw-w64-x86_64-gcc)");
    }

    fs::path proj_root = fs::current_path();
    fs::path src_dir = proj_root / "src";
    fs::path temp_dir = proj_root / ".ezmk/temp";
    fs::path build_dir = proj_root / "build";
    fs::path cache_obj_dir = proj_root / ".ezmk/cache/obj";

    // Check src/ exists
    if (!util::file_exists(src_dir)) {
        util::fatal("src/ directory not found. Run 'ezmk project new' to create a project.");
    }

    // Check main.cpp requirement for executable
    if (cfg.project.type == "executable") {
        if (!util::file_exists(src_dir / "main.cpp") &&
            !util::file_exists(src_dir / "main.c")) {
            util::fatal("src/main.cpp is required for executable project type");
        }
    }

    util::info("Building " + cfg.project.name + " (" + cfg.project.type + ", " +
               cfg.project.language + ")...");

    fs::create_directories(temp_dir);
    fs::create_directories(build_dir);
    fs::create_directories(cache_obj_dir);

    // Load cache
    cache::CacheRecord record = cache::load_record();

    // Update / set global compile options signature
    auto cur_sig = cache::compile_options_signature(cfg.compile);
    if (record.compile_options_signature != cur_sig) {
        if (!record.compile_options_signature.empty()) {
            util::info("  Compile options changed, invalidating all caches");
        }
        record.compile_options_signature = cur_sig;
        record.files.clear();
    }

    auto sources = util::list_files(src_dir, {".c", ".cc", ".cpp", ".cxx"});
    if (sources.empty()) {
        util::fatal("no source files found in src/");
    }

    // Need -fPIC for shared libraries
    bool use_pic = (cfg.project.type == "shared");

    // Detect installed packages in .ezmk/pkg/
    std::vector<fs::path> extra_includes;
    std::vector<fs::path> pkg_archives;
    // Collect link options from dependency packages
    std::vector<std::string> pkg_link_flags;
    std::vector<std::string> pkg_link_dirs;
    std::vector<std::string> pkg_system_targets;
    fs::path pkg_dir = proj_root / ".ezmk/pkg";
    if (util::file_exists(pkg_dir)) {
        for (auto& entry : fs::directory_iterator(pkg_dir)) {
            if (!entry.is_directory()) continue;

            // Parse package config for include/link data
            auto pkg_toml = entry.path() / "ezmk.toml";
            if (util::file_exists(pkg_toml)) {
                try {
                    auto pkg_cfg = config::parse_config(pkg_toml);

                    // Default include/
                    auto pkg_include = entry.path() / "include";
                    if (util::file_exists(pkg_include)) {
                        extra_includes.push_back(pkg_include);
                    }

                    // Extra include dirs from package's compile.include_dirs
                    for (auto& d : pkg_cfg.compile.include_dirs) {
                        fs::path resolved = d;
                        if (resolved.is_relative()) resolved = entry.path() / resolved;
                        if (util::file_exists(resolved) &&
                            resolved != pkg_include) {
                            extra_includes.push_back(resolved);
                        }
                    }

                    // Collect link options
                    for (auto& f : pkg_cfg.link.flags)
                        pkg_link_flags.push_back(f);
                    for (auto& d : pkg_cfg.link.link_dirs)
                        pkg_link_dirs.push_back(d);
                    for (auto& t : pkg_cfg.link.system_targets)
                        pkg_system_targets.push_back(t);
                } catch (...) {
                    // Package config parse failure is non-fatal for collection
                }
            } else {
                // No ezmk.toml — at least add default include/
                auto pkg_include = entry.path() / "include";
                if (util::file_exists(pkg_include)) {
                    extra_includes.push_back(pkg_include);
                }
            }

            // Look for compiled .a
            auto pkg_build = entry.path() / "build";
            if (util::file_exists(pkg_build)) {
                for (auto& f : fs::directory_iterator(pkg_build)) {
                    if (f.path().extension() == ".a") {
                        pkg_archives.push_back(f.path());
                    }
                }
            }
        }
    }

    // Compile phase — clean stale temps from previous crashed builds
    {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(temp_dir, ec)) {
            auto& p = e.path();
            if (p.extension() == ".tmp") {
                util::warn("removing stale temp: " + p.string());
                fs::remove(p, ec);
            }
        }
    }

    std::vector<fs::path> objects;
    int cache_hits = 0;
    int cache_misses = 0;

    for (auto& src : sources) {
        auto rel = fs::relative(src, proj_root);
        fs::path obj = temp_dir / rel;
        obj.replace_extension(EZMK_OBJ_SUFFIX);
        fs::path obj_tmp = temp_dir / rel;
        obj_tmp.replace_extension(".tmp" EZMK_OBJ_SUFFIX);
        fs::path dep = temp_dir / rel;
        dep.replace_extension(".d");

        fs::path cache_obj = cache_obj_dir / rel;
        cache_obj.replace_extension(EZMK_OBJ_SUFFIX);

        fs::create_directories(obj.parent_path());
        fs::create_directories(cache_obj.parent_path());

        // Check cache (unless disabled)
        if (!opts.disable_cache) {
            auto cached = cache::check_cache(src, cfg.compile, record);
            if (cached && util::file_exists(*cached)) {
                std::error_code ec;
                // Copy to temp then rename for atomicity
                fs::copy_file(*cached, obj_tmp, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    fs::rename(obj_tmp, obj, ec);
                    if (ec) {
                        util::warn("rename failed for cache hit, falling back to recompile: " +
                                   obj.string() + " (" + ec.message() + ")");
                    }
                }
                if (!ec) {
                    objects.push_back(obj);
                    ++cache_hits;
                    util::info("  [cached] " + rel.string());
                    continue;
                }
                // Clean up on failure
                fs::remove(obj_tmp, ec);
            }
        }

        // Cache miss: compile to temp file
        ++cache_misses;
        util::info("  Compiling " + rel.string());
        std::string cmd = make_compile_cmd(src, obj_tmp, dep, cfg.compile, lang,
                                           extra_includes, proj_root, use_pic);
        auto res = util::run_command(cmd);
        if (res.exit_code != 0) {
            util::error("compilation failed for " + src.string());
            if (!res.err.empty()) util::error(res.err);
            if (!res.out.empty()) util::error(res.out);
            // Remove partial temp file
            std::error_code ec;
            fs::remove(obj_tmp, ec);
            util::fatal("build failed");
        }

        // Compilation succeeded — atomically rename temp to final
        {
            std::error_code ec;
            fs::rename(obj_tmp, obj, ec);
            if (ec) {
                // Fallback: copy + remove
                fs::copy_file(obj_tmp, obj, fs::copy_options::overwrite_existing, ec);
                fs::remove(obj_tmp, ec);
            }
        }
        objects.push_back(obj);

        // Copy compiled .o to cache (atomic: copy to tmp then rename)
        {
            std::error_code ec;
            fs::path cache_tmp = cache_obj;
            cache_tmp += ".tmp";
            fs::copy_file(obj, cache_tmp, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                fs::rename(cache_tmp, cache_obj, ec);
                if (ec) {
                    // Fallback
                    fs::copy_file(obj, cache_obj, fs::copy_options::overwrite_existing, ec);
                }
            }
        }

        // Update cache record
        auto rel_src = rel.generic_string();
        auto& entry = record.files[rel_src];

        // Check if dependency path set changed (include structure change)
        auto new_deps = cache::parse_depfile_and_hash(dep);
        if (!record.files.empty()) {
            auto old_it = record.files.find(rel_src);
            if (old_it != record.files.end() &&
                !cache::same_dependency_paths(old_it->second.dependencies, new_deps)) {
                util::info("    include structure changed for " + rel_src);
            }
        }

        entry.source_hash = crypto::sha256_file(src);
        entry.object_file = fs::relative(cache_obj, proj_root).generic_string();
        entry.compiler = lang.compiler;
        entry.compile_opts = cfg.compile.flags;
        entry.dependencies = std::move(new_deps);
        entry.last_build_time = cache::iso_time();
    }

    // Save updated cache record
    cache::save_record(record);

    if (cache_hits > 0 || cache_misses > 0) {
        std::string summary = "  ";
        if (cache_hits > 0) summary += std::to_string(cache_hits) + " cached, ";
        summary += std::to_string(cache_misses) + " compiled";
        util::info(summary);
    }

    // Merge package link options into project link config
    config::LinkSection merged_link = cfg.link;
    for (auto& f : pkg_link_flags) merged_link.flags.push_back(f);
    for (auto& d : pkg_link_dirs) merged_link.link_dirs.push_back(d);
    for (auto& t : pkg_system_targets) merged_link.system_targets.push_back(t);

    // Link phase — varies by project type
    if (cfg.project.type == "static") {
        // Static library: ar rcs (atomic via tmp)
        fs::path lib = build_dir / ("lib" + cfg.project.name + ".a");
        fs::path lib_tmp = build_dir / ("lib" + cfg.project.name + ".a.tmp");

        // Remove stale tmp
        {
            std::error_code ec;
            fs::remove(lib_tmp, ec);
        }

        util::info("  Archiving " + lib.filename().string());
        std::ostringstream ar_cmd;
        ar_cmd << "ar rcs \"" << lib_tmp.string() << "\"";
        for (auto& o : objects) {
            ar_cmd << " \"" << o.string() << "\"";
        }
        auto ar_res = util::run_command(ar_cmd.str());
        if (ar_res.exit_code != 0) {
            std::error_code ec;
            fs::remove(lib_tmp, ec);
            util::error("archive creation failed");
            if (!ar_res.err.empty()) util::error(ar_res.err);
            util::fatal("build failed");
        }
        {
            std::error_code ec;
            fs::rename(lib_tmp, lib, ec);
        }
        util::info("Build successful: " + lib.string());
        return lib;
    }

    if (cfg.project.type == "shared") {
        std::string lib_name = "lib" + cfg.project.name;
#ifdef EZMK_WIN
        lib_name += ".dll";
#else
        lib_name += ".so";
#endif
        fs::path lib = build_dir / lib_name;
        fs::path lib_tmp = build_dir / (lib_name + ".tmp");

        // Remove stale tmp
        {
            std::error_code ec;
            fs::remove(lib_tmp, ec);
        }

        util::info("  Linking " + lib.filename().string());
        std::string link_cmd = make_link_cmd(objects, pkg_archives, lib_tmp,
                                             merged_link, lang, true);
        auto link_res = util::run_command(link_cmd);
        if (link_res.exit_code != 0) {
            std::error_code ec;
            fs::remove(lib_tmp, ec);
            util::error("link failed");
            if (!link_res.err.empty()) util::error(link_res.err);
            if (!link_res.out.empty()) util::error(link_res.out);
            util::fatal("build failed");
        }
        {
            std::error_code ec;
            fs::rename(lib_tmp, lib, ec);
        }
        util::info("Build successful: " + lib.string());
        return lib;
    }

    // Default: executable
    fs::path exe = build_dir / cfg.project.name;
#ifdef EZMK_WIN
    exe += ".exe";
#endif
    fs::path exe_tmp = build_dir / (cfg.project.name + ".tmp");
#ifdef EZMK_WIN
    exe_tmp += ".exe";
#endif

    // Remove stale tmp
    {
        std::error_code ec;
        fs::remove(exe_tmp, ec);
    }

    util::info("  Linking " + exe.filename().string());
    std::string link_cmd = make_link_cmd(objects, pkg_archives, exe_tmp,
                                         merged_link, lang);
    auto link_res = util::run_command(link_cmd);
    if (link_res.exit_code != 0) {
        std::error_code ec;
        fs::remove(exe_tmp, ec);
        util::error("link failed");
        if (!link_res.err.empty()) util::error(link_res.err);
        if (!link_res.out.empty()) util::error(link_res.out);
        util::fatal("build failed");
    }
    {
        std::error_code ec;
        fs::rename(exe_tmp, exe, ec);
    }

    util::info("Build successful: " + exe.string());
    return exe;
}

} // namespace ezmk::build
