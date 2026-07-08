#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <map>

#include "ezmk/i18n.hpp"

namespace ezmk {

// Fatal error — thrown instead of exit(1) so destructors run and temp files get cleaned.
class fatal_error : public std::runtime_error {
public:
    explicit fatal_error(std::string_view msg) : std::runtime_error(std::string(msg)) {}
};

} // namespace ezmk

// Platform detection
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
  #define EZMK_WIN 1
#elif defined(__APPLE__)
  #define EZMK_MACOS 1
#elif defined(__linux__)
  #define EZMK_LINUX 1
#endif

// Executable suffix (Windows only)
#ifdef EZMK_WIN
  #define EZMK_EXE_SUFFIX ".exe"
#else
  #define EZMK_EXE_SUFFIX ""
#endif

// Object file suffix — all platforms use .o (MinGW g++ also produces .o)
#define EZMK_OBJ_SUFFIX ".o"

namespace ezmk::util {
namespace fs = std::filesystem;

// ---- Logging ----
void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);
[[noreturn]] void fatal(std::string_view msg);  // throws ezmk::fatal_error

// I18n-aware logging overloads — format a localized string by key + args,
// then output with the same prefix/color conventions as the raw overloads.
void info(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args = {});
void warn(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args = {});
void error(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args = {});
[[noreturn]] void fatal(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args = {});

// ---- Color support (VT100/ANSI) ----
// Call once at startup to enable VT100 processing on Windows.
void init_console();
// Returns true if the output stream supports ANSI color codes.
bool supports_color();
// Wrap a message in color codes (no-op if color is not supported).
std::string color_msg(const char* color, std::string_view msg);
// ANSI escape codes for colored output.
namespace color {
    extern const char* reset;
    extern const char* green;
    extern const char* yellow;
    extern const char* red;
    extern const char* cyan;
    extern const char* bold;
    extern const char* dim;
}

// ---- Filesystem ----
bool file_exists(const fs::path& p);
std::string file_read(const fs::path& p);
bool file_write(const fs::path& p, std::string_view content);
void create_directories(const fs::path& p);
void remove_all(const fs::path& p);
void copy_recursive(const fs::path& from, const fs::path& to);

// Collect files matching extensions in a directory (non-recursive)
std::vector<fs::path> list_files(const fs::path& dir,
                                 const std::vector<std::string>& exts);

// Platform-specific paths
fs::path get_home_dir();
fs::path get_exe_dir();

// ---- Archive extraction ----
// Wraps miniz for zip; wraps miniz+gzip + custom tar parser for .tar.gz
void extract_zip(const fs::path& archive, const fs::path& dest);
void extract_targz(const fs::path& archive, const fs::path& dest);

// Auto-detect archive type by extension and extract
void extract_archive(const fs::path& archive, const fs::path& dest);

// ---- HTTP download ----
// Downloads a URL to a local file. On Windows uses WinHTTP; Linux falls back to curl.
void download(std::string_view url, const fs::path& dest);

// ---- Process ----
// Run a command and return {exit_code, stdout, stderr}
struct ProcResult {
    int exit_code;
    std::string out;
    std::string err;
};
ProcResult run_command(const std::string& cmd);

// ---- Git helpers ----
// Check if git is available in PATH.
bool git_available();

// Clone a git repository. Returns true on success.
// branch: branch to track (default "main").
bool git_clone(const std::string& url, const fs::path& dest, std::string_view branch = "main");

// Pull latest changes in a git repository. Returns true on success.
bool git_pull(const fs::path& repo_dir, std::string_view branch = "main");

// Get the ISO 8601 timestamp of the last commit in a git repo.
// Returns empty string on failure.
std::string git_last_commit_time(const fs::path& repo_dir);

// ---- Editor & script execution ----

// Find the best available system text editor.
// Windows: "notepad".  Linux: first found of vim, nano, emacs.
// Returns empty string if no editor is available.
std::string find_editor();

// Open a file in the system editor (blocking — waits for editor to close).
// Falls back to printing a warning if no editor is available.
void open_in_editor(const fs::path& file);

// Run an install script with the appropriate interpreter.
// Supported extensions: .sh (bash), .ps1 (powershell), .bat (cmd /c).
// cwd: working directory for the script.
ProcResult run_script(const fs::path& script, const fs::path& cwd);

// ---- Cross-platform ----
// Make a path use forward slashes (MSYS2-compatible)
std::string native_path(const fs::path& p);

// ---- Version comparison ----
// Compare two semantic version strings (major.minor.patch).
// Returns -1 if a < b, 0 if equal, 1 if a > b.
// Pre-release tags (-alpha, -beta) and build metadata (+build) are ignored.
// Missing segments are treated as 0 (e.g. "1.0" == "1.0.0").
int compare_version(std::string_view a, std::string_view b);

// ---- Shell safety ----
// Escape a string for safe use inside double-quoted shell arguments.
// Escapes: " \ ` $
// This prevents command injection when constructing shell commands with paths/URLs.
std::string escape_shell_arg(std::string_view s);

// ---- Utils / Lua plugin discovery ----

// Search for a utils Lua script by name across all installed packages.
// Scans project → user → global scopes, returns first matching .lua path.
// Returns empty path if not found.
fs::path find_utils_script(const std::string& name);

} // namespace ezmk::util
