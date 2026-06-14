#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/util.hpp"

#include <cstdlib>
#include <ctime>
#include <sstream>

namespace ezmk::build {

// ---- helpers ----

static fs::path g_gxx;

static const fs::path& find_compiler() {
    if (!g_gxx.empty()) return g_gxx;
#ifdef EZMK_WIN
    g_gxx = "g++.exe";
#else
    g_gxx = "g++";
#endif
    return g_gxx;
}

static std::string make_compile_cmd(const fs::path& src,
                                    const fs::path& obj,
                                    const fs::path& depfile,
                                    const config::CompileSection& compile,
                                    const std::vector<fs::path>& extra_includes,
                                    const fs::path& proj_root) {
    std::ostringstream cmd;
    cmd << find_compiler() << " -std=c++17 -c ";
    cmd << "\"" << src.string() << "\" -o \"" << obj.string() << "\"";

    for (auto& f : compile.flags) {
        cmd << " " << f;
    }

    cmd << " -I\"" << (fs::current_path() / "include").string() << "\"";
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
                                 const config::LinkSection& link) {
    std::ostringstream cmd;
    cmd << find_compiler();

    for (auto& o : objs) {
        cmd << " \"" << o.string() << "\"";
    }
    for (auto& a : archives) {
        cmd << " \"" << a.string() << "\"";
    }

    cmd << " -o \"" << output.string() << "\"";

    for (auto& f : link.flags) {
        cmd << " " << f;
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
    // Verify compiler
    auto& gxx = find_compiler();
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

    if (!util::file_exists(src_dir)) {
        util::fatal("src/ directory not found. Run 'ezmk new' to create a project.");
    }

    util::info("Building " + cfg.project.name + "...");

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
        record.files.clear(); // invalidate all entries
    }

    auto sources = util::list_files(src_dir, {".c", ".cc", ".cpp", ".cxx"});
    if (sources.empty()) {
        util::fatal("no source files found in src/");
    }

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
                // Cache hit: copy cached .o to temp
                std::error_code ec;
                fs::copy_file(*cached, obj, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    objects.push_back(obj);
                    ++cache_hits;
                    util::info("  [cached] " + rel.string());
                    continue;
                }
                // If copy fails, fall through to compile
            }
        }

        // Cache miss: compile
        ++cache_misses;
        util::info("  Compiling " + rel.string());
        std::string cmd = make_compile_cmd(src, obj, dep, cfg.compile, extra_includes, proj_root);
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
        entry.source_hash = util::sha256_file(src);
        entry.object_file = fs::relative(cache_obj, proj_root).generic_string();
        entry.compiler = gxx.string();
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

    // Link phase
    fs::path exe = build_dir / cfg.project.name;
#ifdef EZMK_WIN
    exe += ".exe";
#endif

    util::info("  Linking " + exe.filename().string());
    std::string link_cmd = make_link_cmd(objects, pkg_archives, exe, cfg.link);
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
