#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/config.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/toolchain.hpp"
#include "ezmk/util.hpp"
#include "ezmk/version.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace ezmk::build {

// ---- helpers ----

// 0.2.2+: Check if a string is a plain integer (no quoting needed for -D flags)
static bool is_plain_integer(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-') i++;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

// 0.2.2+: Escape a macro value for use in -D flag.
// Plain integers and empty strings are used as-is; strings get quoted with
// internal " and \ escaped.
static std::string escape_macro_value(const std::string& val) {
    if (val.empty() || is_plain_integer(val)) return val;
    std::string escaped;
    for (char c : val) {
        if (c == '"' || c == '\\') escaped += '\\';
        escaped += c;
    }
    return "\"" + escaped + "\"";
}

// 0.2.2+: Convert a macros map to -D flag vector.
// Empty value → -DKEY; non-empty → -DKEY=VALUE.
std::vector<std::string> macros_to_flags(
    const std::map<std::string, std::string>& macros) {
    std::vector<std::string> result;
    for (auto& [key, val] : macros) {
        if (val.empty()) {
            result.push_back("-D" + key);
        } else {
            result.push_back("-D" + key + "=" + escape_macro_value(val));
        }
    }
    return result;
}

// 0.2.2+: Generate standard EZMK_* preprocessor macros from project config.
std::vector<std::string> generate_ezmk_macros(const config::EzConfig& cfg) {
    std::vector<std::string> result;
    result.push_back("-DEZMK=1");
    result.push_back("-DEZMK_VERSION=\"" EZMK_VERSION "\"");
    if (!cfg.project.name.empty()) {
        result.push_back("-DEZMK_PROJECT_NAME=\"" +
            util::escape_shell_arg(cfg.project.name) + "\"");
    }
    if (!cfg.project.version.empty()) {
        result.push_back("-DEZMK_PROJECT_VERSION=\"" +
            util::escape_shell_arg(cfg.project.version) + "\"");
    }
    if (!cfg.project.type.empty()) {
        result.push_back("-DEZMK_PROJECT_TYPE=\"" +
            util::escape_shell_arg(cfg.project.type) + "\"");
    }
    if (!cfg.project.language.empty()) {
        result.push_back("-DEZMK_LANG=\"" +
            util::escape_shell_arg(cfg.project.language) + "\"");
    }
    return result;
}

// 0.2.2+: Convert a package name to the EZMK_LIB_MISS_* macro name.
// Uppercase, replace -/. /space with _, drop other special chars.
std::string want_to_macro_name(const std::string& pkg_name) {
    std::string result = "EZMK_LIB_MISS_";
    for (char c : pkg_name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        } else if (c == '-' || c == '.' || c == ' ') {
            result += '_';
        }
        // other special characters are dropped
    }
    return result;
}

// 0.2.2+: Collect source files from multiple src_dirs.
// Returns deduplicated list; warns on missing/empty directories.
// Throws if no source files found across all directories.
std::vector<fs::path> collect_sources(
    const std::vector<std::string>& src_dirs,
    const fs::path& proj_root,
    const std::string& project_type) {
    std::vector<fs::path> result;
    std::set<std::string> seen_names; // filename stem for dedup
    bool any_dir_exists = false;

    for (auto& d : src_dirs) {
        fs::path dir = d;
        if (dir.is_relative()) dir = proj_root / dir;

        if (!util::file_exists(dir)) {
            util::warn(std::string("source directory not found, skipping: ") + d);
            continue;
        }
        any_dir_exists = true;

        auto files = util::list_files(dir, {".c", ".cc", ".cpp", ".cxx"});
        for (auto& f : files) {
            std::string fname = f.filename().string();
            if (!seen_names.insert(fname).second) {
                util::warn(std::string("duplicate source filename '") + fname +
                          "' — using first occurrence");
                continue;
            }
            result.push_back(f);
        }
    }

    if (!any_dir_exists) {
        util::fatal(ezmk::i18n::I18nKey::src_dir_missing);
    }

    if (result.empty()) {
        util::fatal(ezmk::i18n::I18nKey::no_source_files);
    }

    // Check main.cpp requirement for executables
    if (project_type == "executable") {
        bool has_main = false;
        for (auto& f : result) {
            auto fn = f.filename().string();
            if (fn == "main.cpp" || fn == "main.c") {
                has_main = true;
                break;
            }
        }
        if (!has_main) {
            util::fatal(ezmk::i18n::I18nKey::main_missing);
        }
    }

    return result;
}

