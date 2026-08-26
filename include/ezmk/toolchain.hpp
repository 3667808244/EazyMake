#pragma once

#include <map>
#include <string>
#include <vector>
#include <filesystem>

namespace ezmk::toolchain {
namespace fs = std::filesystem;

// ---- Compiler family ----

enum class CompilerFamily { Gcc, Clang, Msvc };

// ---- Toolchain descriptor ----

struct Toolchain {
    CompilerFamily family = CompilerFamily::Gcc;
    fs::path c_compiler;       // cl.exe / gcc / clang
    fs::path cxx_compiler;     // cl.exe / g++ / clang++
    fs::path linker;           // link.exe / g++ / clang++
    fs::path archiver;         // lib.exe / ar
    fs::path vcvars_path;      // path to vcvars64.bat (MSVC only)
    std::string version;       // 1.1.0: compiler version string (first line of --version)
};

// Detect the available toolchain on the current platform.
// Respects $CXX/$CC env vars.
Toolchain detect_toolchain();

// 1.2.0-dev.10: Precompiled-package compiler tag from a detected toolchain.
// Returns "gcc13" / "clang18" / "msvc143" by parsing tc.version (which
// detect_toolchain() already cached — pure function, no subprocess):
//   GCC:   "g++ (GCC) 13.2.0"               → gcc13
//   Clang: "clang version 18.1.8"           → clang18
//          "Apple clang version 15.0.0"     → clang15
//   MSVC:  "...Version 19.43.34808 for x64" → msvc143 (_MSC_VER 1943 → toolset 143)
// Returns "" when the version string cannot be parsed (callers treat it as
// "no compiler tag" — the full tag degrades to os-arch).
std::string compiler_tag(const Toolchain& tc);

// 1.4.0-dev.2: Highest -std= standard this toolchain actually supports, as a
// canonical C++ form ("CPP20" / "CPP17" / ... — parse_language's normalized
// C++ spelling). The C side of the same compiler generation caps at C17
// (gcc >= 8 / clang >= 5) or C11 (older / unknown). Unknown versions →
// conservative floor "CPP11". Segmentation sources are documented at the
// implementation; see plans/1.4.x/1.4.0-dev.2.md §3.1. Pure — no subprocess.
std::string max_supported_std(CompilerFamily family, const std::string& version);

// Run vcvars64.bat and capture the resulting environment variables.
// Returns a map of env vars. Windows/MSVC only.
std::map<std::string, std::string> load_msvc_env(const fs::path& vcvars_path);

// ---- Flag translation ----

struct FlagTranslation {
    std::vector<std::string> translated;
    std::vector<std::string> unrecognized;  // warn about these
};

// Translate GCC-style compile flags to the target compiler family.
FlagTranslation translate_compile_flags(const std::vector<std::string>& gcc_flags,
                                        CompilerFamily target);

// Translate GCC-style link flags to the target compiler family.
FlagTranslation translate_link_flags(const std::vector<std::string>& gcc_flags,
                                      CompilerFamily target);

// 1.1.0-dev.4: Get stdlib-related compile flags based on stdlib choice + compiler family.
// Returns flags like "-stdlib=libc++" (Clang/GCC) or empty (MSVC).
std::vector<std::string> get_stdlib_flags(const std::string& stdlib,
                                           CompilerFamily family);

// ---- MSVC dependency parsing ----

// Parse the output of cl.exe /showIncludes into a list of header paths.
// Format: "Note: including file:  C:\path\to\header.h"
std::vector<fs::path> parse_show_includes(const std::string& compiler_output);

} // namespace ezmk::toolchain
