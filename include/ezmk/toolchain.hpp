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
};

// Detect the available toolchain on the current platform.
// Respects $CXX/$CC env vars.
Toolchain detect_toolchain();

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

// ---- MSVC dependency parsing ----

// Parse the output of cl.exe /showIncludes into a list of header paths.
// Format: "Note: including file:  C:\path\to\header.h"
std::vector<fs::path> parse_show_includes(const std::string& compiler_output);

} // namespace ezmk::toolchain
