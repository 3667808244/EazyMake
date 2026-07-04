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
    int version = 1;
    std::string compile_options_signature;
    std::map<std::string, FileEntry> files; // keyed by source path relative to project root
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

// Parse .d file (gcc -MMD output) and hash every listed header.
// Returns vector of {path, sha256} for each dependency.
std::vector<DepEntry> parse_depfile_and_hash(const fs::path& depfile);

// Compute a signature from compile flags, include dirs, MSVC flags, and
// optionally the language standard flag (for cache invalidation).
std::string compile_options_signature(const config::CompileSection& compile);

// Compute a signature including extra include paths and language standard.
std::string compile_options_signature(const config::CompileSection& compile,
                                      const std::vector<fs::path>& extra_includes,
                                      std::string_view std_flag = "");

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
                                    std::string_view std_flag = "");

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
