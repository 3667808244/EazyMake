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
    fs::path pkg_dir = proj_root / ".ezmk/pkg";
    if (util::file_exists(pkg_dir)) {
        for (auto& entry : fs::directory_iterator(pkg_dir)) {
            if (!entry.is_directory()) continue;
            auto pkg_include = entry.path() / "include";
            if (util::file_exists(pkg_include)) {
                extra_includes.push_back(pkg_include);
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

    // Compile phase
    std::vector<fs::path> objects;
    int cache_hits = 0;
    int cache_misses = 0;

    for (auto& src : sources) {
        auto rel = fs::relative(src, proj_root);
        fs::path obj = temp_dir / rel;
        obj.replace_extension(EZMK_OBJ_SUFFIX);
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
                fs::copy_file(*cached, obj, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    objects.push_back(obj);
                    ++cache_hits;
                    util::info("  [cached] " + rel.string());
                    continue;
                }
            }
        }

        // Cache miss: compile
        ++cache_misses;
        util::info("  Compiling " + rel.string());
        std::string cmd = make_compile_cmd(src, obj, dep, cfg.compile, lang,
                                           extra_includes, proj_root, use_pic);
        auto res = util::run_command(cmd);
        if (res.exit_code != 0) {
            util::error("compilation failed for " + src.string());
            if (!res.err.empty()) util::error(res.err);
            if (!res.out.empty()) util::error(res.out);
            util::fatal("build failed");
        }
        objects.push_back(obj);

        // Copy compiled .o to cache
        std::error_code ec;
        fs::copy_file(obj, cache_obj, fs::copy_options::overwrite_existing, ec);

        // Update cache record
        auto rel_src = rel.generic_string();
        auto& entry = record.files[rel_src];
        entry.source_hash = crypto::sha256_file(src);
        entry.object_file = fs::relative(cache_obj, proj_root).generic_string();
        entry.compiler = lang.compiler;
        entry.compile_opts = cfg.compile.flags;
        entry.dependencies = cache::parse_depfile_and_hash(dep);
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

    // Link phase — varies by project type
    if (cfg.project.type == "static") {
        // Static library: ar rcs
        fs::path lib = build_dir / ("lib" + cfg.project.name + ".a");

        util::info("  Archiving " + lib.filename().string());
        std::ostringstream ar_cmd;
        ar_cmd << "ar rcs \"" << lib.string() << "\"";
        for (auto& o : objects) {
            ar_cmd << " \"" << o.string() << "\"";
        }
        auto ar_res = util::run_command(ar_cmd.str());
        if (ar_res.exit_code != 0) {
            util::error("archive creation failed");
            if (!ar_res.err.empty()) util::error(ar_res.err);
            util::fatal("build failed");
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

        util::info("  Linking " + lib.filename().string());
        std::string link_cmd = make_link_cmd(objects, pkg_archives, lib,
                                             cfg.link, lang, true);
        auto link_res = util::run_command(link_cmd);
        if (link_res.exit_code != 0) {
            util::error("link failed");
            if (!link_res.err.empty()) util::error(link_res.err);
            if (!link_res.out.empty()) util::error(link_res.out);
            util::fatal("build failed");
        }
        util::info("Build successful: " + lib.string());
        return lib;
    }

    // Default: executable
    fs::path exe = build_dir / cfg.project.name;
#ifdef EZMK_WIN
    exe += ".exe";
#endif

    util::info("  Linking " + exe.filename().string());
    std::string link_cmd = make_link_cmd(objects, pkg_archives, exe,
                                         cfg.link, lang);
    auto link_res = util::run_command(link_cmd);
    if (link_res.exit_code != 0) {
        util::error("link failed");
        if (!link_res.err.empty()) util::error(link_res.err);
        if (!link_res.out.empty()) util::error(link_res.out);
        util::fatal("build failed");
    }

    util::info("Build successful: " + exe.string());
    return exe;
}

} // namespace ezmk::build
