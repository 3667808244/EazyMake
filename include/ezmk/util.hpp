#pragma once

#include <ctime>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <map>
#include <optional>

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
// 0.9.8+: structured info line — [ezmk] prefix, no color, stderr, with newline.
// Intended for multi-line structured output (pkg info, repo info, repo list).
void info_line(std::string_view msg);

// I18n-aware logging overloads — format a localized string by key + args,
// then output with the same prefix/color conventions as the raw overloads.
void info(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args = {});
void warn(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args = {});
void error(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args = {});
[[noreturn]] void fatal(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args = {});

// ---- Color support (VT100/ANSI) ----
// Call once at startup to enable VT100 processing on Windows.
void init_console();
// Global color output policy (0.2.6+). Auto = decide from TTY/NO_COLOR;
// Always/Never force the decision (overriding NO_COLOR).
enum class ColorMode { Auto, Always, Never };
void set_color_mode(ColorMode mode);
ColorMode get_color_mode();
// Returns true if the output stream supports ANSI color codes.
bool supports_color();
// Returns true if stderr is a terminal (raw TTY check, ignores NO_COLOR / --color).
bool stderr_is_tty();
// 0.9.6+: Output a progress line with \r (carriage return) for in-place refresh.
// Uses the same mutex as info/warn/error for thread safety.
// Does NOT append a newline — caller should call progress_newline() after the final update.
void progress(std::string_view msg);
// 0.9.6+: Print a newline to move past the last \r progress line (thread-safe).
void progress_newline();
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

// 1.1.2 C1: atomically move `from` into place at `to` (rename first; on failure
// e.g. a locked target on Windows, fall back to copy_file + remove temp). Throws
// ezmk::fatal_error if both fail, so callers must NOT report success.
void atomic_rename(const fs::path& from, const fs::path& to);

// 1.1.2 C5: wrap a string as a TOML double-quoted string literal, escaping
// `"` `\` and control chars. Writers that interpolate user-controlled strings
// (project/package names, URLs) MUST use this — raw interpolation produces
// invalid or injected TOML (e.g. a name containing a quote or newline).
std::string toml_quote(std::string_view s);

// 1.1.3 S2: 校验包/项目名可作为单一路径段使用。拒绝空、`.`、`..`、路径分隔符、
// 盘符、以及绝对/隐藏前缀 —— 任何此类名字拼进安装路径（dest_dir / name）都会
// 造成路径穿越。非法即抛 std::runtime_error。
void validate_pkg_name(const std::string& name);

// 1.4.2 F-23: does `child` stay inside `parent` after lexical normalization?
// Relative children are resolved against `parent`; absolute paths, drive-letter
// and UNC prefixes, and `..` escapes are rejected. Case-insensitive on Windows.
// Used to constrain repo index.toml `file` / `[platform]` prefixes to the repo.
bool is_path_within(const fs::path& child, const fs::path& parent);

// Collect files matching extensions in a directory (non-recursive)
std::vector<fs::path> list_files(const fs::path& dir,
                                 const std::vector<std::string>& exts);

// ---- Project root (1.2.0-dev.7) ----
// Upward project-root search limit — prevents scanning up to the filesystem
// root / user home when no ezmk.toml exists anywhere above the start point.
inline constexpr int kProjectRootMaxUp = 5;

// Starting from `start` (itself counted as level 0), walk up at most `max_up`
// parent directories looking for one that contains an ezmk.toml. Returns that
// directory if found, otherwise nullopt — callers fall back to `start` so the
// no-config behavior is unchanged.
std::optional<fs::path> locate_project_root(const fs::path& start,
                                            int max_up = kProjectRootMaxUp);

// ---- 1.4.2 F-12: project-root-relative path flags ----
// True when the process CWD is `dir` (weakly-canonical comparison). Used to
// skip command-line path rewriting when a build is invoked from the project
// root, so root-invoked builds keep byte-identical command lines — and
// therefore identical cache signatures.
bool cwd_is(const fs::path& dir);

// Resolve relative entries of a path list (e.g. [link].link_dirs) against
// `base`; absolute entries are kept verbatim.
std::vector<std::string> resolve_relative_paths(const std::vector<std::string>& paths,
                                                const fs::path& base);

// Rewrite relative operands of the known path-carrying flags (-I, -L, -include,
// -isystem; MSVC /I, /LIBPATH:) so they resolve against `base` instead of the
// process CWD. Both the joined form ("-Iinc") and the split form ("-I" "inc")
// are handled; every other flag is passed through verbatim.
std::vector<std::string> resolve_relative_path_flags(const std::vector<std::string>& flags,
                                                     const fs::path& base);

// ---- Platform detection ----
// 1.1.0-dev.2: Returns a simplified platform tag in "os-arch" format.
// Examples: "win-x64", "linux-x64", "mac-arm64".
// Used for filename matching (lib<name>.<tag>.a) and index.toml platform filtering.
// Different from repo.cpp's build_platform_key() which uses "os_arch_toolchain" triplets.
std::string detect_platform_tag();

