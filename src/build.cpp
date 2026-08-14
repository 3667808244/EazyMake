#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/compile_db.hpp"
#include "ezmk/config.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/lockfile.hpp"
#include "ezmk/lua_api.hpp"
#include "ezmk/pkg.hpp"
#include "ezmk/thread_pool.hpp"
#include "ezmk/toolchain.hpp"
#include "ezmk/util.hpp"
#include "ezmk/version.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

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
        // 1.1.0-dev.4: Use normalized form for EZMK_LANG
        auto lang_info = config::parse_language(cfg.project.language);
        result.push_back("-DEZMK_LANG=\"" +
            util::escape_shell_arg(lang_info.normalized_lang) + "\"");
    }
    // 1.1.0-dev.4: EZMK_STDLIB — replace ++ with xx in macro value
    if (!cfg.project.stdlib.empty()) {
        std::string stdlib_macro = cfg.project.stdlib;
        // Replace "++" with "xx" to avoid + being misinterpreted
        size_t pos = 0;
        while ((pos = stdlib_macro.find("++", pos)) != std::string::npos) {
            stdlib_macro.replace(pos, 2, "xx");
            pos += 2;
        }
        result.push_back("-DEZMK_STDLIB=\"" +
            util::escape_shell_arg(stdlib_macro) + "\"");
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
        // 1.1.3 S4: 与 link_dirs 一致双引号包裹 — 含 `;|&` 等的 flags 在 POSIX
        // `sh -c` 下会被当 shell 语法执行（命令注入）；Windows 走 CreateProcessA
        // 不解释这些字符，但一致转义无害。
        cmd << " \"" << util::escape_shell_arg(f) << "\"";
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
        // 1.1.3 S4: 双引号包裹（CreateProcessA 无 shell，但与 GCC 侧统一转义无害）
        cmd << "\"" << util::escape_shell_arg(f) << "\" ";
    }

    // MSVC-specific link flags
    for (auto& f : link.msvc_flags) {
        cmd << "\"" << util::escape_shell_arg(f) << "\" ";
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
    std::string stdlib;  // 1.1.0-dev.4: standard library (libstdc++ / libc++)
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
    st.stdlib = cfg.project.stdlib;  // 1.1.0-dev.4
    st.tc = toolchain::detect_toolchain();
    st.is_msvc = (st.tc.family == toolchain::CompilerFamily::Msvc);
    if (!st.is_msvc) {
        st.lang.detected_compiler = detect_compiler(
            st.lang.compiler == "g++" ? "C++" : "C");
    }

    // Apply build profile
    st.compile_cfg = cfg.compile;
    st.link_cfg = cfg.link;
    // 1.2.0-dev.3: no explicit --profile → fall back to [compile].default_profile (if set)
    std::string active_profile = opts.profile;
    if (active_profile.empty()) active_profile = cfg.compile.default_profile;
    if (!active_profile.empty()) {
        auto it = cfg.compile_profiles.find(active_profile);
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

            auto matches = util::closest_match(active_profile, profile_names, 2);
            if (!matches.empty()) {
                std::string suggestion = matches[0];
                for (size_t i = 1; i < matches.size() && i < 3; ++i)
                    suggestion += ", " + matches[i];
                util::fatal(std::string("unknown profile: '") + active_profile +
                            "'. Did you mean: " + suggestion + "?");
            }

            if (profile_names.empty()) {
                util::fatal(std::string("unknown profile: '") + active_profile +
                            "'. No profiles defined in ezmk.toml.");
            } else {
                std::string avail;
                for (size_t i = 0; i < profile_names.size(); ++i) {
                    if (i > 0) avail += ", ";
                    avail += profile_names[i];
                }
                util::fatal(std::string("unknown profile: '") + active_profile +
                            "'. Available: " + avail);
            }
        }
        auto lit = cfg.link_profiles.find(active_profile);
        if (lit != cfg.link_profiles.end()) {
            st.link_cfg = merge_link_profile(st.link_cfg, lit->second);
        }
    }

    // 1.1.0: deterministic build — resolve SOURCE_DATE_EPOCH
    if (st.compile_cfg.deterministic && st.compile_cfg.source_date_epoch == 0) {
        // Priority: env → git HEAD commit time → ezmk.toml mtime (fallback)
        const char* env_sde = std::getenv("SOURCE_DATE_EPOCH");
        if (env_sde && env_sde[0]) {
            st.compile_cfg.source_date_epoch = static_cast<uint64_t>(std::stoull(env_sde));
        } else {
            // Try git HEAD commit timestamp
            std::error_code ec;
            auto git_dir = st.proj_root / ".git";
            if (fs::exists(git_dir, ec)) {
                auto git_res = util::run_command("git log -1 --format=%ct 2>&1");
                if (git_res.exit_code == 0 && !git_res.out.empty()) {
                    try {
                        st.compile_cfg.source_date_epoch = static_cast<uint64_t>(std::stoull(git_res.out));
                    } catch (...) {}
                }
            }
            // Fallback: ezmk.toml modification time
            if (st.compile_cfg.source_date_epoch == 0) {
                auto toml_path = st.proj_root / "ezmk.toml";
                if (fs::exists(toml_path, ec)) {
                    auto ftime = fs::last_write_time(toml_path, ec);
                    if (!ec) {
                        auto sctp = std::chrono::system_clock::from_time_t(
                            std::chrono::system_clock::to_time_t(
                                std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                                    ftime - fs::file_time_type::clock::now()
                                    + std::chrono::system_clock::now())));
                        st.compile_cfg.source_date_epoch = static_cast<uint64_t>(
                            std::chrono::system_clock::to_time_t(sctp));
                    }
                }
            }
        }
    }

    // Collect sources + validate
    collect_sources(st.compile_cfg.src_dirs, st.proj_root, cfg.project.type);

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
    // 0.9.6+: Track installed versions for constraint validation
    std::map<std::string, std::string> installed_versions;
    fs::path pkg_dir = st.proj_root / ".ezmk/pkg";
    if (util::file_exists(pkg_dir)) {
        for (auto& entry : fs::directory_iterator(pkg_dir)) {
            if (!entry.is_directory()) continue;
            auto pkg_toml = entry.path() / "ezmk.toml";
            if (util::file_exists(pkg_toml)) {
                try {
                    auto pkg_cfg = config::parse_config(pkg_toml);
                    installed_pkgs.insert(pkg_cfg.project.name);
                    installed_versions[pkg_cfg.project.name] = pkg_cfg.project.version;
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
            // 0.9.7+: also collect precompiled archives from lib/
            // 1.1.0-dev.2: platform-aware — select only the archive for current platform
            auto pkg_lib = entry.path() / "lib";
            if (util::file_exists(pkg_lib)) {
                try {
                    auto archive = pkg::select_precompiled_archive(pkg_lib,
                        entry.path().filename().string());
                    st.pkg_archives.push_back(archive);
                } catch (const std::exception& e) {
                    util::warn(std::string("skipping precompiled archive for '") +
                               entry.path().filename().string() + "': " + e.what());
                }
            }
        }
    }

    // want.lib: process optional dependencies
    {
        std::set<std::string> lib_set;
        for (auto& entry : cfg.depends.libs) lib_set.insert(entry.name);
        for (auto& want_entry : cfg.depends.want) {
            auto& want_name = want_entry.name;
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

    // 1.1.0-dev.7: Hard dependency pre-check — catch missing deps before
    // the compiler produces cryptic "file not found" / "cannot find -l" errors.
    {
        std::vector<std::string> missing_hard_deps;
        for (auto& entry : cfg.depends.libs) {
            if (installed_pkgs.find(entry.name) == installed_pkgs.end()) {
                missing_hard_deps.push_back(entry.name);
            }
        }

        if (!missing_hard_deps.empty()) {
            // Check registered repos to see which ones are installable
            std::vector<std::string> installable;
            for (auto& name : missing_hard_deps) {
                if (pkg::package_available(name)) {
                    installable.push_back(name);
                }
            }

            std::string msg = ezmk::i18n::get(ezmk::i18n::I18nKey::missing_dep_at_build);
            for (size_t i = 0; i < missing_hard_deps.size(); ++i) {
                if (i > 0) msg += ", ";
                msg += "'" + missing_hard_deps[i] + "'";
            }
            msg += ".";

            if (!installable.empty()) {
                msg += " " + ezmk::i18n::get(ezmk::i18n::I18nKey::missing_dep_install_hint);
                for (size_t i = 0; i < installable.size(); ++i) {
                    if (i > 0) msg += " ";
                    msg += installable[i];
                }
            } else {
                msg += " " + ezmk::i18n::get(ezmk::i18n::I18nKey::missing_dep_no_repo);
            }

            util::fatal(msg);
        }
    }

    // 0.9.6+: Validate installed package versions against declared constraints
    for (auto& entry : cfg.depends.libs) {
        if (entry.constraint.op == config::VersionConstraint::None) continue;
        auto it = installed_versions.find(entry.name);
        if (it == installed_versions.end()) continue; // not installed → handled by missing_dep later
        if (!pkg::satisfies_version_constraint(it->second, entry.constraint)) {
            util::fatal(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_constraint_unsatisfied,
                        {{"pkg", entry.name},
                         {"constraint", entry.constraint.version},
                         {"available", it->second}}));
        }
    }
    for (auto& entry : cfg.depends.want) {
        if (entry.constraint.op == config::VersionConstraint::None) continue;
        auto it = installed_versions.find(entry.name);
        if (it == installed_versions.end()) continue; // want dep not installed is OK
        if (!pkg::satisfies_version_constraint(it->second, entry.constraint)) {
            util::warn(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_constraint_unsatisfied,
                       {{"pkg", entry.name},
                        {"constraint", entry.constraint.version},
                        {"available", it->second}}));
        }
    }

    // 1.1.0: lockfile verification
    auto lf = lockfile::load(st.proj_root);
    if (lf.has_value()) {
        if (lockfile::depends_changed(cfg, *lf)) {
            util::warn(ezmk::i18n::get(ezmk::i18n::I18nKey::lock_depends_changed));
        }
        auto mismatches = lockfile::verify(st.proj_root, *lf);
        if (!mismatches.empty()) {
            if (st.compile_cfg.deterministic) {
                std::string names;
                for (auto& m : mismatches) {
                    if (!names.empty()) names += ", ";
                    names += m;
                }
                util::fatal(ezmk::i18n::fmt(ezmk::i18n::I18nKey::lock_integrity_fail,
                                             {{"pkgs", names}}));
            } else {
                for (auto& m : mismatches) {
                    util::warn(ezmk::i18n::fmt(ezmk::i18n::I18nKey::lock_sha256_mismatch,
                                                {{"pkg", m}}));
                }
            }
        }
    } else if (st.compile_cfg.deterministic) {
        util::fatal(ezmk::i18n::get(ezmk::i18n::I18nKey::lock_missing_deterministic));
    }

    // Pre-build hook
    run_hook(cfg.hooks.pre_build, st.proj_root, "" /* no output yet */,
             opts.profile, ezmk::i18n::I18nKey::pre_build_hook);

    return st;
}

// 1.1.1: Build a cache::CompileInput from prepared build state.
// Shared by the build's compile_phase and compile_commands.json generation
// (prepare_compile_input), so the index can never drift from a real build.
cache::CompileInput make_compile_input(BuildState& st, const cli::BuildOptions& opts) {
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
    cin.stdlib = st.stdlib;  // 1.1.0-dev.4
    return cin;
}

// 1.2.0-dev.6: per-file compile time diagnostics.
// Slow-build auto-detection threshold (seconds of total wall-clock elapsed) and
// the number of slowest units shown in the default (non-verbose) path. Kept as
// named constants, not config fields, to preserve the zero-config surface.
constexpr double BUILD_TIME_SLOW_THRESHOLD = 5.0;
constexpr int BUILD_TIME_TOP_N = 10;

// Phase 2: Compile all sources (cache check + compilation).
// Returns the list of compiled object paths.
std::vector<fs::path> compile_phase(BuildState& st, const cli::BuildOptions& opts) {
    // Load cache
    auto record = cache::load_record();

    // 1.1.0: check compiler version — if changed, invalidate all caches
    std::string compiler_name = st.tc.cxx_compiler.filename().string();
    if (!record.compiler_version.empty() && record.compiler_version != st.tc.version) {
        util::info(ezmk::i18n::I18nKey::cache_compiler_changed);
        record.files.clear();
    }
    record.compiler = compiler_name;
    record.compiler_version = st.tc.version;

    // 1.1.0: track deterministic flag — toggling triggers full rebuild
    if (record.deterministic != st.compile_cfg.deterministic) {
        if (!record.files.empty()) {
            util::info(ezmk::i18n::I18nKey::compile_options_changed);
            record.files.clear();
        }
        record.deterministic = st.compile_cfg.deterministic;
    }

    auto cur_sig = cache::compile_options_signature(st.compile_cfg, st.extra_includes,
                                                    st.lang.std_flag,
                                                    st.stdlib, st.use_pic);
    // 1.1.0: deterministic build — include lockfile hash in signature
    if (st.compile_cfg.deterministic) {
        auto lock_path = st.proj_root / "ezmk.lock";
        if (util::file_exists(lock_path)) {
            cur_sig += ":" + crypto::sha256_file(lock_path);
        }
    }
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
    auto cin = make_compile_input(st, opts);

    int num_jobs = opts.jobs;
    if (num_jobs <= 0) {
        num_jobs = static_cast<int>(std::thread::hardware_concurrency());
        if (num_jobs <= 0) num_jobs = 1;
    }

    cache::CompileResult comp_result;
    std::vector<cache::SingleCompileResult> single_results;
    single_results.reserve(cin.sources.size());
    // 1.2.0-dev.6: per-file compile times (ms), indexed by source so each
    // parallel worker writes a distinct element — no cross-thread
    // synchronization needed; the future's get() below orders the reads after
    // the write. Only the parallel path populates it.
    std::vector<double> compile_times(cin.sources.size(), 0.0);
    auto build_start = std::chrono::steady_clock::now();

    // INVARIANT: In parallel mode, compile_one_source() only reads from
    // `record` (const ref). record.files is updated below after all
    // threads complete, so no concurrent write occurs.
    if (num_jobs > 1 && cin.sources.size() > 1) {
        // 0.9.6+: Use \r in-place refresh when non-verbose and stderr is a TTY
        bool use_inplace = !opts.verbose && util::stderr_is_tty();
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
            futures.push_back(pool.submit([&cin, &record, &task_index, total, i, use_inplace, &compile_times]() {
                auto idx = task_index.fetch_add(1) + 1;
                auto t0 = std::chrono::steady_clock::now();
                auto result = cache::compile_one_source(cin.sources[i], cin, record);
                compile_times[i] = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0).count();
                // 0.9.6+: Always show progress in parallel mode
                std::string msg = std::string("[") + std::to_string(idx) +
                    "/" + std::to_string(total) + "] " + result.rel_src;
                if (result.cache_hit) {
                    msg += "  (cached)";
                }
                if (use_inplace) {
                    util::progress(msg);
                } else {
                    util::info(msg);
                }
                return result;
            }));
        }
        for (auto& f : futures) {
            single_results.push_back(f.get());
        }
        // 0.9.6+: Move to next line after in-place progress
        if (use_inplace) {
            util::progress_newline();
        }
    } else {
        comp_result = cache::compile_sources(cin, record);
    }

    // Process parallel results
    if (!single_results.empty()) {
        bool has_failure = false;
        int error_count = 0;
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
                ++error_count;
                util::error(sr.error_msg);
            }
        }
        if (has_failure) {
            // 0.9.6+: Build failure summary before fatal exit
            std::string summary = std::to_string(comp_result.cache_hits) + " cached, " +
                                  std::to_string(comp_result.cache_misses) + " compiled, " +
                                  std::to_string(error_count) + " error(s)";
            util::error(ezmk::i18n::fmt(ezmk::i18n::I18nKey::build_failed_summary,
                                        {{"summary", summary}}));
            util::fatal(ezmk::i18n::I18nKey::build_failed);
        }
    }

    cache::save_record(record);

    // 0.9.6+: Build completion summary with elapsed time
    {
        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - build_start).count();
        if (comp_result.cache_hits > 0 || comp_result.cache_misses > 0) {
            util::info(ezmk::i18n::I18nKey::cache_summary,
                       {{"cached", std::to_string(comp_result.cache_hits)},
                        {"compiled", std::to_string(comp_result.cache_misses)}});
        }
        // 0.9.6+: Always show total elapsed time in parallel mode
        if (num_jobs > 1 && cin.sources.size() > 1) {
            std::ostringstream elapsed_str;
            elapsed_str << std::fixed << std::setprecision(1) << elapsed << "s";
            util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::build_elapsed_time,
                        {{"time", elapsed_str.str()}}));
        }

        // 1.2.0-dev.6: per-file compile time detail (parallel path only).
        // Only cache-miss files are timed (cache hits cost ~0); serial path
        // (`compile_sources`) has no per-file timing and shows no detail.
        if (num_jobs > 1 && cin.sources.size() > 1 && !single_results.empty()) {
            struct TimeEntry { std::string rel_src; double ms; };
            std::vector<TimeEntry> timed;
            timed.reserve(single_results.size());
            for (size_t i = 0; i < single_results.size(); ++i) {
                if (!single_results[i].cache_hit) {
                    timed.push_back(TimeEntry{single_results[i].rel_src, compile_times[i]});
                }
            }
            if (!timed.empty()) {
                std::sort(timed.begin(), timed.end(),
                          [](const TimeEntry& a, const TimeEntry& b) { return a.ms > b.ms; });
                double total_ms = 0.0;
                for (const auto& t : timed) total_ms += t.ms;
                auto fmt_secs = [](double ms) {
                    std::ostringstream os;
                    os << std::fixed << std::setprecision(1) << (ms / 1000.0) << "s";
                    return os.str();
                };
                size_t compiled = timed.size();
                // -v: full sorted detail; default: top-N only when build is slow.
                if (opts.verbose || elapsed > BUILD_TIME_SLOW_THRESHOLD) {
                    size_t limit = opts.verbose
                        ? compiled
                        : std::min(static_cast<size_t>(BUILD_TIME_TOP_N), compiled);
                    util::info(ezmk::i18n::I18nKey::build_time_header,
                               {{"compiled", std::to_string(compiled)},
                                {"time", fmt_secs(total_ms)}});
                    for (size_t i = 0; i < limit; ++i) {
                        util::info(ezmk::i18n::I18nKey::build_time_entry,
                                   {{"time", fmt_secs(timed[i].ms)},
                                    {"file", timed[i].rel_src}});
                    }
                    if (!opts.verbose && limit < compiled) {
                        util::info(ezmk::i18n::I18nKey::build_time_truncated,
                                   {{"shown", std::to_string(limit)},
                                    {"compiled", std::to_string(compiled)}});
                    }
                }
            }
        }
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

    // 1.1.2 C1: check the atomic-rename result — a failed rename (locked target)
    // previously still printed build_success with a stale/missing output.
    util::atomic_rename(output_tmp, output);
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

    // 1.1.1: progress message lives here (not in prepare_build_state) so
    // compile-db reuse via prepare_compile_input() doesn't print it.
    util::info(ezmk::i18n::I18nKey::building,
               {{"name", cfg.project.name},
                {"type", cfg.project.type},
                {"lang", cfg.project.language}});

    // Phase 2: Compile all sources
    auto objects = compile_phase(st, opts);

    // Phase 3: Link
    auto output = link_phase(st, objects, opts, cfg);

    // 1.1.1: auto-generate compile_commands.json after a successful link.
    // Reuses the build-time state (make_compile_input) so the index matches
    // this build's flags/includes/profile entry-for-entry. Failure is
    // non-blocking (warning) — the build itself already succeeded.
    if (st.compile_cfg.compile_commands || opts.compile_commands) {
        try {
            auto cin = make_compile_input(st, opts);
            compile_db::generate_compile_db(cin, st.proj_root, {});
        } catch (const std::exception& e) {
            util::warn(ezmk::i18n::I18nKey::compile_db_generate_failed,
                       {{"msg", e.what()}});
        }
    }

    // Post-build hook
    run_hook(cfg.hooks.post_build, st.proj_root, output, opts.profile,
             ezmk::i18n::I18nKey::post_build_hook);

    return output;
}