std::string detect_compiler(const std::string& language) {
    bool is_cxx = (language == "C++");

    // Cache: probe only once per language per process
    static std::string cached_cxx;
    static std::string cached_c;
    std::string& cached = is_cxx ? cached_cxx : cached_c;
    if (!cached.empty()) return cached;

    // 1. Check environment variable override ($CXX / $CC)
    const char* env = is_cxx ? std::getenv("CXX") : std::getenv("CC");
    if (env && env[0] != '\0') {
        std::string candidate(env);
        auto res = util::run_command(candidate + " --version 2>&1");
        if (res.exit_code == 0) {
            cached = candidate;
            return cached;
        }
        util::warn(std::string("$") + (is_cxx ? "CXX" : "CC") +
                   " is set to '" + candidate + "' but it is not executable — falling back to auto-detect");
    }

    // 2. Platform-specific candidate list
    std::vector<std::string> candidates;
#ifdef EZMK_WIN
    // MSVC (cl.exe) is now handled by toolchain::detect_toolchain() (0.2.1+).
    // When MSVC is the active toolchain, detect_compiler() is not called —
    // this function only serves GCC/Clang detection for non-MSVC builds.
    candidates = is_cxx
        ? std::vector<std::string>{"g++", "clang++"}
        : std::vector<std::string>{"gcc", "clang"};
#else
    // macOS and Linux share the same candidate list
    candidates = is_cxx
        ? std::vector<std::string>{"g++", "clang++", "c++"}
        : std::vector<std::string>{"gcc", "clang", "cc"};
#endif

    // 3. Probe each candidate
    for (auto& c : candidates) {
        auto res = util::run_command(c + " --version 2>&1");
        if (res.exit_code == 0) {
#ifdef EZMK_MACOS
            // Detect Apple Clang alias
            if (res.out.find("Apple") != std::string::npos ||
                res.out.find("apple") != std::string::npos) {
                util::info(std::string("detected Apple Clang as '") + c + "'");
            }
#endif
            cached = c;
            return cached;
        }
    }

    // 4. None found — fatal with platform-specific install instructions
    std::string msg = "no C";
    msg += (is_cxx ? "++" : "");
    msg += " compiler found.\n\n";
#ifdef EZMK_WIN
    msg += "  Install MSYS2: https://www.msys2.org/\n";
    msg += "  Then: pacman -S mingw-w64-x86_64-gcc";
#elif defined(EZMK_MACOS)
    msg += "  Option A: xcode-select --install  (Apple Clang)\n";
    msg += "  Option B: brew install gcc          (GNU GCC)";
#else // Linux
    msg += "  Debian/Ubuntu: sudo apt install g++\n";
    msg += "  RHEL/Fedora:   sudo dnf install gcc-c++\n";
    msg += "  Arch:          sudo pacman -S gcc";
#endif
    util::fatal(msg);
    return {}; // unreachable
}

