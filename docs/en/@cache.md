# Build Cache

## Cache Contents

| Type                                      | Storage Location          | Description                                              |
| ----------------------------------------- | ------------------------- | -------------------------------------------------------- |
| Compiled object files (`.o`/`.obj`)       | `.ezmk/cache/obj/`        | Object file corresponding to each source file            |
| Compilation dependency info (incl. header hashes) | `.ezmk/cache/record.json` | Records compilation metadata, used to determine cache hit |

## Cache Hit Determination Process

When executing `ezmk build`, for each source file `src/foo.cpp`:

1. **Compute current source file hash** (based on file content).
2. **Read the cache record** entry for this source file from `record.json`.
3. **Compare basic conditions**:
   - Is the source file hash identical to the last recorded hash?
   - Are the compilation commands (e.g., `[compile] flags`) identical to the last recorded ones?  
     (Compilation options affect output and must be part of the key)
4. **Check all dependent header files**:
   - Iterate over each header file path recorded last time, compute the current hash of that header, and compare it with the last recorded hash.
   - If any hash changes or the set of header file paths changes (added/removed), it is a miss.
5. **Determine result**:
   - If all match → **cache hit**, directly reuse the existing `.o` file, do not recompile.
   - Otherwise → **cache miss**, recompile the source file, and update `record.json` and the `.o` file.

### Flowchart

```
For each source file:
    cur_source_hash = hash(file)
    cur_compile_opts = get_current_flags()
    Read last_entry from record.json
    if last_entry exists 
        && last_entry.source_hash == cur_source_hash
        && last_entry.compile_opts == cur_compile_opts:
        # Check header files
        all_header_match = true
        for each (hdr, last_hash) in last_entry.headers:
            cur_hash = hash(hdr)
            if cur_hash != last_hash:
                all_header_match = false; break
        if all_header_match && header_set_size_equal:
            # Cache hit
            continue
    # Miss: recompile
    compile source -> .o
    generate .d to obtain header file list and hashes
    update record.json
```

## Cache Record Structure (`record.json`)

```json
{
  "version": 1,
  "compile_options_signature": "sha256_of_flags_include_dirs_std_flag_and_env",  // Global compilation options fingerprint (includes msvc_flags, std_flag, include_dirs)
  "files": {
    "src/main.cpp": {
      "source_hash": "a3f5c9...",
      "object_file": ".ezmk/cache/obj/main.o",
      "compiler": "g++",
      "compile_opts": ["-Wall", "-O2"],   // May be redundant with the global fingerprint, useful for debugging
      "dependencies": [
        {"path": "include/foo.h", "hash": "b4e8d2..."},
        {"path": "/usr/include/iostream", "hash": "c6a0b1..."}
      ],
      "last_build_time": "2025-03-01T12:34:56Z"
    },
    "src/utils.cpp": { ... }
  }
}
```

### Field Descriptions
- `version`: Used for cache format evolution.
- `compile_options_signature`: SHA-256 fingerprint of global compilation options, covering `[compile] flags`, `msvc_flags`, `include_dirs`, `std_flag`, `extra_includes`, etc. Any change invalidates the cache for all source files. Can be combined with per-entry `compile_opts` for finer granularity.
- `object_file` relative path.
- System header files (e.g., `/usr/include/iostream`) hashes must also be recorded, since system header upgrades may change compilation results.

## Cache Consistency Maintenance

### Automatic Trigger on Header File Update
The dependent header hash comparison mechanism naturally guarantees: modifying any header file causes all source files that reference it to be recompiled.

### Compilation Option Changes
- Global flag changes → clear all cache entries (simple implementation) or compare `compile_opts` per entry (complex).
- Recommended: store a global flag fingerprint in `record.json`, compare with the current fingerprint before building; if they differ, discard the entire cache and perform a full recompilation.

### File Move/Rename
- If a source file path changes, the old cache entry is retained but will never be hit (because the build scan produces a new entry). Users can manually clean up via `ezmk clean`.
- Header file path changes: old paths in the record become invalid, and compilation will error due to missing headers. Users must ensure paths are correct.

## Cache Disabling

`ezmk build --disable-cache`:
- Ignores `record.json`, recompiles all source files.
- After compilation completes, **overwrites** and updates the cache (so the cache can benefit the next time it is enabled). Alternatively, the cache could be left untouched, but updating is more sensible.

## Cache Debugging (`--verbose`)

`ezmk build --verbose` / `ezmk build -v` prints cache determination details for each source file:

- **On hit**: outputs `[cached]` + matching source file hash and number of header files
- **On miss**: outputs the specific reason (source hash changed / compilation option signature changed / a certain header hash changed / dependency path set changed / cache record missing)

Example output:
```
[ezmk]   [cached] src/utils.cpp
[ezmk]     cache hit: source hash matches, all 5 headers unchanged
[ezmk]   Compiling src/main.cpp
[ezmk]     cache miss: header hash changed — include/foo.h
[ezmk]     cmd: g++ -std=c++17 -c "src/main.cpp" -o ".ezmk/temp/main.o" ...
```

## Atomicity of Incremental Builds
If a build process fails midway, cache consistency must not be corrupted. Approach:
- When compiling a new object file, write to a temporary file first (e.g., `.tmp.o`), then atomically replace the old file after successful compilation.
- When updating `record.json`, write to a temporary file first, then `rename`.

## Cache Workflow

1. First `ezmk build`:
   - Scans `src/`, compiles `main.cpp`, generates `main.o`, records dependent headers and their hashes in `record.json`.
   - Handles other source files similarly.
   - Links to produce the executable.

2. Modifying `include/foo.h` (e.g., changing a macro):
   - Running `ezmk build` again, iterates over source files, finds that the dependent header `foo.h` hash for `main.cpp` has changed → recompiles `main.cpp`.
   - Other source files not depending on `foo.h` (e.g., `utils.cpp`) still hit the cache.
   - Relinks.

3. Running `ezmk clean`:
   - Deletes the `.ezmk/cache/` directory and `record.json`.
   - Next build performs a full compilation.

---

## Implementation Notes

`record.json` reading and writing uses the [nlohmann/json](https://github.com/nlohmann/json) library (single-header `include/vendor/nlohmann_json.hpp`), replacing the hand-written JSON parser from earlier versions. The atomic write strategy remains unchanged: serialize to a `.tmp` file first, then `rename` to overwrite.