// 1.1.1: Prepare a cache::CompileInput exactly as a real build would — include
// collection, profile merge, macro folding, package extra_includes. Shared by
// compile_commands.json generation (`ezmk utils cc` interception).
cache::CompileInput prepare_compile_input(const config::EzConfig& cfg,
                                          const cli::BuildOptions& opts) {
    auto st = prepare_build_state(cfg, opts);
    return make_compile_input(st, opts);
}

// 1.1.0: install_project — copy build artifacts to install prefix
void install_project(const config::EzConfig& cfg,
                     const cli::ProjectInstallOptions& opts,
                     const fs::path& proj_root) {
    auto& inst = cfg.install;

    // Resolve prefix (CLI override takes priority)
    fs::path prefix = opts.prefix.empty() ? fs::path(inst.prefix) : fs::path(opts.prefix);

    // Resolve subdirectories
    auto bindir     = prefix / inst.bindir;
    auto libdir     = prefix / inst.libdir;
    auto includedir = prefix / inst.includedir;

    std::string name = cfg.project.name;
    std::string type = cfg.project.type;
    fs::path build_dir = proj_root / "build";

    fs::create_directories(bindir);
    fs::create_directories(libdir);
    if (!opts.no_headers) fs::create_directories(includedir / name);

    int files_copied = 0;
    size_t total_bytes = 0;

    auto copy_file = [&](const fs::path& src, const fs::path& dst) {
        if (!util::file_exists(src)) {
            util::warn(std::string("artifact not found, skipping: ") + src.string());
            return;
        }
        if (opts.verbose) {
            util::info(std::string("  ") + dst.string());
        }
        if (!opts.dry_run) {
            std::error_code ec;
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                util::warn(std::string("failed to copy: ") + src.string() + " → " + dst.string() + ": " + ec.message());
            } else {
                files_copied++;
                total_bytes += fs::file_size(dst, ec);
            }
        } else {
            files_copied++;
        }
    };

    if (type == "executable") {
    #ifdef EZMK_WIN
        fs::path exe_src = build_dir / (name + ".exe");
        fs::path exe_dst = bindir / (name + ".exe");
    #else
        fs::path exe_src = build_dir / name;
        fs::path exe_dst = bindir / name;
    #endif
        copy_file(exe_src, exe_dst);
    } else if (type == "static") {
        // Try both .a and .lib
        for (auto& ext : {".a", ".lib"}) {
            fs::path lib_src = build_dir / ("lib" + name + ext);
            if (util::file_exists(lib_src)) {
                copy_file(lib_src, libdir / ("lib" + name + ext));
                break;
            }
        }
    } else if (type == "shared") {
    #ifdef EZMK_WIN
        fs::path dll_src = build_dir / (name + ".dll");
        fs::path lib_src = build_dir / (name + ".lib");
        if (util::file_exists(dll_src)) copy_file(dll_src, bindir / (name + ".dll"));
        if (util::file_exists(lib_src)) copy_file(lib_src, libdir / (name + ".lib"));
    #else
        fs::path so_src = build_dir / ("lib" + name + ".so");
        copy_file(so_src, libdir / ("lib" + name + ".so"));
    #endif
    }

    // Copy headers
    if (!opts.no_headers) {
        auto include_src = proj_root / "include";
        if (util::file_exists(include_src)) {
            auto include_dst = includedir / name;
            fs::create_directories(include_dst);
            for (auto& entry : fs::recursive_directory_iterator(include_src)) {
                if (entry.is_regular_file()) {
                    auto rel = fs::relative(entry.path(), include_src);
                    auto dst = include_dst / rel;
                    fs::create_directories(dst.parent_path());
                    copy_file(entry.path(), dst);
                }
            }
        }
    }

    // Summary
    std::string size_str;
    if (!opts.dry_run && total_bytes > 0) {
        if (total_bytes < 1024) size_str = ", " + std::to_string(total_bytes) + " B";
        else if (total_bytes < 1024 * 1024) size_str = ", " + std::to_string(total_bytes / 1024) + " KB";
        else size_str = ", " + std::to_string(total_bytes / (1024 * 1024)) + " MB";
    }
    util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::install_summary,
        {{"name", name},
         {"version", cfg.project.version},
         {"prefix", prefix.string()},
         {"files", std::to_string(files_copied)},
         {"size", size_str}}));
}

