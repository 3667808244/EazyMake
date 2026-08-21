#pragma once

#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include "ezmk/config.hpp"
#include "ezmk/toolchain.hpp"

namespace ezmk::cache {
namespace fs = std::filesystem;

struct DepEntry {
    std::string path;  // header file path (may be absolute or relative)
    std::string hash;  // SHA-256 of the header content
};

struct FileEntry {
    std::string source_hash;
    std::string object_file;    // relative path under cache obj dir
    std::string compiler;
    std::vector<std::string> compile_opts;
    std::vector<DepEntry> dependencies;
    std::string last_build_time;
};

struct CacheRecord {
    int version = 2;                             // 1.1.0: v1 → v2 (added compiler_version + deterministic)
    std::string compiler;                        // compiler name (e.g. "g++", "cl.exe")
    std::string compiler_version;                // 1.1.0: compiler version string
    std::string compile_options_signature;
    bool deterministic = false;                  // 1.1.0: whether deterministic build was used
    std::map<std::string, FileEntry> files;      // keyed by source path relative to project root
};

// ===================================================================
// Unified compile interface (0.1.5 DRY refactoring)
// ===================================================================

struct CompileInput {
    std::vector<fs::path> sources;           // source files to compile
    fs::path obj_dir;                        // where .o/.obj files go during build
    fs::path dep_dir;                        // where .d dependency files go (GCC only)
    fs::path proj_root;                      // project/package root (for relative cache keys)
    config::CompileSection compile;          // compile options
    config::LanguageInfo lang;               // compiler + std flag
    std::string stdlib;                      // 1.1.0-dev.4: standard library (libstdc++ / libc++)
    std::vector<fs::path> extra_includes;    // extra -I dirs (dependency packages)
    fs::path cache_obj_dir;                  // where cached .o/.obj files are stored permanently
    bool disable_cache = false;              // --disable-cache
    bool use_pic = false;                    // -fPIC for shared libs
    bool verbose = false;                    // --verbose: print compile commands & cache details
    toolchain::Toolchain tc;                 // 0.2.1+ detected toolchain
};

struct CompileResult {
    std::vector<fs::path> objects;   // compiled .o/.obj paths (in obj_dir)
    int cache_hits = 0;
    int cache_misses = 0;
};

// Unified compile loop: for each source, check cache or compile to temp + atomic rename.
// Updates `record` in place with new cache entries. Caller loads/saves the record.
// Dep paths are normalized: absolute paths under proj_root become relative (so package
// caches survive relocation); system headers outside proj_root stay absolute.
CompileResult compile_sources(const CompileInput& in, CacheRecord& record);

// 0.2.3+: Per-file compile result returned by compile_one_source().
// Used for parallel compilation — each thread compiles one file and returns
// its result; the main thread merges results into the record afterward.
struct SingleCompileResult {
    fs::path source;           // original source path
    fs::path object;           // compiled .o/.obj path (in obj_dir)
    bool cache_hit = false;    // true if served from cache
    bool success = false;      // false if compilation failed
    std::string error_msg;     // error details on failure
    std::string rel_src;       // source path relative to proj_root (cache key)
    FileEntry record_entry;    // new cache record entry (only valid if success && !cache_hit)
    std::vector<DepEntry> new_deps; // parsed dependencies (for dependency change detection)
};

// 0.2.3+: Compile a single source file (check cache, compile if needed).
// Designed for parallel use — reads from `record` (read-only during parallel phase),
// does NOT write to record. Returns per-file result for later merge.
// Thread-safe: only reads from record and filesystem; no shared mutable state.
SingleCompileResult compile_one_source(const fs::path& src,
                                       const CompileInput& in,
                                       const CacheRecord& record);

// 1.1.1: Construct the real (unescaped) compile args for a single source file.
// Single source of truth for the compile command: the build path
// (compile_one_source) runs it via join_shell_args(), and compile_db reuses
// the same args for compile_commands.json — so the index can never drift from
// the actual build. `obj` is the (temp) object path the compiler writes to.
std::vector<std::string> build_compile_args(const CompileInput& in,
                                            const fs::path& src,
                                            const fs::path& obj);

// 1.1.3 C1: 解析 SOURCE_DATE_EPOCH。优先级：config[compile].source_date_epoch > 0 用
// 配置值；否则 deterministic 时读环境变量（非数字则警告并按 0 处理，不抛异常——
// -jN 下在 worker 线程调用，抛异常会崩线程）。
uint64_t resolve_source_date_epoch(const config::CompileSection& compile);

// 1.1.1: Join an arg vector into a shell command string (escaping + quoting).
// Args containing whitespace or shell metacharacters are double-quoted so
// run_command() parses them intact (paths with spaces); others are emitted bare.
std::string join_shell_args(const std::vector<std::string>& args);

// 1.3.0-dev.2: command string + optional GCC response file to clean up after
// the run (the .rsp.tmp suffix also lets the build's stale-temp cleanup reap
// leftovers from crashed runs).
struct JoinedCommand {
    std::string cmd;
    fs::path rsp_file;  // non-empty when a response file was written
};

// 1.3.0-dev.2: command-line length fallback — compile/link commands exceeding
// the Windows CreateProcess limit fail cryptically ("command line too long"),
// and injected workspace sibling args (-I/-L/-l) make large projects hit it
// easily. When the joined command would exceed kResponseFileThreshold (16K
// chars, conservative vs the 32767 limit), the args (minus argv[0] = the
// compiler) are written one-per-line into a GCC response file under rsp_dir
// and the command becomes `compiler @<rsp>` (GCC/Clang native response-file
// support; each line is one literal arg, so paths with spaces survive
// without shell quoting). Only used on the GCC/Clang path (MSVC has its own
// response-file syntax and is out of scope — see dev.2 §3.5).
inline constexpr size_t kResponseFileThreshold = 16 * 1024;
JoinedCommand join_args_with_response_file(const std::vector<std::string>& args,
                                           const fs::path& rsp_dir);

// Parse .d file (gcc -MMD output) and hash every listed header.
// Returns vector of {path, sha256} for each dependency.
std::vector<DepEntry> parse_depfile_and_hash(const fs::path& depfile);

// Compute a signature from compile flags, include dirs, MSVC flags, and
// optionally the language standard flag (for cache invalidation).
std::string compile_options_signature(const config::CompileSection& compile);

// Compute a signature including extra include paths and language standard.
// 1.1.2 C2: also folds in stdlib and use_pic — both change the emitted compile
// command (build_compile_args injects -stdlib=... and -fPIC), so omitting them
// served stale objects when [project].stdlib or type static↔shared changed.
std::string compile_options_signature(const config::CompileSection& compile,
                                      const std::vector<fs::path>& extra_includes,
                                      std::string_view std_flag = "",
                                      std::string_view stdlib = "",
                                      bool use_pic = false);

// Check whether a cached .o file is still valid, by comparing:
//  1) source file hash, 2) compile options signature, 3) all stored header hashes.
// Does NOT require the .d file — uses the dependency list stored in the record.
// Returns the path to the cached .o if valid, std::nullopt if cache miss.
std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record);

// Overload with explicit project/package root (for non-cwd builds).
std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record,
                                    const fs::path& proj_root);

// Full overload with extra_includes and std_flag (for cache signature accuracy).
// The extra_includes and std_flag are folded into the compile options signature
// comparison, so installing/removing a dependency package or changing the
// language standard correctly invalidates the cache.
std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record,
                                    const fs::path& proj_root,
                                    const std::vector<fs::path>& extra_includes,
                                    std::string_view std_flag = "",
                                    std::string_view stdlib = "",
                                    bool use_pic = false);

// Load record.json. Returns empty record if file doesn't exist.
CacheRecord load_record();
CacheRecord load_record(const fs::path& json_path);

// Write record.json atomically.
void save_record(const CacheRecord& record);
void save_record(const CacheRecord& record, const fs::path& json_path);

// Remove .ezmk/cache/ entirely.
void clear_cache();

// Current time in ISO 8601 format (for cache entries).
std::string iso_time();

// Compare two dependency lists by path set (ignoring hashes).
// Returns true if the path sets are identical.
bool same_dependency_paths(const std::vector<DepEntry>& old_deps,
                            const std::vector<DepEntry>& new_deps);

} // namespace ezmk::cache
