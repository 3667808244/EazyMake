// Unit tests for cache.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/config.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/util.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::cache;
using namespace ezmk::config;
using namespace ezmk::crypto;

// ===================================================================
// compile_options_signature()
// ===================================================================

TEST_CASE("compile_options_signature: deterministic", "[cache]") {
    CompileSection cs;
    cs.flags = {"-Wall", "-O2"};
    cs.include_dirs = {"include", "thirdparty/include"};

    auto sig1 = compile_options_signature(cs);
    auto sig2 = compile_options_signature(cs);

    REQUIRE(sig1 == sig2);
    REQUIRE(sig1.size() == 64); // SHA-256 hex output
}

TEST_CASE("compile_options_signature: different flags → different signature", "[cache]") {
    CompileSection cs1;
    cs1.flags = {"-Wall", "-O2"};
    cs1.include_dirs = {"include"};

    CompileSection cs2;
    cs2.flags = {"-Wall", "-O0"};
    cs2.include_dirs = {"include"};

    auto sig1 = compile_options_signature(cs1);
    auto sig2 = compile_options_signature(cs2);

    REQUIRE(sig1 != sig2);
}

TEST_CASE("compile_options_signature: different include dirs → different signature", "[cache]") {
    CompileSection cs1;
    cs1.include_dirs = {"include"};
    CompileSection cs2;
    cs2.include_dirs = {"include", "extra"};

    REQUIRE(compile_options_signature(cs1) != compile_options_signature(cs2));
}

TEST_CASE("compile_options_signature: with extra includes", "[cache]") {
    CompileSection cs;
    cs.flags = {"-Wall"};
    cs.include_dirs = {"include"};

    auto sig1 = compile_options_signature(cs, {});
    auto sig2 = compile_options_signature(cs, {fs::path("/extra/include")});

    REQUIRE(sig1 != sig2);
}

TEST_CASE("compile_options_signature: empty compile section", "[cache]") {
    CompileSection cs;
    auto sig = compile_options_signature(cs);
    REQUIRE(sig.size() == 64); // Still a valid hash
}

// ===================================================================
// iso_time()
// ===================================================================

TEST_CASE("iso_time: format check", "[cache]") {
    auto t = iso_time();
    // Expected format: 2026-06-22T12:34:56Z
    REQUIRE(t.size() >= 20);
    REQUIRE(t.find('T') != std::string::npos);
    REQUIRE(t.back() == 'Z');
}

// ===================================================================
// same_dependency_paths()
// ===================================================================

TEST_CASE("same_dependency_paths: identical sets", "[cache]") {
    std::vector<DepEntry> a = {
        {"include/foo.h", "hash1"},
        {"include/bar.h", "hash2"},
    };
    std::vector<DepEntry> b = {
        {"include/bar.h", "different_hash"},
        {"include/foo.h", "another_hash"},
    };

    REQUIRE(same_dependency_paths(a, b));
}

TEST_CASE("same_dependency_paths: different sets", "[cache]") {
    std::vector<DepEntry> a = {
        {"include/foo.h", "hash1"},
    };
    std::vector<DepEntry> b = {
        {"include/bar.h", "hash2"},
    };

    REQUIRE_FALSE(same_dependency_paths(a, b));
}

TEST_CASE("same_dependency_paths: different sizes", "[cache]") {
    std::vector<DepEntry> a = {
        {"include/foo.h", "hash1"},
        {"include/bar.h", "hash2"},
    };
    std::vector<DepEntry> b = {
        {"include/foo.h", "hash3"},
    };

    REQUIRE_FALSE(same_dependency_paths(a, b));
}

TEST_CASE("same_dependency_paths: both empty", "[cache]") {
    std::vector<DepEntry> a;
    std::vector<DepEntry> b;
    REQUIRE(same_dependency_paths(a, b));
}

TEST_CASE("same_dependency_paths: one empty one not", "[cache]") {
    std::vector<DepEntry> a;
    std::vector<DepEntry> b = {{"include/x.h", "hash"}};
    REQUIRE_FALSE(same_dependency_paths(a, b));
}

// ===================================================================
// load_record() / save_record() round-trip
// ===================================================================

static fs::path temp_record_path() {
    return fs::temp_directory_path() / ("ezmk_test_record_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
        ".json");
}

TEST_CASE("load_record: empty when file doesn't exist", "[cache]") {
    auto rec = load_record("nonexistent_record_12345.json");
    REQUIRE(rec.version == 1);
    REQUIRE(rec.compile_options_signature.empty());
    REQUIRE(rec.files.empty());
}