// 1.1.0-dev.2: pack_project — create a distributable .tar.gz from a static project
void pack_project(const config::EzConfig& cfg,
                  const cli::ProjectPackOptions& opts,
                  const fs::path& proj_root) {
    std::string name = cfg.project.name;
    std::string version = cfg.project.version;

    // Step 1: Validate project type
    if (cfg.project.type != "static") {
        util::fatal("project pack requires type=\"static\", got type=\"" +
                     cfg.project.type + "\"");
    }

    // Step 2: Build the project if needed
    auto archive_path = proj_root / "build" / ("lib" + name + ".a");
    // On MSVC, the built archive may be .lib
    if (!util::file_exists(archive_path)) {
        auto alt = proj_root / "build" / ("lib" + name + ".lib");
        if (util::file_exists(alt)) archive_path = alt;
    }

    if (!util::file_exists(archive_path)) {
        util::info("building project before packing...");
        cli::BuildOptions build_opts;
        build_opts.jobs = 0; // auto-detect
        build_project(cfg, build_opts);

        // Re-check
        if (!util::file_exists(archive_path)) {
            auto alt = proj_root / "build" / ("lib" + name + ".lib");
            if (util::file_exists(alt)) archive_path = alt;
        }
        if (!util::file_exists(archive_path)) {
            util::fatal("build did not produce lib" + name + ".a/.lib — "
                        "cannot pack");
        }
    }

    // Step 3: Create staging directory
    fs::path output_dir = fs::absolute(opts.output_dir);
    std::string archive_name = name + "-" + version + ".tar.gz";
    std::string stage_name = name + "-" + version;
    fs::path stage_dir = output_dir / stage_name;

    util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pack_collecting,
                                {{"name", name}, {"version", version}}));
    // Clean up any leftover staging (best-effort)
    if (util::file_exists(stage_dir)) {
        std::error_code ec;
        fs::remove_all(stage_dir, ec);
    }
    fs::create_directories(stage_dir / "lib");

    // Step 4: Copy files
    // include/
    auto include_src = proj_root / "include";
    if (util::file_exists(include_src)) {
        util::copy_recursive(include_src, stage_dir / "include");
    }

    // lib/lib<name>.a
    auto lib_dst = stage_dir / "lib" / archive_path.filename();
    fs::copy_file(archive_path, lib_dst);

    // ezmk.toml
    fs::copy_file(proj_root / "ezmk.toml", stage_dir / "ezmk.toml");

    // Step 5: Create archive
    fs::path output_archive = output_dir / archive_name;
    util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pack_creating,
                                {{"file", archive_name}}));
    util::create_targz(stage_dir, output_archive);

    // Step 6: SHA-256
    std::string sha = crypto::sha256_file(output_archive);
    util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pack_sha256,
                                {{"sha256", sha}}));

    // Step 7: Cleanup staging (best-effort — failure to remove a temp dir should
    // not fail the pack command)
    { std::error_code ec; fs::remove_all(stage_dir, ec); }

    // Step 8: Verbose output — index.toml snippet
    if (opts.verbose) {
        util::info_line("-- index.toml entry --");
        std::string entry = "[[packages]]\n"
                            "name = \"" + name + "\"\n"
                            "version = \"" + version + "\"\n"
                            "file = \"" + archive_name + "\"\n"
                            "platform = \"" + util::detect_platform_tag() + "\"\n"
                            "sha256 = \"" + sha + "\"\n";
        util::info_line(entry);
    }

    util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pack_success,
                                {{"file", (output_dir / archive_name).string()}}));
}

