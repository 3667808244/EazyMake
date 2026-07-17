#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/config.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/lua_api.hpp"
#include "ezmk/thread_pool.hpp"
#include "ezmk/toolchain.hpp"
#include "ezmk/util.hpp"
#include "ezmk/version.hpp"

#include <algorithm>
#include <atomic>
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
// Empty value 鈫?-DKEY; non-empty 鈫?-DKEY=VALUE.
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
                          "' 鈥?using first occurrence");
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
                   " is set to '" + candidate + "' but it is not executable 鈥?falling back to auto-detect");
    }

    // 2. Platform-specific candidate list
    std::vector<std::string> candidates;
#ifdef EZMK_WIN
    // MSVC (cl.exe) is now handled by toolchain::detect_toolchain() (0.2.1+).
    // When MSVC is the active toolchain, detect_compiler() is not called 鈥?    // this function only serves GCC/Clang detection for non-MSVC builds.
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

    // 4. None found 鈥?fatal with platform-specific install instructions
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
        cmd << " \"" << util::escape_shell_arg(o.string()) << "\"";
    }
    for (auto& a : archives) {
        cmd << " \"" << util::escape_shell_arg(a.string()) << "\"";
    }

    cmd << " -o \"" << util::escape_shell_arg(output.string()) << "\"";

    if (shared) {
        cmd << " -shared";
    }

    for (auto& f : link.flags) {
        cmd << " " << util::escape_shell_arg(f);
    }
    for (auto& d : link.link_dirs) {
        cmd << " -L\"" << util::escape_shell_arg(d) << "\"";
    }
    for (auto& t : link.system_targets) {
        cmd << " -l" << util::escape_shell_arg(t);
    }

    return cmd.str();
}

// MSVC link command builder 鈥?executable
static std::string make_msvc_exe_cmd(const std::vector<fs::path>& objs,
                                     const std::vector<fs::path>& archives,
                                     const fs::path& output,
                                     const config::LinkSection& link) {
    std::ostringstream cmd;
    cmd << "link.exe /OUT:\"" << util::escape_shell_arg(output.string()) << "\" ";

    for (auto& o : objs) {
        cmd << "\"" << util::escape_shell_arg(o.string()) << "\" ";
    }
    for (auto& a : archives) {
        cmd << "\"" << util::escape_shell_arg(a.string()) << "\" ";
    }

    // Translate and add link flags
    auto translated = toolchain::translate_link_flags(link.flags,
        toolchain::CompilerFamily::Msvc);
    for (auto& f : translated.translated) {
        cmd << util::escape_shell_arg(f) << " ";
    }

    // MSVC-specific link flags
    for (auto& f : link.msvc_flags) {
        cmd << util::escape_shell_arg(f) << " ";
    }

    // Link dirs 鈫?/LIBPATH
    for (auto& d : link.link_dirs) {
        cmd << "/LIBPATH:\"" << util::escape_shell_arg(d) << "\" ";
    }

    // System targets: -l<name> 鈫?<name>.lib
    for (auto& t : link.system_targets) {
        cmd << "\"" << util::escape_shell_arg(t) << ".lib\" ";
    }

    return cmd.str();
}

// MSVC link command builder 鈥?shared library (DLL)
static std::string make_msvc_dll_cmd(const std::vector<fs::path>& objs,
                                     const std::vector<fs::path>& archives,
                                     const fs::path& output_dll,
                                     const fs::path& output_implib,
                                     const config::LinkSection& link) {
    std::ostringstream cmd;
    cmd << "link.exe /DLL /OUT:\"" << util::escape_shell_arg(output_dll.string()) << "\" ";
    cmd << "/IMPLIB:\"" << util::escape_shell_arg(output_implib.string()) << "\" ";

    for (auto& o : objs) {
        cmd << "\"" << util::escape_shell_arg(o.string()) << "\" ";
    }
    for (auto& a : archives) {
        cmd << "\"" << util::escape_shell_arg(a.string()) << "\" ";
    }

    auto translated = toolchain::translate_link_flags(link.flags,
        toolchain::CompilerFamily::Msvc);
    for (auto& f : translated.translated) {
        cmd << util::escape_shell_arg(f) << " ";
    }
    for (auto& f : link.msvc_flags) {
        cmd << util::escape_shell_arg(f) << " ";
    }
    for (auto& d : link.link_dirs) {
        cmd << "/LIBPATH:\"" << util::escape_shell_arg(d) << "\" ";
    }
    for (auto& t : link.system_targets) {
        cmd << "\"" << util::escape_shell_arg(t) << ".lib\" ";
    }

    return cmd.str();
}