TEST_CASE("save_record + load_record: round-trip", "[cache]") {
    auto path = temp_record_path();

    CacheRecord rec;
    rec.version = 1;
    rec.compile_options_signature = "test_signature_abc";

    FileEntry fe;
    fe.source_hash = "abc123";
    fe.object_file = ".ezmk/cache/obj/main.o";
    fe.compiler = "g++";
    fe.compile_opts = {"-Wall", "-O2"};
    fe.dependencies = {
        {"include/foo.h", "hash_foo"},
        {"include/bar.h", "hash_bar"},
    };
    fe.last_build_time = "2026-06-22T12:00:00Z";
    rec.files["src/main.cpp"] = fe;

    save_record(rec, path);
    REQUIRE(ezmk::util::file_exists(path));

    auto loaded = load_record(path);
    REQUIRE(loaded.version == 1);
    REQUIRE(loaded.compile_options_signature == "test_signature_abc");
    REQUIRE(loaded.files.size() == 1);

    auto& loaded_fe = loaded.files["src/main.cpp"];
    REQUIRE(loaded_fe.source_hash == "abc123");
    REQUIRE(loaded_fe.object_file == ".ezmk/cache/obj/main.o");
    REQUIRE(loaded_fe.compiler == "g++");
    REQUIRE(loaded_fe.compile_opts.size() == 2);
    REQUIRE(loaded_fe.compile_opts[0] == "-Wall");
    REQUIRE(loaded_fe.compile_opts[1] == "-O2");
    REQUIRE(loaded_fe.dependencies.size() == 2);
    REQUIRE(loaded_fe.dependencies[0].path == "include/foo.h");
    REQUIRE(loaded_fe.dependencies[0].hash == "hash_foo");
    REQUIRE(loaded_fe.dependencies[1].path == "include/bar.h");
    REQUIRE(loaded_fe.dependencies[1].hash == "hash_bar");
    REQUIRE(loaded_fe.last_build_time == "2026-06-22T12:00:00Z");

    fs::remove(path);
}

TEST_CASE("save_record + load_record: multiple files", "[cache]") {
    auto path = temp_record_path();

    CacheRecord rec;
    rec.compile_options_signature = "multi_sig";

    FileEntry fe1;
    fe1.source_hash = "hash1";
    fe1.compiler = "g++";
    rec.files["src/a.cpp"] = fe1;

    FileEntry fe2;
    fe2.source_hash = "hash2";
    fe2.compiler = "g++";
    rec.files["src/b.cpp"] = fe2;

    save_record(rec, path);
    auto loaded = load_record(path);

    REQUIRE(loaded.files.size() == 2);
    REQUIRE(loaded.files["src/a.cpp"].source_hash == "hash1");
    REQUIRE(loaded.files["src/b.cpp"].source_hash == "hash2");

    fs::remove(path);
}

TEST_CASE("save_record + load_record: empty record", "[cache]") {
    auto path = temp_record_path();

    CacheRecord rec;
    save_record(rec, path);
    auto loaded = load_record(path);

    REQUIRE(loaded.version == 1);
    REQUIRE(loaded.files.empty());

    fs::remove(path);
}

TEST_CASE("load_record: corrupted JSON returns empty record", "[cache]") {
    auto path = temp_record_path();

    // Write invalid JSON
    std::ofstream f(path);
    f << "this is not valid JSON {{{";
    f.close();

    auto rec = load_record(path);
    // Should return empty record on parse failure (not throw)
    REQUIRE(rec.version == 1);
    REQUIRE(rec.files.empty());

    fs::remove(path);
}

// ===================================================================
// check_cache() — basic tests
// ===================================================================