// ===================================================================
// 1.1.0-dev.6: Test runner
// ===================================================================

namespace {

// Detect Catch2 availability. Returns the include path (empty if not found).
// Priority: 1) project-scope installed catch2 (from [depends])
//           2) include/vendor/catch2.hpp (single-header)
//           3) user/global-scope installed catch2
//           4) error (not found)
std::string detect_catch2(const fs::path& proj_root,
                           const config::DependsSection& depends) {
    // 1. Check project-scope installed catch2 (from [depends] or manual install)
    fs::path proj_pkg = proj_root / ".ezmk/pkg/catch2";
    if (util::file_exists(proj_pkg)) {
        auto inc = proj_pkg / "include";
        if (util::file_exists(inc)) return inc.string();
    }

    // 2. Check for single-header in include/vendor/
    fs::path vendor_hpp = proj_root / "include/vendor/catch2.hpp";
    if (util::file_exists(vendor_hpp)) {
        return (proj_root / "include/vendor").string();
    }

    // 3. Check user/global scopes for installed catch2
    auto home = util::get_home_dir();
#ifdef EZMK_WIN
    const char* appdata = std::getenv("LOCALAPPDATA");
    fs::path local_ezmk = appdata ? fs::path(appdata) / "ezmk" : home / "AppData/Local/ezmk";
    std::vector<fs::path> system_dirs = {
        home / ".ezmk/pkg/catch2",
        local_ezmk / "pkg/catch2",
    };
#else
    std::vector<fs::path> system_dirs = {
        home / ".ezmk/pkg/catch2",
        home / ".local/share/ezmk/pkg/catch2",
    };
#endif
    for (auto& scope : system_dirs) {
        if (util::file_exists(scope)) {
            auto inc = scope / "include";
            if (util::file_exists(inc)) return inc.string();
        }
    }

    // 4. Not found
    return {};
}

// Check if a test source file defines its own main() or CATCH_CONFIG_MAIN.
bool has_user_main(const fs::path& src_path) {
    auto content = util::file_read(src_path);
    // Check for CATCH_CONFIG_MAIN
    if (content.find("CATCH_CONFIG_MAIN") != std::string::npos) return true;
    // Check for main() function definition (simple heuristic)
    if (content.find("int main(") != std::string::npos) return true;
    if (content.find("void main(") != std::string::npos) return true;
    return false;
}

// Collect test source files from test directories.
std::vector<fs::path> collect_test_sources(
    const std::vector<std::string>& test_dirs,
    const fs::path& proj_root) {
    std::vector<fs::path> result;
    for (auto& d : test_dirs) {
        fs::path dir = d;
        if (dir.is_relative()) dir = proj_root / dir;
        if (!util::file_exists(dir)) {
            util::warn(std::string("test directory not found, skipping: ") + d);
            continue;
        }
        auto files = util::list_files(dir, {".c", ".cc", ".cpp", ".cxx"});
        for (auto& f : files) {
            result.push_back(f);
        }
    }
    return result;
}

// Simple XML parser for Catch2 output.
// Extracts test case results from Catch2 XML.
struct Catch2TestResult {
    std::string name;
    std::string filename;
    int line = 0;
    bool passed = true;
    std::string failure_msg;
};

std::vector<Catch2TestResult> parse_catch2_xml(const std::string& xml) {
    std::vector<Catch2TestResult> results;

    // Simple state-machine XML parser for Catch2 output format:
    // <TestCase name="..." filename="..." line="...">
    //   <OverallResult success="true|false"/>
    //   <Failure>...</Failure> or <Expression>...</Expression>
    // </TestCase>

    size_t pos = 0;
    while (true) {
        auto tc_start = xml.find("<TestCase", pos);
        if (tc_start == std::string::npos) break;

        auto tc_end = xml.find("</TestCase>", tc_start);
        if (tc_end == std::string::npos) break;

        std::string tc_block = xml.substr(tc_start, tc_end - tc_start + 12);
        pos = tc_end + 12;

        Catch2TestResult r;

        // Extract name
        auto name_pos = tc_block.find("name=\"");
        if (name_pos != std::string::npos) {
            name_pos += 6;
            auto name_end = tc_block.find("\"", name_pos);
            if (name_end != std::string::npos) {
                r.name = tc_block.substr(name_pos, name_end - name_pos);
            }
        }

        // Extract filename
        auto file_pos = tc_block.find("filename=\"");
        if (file_pos != std::string::npos) {
            file_pos += 10;
            auto file_end = tc_block.find("\"", file_pos);
            if (file_end != std::string::npos) {
                r.filename = tc_block.substr(file_pos, file_end - file_pos);
            }
        }

        // Extract line
        auto line_pos = tc_block.find("line=\"");
        if (line_pos != std::string::npos) {
            line_pos += 6;
            auto line_end = tc_block.find("\"", line_pos);
            if (line_end != std::string::npos) {
                try { r.line = std::stoi(tc_block.substr(line_pos, line_end - line_pos)); }
                catch (...) {}
            }
        }

        // Check success
        if (tc_block.find("success=\"false\"") != std::string::npos ||
            tc_block.find("success=\"no\"") != std::string::npos) {
            r.passed = false;
        } else if (tc_block.find("success=\"true\"") != std::string::npos ||
                   tc_block.find("success=\"yes\"") != std::string::npos) {
            r.passed = true;
        } else {
            // No success attribute → passed if no Failure/Expression with failure
            auto fail_pos = tc_block.find("<Failure");
            auto expr_pos = tc_block.find("<Expression");
            auto section_pos = tc_block.find("<Section");
            if (fail_pos != std::string::npos && fail_pos < tc_block.find("</OverallResult>")) {
                r.passed = false;
            }
        }

        // Extract failure details
        if (!r.passed) {
            auto fail_start = tc_block.find("<Failure");
            if (fail_start != std::string::npos) {
                auto fail_content_start = tc_block.find(">", fail_start);
                auto fail_content_end = tc_block.find("</Failure>", fail_start);
                if (fail_content_start != std::string::npos && fail_content_end != std::string::npos) {
                    r.failure_msg = tc_block.substr(fail_content_start + 1,
                                                     fail_content_end - fail_content_start - 1);
                    // Trim whitespace
                    auto b = r.failure_msg.find_first_not_of(" \t\n\r");
                    auto e = r.failure_msg.find_last_not_of(" \t\n\r");
                    if (b != std::string::npos) {
                        r.failure_msg = r.failure_msg.substr(b, e - b + 1);
                    }
                }
            }
            // Also check Expression with success="false"
            auto expr_start = tc_block.find("<Expression");
            while (expr_start != std::string::npos) {
                auto expr_end = xml.find(">", expr_start);
                auto expr_close = xml.find("/>", expr_start);
                auto expr_full_end = xml.find("</Expression>", expr_start);
                std::string expr_block = xml.substr(expr_start,
                    std::min({expr_end != std::string::npos ? expr_end : std::string::npos,
                              expr_close != std::string::npos ? expr_close : std::string::npos,
                              expr_full_end != std::string::npos ? expr_full_end : std::string::npos})
                    - expr_start + 2);
                if (expr_block.find("success=\"false\"") != std::string::npos) {
                    // Extract expression content
                    auto content_start = xml.find(">", expr_start);
                    auto content_end = xml.find("</Expression>", expr_start);
                    if (content_start != std::string::npos && content_end != std::string::npos) {
                        std::string expr_content = xml.substr(content_start + 1,
                                                               content_end - content_start - 1);
                        auto b2 = expr_content.find_first_not_of(" \t\n\r");
                        auto e2 = expr_content.find_last_not_of(" \t\n\r");
                        if (b2 != std::string::npos) {
                            expr_content = expr_content.substr(b2, e2 - b2 + 1);
                        }
                        if (!r.failure_msg.empty()) r.failure_msg += "\n";
                        r.failure_msg += "  assertion: " + expr_content;
                    }
                    // Also get expected/actual
                }
                expr_start = xml.find("<Expression", expr_start + 11);
            }
        }

        results.push_back(r);
    }

    return results;
}

} // anonymous namespace

