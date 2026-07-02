#include "ezmk/toolchain.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

namespace ezmk::toolchain {

// ===================================================================
// GCC → MSVC compile flag map
// ===================================================================

namespace {

struct FlagMapEntry {
    const char* gcc_flag;
    const char* msvc_flag;
    bool is_prefix = false; // true → match prefix (e.g. "-I" → "/I")
};

const FlagMapEntry COMPILE_FLAG_MAP[] = {
    // Language standard
    {"-std=c++98",   "/std:c++98"},
    {"-std=c++03",   "/std:c++14"},   // MSVC has no c++03; nearest is c++14
    {"-std=c++11",   "/std:c++11"},
    {"-std=c++14",   "/std:c++14"},
    {"-std=c++17",   "/std:c++17"},
    {"-std=c++20",   "/std:c++20"},
    {"-std=c++23",   "/std:c++latest"},
    {"-std=c++26",   "/std:c++latest"},
    {"-std=c89",     "/std:c11"},      // MSVC has no c89; nearest is c11
    {"-std=c99",     "/std:c11"},      // MSVC has no c99
    {"-std=c11",     "/std:c11"},
    {"-std=c17",     "/std:c17"},
    {"-std=c23",     "/std:c11"},      // MSVC has no c23 yet

    // Warning levels
    {"-Wall",        "/W4"},
    {"-Wextra",      "/W4"},
    {"-Werror",      "/WX"},
    {"-w",           "/W0"},

    // Optimization
    {"-O0",          "/Od"},
    {"-O1",          "/O1"},
    {"-O2",          "/O2"},
    {"-O3",          "/Ox"},
    {"-Os",          "/O1"},           // size optimisation → /O1

    // Debug
    {"-g",           "/Zi"},
    {"-g3",          "/Zi"},

    // Pedantic
    {"-pedantic",    "/permissive-"},

    // PIC — MSVC is always position-independent
    {"-fPIC",        ""},              // ignore (empty = skip without warn)
    {"-fpic",        ""},              // ignore

    // pthread — not applicable on Windows
    {"-pthread",     ""},              // ignore

    // Misc
    {"-c",           "/c"},

    // These are prefix-based matches:
    // -D<name>=<value> → /D<name>=<value>
    // -I<path>         → /I<path>
    // -o <file>        → handled specially (compile: /Fo, link: /Fe)
    // -L<path>         → /LIBPATH:<path> (link only)
    // -l<lib>          → <lib>.lib (link only)
};

// Prefix-based GCC → MSVC translations (checked after exact matches)
// Each entry: {gcc_prefix, msvc_prefix, strip_prefix_len}
struct PrefixMapEntry {
    const char* gcc_prefix;
    const char* msvc_prefix;
};

const PrefixMapEntry COMPILE_PREFIX_MAP[] = {
    {"-D", "/D"},
    {"-I", "/I"},
};

const PrefixMapEntry LINK_PREFIX_MAP[] = {
    {"-L", "/LIBPATH:"},
};

// Map a single GCC compile flag to MSVC. Returns empty string for flags to skip silently.
std::string map_compile_flag(const std::string& flag) {
    // 1. Exact matches
    for (auto& e : COMPILE_FLAG_MAP) {
        if (flag == e.gcc_flag) {
            return e.msvc_flag; // empty string = skip silently (e.g. -fPIC)
        }
    }

    // 2. Prefix matches for compile flags
    for (auto& p : COMPILE_PREFIX_MAP) {
        size_t plen = std::strlen(p.gcc_prefix);
        if (flag.size() > plen && flag.compare(0, plen, p.gcc_prefix) == 0) {
            std::string result = p.msvc_prefix;
            result += flag.substr(plen);
            return result;
        }
    }

    // 3. Not recognized → return empty (caller treats as unrecognized)
    return {};
}

// Map a single GCC link flag to MSVC.
std::string map_link_flag(const std::string& flag) {
    // -l<lib> → <lib>.lib
    if (flag.size() > 2 && flag[0] == '-' && flag[1] == 'l') {
        return flag.substr(2) + ".lib";
    }

    // -L<path> → /LIBPATH:<path>
    for (auto& p : LINK_PREFIX_MAP) {
        size_t plen = std::strlen(p.gcc_prefix);
        if (flag.size() > plen && flag.compare(0, plen, p.gcc_prefix) == 0) {
            std::string result = p.msvc_prefix;
            result += flag.substr(plen);
            return result;
        }
    }

    // -shared → /DLL
    if (flag == "-shared") return "/DLL";

    // -o handled specially by caller

    return {};
}

// Determine if a flag looks like it's already MSVC-style (starts with /).
bool is_msvc_style(const std::string& flag) {
    return !flag.empty() && flag[0] == '/';
}

} // anonymous namespace

// ===================================================================
// Flag translation API
// ===================================================================

FlagTranslation translate_compile_flags(const std::vector<std::string>& gcc_flags,
                                        CompilerFamily target) {
    FlagTranslation result;
    if (target != CompilerFamily::Msvc) {
        result.translated = gcc_flags; // no translation needed
        return result;
    }

    for (auto& f : gcc_flags) {
        // Already MSVC-style → pass through directly
        if (is_msvc_style(f)) {
            result.translated.push_back(f);
            continue;
        }

        // Handle -D with embedded quotes (e.g. -DNAME="value with spaces")
        // These come from compile.macros via macros_to_flags in build.cpp.
        // map_compile_flag handles prefix matching for -D correctly.

        auto mapped = map_compile_flag(f);
        if (mapped.empty()) {
            // Check if it was intentionally skipped (in COMPILE_FLAG_MAP with empty value)
            bool intentionally_skipped = false;
            for (auto& e : COMPILE_FLAG_MAP) {
                if (f == e.gcc_flag && e.msvc_flag[0] == '\0') {
                    intentionally_skipped = true;
                    break;
                }
            }
            if (!intentionally_skipped) {
                result.unrecognized.push_back(f);
            }
        } else {
            result.translated.push_back(std::move(mapped));
        }
    }

    return result;
}

FlagTranslation translate_link_flags(const std::vector<std::string>& gcc_flags,
                                      CompilerFamily target) {
    FlagTranslation result;
    if (target != CompilerFamily::Msvc) {
        result.translated = gcc_flags;
        return result;
    }

    for (auto& f : gcc_flags) {
        if (is_msvc_style(f)) {
            result.translated.push_back(f);
            continue;
        }

        auto mapped = map_link_flag(f);
        if (mapped.empty()) {
            result.unrecognized.push_back(f);
        } else {
            result.translated.push_back(std::move(mapped));
        }
    }

    return result;
}

// ===================================================================
// MSVC environment loading
// ===================================================================

std::map<std::string, std::string> load_msvc_env(const fs::path& vcvars_path) {
    std::map<std::string, std::string> env;

#ifdef EZMK_WIN
    if (!util::file_exists(vcvars_path)) return env;

    // Strategy: run "vcvars64.bat > NUL && set" to capture the modified environment.
    // The `set` command prints all env vars as KEY=VALUE lines.
    std::ostringstream cmd;
    cmd << "cmd /c \"call \\\"" << vcvars_path.string() << "\\\" > NUL && set\"";

    auto res = util::run_command(cmd.str());
    if (res.exit_code != 0) return env;

    // Parse KEY=VALUE lines from `set` output
    std::istringstream stream(res.out);
    std::string line;
    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty()) continue;

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Skip noise variables
        if (key.empty()) continue;

