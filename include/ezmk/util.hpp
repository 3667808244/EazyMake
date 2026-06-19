#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

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
  #define EZMK_EXE_SUFFIX ".exe"
  #define EZMK_OBJ_SUFFIX ".obj"
#else
  #define EZMK_EXE_SUFFIX ""
  #define EZMK_OBJ_SUFFIX ".o"
#endif

namespace ezmk::util {
namespace fs = std::filesystem;

// ---- Logging ----
void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);
[[noreturn]] void fatal(std::string_view msg);  // throws ezmk::fatal_error

// ---- Filesystem ----
bool file_exists(const fs::path& p);
std::string file_read(const fs::path& p);
void file_write(const fs::path& p, std::string_view content);
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

// ---- Cross-platform ----
// Make a path use forward slashes (MSYS2-compatible)
std::string native_path(const fs::path& p);

} // namespace ezmk::util