// MSVC static library command builder (lib.exe)
static std::string make_msvc_lib_cmd(const std::vector<fs::path>& objs,
                                     const fs::path& output) {
    std::ostringstream cmd;
    cmd << "lib.exe /OUT:\"" << util::escape_shell_arg(output.string()) << "\" ";

    for (auto& o : objs) {
        cmd << "\"" << util::escape_shell_arg(o.string()) << "\" ";
    }

    return cmd.str();
}

// ===================================================================
// 0.2.3+: Profile merging
// ===================================================================

config::CompileSection merge_compile_profile(
    const config::CompileSection& base,
    const config::ProfileConfig& profile) {
    config::CompileSection result = base;

    // Append profile flags after base flags (later overrides earlier)
    result.flags.insert(result.flags.end(),
                        profile.flags.begin(), profile.flags.end());

    // Append profile MSVC flags
    result.msvc_flags.insert(result.msvc_flags.end(),
                             profile.msvc_flags.begin(), profile.msvc_flags.end());

    // Merge macros: profile macros override base macros with the same key
    for (auto& [key, val] : profile.macros) {
        result.macros[key] = val;
    }

    return result;
}

config::LinkSection merge_link_profile(
    const config::LinkSection& base,
    const config::ProfileLinkConfig& profile) {
    config::LinkSection result = base;

    // Append profile flags after base flags
    result.flags.insert(result.flags.end(),
                        profile.flags.begin(), profile.flags.end());

    // Append profile MSVC flags
    result.msvc_flags.insert(result.msvc_flags.end(),
                             profile.msvc_flags.begin(), profile.msvc_flags.end());

    return result;
}


// ===================================================================
// Build — internal helpers
// ===================================================================

