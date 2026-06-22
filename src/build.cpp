#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/util.hpp"

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
        // Platform-specific install instructions (untranslated) + fatal
        std::string msg = ezmk::i18n::fmt(ezmk::i18n::I18nKey::compiler_not_found,
                                           {{"compiler", gxx.string()}});
        msg += "\n  Windows (MSYS2): pacman -S mingw-w64-x86_64-gcc";
        msg += "\n  Linux (Debian):   apt install g++";
        msg += "\n  Linux (RHEL):     dnf install gcc-c++";
        msg += "\n  macOS:            brew install gcc";
        util::fatal(msg);
    }

    fs::path proj_root = fs::current_path();
    fs::path src_dir = proj_root / "src";
    fs::path temp_dir = proj_root / ".ezmk/temp";
    fs::path build_dir = proj_root / "build";
    fs::path cache_obj_dir = proj_root / ".ezmk/cache/obj";

    // Check src/ exists
    if (!util::file_exists(src_dir)) {
        util::fatal(ezmk::i18n::I18nKey::src_dir_missing);
    }

    // Check main.cpp requirement for executable
    if (cfg.project.type == "executable") {
        if (!util::file_exists(src_dir / "main.cpp") &&
            !util::file_exists(src_dir / "main.c")) {
            util::fatal(ezmk::i18n::I18nKey::main_missing);
        }
    }

    util::info(ezmk::i18n::I18nKey::building,
               {{"name", cfg.project.name},
                {"type", cfg.project.type},
                {"lang", cfg.project.language}});

    fs::create_directories(temp_dir);
    fs::create_directories(build_dir);
    fs::create_directories(cache_obj_dir);

    // Load cache
    auto record = cache::load_record();

    // Update / set global compile options signature
    auto cur_sig = cache::compile_options_signature(cfg.compile);
    if (record.compile_options_signature != cur_sig) {
        if (!record.compile_options_signature.empty()) {
            util::info(ezmk::i18n::I18nKey::compile_options_changed);
        }
        record.compile_options_signature = cur_sig;
        record.files.clear();
    }

    auto sources = util::list_files(src_dir, {".c", ".cc", ".cpp", ".cxx"});
    if (sources.empty()) {
        util::fatal(ezmk::i18n::I18nKey::no_source_files);
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

    // Clean stale temps from previous crashed builds
    {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(temp_dir, ec)) {
            auto& p = e.path();
            if (p.extension() == ".tmp") {
                util::warn(ezmk::i18n::I18nKey::clean_stale, {{"path", p.string()}});
                fs::remove(p, ec);
            }
        }
    }

    // ---- Unified compile phase ----
    cache::CompileInput cin;
    cin.sources = std::move(sources);
    cin.obj_dir = temp_dir;
    cin.dep_dir = temp_dir;
    cin.proj_root = proj_root;
    cin.compile = cfg.compile;
    cin.lang = lang;
    cin.extra_includes = extra_includes;
    cin.cache_obj_dir = cache_obj_dir;
    cin.disable_cache = opts.disable_cache;
    cin.use_pic = use_pic;
    cin.verbose = opts.verbose;

    auto comp_result = cache::compile_sources(cin, record);

    // Save updated cache record
    cache::save_record(record);

    if (comp_result.cache_hits > 0 || comp_result.cache_misses > 0) {
        util::info(ezmk::i18n::I18nKey::cache_summary,
                   {{"cached", std::to_string(comp_result.cache_hits)},
                    {"compiled", std::to_string(comp_result.cache_misses)}});
    }

    auto& objects = comp_result.objects;

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

        util::info(ezmk::i18n::I18nKey::archiving, {{"target", lib.filename().string()}});
        std::ostringstream ar_cmd;
        ar_cmd << "ar rcs \"" << lib_tmp.string() << "\"";
        for (auto& o : objects) {
            ar_cmd << " \"" << o.string() << "\"";
        }
        auto ar_res = util::run_command(ar_cmd.str());
        if (ar_res.exit_code != 0) {
            std::error_code ec;
            fs::remove(lib_tmp, ec);
            util::error(ezmk::i18n::I18nKey::archive_failed,
                        {{"code", std::to_string(ar_res.exit_code)}});
            util::error("  cmd: " + ar_cmd.str());
            if (!ar_res.err.empty()) util::error(ar_res.err);
            util::fatal(ezmk::i18n::I18nKey::build_failed);
        }
        {
            std::error_code ec;
            fs::rename(lib_tmp, lib, ec);
        }
        util::info(ezmk::i18n::I18nKey::build_success, {{"path", lib.string()}});
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

        util::info(ezmk::i18n::I18nKey::linking, {{"target", lib.filename().string()}});
        std::string link_cmd = make_link_cmd(objects, pkg_archives, lib_tmp,
                                             merged_link, lang, true);
        if (opts.verbose) util::info("    cmd: " + link_cmd);
        auto link_res = util::run_command(link_cmd);
        if (link_res.exit_code != 0) {
            std::error_code ec;
            fs::remove(lib_tmp, ec);
            util::error(ezmk::i18n::I18nKey::link_failed,
                        {{"code", std::to_string(link_res.exit_code)}});
            util::error("  cmd: " + link_cmd);
            if (!link_res.err.empty()) util::error(link_res.err);
            if (!link_res.out.empty()) util::error(link_res.out);
            util::fatal(ezmk::i18n::I18nKey::build_failed);
        }
        {
            std::error_code ec;
            fs::rename(lib_tmp, lib, ec);
        }
        util::info(ezmk::i18n::I18nKey::build_success, {{"path", lib.string()}});
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

    util::info(ezmk::i18n::I18nKey::linking, {{"target", exe.filename().string()}});
    std::string link_cmd = make_link_cmd(objects, pkg_archives, exe_tmp,
                                         merged_link, lang);
    if (opts.verbose) util::info("    cmd: " + link_cmd);
    auto link_res = util::run_command(link_cmd);
    if (link_res.exit_code != 0) {
        std::error_code ec;
        fs::remove(exe_tmp, ec);
        util::error(ezmk::i18n::I18nKey::link_failed,
                    {{"code", std::to_string(link_res.exit_code)}});
        util::error("  cmd: " + link_cmd);
        if (!link_res.err.empty()) util::error(link_res.err);
        if (!link_res.out.empty()) util::error(link_res.out);
        util::fatal(ezmk::i18n::I18nKey::build_failed);
    }
    {
        std::error_code ec;
        fs::rename(exe_tmp, exe, ec);
    }

    util::info(ezmk::i18n::I18nKey::build_success, {{"path", exe.string()}});
    return exe;
}

} // namespace ezmk::build