// Platform-specific paths
fs::path get_home_dir();
fs::path get_exe_dir();

// 1.4.2 F-37: thread-safe local time. std::localtime returns a pointer to a
// shared static std::tm — -jN workers (build cache writes, package metadata)
// racing on it can format a torn timestamp. Both helpers below use
// localtime_r / localtime_s and never return null; an unrepresentable time_t
// yields an empty string instead of a crash. The "%Y-%m-%dT%H:%M:%SZ" string
// is historically local time carrying a trailing Z — kept verbatim for
// compatibility with existing record/lockfile files.
std::string iso_time_now();                       // "%Y-%m-%dT%H:%M:%SZ"
std::string local_time_string(std::time_t t);     // "%Y-%m-%d %H:%M:%S"

#ifdef EZMK_WIN
// 1.4.2 F-20: UTF-8 <-> UTF-16 (CP_UTF8) conversion for every Windows API that
// has no UTF-8 form (CreateProcessW, GetModuleFileNameW, CreateFileW, ...).
// Without it, non-ASCII (e.g. Chinese) paths are read as cp936 bytes and get
// mangled. Invalid sequences degrade to an empty result rather than throwing.
std::wstring utf8_to_wide(std::string_view s);
std::string wide_to_utf8(std::wstring_view s);
#endif

// ---- Archive extraction & creation ----
// Wraps miniz for zip; wraps miniz+gzip + custom tar parser for .tar.gz
void extract_zip(const fs::path& archive, const fs::path& dest);
void extract_targz(const fs::path& archive, const fs::path& dest);
// 1.2.0-dev.11: deterministically pick a package's built archive from its
// build/ dir — prefer lib<name>.a/.lib, else the lexicographically smallest
// .a/.lib. Shared by lockfile record/verify so both hash the SAME file.
fs::path find_package_archive(const fs::path& build_dir,
                              const std::string& pkg_name);

// Auto-detect archive type by extension and extract
void extract_archive(const fs::path& archive, const fs::path& dest);

// 1.1.0-dev.2: Create a .tar.gz from a source directory (ustar tar + raw deflate gzip)
void create_targz(const fs::path& source_dir, const fs::path& output_file);

// 1.3.6: a staged entry for archive creation (shared by the tar.gz and zip
// writers): name is relative to source_dir with forward slashes (dirs end in
// '/'), content holds the file bytes, is_dir marks directory entries.
struct StageEntry {
    std::string name;
    std::vector<uint8_t> content;
    bool is_dir = false;
};

// 1.3.6: walk a staging directory into sorted StageEntries (relative names,
// forward slashes, directories trailing '/') — the single traversal shared by
// create_targz and create_zip so both formats stay byte-identical in layout.
std::vector<StageEntry> collect_stage_entries(const fs::path& source_dir);

// 1.3.5: Create a .zip from a source directory (miniz zip writer). Entry names
// match create_targz exactly (relative to source_dir, forward slashes, sorted)
// so consumers behave identically for both formats.
void create_zip(const fs::path& source_dir, const fs::path& output_file);

// ---- HTTP download ----
// Downloads a URL to a local file. On Windows uses WinHTTP; Linux falls back to curl.
void download(std::string_view url, const fs::path& dest);

// ---- Process ----
// Run a command and return {exit_code, stdout, stderr}
struct ProcResult {
    int exit_code;
    std::string out;
    std::string err;
    bool timed_out = false; // true if the child was killed by the timeout
};
// Run a command, returning exit code plus captured stdout/stderr.
// timeout_sec > 0 kills the child if it runs longer than that many seconds
// (sets ProcResult::timed_out = true); 0 means no timeout (default).
ProcResult run_command(const std::string& cmd, int timeout_sec = 0);

// 1.1.2 C4/C7: extended run_command options.
struct RunOptions {
    int timeout_sec = 0;                       // 0 = no timeout (default)
    fs::path cwd;                              // child working directory (empty = inherit)
    std::map<std::string, std::string> env;    // extra env vars for the child (empty = inherit)
};
// Overload taking RunOptions. POSIX: cwd/env applied via chdir/setenv AFTER fork
// (child-only, no race). Windows: lpCurrentDirectory + a built environment block.
// NOTE: keep the int overload above — it forwards here so existing callers work.
ProcResult run_command(const std::string& cmd, const RunOptions& opts);

// ---- Git helpers ----
// 1.4.1: guess if a string is a git repository URL. Loose heuristic shared by
// `repo add` and `pkg install` source detection:
//   - "git@host:user/repo.git" (SSH scp-style)
//   - any "scheme://" URL
//   - a bare "host/user/repo" string (contains '.' and '/') that is NOT a
//     local filesystem path (drive-letter / absolute / rooted)
// Local paths (C:/..., /abs, \root) are never URLs → false.
bool is_git_url(std::string_view s);