namespace {

// Shared state that flows through the build phases.
struct BuildState {
    fs::path proj_root;
    fs::path temp_dir;
    fs::path build_dir;
    fs::path cache_obj_dir;
    config::CompileSection compile_cfg;
    config::LinkSection link_cfg;
    config::LanguageInfo lang;
    toolchain::Toolchain tc;
    bool is_msvc = false;
    bool use_pic = false;
    std::vector<fs::path> pkg_archives;
    std::vector<fs::path> extra_includes;
    std::vector<std::string> pkg_link_flags;
    std::vector<std::string> pkg_link_dirs;
    std::vector<std::string> pkg_system_targets;
};

// 0.2.4+: Unified hook execution — runs a Lua hook script if configured.
void run_hook(const std::string& hook_path_cfg, const fs::path& proj_root,
              const fs::path& hook_output, const std::string& profile,
              ezmk::i18n::I18nKey info_key) {
    if (hook_path_cfg.empty()) return;
    fs::path hook_path = hook_path_cfg;
    if (hook_path.is_relative()) hook_path = proj_root / hook_path;
    if (util::file_exists(hook_path)) {
        util::info(info_key, {{"path", hook_path_cfg}});
        int rc = lua::run_hook_script(lua::state(), hook_path,
                                      hook_output, proj_root, profile);
        if (rc != 0) {
            util::warn(ezmk::i18n::I18nKey::hook_nonzero,
                       {{"path", hook_path_cfg},
                        {"code", std::to_string(rc)}});
        }
    } else {
        util::warn(ezmk::i18n::I18nKey::hook_not_found,
                   {{"path", hook_path_cfg}});
    }
}

// Phase 1: Setup + package scanning + pre-build hook.
// Returns fully initialized build state with effective compile flags.
BuildState prepare_build_state(const config::EzConfig& cfg,
                               const cli::BuildOptions& opts) {
    BuildState st;

    st.proj_root = fs::current_path();
    st.temp_dir = st.proj_root / ".ezmk/temp";
    st.build_dir = st.proj_root / "build";
    st.cache_obj_dir = st.proj_root / ".ezmk/cache/obj";

    // Language + toolchain detection
    st.lang = config::parse_language(cfg.project.language);
    st.tc = toolchain::detect_toolchain();
    st.is_msvc = (st.tc.family == toolchain::CompilerFamily::Msvc);
    if (!st.is_msvc) {
        st.lang.detected_compiler = detect_compiler(
            st.lang.compiler == "g++" ? "C++" : "C");
    }

    // Apply build profile
    st.compile_cfg = cfg.compile;
    st.link_cfg = cfg.link;
    if (!opts.profile.empty()) {
        auto it = cfg.compile_profiles.find(opts.profile);
        if (it != cfg.compile_profiles.end()) {
            st.compile_cfg = merge_compile_profile(st.compile_cfg, it->second);
        } else {
            // 0.9.4+: collect available profile names + suggest closest matches
            std::vector<std::string> profile_names;
            for (const auto& [name, _] : cfg.compile_profiles) profile_names.push_back(name);
            for (const auto& [name, _] : cfg.link_profiles)
                if (std::find(profile_names.begin(), profile_names.end(), name) == profile_names.end())
                    profile_names.push_back(name);
            std::sort(profile_names.begin(), profile_names.end());

            auto matches = util::closest_match(opts.profile, profile_names, 2);
            if (!matches.empty()) {
                std::string suggestion = matches[0];
                for (size_t i = 1; i < matches.size() && i < 3; ++i)
                    suggestion += ", " + matches[i];
                util::fatal(std::string("unknown profile: '") + opts.profile +
                            "'. Did you mean: " + suggestion + "?");
            }

            if (profile_names.empty()) {
                util::fatal(std::string("unknown profile: '") + opts.profile +
                            "'. No profiles defined in ezmk.toml.");
            } else {
                std::string avail;
                for (size_t i = 0; i < profile_names.size(); ++i) {
                    if (i > 0) avail += ", ";
                    avail += profile_names[i];
                }
                util::fatal(std::string("unknown profile: '") + opts.profile +
                            "'. Available: " + avail);
            }
        }
        auto lit = cfg.link_profiles.find(opts.profile);
        if (lit != cfg.link_profiles.end()) {
            st.link_cfg = merge_link_profile(st.link_cfg, lit->second);
        }
    }

    // Collect sources + validate
    collect_sources(st.compile_cfg.src_dirs, st.proj_root, cfg.project.type);

    util::info(ezmk::i18n::I18nKey::building,
               {{"name", cfg.project.name},
                {"type", cfg.project.type},
                {"lang", cfg.project.language}});

    fs::create_directories(st.temp_dir);
    fs::create_directories(st.build_dir);
    fs::create_directories(st.cache_obj_dir);

    // Build effective compile flags (order: ezmk_macros → flags → macros)
    std::vector<std::string> effective_flags;
    if (st.compile_cfg.ezmk_macros) {
        auto em = generate_ezmk_macros(cfg);
        effective_flags.insert(effective_flags.end(), em.begin(), em.end());
    }
    effective_flags.insert(effective_flags.end(),
                           st.compile_cfg.flags.begin(),
                           st.compile_cfg.flags.end());
    auto macro_flags = macros_to_flags(st.compile_cfg.macros);
    effective_flags.insert(effective_flags.end(),
                           macro_flags.begin(), macro_flags.end());

    st.compile_cfg.flags = std::move(effective_flags);
    st.use_pic = (cfg.project.type == "shared");

    // Scan installed packages
    std::set<std::string> installed_pkgs;
    fs::path pkg_dir = st.proj_root / ".ezmk/pkg";
    if (util::file_exists(pkg_dir)) {
        for (auto& entry : fs::directory_iterator(pkg_dir)) {
            if (!entry.is_directory()) continue;
            auto pkg_toml = entry.path() / "ezmk.toml";
            if (util::file_exists(pkg_toml)) {
                try {
                    auto pkg_cfg = config::parse_config(pkg_toml);
                    installed_pkgs.insert(pkg_cfg.project.name);
                    auto pkg_include = entry.path() / "include";
                    if (util::file_exists(pkg_include)) {
                        st.extra_includes.push_back(pkg_include);
                    }
                    for (auto& d : pkg_cfg.compile.include_dirs) {
                        fs::path resolved = d;
                        if (resolved.is_relative()) resolved = entry.path() / resolved;
                        if (util::file_exists(resolved) &&
                            resolved != pkg_include) {
                            st.extra_includes.push_back(resolved);
                        }
                    }
                    for (auto& f : pkg_cfg.link.flags)
                        st.pkg_link_flags.push_back(f);
                    for (auto& d : pkg_cfg.link.link_dirs)
                        st.pkg_link_dirs.push_back(d);
                    for (auto& t : pkg_cfg.link.system_targets)
                        st.pkg_system_targets.push_back(t);
                } catch (...) {
                    util::warn(std::string("failed to parse ezmk.toml for dependency package: ") +
                               entry.path().filename().string() + " — skipping");
                }
            } else {
                auto pkg_include = entry.path() / "include";
                if (util::file_exists(pkg_include)) {
                    st.extra_includes.push_back(pkg_include);
                }
            }
            // Collect built archives
            auto pkg_build = entry.path() / "build";
            if (util::file_exists(pkg_build)) {
                for (auto& f : fs::directory_iterator(pkg_build)) {
                    auto ext = f.path().extension().string();
                    if (ext == ".a" || (st.is_msvc && ext == ".lib")) {
                        st.pkg_archives.push_back(f.path());
                    }
                }
            }
        }
    }

    // want.lib: process optional dependencies
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
                st.compile_cfg.flags.push_back("-D" + want_to_macro_name(want_name));
            }
        }
    }

    // Pre-build hook
    run_hook(cfg.hooks.pre_build, st.proj_root, "" /* no output yet */,
             opts.profile, ezmk::i18n::I18nKey::pre_build_hook);

    return st;
}