        env[std::move(key)] = std::move(value);
    }
#endif

    return env;
}

// ===================================================================
// Toolchain detection
// ===================================================================

static Toolchain detect_gcc_like(const std::string& cxx, const std::string& cc) {
    Toolchain tc;
    tc.family = CompilerFamily::Gcc;

    // Determine family from compiler name
    std::string cxx_lower = cxx;
    std::transform(cxx_lower.begin(), cxx_lower.end(), cxx_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (cxx_lower.find("clang") != std::string::npos) {
        tc.family = CompilerFamily::Clang;
    }

    tc.cxx_compiler = fs::path(cxx);
    tc.c_compiler = fs::path(cc);
    // GCC/Clang use the compiler driver for linking
    tc.linker = fs::path(cxx);
    tc.archiver = fs::path("ar");

    return tc;
}

#ifdef EZMK_WIN

static fs::path find_vcvars64() {
    // 1. Check common VS 2022 / 2019 installation via vswhere
    const char* vswhere_paths[] = {
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\Installer\\vswhere.exe",
    };

    for (auto* vsw : vswhere_paths) {
        if (!util::file_exists(fs::path(vsw))) continue;

        std::string cmd = std::string("\"") + vsw +
            "\" -latest -property installationPath";
        auto res = util::run_command(cmd);
        if (res.exit_code != 0 || res.out.empty()) continue;

        // Trim trailing whitespace/newlines
        std::string vs_path = res.out;
        while (!vs_path.empty() && (vs_path.back() == '\n' || vs_path.back() == '\r' ||
                                     vs_path.back() == ' ' || vs_path.back() == '\t')) {
            vs_path.pop_back();
        }

        if (vs_path.empty()) continue;

        fs::path vcvars = fs::path(vs_path) / "VC\\Auxiliary\\Build\\vcvars64.bat";
        if (util::file_exists(vcvars)) return vcvars;
    }

    // 2. Check common installation paths
    const char* common_paths[] = {
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
    };

    for (auto* p : common_paths) {
        fs::path candidate(p);
        if (util::file_exists(candidate)) return candidate;
    }

    return {};
}

static std::string find_msvc_cl() {
    // Check if cl.exe is in PATH (via run_command)
    auto res = util::run_command("cl 2>&1");
    if (res.exit_code == 0) return "cl.exe";

    // If cl.exe isn't directly callable, we need vcvars first.
    // Return "cl.exe" — the caller will load vcvars environment and retry.
    return "cl.exe";
}

#endif // EZMK_WIN

Toolchain detect_toolchain() {
    // Cache the result — detect only once per process
    static Toolchain cached;
    static bool cached_valid = false;
    if (cached_valid) return cached;

    // 1. Check $CXX/$CC environment variable override
    const char* cxx_env = std::getenv("CXX");
    const char* cc_env = std::getenv("CC");

    if (cxx_env && cxx_env[0] != '\0') {
        std::string cxx(cxx_env);
        auto res = util::run_command(cxx + " --version 2>&1");
        if (res.exit_code == 0) {
            std::string cc = (cc_env && cc_env[0] != '\0') ? std::string(cc_env)
                : (cxx.find("clang") != std::string::npos ? "clang" : "gcc");
            cached = detect_gcc_like(cxx, cc);
            cached_valid = true;
            return cached;
        }
        util::warn(std::string("$CXX is set to '") + cxx +
                   "' but it is not executable — falling back to auto-detect");
    }

#ifdef EZMK_WIN
    // 2. Windows: try MSVC first, then MinGW
    {
        // Try to find vcvars64.bat
        fs::path vcvars = find_vcvars64();
        if (!vcvars.empty()) {
            // Load MSVC environment
            auto msvc_env = load_msvc_env(vcvars);

            // Build a command that runs cl within the vcvars environment
            // We check if cl.exe works by running it in the vcvars context
            std::ostringstream test_cmd;
            test_cmd << "cmd /c \"call \\\"" << vcvars.string() << "\\\" > NUL && cl 2>&1\"";
            auto res = util::run_command(test_cmd.str());
            if (res.exit_code == 0) {
                // MSVC is available
                Toolchain tc;
                tc.family = CompilerFamily::Msvc;
                tc.cxx_compiler = fs::path("cl.exe");
                tc.c_compiler = fs::path("cl.exe");
                tc.linker = fs::path("link.exe");
                tc.archiver = fs::path("lib.exe");
                tc.vcvars_path = vcvars;
                cached = tc;
                cached_valid = true;
                util::info("MSVC toolchain detected (cl.exe + link.exe + lib.exe)");
                return cached;
            }
        }

        // 3. Fall back to MinGW (g++ / clang++)
        std::vector<std::string> cxx_candidates = {"g++", "clang++"};
        std::vector<std::string> c_candidates = {"gcc", "clang"};

        for (size_t i = 0; i < cxx_candidates.size(); ++i) {
            auto res = util::run_command(cxx_candidates[i] + " --version 2>&1");
            if (res.exit_code == 0) {
                cached = detect_gcc_like(cxx_candidates[i], c_candidates[i]);
                cached_valid = true;
                return cached;
            }
        }

        // 4. Nothing found
        std::string msg = "no C/C++ compiler found.\n\n";
        msg += "  Option A: Install MSYS2 MinGW — https://www.msys2.org/\n";
        msg += "    Then: pacman -S mingw-w64-x86_64-gcc\n";
        msg += "  Option B: Install Visual Studio Build Tools — https://visualstudio.microsoft.com/downloads/\n";
        util::fatal(msg);
    }
#else
    // Linux / macOS: detect GCC or Clang
    std::vector<std::string> cxx_candidates;
    std::vector<std::string> c_candidates;

#ifdef EZMK_MACOS
    cxx_candidates = {"g++", "clang++", "c++"};
    c_candidates   = {"gcc", "clang",   "cc"};
#else
    cxx_candidates = {"g++", "clang++"};
    c_candidates   = {"gcc", "clang"};
#endif

    for (size_t i = 0; i < cxx_candidates.size(); ++i) {
        auto res = util::run_command(cxx_candidates[i] + " --version 2>&1");
        if (res.exit_code == 0) {
#ifdef EZMK_MACOS
            if (res.out.find("Apple") != std::string::npos ||
                res.out.find("apple") != std::string::npos) {
                util::info(std::string("detected Apple Clang as '") + cxx_candidates[i] + "'");
            }
#endif
            cached = detect_gcc_like(cxx_candidates[i], c_candidates[i]);
            cached_valid = true;
            return cached;
        }
    }

    // Nothing found
    std::string msg = "no C";
    msg += "++ compiler found.\n\n";
#ifdef EZMK_MACOS
    msg += "  Option A: xcode-select --install  (Apple Clang)\n";
    msg += "  Option B: brew install gcc          (GNU GCC)";
#else
    msg += "  Debian/Ubuntu: sudo apt install g++\n";
    msg += "  RHEL/Fedora:   sudo dnf install gcc-c++\n";
    msg += "  Arch:          sudo pacman -S gcc";
#endif
    util::fatal(msg);
#endif

    return {}; // unreachable
}

// ===================================================================
// MSVC /showIncludes parser
// ===================================================================

std::vector<fs::path> parse_show_includes(const std::string& compiler_output) {
    std::vector<fs::path> includes;

    // Format: "Note: including file:  C:\path\to\header.h\n"
    // Each line starts with "Note: including file:" followed by the path.
    // The path may have leading/trailing whitespace.

    std::istringstream stream(compiler_output);
    std::string line;
    const std::string NOTE_PREFIX = "Note: including file:";

    while (std::getline(stream, line)) {
        // Trim trailing \r (CRLF)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Check for the "Note: including file:" prefix
        auto pos = line.find(NOTE_PREFIX);
        if (pos == std::string::npos) continue;

        // Extract the path after the prefix
        std::string path_str = line.substr(pos + NOTE_PREFIX.size());

        // Trim leading and trailing whitespace
        size_t start = 0;
        while (start < path_str.size() && (path_str[start] == ' ' || path_str[start] == '\t')) {
            ++start;
        }
        size_t end = path_str.size();
        while (end > start && (path_str[end - 1] == ' ' || path_str[end - 1] == '\t')) {
            --end;
        }

        if (start < end) {
            includes.push_back(fs::path(path_str.substr(start, end - start)));
        }
    }

    return includes;
}

} // namespace ezmk::toolchain