// GCC/Clang link command builder
static std::string make_gcc_link_cmd(const std::vector<fs::path>& objs,
                                     const std::vector<fs::path>& archives,
                                     const fs::path& output,
                                     const config::LinkSection& link,
                                     const config::LanguageInfo& lang,
                                     bool shared = false) {
    std::ostringstream cmd;
    cmd << (lang.detected_compiler.empty() ? lang.compiler : lang.detected_compiler);

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

// MSVC link command builder — executable
static std::string make_msvc_exe_cmd(const std::vector<fs::path>& objs,
                                     const std::vector<fs::path>& archives,
                                     const fs::path& output,
                                     const config::LinkSection& link) {
    std::ostringstream cmd;
    cmd << "link.exe /OUT:\"" << output.string() << "\" ";

    for (auto& o : objs) {
        cmd << "\"" << o.string() << "\" ";
    }
    for (auto& a : archives) {
        cmd << "\"" << a.string() << "\" ";
    }

    // Translate and add link flags
    auto translated = toolchain::translate_link_flags(link.flags,
        toolchain::CompilerFamily::Msvc);
    for (auto& f : translated.translated) {
        cmd << f << " ";
    }

    // MSVC-specific link flags
    for (auto& f : link.msvc_flags) {
        cmd << f << " ";
    }

    // Link dirs → /LIBPATH
    for (auto& d : link.link_dirs) {
        cmd << "/LIBPATH:\"" << d << "\" ";
    }

    // System targets: -l<name> → <name>.lib
    for (auto& t : link.system_targets) {
        cmd << "\"" << t << ".lib\" ";
    }

    return cmd.str();
}

// MSVC link command builder — shared library (DLL)
static std::string make_msvc_dll_cmd(const std::vector<fs::path>& objs,
                                     const std::vector<fs::path>& archives,
                                     const fs::path& output_dll,
                                     const fs::path& output_implib,
                                     const config::LinkSection& link) {
    std::ostringstream cmd;
    cmd << "link.exe /DLL /OUT:\"" << output_dll.string() << "\" ";
    cmd << "/IMPLIB:\"" << output_implib.string() << "\" ";

    for (auto& o : objs) {
        cmd << "\"" << o.string() << "\" ";
    }
    for (auto& a : archives) {
        cmd << "\"" << a.string() << "\" ";
    }

    auto translated = toolchain::translate_link_flags(link.flags,
        toolchain::CompilerFamily::Msvc);
    for (auto& f : translated.translated) {
        cmd << f << " ";
    }
    for (auto& f : link.msvc_flags) {
        cmd << f << " ";
    }
    for (auto& d : link.link_dirs) {
        cmd << "/LIBPATH:\"" << d << "\" ";
    }
    for (auto& t : link.system_targets) {
        cmd << "\"" << t << ".lib\" ";
    }

    return cmd.str();
}

// MSVC static library command builder (lib.exe)
static std::string make_msvc_lib_cmd(const std::vector<fs::path>& objs,
                                     const fs::path& output) {
    std::ostringstream cmd;
    cmd << "lib.exe /OUT:\"" << output.string() << "\" ";

    for (auto& o : objs) {
        cmd << "\"" << o.string() << "\" ";
    }

    return cmd.str();
}

// ===================================================================
// Build
// ===================================================================

fs::path build_project(const config::EzConfig& cfg, const cli::BuildOptions& opts) {
    // Parse language → compiler + std flag
    auto lang = config::parse_language(cfg.project.language);

    // 0.2.1+: Detect full toolchain FIRST (GCC/Clang/MSVC).
    // Must run before detect_compiler() so we don't fatal on a pure-MSVC system
    // that has no MinGW g++ installed.
    auto tc = toolchain::detect_toolchain();
    bool is_msvc = (tc.family == toolchain::CompilerFamily::Msvc);

    // Detect best available compiler (respects $CXX/$CC, probes platform candidates).
    // Only needed for GCC/Clang; MSVC uses cl.exe directly.
    if (!is_msvc) {
        lang.detected_compiler = detect_compiler(lang.compiler == "g++" ? "C++" : "C");
    }

    fs::path proj_root = fs::current_path();
    fs::path temp_dir = proj_root / ".ezmk/temp";
    fs::path build_dir = proj_root / "build";
    fs::path cache_obj_dir = proj_root / ".ezmk/cache/obj";

    // 0.2.2+: Collect sources from multiple src_dirs (validates directories, main.cpp check)
    auto sources = collect_sources(cfg.compile.src_dirs, proj_root, cfg.project.type);

    util::info(ezmk::i18n::I18nKey::building,
               {{"name", cfg.project.name},
                {"type", cfg.project.type},
                {"lang", cfg.project.language}});

    fs::create_directories(temp_dir);
    fs::create_directories(build_dir);
    fs::create_directories(cache_obj_dir);

    // 0.2.2+: Build effective compile flags by merging ezmk_macros + flags + macros + want macros.
    // The order matters: later -D flags override earlier ones (compiler behavior).
    // ezmk_macros (standard) → flags (user -D in flags) → macros (compile.macros) → want (missing optional deps)
    std::vector<std::string> effective_flags;

    // 1. Standard EZMK_* macros (if enabled, default true)
    if (cfg.compile.ezmk_macros) {
        auto em = generate_ezmk_macros(cfg);
        effective_flags.insert(effective_flags.end(), em.begin(), em.end());
    }

    // 2. User flags (may contain -D flags)
    effective_flags.insert(effective_flags.end(),
                          cfg.compile.flags.begin(), cfg.compile.flags.end());

    // 3. compile.macros
    auto macro_flags = macros_to_flags(cfg.compile.macros);
    effective_flags.insert(effective_flags.end(),
                          macro_flags.begin(), macro_flags.end());

    // 4. want.lib: handle after package scan (see below)

    // Create a modified compile section with effective flags (want macros appended later)
    config::CompileSection effective_compile = cfg.compile;
    effective_compile.flags = std::move(effective_flags);

    // Need -fPIC for shared libraries
    bool use_pic = (cfg.project.type == "shared");

    // Single pass over .ezmk/pkg/: collect installed package names (for want handling),
    // include dirs, link info, and built archives.
    std::set<std::string> installed_pkgs;
    std::vector<fs::path> extra_includes;
    std::vector<fs::path> pkg_archives;
    std::vector<std::string> pkg_link_flags;
    std::vector<std::string> pkg_link_dirs;
    std::vector<std::string> pkg_system_targets;
    fs::path pkg_dir = proj_root / ".ezmk/pkg";
    if (util::file_exists(pkg_dir)) {
        for (auto& entry : fs::directory_iterator(pkg_dir)) {
            if (!entry.is_directory()) continue;

            auto pkg_toml = entry.path() / "ezmk.toml";
            if (util::file_exists(pkg_toml)) {
                try {
                    auto pkg_cfg = config::parse_config(pkg_toml);

                    // Package name for want/lib lookup
                    installed_pkgs.insert(pkg_cfg.project.name);

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

            // Look for compiled library (.a for GCC, .lib for MSVC)
            auto pkg_build = entry.path() / "build";
            if (util::file_exists(pkg_build)) {
                for (auto& f : fs::directory_iterator(pkg_build)) {
                    auto ext = f.path().extension().string();
                    if (ext == ".a" || (is_msvc && ext == ".lib")) {
                        pkg_archives.push_back(f.path());
                    }
                }
            }
        }
    }

    // want.lib: process optional dependencies using names collected in the scan above
    {
        std::set<std::string> lib_set(cfg.depends.libs.begin(), cfg.depends.libs.end());
        for (auto& want_name : cfg.depends.want) {
            if (lib_set.count(want_name)) {
                util::warn(std::string("package '") + want_name +
                           "' is in both [depends].lib and [depends].want — treating as hard dependency");
                continue;
            }
            if (installed_pkgs.find(want_name) == installed_pkgs.end()) {
                util::warn(std::string("optional dependency not installed: ") + want_name);
                effective_compile.flags.push_back("-D" + want_to_macro_name(want_name));
            }
            // If installed, it's already handled by the package scan above
        }
    }

    // Load cache
    auto record = cache::load_record();

    // Update / set global compile options signature (includes extra_includes for
    // correct invalidation when dependency packages are installed/removed)
    auto cur_sig = cache::compile_options_signature(effective_compile, extra_includes,
                                                   lang.std_flag);
    if (record.compile_options_signature != cur_sig) {
        if (!record.compile_options_signature.empty()) {
            util::info(ezmk::i18n::I18nKey::compile_options_changed);
        }
        record.compile_options_signature = cur_sig;
        record.files.clear();
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
    cin.compile = effective_compile;
    cin.lang = lang;
    cin.extra_includes = extra_includes;
    cin.cache_obj_dir = cache_obj_dir;
    cin.disable_cache = opts.disable_cache;
    cin.use_pic = use_pic;
    cin.verbose = opts.verbose;
    cin.tc = tc;  // 0.2.1+ pass detected toolchain

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
        if (is_msvc) {
            // MSVC: lib.exe for static library
            fs::path lib = build_dir / (cfg.project.name + ".lib");
            fs::path lib_tmp = build_dir / (cfg.project.name + ".lib.tmp");

            {
                std::error_code ec;
                fs::remove(lib_tmp, ec);
            }

            util::info(ezmk::i18n::I18nKey::archiving, {{"target", lib.filename().string()}});
            std::string lib_cmd = make_msvc_lib_cmd(objects, lib_tmp);
            if (opts.verbose) util::info("    cmd: " + lib_cmd);
            auto lib_res = util::run_command(lib_cmd);
            if (lib_res.exit_code != 0) {
                std::error_code ec;
                fs::remove(lib_tmp, ec);
                util::error(ezmk::i18n::I18nKey::archive_failed,
                            {{"code", std::to_string(lib_res.exit_code)}});
                util::error("  cmd: " + lib_cmd);
                if (!lib_res.err.empty()) util::error(lib_res.err);
                util::fatal(ezmk::i18n::I18nKey::build_failed);
            }
            {
                std::error_code ec;
                fs::rename(lib_tmp, lib, ec);
            }
            util::info(ezmk::i18n::I18nKey::build_success, {{"path", lib.string()}});
            return lib;
        }

        // GCC/Clang: ar rcs (atomic via tmp)
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
        if (is_msvc) {
            // MSVC: link.exe /DLL for shared library
            fs::path dll = build_dir / (cfg.project.name + ".dll");
            fs::path implib = build_dir / (cfg.project.name + "_implib.lib");
            fs::path dll_tmp = build_dir / (cfg.project.name + ".dll.tmp");

            {
                std::error_code ec;
                fs::remove(dll_tmp, ec);
            }

            util::info(ezmk::i18n::I18nKey::linking, {{"target", dll.filename().string()}});
            std::string link_cmd = make_msvc_dll_cmd(objects, pkg_archives,
                                                     dll_tmp, implib, merged_link);
            if (opts.verbose) util::info("    cmd: " + link_cmd);
            auto link_res = util::run_command(link_cmd);
            if (link_res.exit_code != 0) {
                std::error_code ec;
                fs::remove(dll_tmp, ec);
                util::error(ezmk::i18n::I18nKey::link_failed,
                            {{"code", std::to_string(link_res.exit_code)}});
                util::error("  cmd: " + link_cmd);
                if (!link_res.err.empty()) util::error(link_res.err);
                if (!link_res.out.empty()) util::error(link_res.out);
                util::fatal(ezmk::i18n::I18nKey::build_failed);
            }
            {
                std::error_code ec;
                fs::rename(dll_tmp, dll, ec);
            }
            util::info(ezmk::i18n::I18nKey::build_success, {{"path", dll.string()}});
            return dll;
        }

        // GCC/Clang: g++ -shared
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
        std::string link_cmd = make_gcc_link_cmd(objects, pkg_archives, lib_tmp,
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
    if (is_msvc) {
        // MSVC: link.exe for executable
        fs::path exe = build_dir / (cfg.project.name + ".exe");
        fs::path exe_tmp = build_dir / (cfg.project.name + ".exe.tmp");

        {
            std::error_code ec;
            fs::remove(exe_tmp, ec);
        }

        util::info(ezmk::i18n::I18nKey::linking, {{"target", exe.filename().string()}});
        std::string link_cmd = make_msvc_exe_cmd(objects, pkg_archives,
                                                 exe_tmp, merged_link);
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

    // GCC/Clang: g++ for executable
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
    std::string link_cmd = make_gcc_link_cmd(objects, pkg_archives, exe_tmp,
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