// Phase 2: Compile all sources (cache check + compilation).
// Returns the list of compiled object paths.
std::vector<fs::path> compile_phase(BuildState& st, const cli::BuildOptions& opts) {
    // Load cache
    auto record = cache::load_record();
    auto cur_sig = cache::compile_options_signature(st.compile_cfg, st.extra_includes,
                                                    st.lang.std_flag);
    if (record.compile_options_signature != cur_sig) {
        if (!record.compile_options_signature.empty()) {
            util::info(ezmk::i18n::I18nKey::compile_options_changed);
        }
        record.compile_options_signature = cur_sig;
        record.files.clear();
    }

    // Clean stale temps
    {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(st.temp_dir, ec)) {
            auto& p = e.path();
            if (p.extension() == ".tmp") {
                util::warn(ezmk::i18n::I18nKey::clean_stale, {{"path", p.string()}});
                fs::remove(p, ec);
            }
        }
    }

    // Build compile input (re-collect sources — already validated in prepare)
    cache::CompileInput cin;
    cin.sources = collect_sources(st.compile_cfg.src_dirs, st.proj_root,
                                  "utils" /* skip main.cpp check — already done */);
    cin.obj_dir = st.temp_dir;
    cin.dep_dir = st.temp_dir;
    cin.proj_root = st.proj_root;
    cin.compile = st.compile_cfg;
    cin.lang = st.lang;
    cin.extra_includes = st.extra_includes;
    cin.cache_obj_dir = st.cache_obj_dir;
    cin.disable_cache = opts.disable_cache;
    cin.use_pic = st.use_pic;
    cin.verbose = opts.verbose;
    cin.tc = st.tc;

    int num_jobs = opts.jobs;
    if (num_jobs <= 0) {
        num_jobs = static_cast<int>(std::thread::hardware_concurrency());
        if (num_jobs <= 0) num_jobs = 1;
    }

    cache::CompileResult comp_result;
    std::vector<cache::SingleCompileResult> single_results;
    single_results.reserve(cin.sources.size());

    // INVARIANT: In parallel mode, compile_one_source() only reads from
    // `record` (const ref). record.files is updated below after all
    // threads complete, so no concurrent write occurs.
    if (num_jobs > 1 && cin.sources.size() > 1) {
        if (opts.verbose) {
            util::info(ezmk::i18n::I18nKey::parallel_jobs_info,
                       {{"jobs", std::to_string(num_jobs)},
                        {"total", std::to_string(cin.sources.size())}});
        }
        util::ThreadPool pool(static_cast<size_t>(num_jobs));
        std::vector<std::future<cache::SingleCompileResult>> futures;
        futures.reserve(cin.sources.size());
        std::atomic<int> task_index{0};
        int total = static_cast<int>(cin.sources.size());
        for (size_t i = 0; i < cin.sources.size(); ++i) {
            futures.push_back(pool.submit([&cin, &record, &task_index, total, i]() {
                auto idx = task_index.fetch_add(1) + 1;
                auto result = cache::compile_one_source(cin.sources[i], cin, record);
                if (cin.verbose && result.success && !result.cache_hit) {
                    util::info(std::string("[") + std::to_string(idx) + "/" +
                               std::to_string(total) + "] compiled: " + result.rel_src);
                }
                return result;
            }));
        }
        for (auto& f : futures) {
            single_results.push_back(f.get());
        }
    } else {
        comp_result = cache::compile_sources(cin, record);
    }

    // Process parallel results
    if (!single_results.empty()) {
        bool has_failure = false;
        for (auto& sr : single_results) {
            if (sr.cache_hit) {
                comp_result.objects.push_back(sr.object);
                ++comp_result.cache_hits;
            } else if (sr.success) {
                comp_result.objects.push_back(sr.object);
                ++comp_result.cache_misses;
                auto& entry = record.files[sr.rel_src];
                auto old_it = record.files.find(sr.rel_src);
                if (old_it != record.files.end() &&
                    !cache::same_dependency_paths(old_it->second.dependencies, sr.new_deps)) {
                    util::info(ezmk::i18n::I18nKey::include_structure_changed,
                               {{"file", sr.rel_src}});
                }
                entry = std::move(sr.record_entry);
            } else {
                has_failure = true;
                util::error(sr.error_msg);
            }
        }
        if (has_failure) {
            util::fatal(ezmk::i18n::I18nKey::build_failed);
        }
    }

    cache::save_record(record);

    if (comp_result.cache_hits > 0 || comp_result.cache_misses > 0) {
        util::info(ezmk::i18n::I18nKey::cache_summary,
                   {{"cached", std::to_string(comp_result.cache_hits)},
                    {"compiled", std::to_string(comp_result.cache_misses)}});
    }

    return comp_result.objects;
}