// Check if git is available in PATH.
bool git_available();

// Clone a git repository. Returns true on success.
// branch: branch/tag to clone (default "main"). An EMPTY branch means "clone
// the remote's default branch" (the --branch flag is omitted) — 1.4.1, used by
// `pkg install <git-url>` when no ref is requested.
// shallow: 1.4.1 — add `--depth 1` (single-branch shallow clone). Only safe
// for branch/tag/default-head clones; commit SHAs may be unreachable in a
// shallow clone and must use a full clone + checkout instead.
bool git_clone(const std::string& url, const fs::path& dest,
               std::string_view branch = "main", bool shallow = false);

// Pull latest changes in a git repository. Returns true on success.
bool git_pull(const fs::path& repo_dir, std::string_view branch = "main");

// Get the ISO 8601 timestamp of the last commit in a git repo.
// Returns empty string on failure.
std::string git_last_commit_time(const fs::path& repo_dir);

// 1.4.1: resolve the full commit SHA of the repo's current HEAD
// (`git rev-parse HEAD`, trimmed). Returns empty string on failure.
std::string git_head_commit(const fs::path& repo_dir);

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

// ---- Version comparison ----
// Compare two semantic version strings (major.minor.patch).
// Returns -1 if a < b, 0 if equal, 1 if a > b.
// Pre-release tags (-alpha, -beta) and build metadata (+build) are ignored.
// Missing segments are treated as 0 (e.g. "1.0" == "1.0.0").
int compare_version(std::string_view a, std::string_view b);

// 1.4.2 F-27: deterministic total order for version SELECTION. compare_version()
// intentionally ignores pre-release/build metadata, so "1.2.0-rc.1" and "1.2.0"
// compare equal and the entry picked by a repo index depended on TOML file
// order. This adds the semver tie-break on top of the core comparison: a release
// outranks a pre-release of the same core version, and two pre-releases are
// ordered lexicographically.
int compare_version_precedence(std::string_view a, std::string_view b);

// ---- Shell safety ----
// Escape a string for safe use inside double-quoted shell arguments.
// Escapes: " \ ` $
// This prevents command injection when constructing shell commands with paths/URLs.
std::string escape_shell_arg(std::string_view s);

// 1.2.0-dev.11: escape an argument for cmd.exe /c (Windows). cmd's metachar
// set (& | < > ^ % ") differs from POSIX sh — escape_shell_arg does not cover
// it, so any cmd /c "..." concatenation must use this instead.
std::string escape_cmd_arg(std::string_view s);

// 1.4.2 F-15: quote ONE argv element for a Windows command line. run_command's
// Windows path goes straight to CreateProcess (no shell), so the string is
// parsed by the MSVCRT rules: only embedded quotes and runs of backslashes
// directly before a quote or the end need doubling. Backslashes elsewhere are
// literal — applying POSIX escaping (escape_shell_arg) would double them and
// corrupt every Windows path. Returns the fully quoted argument.
std::string quote_windows_arg(std::string_view s);

// 1.4.2 F-15: platform-correct single-argument quoting — Windows MSVCRT
// quoting on Windows, POSIX double-quote escaping elsewhere. Returns the
// complete token (quotes included), so call sites read uniformly:
//   cmd = quote_cli_arg(exe) + " " + quote_cli_arg(arg);
std::string quote_cli_arg(std::string_view s);

// 1.1.3 S5: 构造「打开文件的编辑器」命令串。editor 与 file 都经 escape_shell_arg +
// 双引号包裹，防止 POSIX shell 注入（EDITOR="vim; evil"）与含空格路径拆分。
// POSIX 侧附加 stdin/stdout 重定向到 /dev/tty。返回可直接执行的命令串。
std::string build_editor_command(const std::string& editor, const fs::path& file);

// ---- Link syntax (1.1.0-dev.5) ----

// Parse "@link:<name>/sub/path" into {link_name, sub_path}.
// Returns {name, sub_path} on success; empty name means the string is not a link reference.
struct LinkRef {
    std::string name;       // link name (e.g. "shared_files")
    std::string sub_path;   // sub-path after the link name (e.g. "src/util"), empty if none
};
LinkRef parse_link_syntax(std::string_view raw);

// ---- Utils / Lua plugin discovery ----

// Search for a utils Lua script by name across all installed packages.
// Scans project → user → global scopes, returns first matching .lua path.
// Returns empty path if not found.
fs::path find_utils_script(const std::string& name);

// ---- Fuzzy matching (0.9.4+) ----

// Return candidates whose Levenshtein distance from `input` is ≤ max_distance,
// sorted by distance ascending. Returns empty vector if no match.
std::vector<std::string> closest_match(
    const std::string& input,
    const std::vector<std::string>& candidates,
    int max_distance = 2);

} // namespace ezmk::util
