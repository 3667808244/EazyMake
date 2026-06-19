#pragma once

#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include "ezmk/config.hpp"

namespace ezmk::cache {
namespace fs = std::filesystem;

struct DepEntry {
    std::string path;  // header file path (may be absolute or relative)
    std::string hash;  // SHA-256 of the header content
};

struct FileEntry {
    std::string source_hash;
    std::string object_file;    // relative path under .ezmk/cache/obj/
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

// Parse .d file (gcc -MMD output) and hash every listed header.
// Returns vector of {path, sha256} for each dependency.
std::vector<DepEntry> parse_depfile_and_hash(const fs::path& depfile);

// Compute a signature from compile flags (for global options check).
std::string compile_options_signature(const config::CompileSection& compile);

// Compute a signature including extra include paths (for package builds).
std::string compile_options_signature(const config::CompileSection& compile,
                                      const std::vector<fs::path>& extra_includes);

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