// Execute a link/archive command with standard error handling and atomic rename.
// Returns the final output path on success; throws fatal_error on failure.
static fs::path execute_link(
    const std::string& cmd,
    const fs::path& output,
    const fs::path& output_tmp,
    bool verbose,
    ezmk::i18n::I18nKey action_key,
    const std::string& target_name,
    ezmk::i18n::I18nKey fail_key,
    bool show_stdout = false)
{
    std::error_code ec;
    fs::remove(output_tmp, ec);

    util::info(action_key, {{"target", target_name}});
    if (verbose) util::info("    cmd: " + cmd);

    auto res = util::run_command(cmd);
    if (res.exit_code != 0) {
        fs::remove(output_tmp, ec);
        util::error(fail_key, {{"code", std::to_string(res.exit_code)}});
        util::error("  cmd: " + cmd);
        if (!res.err.empty()) util::error(res.err);
        if (show_stdout && !res.out.empty()) util::error(res.out);
        util::fatal(ezmk::i18n::I18nKey::build_failed);
    }

    fs::rename(output_tmp, output, ec);
    util::info(ezmk::i18n::I18nKey::build_success, {{"path", output.string()}});
    return output;
}

// Phase 3: Link objects into the final output.
fs::path link_phase(const BuildState& st,
                    const std::vector<fs::path>& objects,
                    const cli::BuildOptions& opts,
                    const config::EzConfig& cfg) {
    // Merge package link options
    config::LinkSection merged_link = st.link_cfg;
    for (auto& f : st.pkg_link_flags) merged_link.flags.push_back(f);
    for (auto& d : st.pkg_link_dirs) merged_link.link_dirs.push_back(d);
    for (auto& t : st.pkg_system_targets) merged_link.system_targets.push_back(t);

    // Helper: try to link; on failure, run on_failure hook before re-throwing.
    auto try_link = [&](auto&& link_fn) -> fs::path {
        try {
            return link_fn();
        } catch (...) {
            run_hook(cfg.hooks.on_failure, st.proj_root, "" /* no output */,
                     opts.profile, ezmk::i18n::I18nKey::on_failure_hook);
            throw;
        }
    };

    if (cfg.project.type == "static") {
        if (st.is_msvc) {
            return try_link([&]() -> fs::path {
                fs::path lib = st.build_dir / (cfg.project.name + ".lib");
                fs::path lib_tmp = st.build_dir / (cfg.project.name + ".lib.tmp");
                return execute_link(make_msvc_lib_cmd(objects, lib_tmp), lib, lib_tmp,
                                    opts.verbose, ezmk::i18n::I18nKey::archiving,
                                    lib.filename().string(), ezmk::i18n::I18nKey::archive_failed);
            });
        } else {
            return try_link([&]() -> fs::path {
                fs::path lib = st.build_dir / ("lib" + cfg.project.name + ".a");
                fs::path lib_tmp = st.build_dir / ("lib" + cfg.project.name + ".a.tmp");
                std::ostringstream ar_cmd;
                ar_cmd << "ar rcs \"" << util::escape_shell_arg(lib_tmp.string()) << "\"";
                for (auto& o : objects)
                    ar_cmd << " \"" << util::escape_shell_arg(o.string()) << "\"";
                return execute_link(ar_cmd.str(), lib, lib_tmp, opts.verbose,
                                    ezmk::i18n::I18nKey::archiving,
                                    lib.filename().string(), ezmk::i18n::I18nKey::archive_failed);
            });
        }
    } else if (cfg.project.type == "shared") {
        if (st.is_msvc) {
            return try_link([&]() -> fs::path {
                fs::path dll = st.build_dir / (cfg.project.name + ".dll");
                fs::path implib = st.build_dir / (cfg.project.name + "_implib.lib");
                fs::path dll_tmp = st.build_dir / (cfg.project.name + ".dll.tmp");
                return execute_link(make_msvc_dll_cmd(objects, st.pkg_archives, dll_tmp, implib, merged_link),
                                    dll, dll_tmp, opts.verbose, ezmk::i18n::I18nKey::linking,
                                    dll.filename().string(), ezmk::i18n::I18nKey::link_failed, true);
            });
        } else {
            return try_link([&]() -> fs::path {
                std::string lib_name = "lib" + cfg.project.name;
#ifdef EZMK_WIN
                lib_name += ".dll";
#else
                lib_name += ".so";
#endif
                fs::path lib = st.build_dir / lib_name;
                fs::path lib_tmp = st.build_dir / (lib_name + ".tmp");
                return execute_link(make_gcc_link_cmd(objects, st.pkg_archives, lib_tmp, merged_link, st.lang, true),
                                    lib, lib_tmp, opts.verbose, ezmk::i18n::I18nKey::linking,
                                    lib.filename().string(), ezmk::i18n::I18nKey::link_failed, true);
            });
        }
    } else {
        // Default: executable
        if (st.is_msvc) {
            return try_link([&]() -> fs::path {
                fs::path exe = st.build_dir / (cfg.project.name + ".exe");
                fs::path exe_tmp = st.build_dir / (cfg.project.name + ".exe.tmp");
                return execute_link(make_msvc_exe_cmd(objects, st.pkg_archives, exe_tmp, merged_link),
                                    exe, exe_tmp, opts.verbose, ezmk::i18n::I18nKey::linking,
                                    exe.filename().string(), ezmk::i18n::I18nKey::link_failed, true);
            });
        } else {
            return try_link([&]() -> fs::path {
                fs::path exe = st.build_dir / cfg.project.name;
#ifdef EZMK_WIN
                exe += ".exe";
#endif
                fs::path exe_tmp = st.build_dir / (cfg.project.name + ".tmp");
#ifdef EZMK_WIN
                exe_tmp += ".exe";
#endif
                return execute_link(make_gcc_link_cmd(objects, st.pkg_archives, exe_tmp, merged_link, st.lang),
                                    exe, exe_tmp, opts.verbose, ezmk::i18n::I18nKey::linking,
                                    exe.filename().string(), ezmk::i18n::I18nKey::link_failed, true);
            });
        }
    }
}

} // anonymous namespace

// ===================================================================
// Build — public entry point
// ===================================================================

fs::path build_project(const config::EzConfig& cfg, const cli::BuildOptions& opts) {
    // Phase 1: Setup, config merge, package scan, pre-build hook
    auto st = prepare_build_state(cfg, opts);

    // Phase 2: Compile all sources
    auto objects = compile_phase(st, opts);

    // Phase 3: Link
    auto output = link_phase(st, objects, opts, cfg);

    // Post-build hook
    run_hook(cfg.hooks.post_build, st.proj_root, output, opts.profile,
             ezmk::i18n::I18nKey::post_build_hook);

    return output;
}

} // namespace ezmk::build