void run_tests(const config::EzConfig& cfg,
               const std::string& test_framework_override,
               const std::string& test_filter,
               bool verbose) {
    fs::path proj_root = fs::current_path();
    fs::path build_dir = proj_root / "build";
    fs::path cache_dir = proj_root / ".ezmk/cache";

    // Determine framework (CLI override takes priority)
    std::string framework = test_framework_override.empty()
        ? cfg.test.framework
        : config::normalize_lang(test_framework_override);

    // Build project first if needed
    bool project_built = false;
    auto check_built = [&]() -> bool {
        if (cfg.project.type == "executable") {
#ifdef EZMK_WIN
            return util::file_exists(build_dir / (cfg.project.name + ".exe"));
#else
            return util::file_exists(build_dir / cfg.project.name);
#endif
        } else if (cfg.project.type == "static") {
            return util::file_exists(build_dir / ("lib" + cfg.project.name + ".a")) ||
                   util::file_exists(build_dir / ("lib" + cfg.project.name + ".lib"));
        } else if (cfg.project.type == "shared") {
            // Check the actual shared-library artifact, not the import library —
            // a dangling .lib with a deleted .dll must still trigger a rebuild.
#ifdef EZMK_WIN
            return util::file_exists(build_dir / (cfg.project.name + ".dll")) ||   // MSVC
                   util::file_exists(build_dir / ("lib" + cfg.project.name + ".dll")); // MinGW
#else
            return util::file_exists(build_dir / ("lib" + cfg.project.name + ".so"));
#endif
        }
        // utils (Lua-tool package): there is no compiled artifact to check and
        // building produces nothing meaningful — skip the build-first step.
        return true;
    };

    if (!check_built()) {
        util::info("project not built — building first...");
        cli::BuildOptions build_opts;
        build_opts.jobs = 0; // auto-detect
        try {
            build_project(cfg, build_opts);
            project_built = true;
        } catch (const std::exception& e) {
            util::fatal(std::string("project build failed, cannot run tests: ") + e.what());
        }
    }

    // Collect test sources
    auto test_sources = collect_test_sources(cfg.test.dirs, proj_root);
    if (test_sources.empty()) {
        util::fatal("no test source files found in test directories. "
                     "Configure test.dirs in ezmk.toml.");
    }

    // Get project object files (from .ezmk/temp/)
    std::vector<fs::path> project_objs;
    fs::path temp_dir = proj_root / ".ezmk/temp";
    if (util::file_exists(temp_dir)) {
        for (auto& entry : fs::directory_iterator(temp_dir)) {
            auto& p = entry.path();
            if (p.extension() == ".o" || p.extension() == ".obj") {
                // Exclude main.o (test runner provides its own main)
                auto fname = p.filename().string();
                if (fname != "main.o" && fname != "main.obj") {
                    project_objs.push_back(p);
                }
            }
        }
    }

    // Collect project include dirs and flags
    auto lang_info = config::parse_language(cfg.project.language);

    // Detect compiler
    std::string compiler;
    auto tc = toolchain::detect_toolchain();
    bool is_msvc = (tc.family == toolchain::CompilerFamily::Msvc);
    if (!is_msvc) {
        compiler = detect_compiler(lang_info.compiler == "g++" ? "C++" : "C");
    } else {
        compiler = tc.cxx_compiler.string();
    }

    // Build compile flags from project config
    std::vector<std::string> base_flags;
    base_flags.push_back(lang_info.std_flag);
    if (cfg.compile.ezmk_macros) {
        auto em = generate_ezmk_macros(cfg);
        base_flags.insert(base_flags.end(), em.begin(), em.end());
    }
    for (auto& f : cfg.compile.flags) base_flags.push_back(f);
    auto macro_flags = macros_to_flags(cfg.compile.macros);
    for (auto& f : macro_flags) base_flags.push_back(f);

    // Add include dirs
    for (auto& d : cfg.compile.include_dirs) {
        fs::path inc = d;
        if (inc.is_relative()) inc = proj_root / inc;
        if (util::file_exists(inc)) {
            base_flags.push_back("-I" + inc.string());
        }
    }

    // Add test flags
    for (auto& f : cfg.test.flags) {
        base_flags.push_back(f);
    }

    // Ensure cache dirs exist
    fs::create_directories(cache_dir / "obj");
    fs::create_directories(build_dir);

    auto run_start = std::chrono::steady_clock::now();

    if (framework == "CATCH2") {
        // ---- Catch2 Mode ----
        util::info("Running tests (Catch2)...");

        // Detect Catch2
        std::string catch2_inc = detect_catch2(proj_root, cfg.depends);
        if (catch2_inc.empty()) {
            util::fatal("Catch2 not found. Install it with:\n"
                        "  ezmk pkg install catch2\n"
                        "or place catch2.hpp in include/vendor/catch2.hpp");
        }
        util::info(std::string("  Catch2 include: ") + catch2_inc);

        // Check if any test file has user-defined main
        bool user_has_main = false;
        for (auto& ts : test_sources) {
            if (has_user_main(ts)) {
                user_has_main = true;
                break;
            }
        }

        // Generate test_main.cpp if needed
        fs::path test_main_cpp;
        if (!user_has_main) {
            test_main_cpp = cache_dir / "test_main.cpp";
            std::string main_content;
            // Use appropriate include depending on Catch2 version
            fs::path vendor_hpp = proj_root / "include/vendor/catch2.hpp";
            if (util::file_exists(vendor_hpp)) {
                // Single-header Catch2 (v2 style): CATCH_CONFIG_MAIN still generates main
                main_content += "#define CATCH_CONFIG_MAIN\n";
                main_content += "#include \"catch2.hpp\"\n";
            } else {
                // Multi-header Catch2 v3: CATCH_CONFIG_MAIN removed → explicit main + Session().run()
                main_content += "#include <catch2/catch_session.hpp>\n";
                main_content += "int main(int argc, char* argv[]) {\n";
                main_content += "    return Catch::Session().run(argc, argv);\n";
                main_content += "}\n";
            }
            // Only write if content changed (avoid unnecessary recompilation)
            bool write_needed = true;
            if (util::file_exists(test_main_cpp)) {
                auto existing = util::file_read(test_main_cpp);
                if (existing == main_content) write_needed = false;
            }
            if (write_needed) {
                util::file_write(test_main_cpp, main_content);
            }
        }

        // Compile test sources + test_main.cpp
        std::vector<fs::path> test_objs;
        int compiled = 0, cached = 0, errors = 0;

        auto compile_one = [&](const fs::path& src) -> std::optional<fs::path> {
            auto obj = cache_dir / "obj" / (src.filename().string() + ".o");
            std::ostringstream cmd;
            if (is_msvc) {
                cmd << compiler << " /c /Fo:\"" << obj.string() << "\"";
                for (auto& f : base_flags) cmd << " " << f;
                cmd << " /I\"" << catch2_inc << "\"";
                cmd << " \"" << src.string() << "\"";
            } else {
                cmd << compiler;
                for (auto& f : base_flags) cmd << " " << f;
                cmd << " -I\"" << catch2_inc << "\"";
                // Add extra includes from installed packages
                cmd << " -c \"" << src.string() << "\" -o \"" << obj.string() << "\"";
            }

            if (verbose) util::info("  " + cmd.str());
            auto res = util::run_command(cmd.str());
            if (res.exit_code != 0) {
                util::error(std::string("  compilation failed: ") + src.filename().string());
                if (!res.err.empty()) util::error(res.err);
                if (!res.out.empty()) util::error(res.out);
                return std::nullopt;
            }
            return obj;
        };

        // Compile all test sources
        for (auto& ts : test_sources) {
            if (auto obj = compile_one(ts)) {
                test_objs.push_back(*obj);
                compiled++;
            } else {
                errors++;
            }
        }

        // Compile test_main.cpp
        if (!user_has_main && !test_main_cpp.empty()) {
            if (auto obj = compile_one(test_main_cpp)) {
                test_objs.push_back(*obj);
            } else {
                errors++;
            }
        }

        if (errors > 0) {
            util::fatal(std::to_string(errors) + " test compilation error(s)");
        }

        // Link test_runner
        fs::path runner = build_dir / "test_runner";
#ifdef EZMK_WIN
        runner += ".exe";
#endif
        std::ostringstream link_cmd;
        if (is_msvc) {
            link_cmd << "link.exe /OUT:\"" << runner.string() << "\"";
            for (auto& o : project_objs) link_cmd << " \"" << o.string() << "\"";
            for (auto& o : test_objs) link_cmd << " \"" << o.string() << "\"";
        } else {
            link_cmd << compiler;
            for (auto& o : project_objs) link_cmd << " \"" << o.string() << "\"";
            for (auto& o : test_objs) link_cmd << " \"" << o.string() << "\"";
            link_cmd << " -o \"" << runner.string() << "\"";
        }

        // Add package link flags
        for (auto& f : cfg.link.flags) link_cmd << " " << f;
        for (auto& d : cfg.link.link_dirs) link_cmd << " -L\"" << d << "\"";
        for (auto& t : cfg.link.system_targets) link_cmd << " -l" << t;

        // Link against Catch2 library (from project/user/global scope)
        {
            fs::path catch2_lib;
            // Try to find libcatch2.a in known locations
            std::vector<fs::path> search_paths = {
                proj_root / ".ezmk/pkg/catch2/build",
            };
            auto home = util::get_home_dir();
#ifdef EZMK_WIN
            const char* appdata = std::getenv("LOCALAPPDATA");
            fs::path local_ezmk = appdata ? fs::path(appdata) / "ezmk" : home / "AppData/Local/ezmk";
            search_paths.push_back(local_ezmk / "pkg/catch2/build");
#endif
            search_paths.push_back(home / ".ezmk/pkg/catch2/build");
            search_paths.push_back(home / ".local/share/ezmk/pkg/catch2/build");

            for (auto& dir : search_paths) {
                auto lib = dir / "libcatch2.a";
                if (util::file_exists(lib)) {
                    catch2_lib = lib;
                    break;
                }
            }
            if (!catch2_lib.empty()) {
                link_cmd << " \"" << catch2_lib.string() << "\"";
            }
        }

        if (verbose) util::info("  " + link_cmd.str());
        auto link_res = util::run_command(link_cmd.str());
        if (link_res.exit_code != 0) {
            util::error("test link failed");
            if (!link_res.err.empty()) util::error(link_res.err);
            util::fatal("build failed");
        }

        // Run test_runner. No `-s`: with it, every passing CHECK prints its own
        // "PASSED" line, which text parsing trivially confuses with a test case.
        // We rely on Catch2's exit code (non-zero on any failure) plus the
        // case-level summary line instead.
        std::string test_cmd = "\"" + runner.string() + "\"";
        if (!test_filter.empty()) {
            test_cmd += " \"" + test_filter + "\"";
        }
        auto test_res = util::run_command(test_cmd);

        // Parse the case-level summary from Catch2 console output. Two shapes:
        //   - failures present: "test cases: M | X passed | Y failed"
        //       (zero-count segments are omitted, e.g. "1 | 1 failed")
        //   - all passed:       "All tests passed (A assertions in M test cases)"
        // If neither parses (reporter format changed), fall back to the exit
        // code for the gate — the counts just become less precise.
        int total_cases = 0;
        int passed_cases = 0;
        int failed_cases = 0;
        bool summary_found = false;

        auto to_int = [](const std::string& s) -> int {
            auto start = s.find_first_of("0123456789");
            if (start == std::string::npos) return -1;
            auto end = s.find_first_not_of("0123456789", start);
            try { return std::stoi(s.substr(start, end - start)); }
            catch (...) { return -1; }
        };

        std::string combined_out = test_res.out + "\n" + test_res.err;
        std::istringstream output_stream(combined_out);
        std::string line;
        while (std::getline(output_stream, line)) {
            // "test cases: M | X passed | Y failed" — authoritative case counts.
            auto tc_pos = line.find("test cases:");
            if (tc_pos != std::string::npos) {
                std::string rest = line.substr(tc_pos + 11);
                std::vector<std::string> segs;
                std::string seg;
                std::istringstream seg_stream(rest);
                while (std::getline(seg_stream, seg, '|')) segs.push_back(seg);
                int m = segs.empty() ? -1 : to_int(segs[0]);
                if (m >= 0) {
                    total_cases = m;
                    passed_cases = 0;
                    failed_cases = 0;
                    for (size_t i = 1; i < segs.size(); ++i) {
                        if (segs[i].find("passed") != std::string::npos) {
                            int x = to_int(segs[i]);
                            if (x >= 0) passed_cases = x;
                        } else if (segs[i].find("failed") != std::string::npos) {
                            int y = to_int(segs[i]);
                            if (y >= 0) failed_cases = y;
                        }
                    }
                    summary_found = true;
                }
                break;
            }
        }

        if (!summary_found) {
            // "All tests passed (A assertions in M test cases)"
            auto at_pos = combined_out.find("All tests passed");
            if (at_pos != std::string::npos) {
                auto in_pos = combined_out.find(" in ", at_pos);
                if (in_pos != std::string::npos) {
                    int m = to_int(combined_out.substr(in_pos + 4));
                    if (m >= 0) {
                        total_cases = m;
                        passed_cases = m;
                        failed_cases = 0;
                        summary_found = true;
                    }
                }
            }
        }

        if (!summary_found) {
            // Format changed — only the exit code is reliable.
            failed_cases = test_res.exit_code != 0 ? 1 : 0;
            passed_cases = 0;
            total_cases = passed_cases + failed_cases;
        }

        // Print results
        util::info(std::string("  cases: ") + std::to_string(total_cases) +
                   " | passed: " + std::to_string(passed_cases) +
                   " | failed: " + std::to_string(failed_cases));

        // Print failure details from output
        if ((failed_cases > 0 || test_res.exit_code != 0) && !test_res.out.empty()) {
            std::istringstream full(test_res.out);
            while (std::getline(full, line)) {
                if (line.find("FAILED") != std::string::npos ||
                    line.find("REQUIRE") != std::string::npos ||
                    line.find("CHECK") != std::string::npos ||
                    line.find("with expansion") != std::string::npos) {
                    util::error("    " + line);
                }
            }
        }

        util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::test_summary,
                    {{"total", std::to_string(total_cases)},
                     {"passed", std::to_string(passed_cases)},
                     {"failed", std::to_string(failed_cases)}}));

        if (failed_cases > 0 || test_res.exit_code != 0) {
            util::fatal(ezmk::i18n::I18nKey::test_failed);
        }

    } else if (framework == "EZMK") {
        // ---- ezmk Built-in Framework Mode ----
        util::info("Running tests (ezmk)...");

        int passed = 0, failed = 0, timed_out = 0;
        double total_time = 0.0;

        for (auto& ts : test_sources) {
            auto test_start = std::chrono::steady_clock::now();

            // Filter by filename if --filter specified
            if (!test_filter.empty()) {
                auto fname = ts.filename().string();
                // Simple glob: * matches any, ? matches single
                // For MVP, do substring match
                if (fname.find(test_filter) == std::string::npos) {
                    if (verbose) {
                        util::info(std::string("  [SKIP] ") + fname + " (filtered)");
                    }
                    continue;
                }
            }

            // Compile individual test executable
            auto test_exe = build_dir / ("test_" + ts.stem().string());
#ifdef EZMK_WIN
            test_exe += ".exe";
#endif
            std::ostringstream comp_cmd;
            if (is_msvc) {
                comp_cmd << compiler << " /Fe:\"" << test_exe.string() << "\"";
                comp_cmd << " \"" << ts.string() << "\"";
                for (auto& o : project_objs) comp_cmd << " \"" << o.string() << "\"";
                for (auto& f : base_flags) comp_cmd << " " << f;
            } else {
                comp_cmd << compiler;
                for (auto& f : base_flags) comp_cmd << " " << f;
                comp_cmd << " \"" << ts.string() << "\"";
                for (auto& o : project_objs) comp_cmd << " \"" << o.string() << "\"";
                comp_cmd << " -o \"" << test_exe.string() << "\"";
                for (auto& f : cfg.link.flags) comp_cmd << " " << f;
                for (auto& d : cfg.link.link_dirs) comp_cmd << " -L\"" << d << "\"";
                for (auto& t : cfg.link.system_targets) comp_cmd << " -l" << t;
            }

            if (verbose) util::info("  " + comp_cmd.str());
            auto comp_res = util::run_command(comp_cmd.str());
            if (comp_res.exit_code != 0) {
                util::error(std::string("  compilation failed: ") + ts.filename().string());
                if (!comp_res.err.empty()) util::error(comp_res.err);
                failed++;
                continue;
            }

            // Run test with a 30s timeout — a hung test must not block the
            // whole suite indefinitely.
            auto run_res = util::run_command("\"" + test_exe.string() + "\"", 30);
            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - test_start).count();
            total_time += elapsed;

            auto fname = ts.filename().string();
            if (run_res.timed_out) {
                util::error("  [TIMEOUT] " + fname + "  (exceeded 30s)");
                timed_out++;
            } else if (run_res.exit_code == 0) {
                util::info("  [PASS] " + fname + "  (" +
                           std::to_string(static_cast<int>(elapsed * 1000)) + "ms)");
                if (verbose && !run_res.out.empty()) {
                    util::info("    stdout: " + run_res.out);
                }
                passed++;
            } else {
                util::error("  [FAIL] " + fname + "  (" +
                          std::to_string(static_cast<int>(elapsed * 1000)) + "ms)");
                if (!run_res.out.empty()) {
                    util::info("    stdout: " + run_res.out);
                }
                if (!run_res.err.empty()) {
                    util::error("    stderr: " + run_res.err);
                }
                failed++;
            }
        }

        int total = passed + failed + timed_out;
        // Fold timeouts into the failed bucket for the summary so the numbers
        // add up (each [TIMEOUT] line already flags the individual case).
        util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::test_summary,
                    {{"total", std::to_string(total)},
                     {"passed", std::to_string(passed)},
                     {"failed", std::to_string(failed + timed_out)}}));

        if (failed > 0 || timed_out > 0) {
            util::fatal(ezmk::i18n::I18nKey::test_failed);
        }

    } else {
        util::fatal(std::string("unknown test framework: '") + framework +
                    "'. Supported: catch2, ezmk");
    }
}

} // namespace ezmk::build