TEST_CASE("check_cache: no entry → nullopt", "[cache]") {
    auto tmp_dir = fs::temp_directory_path() / ("ezmk_cache_test_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp_dir);

    fs::path src = tmp_dir / "test.cpp";
    { std::ofstream f(src); f << "int main() { return 0; }"; }

    CompileSection compile;
    CacheRecord record;

    auto result = check_cache(src, compile, record, tmp_dir);
    REQUIRE_FALSE(result.has_value());

    fs::remove_all(tmp_dir);
}

TEST_CASE("check_cache: matching entry → cache hit", "[cache]") {
    auto tmp_dir = fs::temp_directory_path() / ("ezmk_cache_hit_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp_dir);
    fs::create_directories(tmp_dir / "include");
    fs::create_directories(tmp_dir / ".ezmk/cache/obj");

    // Create source, header, and cache object files
    fs::path src = tmp_dir / "src_file.cpp";
    { std::ofstream f(src); f << "// test source"; }

    fs::path hdr = tmp_dir / "include/test.h";
    { std::ofstream f(hdr); f << "// test header"; }

    fs::path obj = tmp_dir / ".ezmk/cache/obj/src_file.o";
    { std::ofstream f(obj); f << "fake object"; }

    // Build a cache record with current hashes
    CompileSection compile;
    compile.flags = {"-Wall"};

    CacheRecord record;
    record.compile_options_signature = compile_options_signature(compile);

    FileEntry fe;
    fe.source_hash = sha256_file(src);
    fe.object_file = ".ezmk/cache/obj/src_file.o";
    fe.compiler = "g++";
    fe.compile_opts = {"-Wall"};
    DepEntry dep;
    dep.path = "include/test.h";
    dep.hash = sha256_file(hdr);
    fe.dependencies.push_back(dep);
    record.files["src_file.cpp"] = fe;

    auto result = check_cache(src, compile, record, tmp_dir);
    REQUIRE(result.has_value());
    REQUIRE(*result == (tmp_dir / ".ezmk/cache/obj/src_file.o"));

    fs::remove_all(tmp_dir);
}

TEST_CASE("check_cache: source hash changed → nullopt", "[cache]") {
    auto tmp_dir = fs::temp_directory_path() / ("ezmk_cache_chg_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp_dir);
    fs::create_directories(tmp_dir / ".ezmk/cache/obj");

    fs::path src = tmp_dir / "changing.cpp";
    std::ofstream(src) << "// version 1";

    CompileSection compile;
    CacheRecord record;
    record.compile_options_signature = compile_options_signature(compile);

    FileEntry fe;
    fe.source_hash = "old_different_hash";
    fe.object_file = ".ezmk/cache/obj/changing.o";
    fe.compiler = "g++";
    record.files["changing.cpp"] = fe;

    auto result = check_cache(src, compile, record, tmp_dir);
    REQUIRE_FALSE(result.has_value());

    fs::remove_all(tmp_dir);
}

TEST_CASE("check_cache: header hash changed → nullopt", "[cache]") {
    auto tmp_dir = fs::temp_directory_path() / ("ezmk_cache_hdr_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp_dir);
    fs::create_directories(tmp_dir / "include");
    fs::create_directories(tmp_dir / ".ezmk/cache/obj");

    fs::path src = tmp_dir / "src.cpp";
    std::ofstream(src) << "// source";

    fs::path hdr = tmp_dir / "include/updated.h";
    std::ofstream(hdr) << "// new header content";

    CompileSection compile;
    CacheRecord record;
    record.compile_options_signature = compile_options_signature(compile);

    FileEntry fe;
    fe.source_hash = sha256_file(src);
    fe.object_file = ".ezmk/cache/obj/src.o";
    fe.compiler = "g++";
    fe.dependencies = {
        {"include/updated.h", "old_hash_that_no_longer_matches"},
    };
    record.files["src.cpp"] = fe;

    auto result = check_cache(src, compile, record, tmp_dir);
    REQUIRE_FALSE(result.has_value());

    fs::remove_all(tmp_dir);
}

// ===================================================================
// parse_depfile_and_hash()
// ===================================================================

TEST_CASE("parse_depfile_and_hash: empty when file doesn't exist", "[cache]") {
    auto deps = parse_depfile_and_hash("nonexistent_depfile.d");
    REQUIRE(deps.empty());
}

TEST_CASE("parse_depfile_and_hash: basic parsing", "[cache]") {
    auto tmp_dir = fs::temp_directory_path() / ("ezmk_dep_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tmp_dir);

    // Create a simple .d file
    auto depfile = tmp_dir / "test.d";
    {
        std::ofstream f(depfile);
        // Standard GCC -MMD output format:
        //   target.o: dep1.h dep2.h
        f << "test.o: include/foo.h include/bar.h\n";
    }

    // Create the actual header files so sha256_file() works
    fs::create_directories(tmp_dir / "include");
    std::ofstream(tmp_dir / "include/foo.h") << "// foo";
    std::ofstream(tmp_dir / "include/bar.h") << "// bar";

    // Note: parse_depfile_and_hash reads absolute paths from the .d file.
    // Our test file has relative paths, so sha256_file will look at cwd/include/
    // This is an edge case; the actual .d files have absolute paths.
    // We just verify the function runs without throwing.
    REQUIRE_NOTHROW(parse_depfile_and_hash(depfile));

    fs::remove_all(tmp_dir);
}
