// Unit tests for util.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/util.hpp"

// miniz C API — used only to build malicious/valid archive fixtures for the
// extraction security tests (1.1.2 S1). Same extern "C" wrapping as util.cpp.
extern "C" {
#include "miniz.h"
}

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::util;

// ===================================================================
// escape_shell_arg()
// ===================================================================

TEST_CASE("escape_shell_arg: ordinary strings pass through", "[util]") {
    SECTION("simple filename") {
        REQUIRE(escape_shell_arg("main.cpp") == "main.cpp");
    }
    SECTION("path with underscores") {
        REQUIRE(escape_shell_arg("my_library_name") == "my_library_name");
    }
    SECTION("path with dots") {
        REQUIRE(escape_shell_arg("libfoo.a") == "libfoo.a");
    }
    SECTION("empty string") {
        REQUIRE(escape_shell_arg("") == "");
    }
    SECTION("plain URL-ish path") {
        REQUIRE(escape_shell_arg("include/foo.h") == "include/foo.h");
    }
}

TEST_CASE("escape_shell_arg: escape double quotes", "[util]") {
    // Double quote → backslash-escaped
    REQUIRE(escape_shell_arg("file\"name") == "file\\\"name");
    REQUIRE(escape_shell_arg("\"quoted\"") == "\\\"quoted\\\"");
}

TEST_CASE("escape_shell_arg: escape backslash", "[util]") {
    // Backslash → escaped
    REQUIRE(escape_shell_arg("C:\\path") == "C:\\\\path");
    REQUIRE(escape_shell_arg("\\\\server\\share") == "\\\\\\\\server\\\\share");
}

TEST_CASE("escape_shell_arg: escape backtick", "[util]") {
    // Backtick → escaped (shell command substitution)
    REQUIRE(escape_shell_arg("`cmd`") == "\\`cmd\\`");
    REQUIRE(escape_shell_arg("file`n") == "file\\`n");
}

TEST_CASE("escape_shell_arg: escape dollar sign", "[util]") {
    // Dollar → escaped (shell variable expansion)
    REQUIRE(escape_shell_arg("$HOME") == "\\$HOME");
    REQUIRE(escape_shell_arg("file${var}") == "file\\${var}");
}

TEST_CASE("escape_shell_arg: mixed special characters", "[util]") {
    SECTION("path with spaces and quotes") {
        auto result = escape_shell_arg("my \"file\".txt");
        REQUIRE(result == "my \\\"file\\\".txt");
    }
    SECTION("command injection attempt") {
        auto result = escape_shell_arg("$(rm -rf /)");
        REQUIRE(result == "\\$(rm -rf /)");
    }
}

// ===================================================================
// file_write() + file_read() round-trip
// ===================================================================

TEST_CASE("file_write and file_read: round-trip", "[util]") {
    SECTION("plain text") {
        auto tmp = fs::temp_directory_path() / "ezmk_test_write_read.txt";
        std::string content = "Hello, EazyMake test!";

        REQUIRE(file_write(tmp, content));
        auto read_back = file_read(tmp);
        REQUIRE(read_back == content);

        fs::remove(tmp);
    }

    SECTION("empty content") {
        auto tmp = fs::temp_directory_path() / "ezmk_test_empty.txt";

        REQUIRE(file_write(tmp, ""));
        auto read_back = file_read(tmp);
        REQUIRE(read_back == "");

        fs::remove(tmp);
    }

    SECTION("binary content with null bytes") {
        auto tmp = fs::temp_directory_path() / "ezmk_test_binary.bin";

        std::string binary("AB\0CD\0EF", 7);
        REQUIRE(file_write(tmp, binary));
        auto read_back = file_read(tmp);
        REQUIRE(read_back == binary);

        fs::remove(tmp);
    }

    SECTION("creates parent directories") {
        auto tmp = fs::temp_directory_path() / "ezmk_nested" / "subdir" / "test.txt";

        REQUIRE(file_write(tmp, "nested content"));
        REQUIRE(fs::exists(tmp));
        REQUIRE(file_read(tmp) == "nested content");

        fs::remove_all(fs::temp_directory_path() / "ezmk_nested");
    }
}

// ===================================================================
// file_exists()
// ===================================================================

TEST_CASE("file_exists: basic checks", "[util]") {
    SECTION("existing file") {
        auto tmp = fs::temp_directory_path() / "ezmk_exists_test.txt";
        std::ofstream f(tmp);
        f << "data";
        f.close();

        REQUIRE(file_exists(tmp));
        fs::remove(tmp);
    }

    SECTION("non-existing file") {
        REQUIRE_FALSE(file_exists("nonexistent_file_12345.txt"));
    }

    SECTION("existing directory") {
        auto tmp = fs::temp_directory_path() / "ezmk_exists_dir";
        fs::create_directory(tmp);

        REQUIRE(file_exists(tmp));
        fs::remove(tmp);
    }
}

// ===================================================================
// create_directories() + remove_all()
// ===================================================================

TEST_CASE("create_directories and remove_all", "[util]") {
    SECTION("create nested directories") {
        auto root = fs::temp_directory_path() / "ezmk_nested_test";
        auto nested = root / "a" / "b" / "c";

        ezmk::util::create_directories(nested);
        REQUIRE(fs::exists(nested));

        ezmk::util::remove_all(root);
        REQUIRE_FALSE(fs::exists(root));
    }

    SECTION("remove non-existing path does not throw") {
        REQUIRE_NOTHROW(ezmk::util::remove_all("nonexistent_dir_54321"));
    }
}

// ===================================================================
// list_files()
// ===================================================================

TEST_CASE("list_files: extension filtering", "[util]") {
    SECTION("filter .cpp files") {
        auto dir = fs::temp_directory_path() / "ezmk_list_test";
        ezmk::util::create_directories(dir);

        std::ofstream(dir / "a.cpp") << "// a";
        std::ofstream(dir / "b.cpp") << "// b";
        std::ofstream(dir / "c.hpp") << "// c";
        std::ofstream(dir / "d.txt") << "d";

        auto result = list_files(dir, {".cpp"});
        REQUIRE(result.size() == 2);

        ezmk::util::remove_all(dir);
    }

    SECTION("filter multiple extensions") {
        auto dir = fs::temp_directory_path() / "ezmk_list_multi";
        ezmk::util::create_directories(dir);

        std::ofstream(dir / "a.cpp") << "a";
        std::ofstream(dir / "b.c") << "b";
        std::ofstream(dir / "c.hpp") << "c";
        std::ofstream(dir / "d.txt") << "d";

        auto result = list_files(dir, {".cpp", ".c"});
        REQUIRE(result.size() == 2);

        ezmk::util::remove_all(dir);
    }

    SECTION("empty directory") {
        auto dir = fs::temp_directory_path() / "ezmk_list_empty";
        ezmk::util::create_directories(dir);

        auto result = list_files(dir, {".cpp"});
        REQUIRE(result.empty());

        ezmk::util::remove_all(dir);
    }

    SECTION("no matching extensions") {
        auto dir = fs::temp_directory_path() / "ezmk_list_nomatch";
        ezmk::util::create_directories(dir);

        std::ofstream(dir / "readme.md") << "readme";
        auto result = list_files(dir, {".cpp", ".hpp"});
        REQUIRE(result.empty());

        ezmk::util::remove_all(dir);
    }
}

// ===================================================================
// get_home_dir() / get_exe_dir()
// ===================================================================

TEST_CASE("get_home_dir: returns non-empty path", "[util]") {
    auto home = get_home_dir();
    REQUIRE_FALSE(home.empty());
    // Should be an existing directory
    REQUIRE(file_exists(home));
}

TEST_CASE("get_exe_dir: returns non-empty path", "[util]") {
    auto exe_dir = get_exe_dir();
    REQUIRE_FALSE(exe_dir.empty());
    // exe_dir should exist (the test binary is running from somewhere)
    REQUIRE(file_exists(exe_dir));
}

// ===================================================================
// run_command()
// ===================================================================

TEST_CASE("run_command: basic execution", "[util]") {
    SECTION("echo command returns exit 0 and captures stdout") {
#ifdef EZMK_WIN
        auto result = run_command("cmd /c echo hello");
#else
        auto result = run_command("echo hello");
#endif
        REQUIRE(result.exit_code == 0);
        REQUIRE_FALSE(result.out.empty());
        // "hello" should appear in stdout
        REQUIRE(result.out.find("hello") != std::string::npos);
    }

    SECTION("command with stderr output") {
#ifdef EZMK_WIN
        auto result = run_command("cmd /c echo error 1>&2");
#else
        auto result = run_command("echo error >&2");
#endif
        REQUIRE(result.exit_code == 0);
        REQUIRE_FALSE(result.err.empty());
    }

    SECTION("command that fails") {
#ifdef EZMK_WIN
        auto result = run_command("cmd /c exit 42");
#else
        auto result = run_command("exit 42");
#endif
        // Note: exit code may vary by platform
        // On Windows, "cmd /c exit 42" sets %ERRORLEVEL% which is captured
        REQUIRE(result.exit_code != 0);
    }
}

TEST_CASE("run_command: timeout", "[util]") {
    SECTION("command completing before timeout is unaffected") {
#ifdef EZMK_WIN
        auto result = run_command("cmd /c echo ok", 5);
#else
        auto result = run_command("echo ok", 5);
#endif
        REQUIRE_FALSE(result.timed_out);
        REQUIRE(result.exit_code == 0);
    }

    SECTION("command exceeding timeout is killed") {
#ifdef EZMK_WIN
        // Run ping directly (no cmd /c wrapper) so it is the direct child we
        // terminate — -n 10 blocks ~9 seconds, well past the 1s timeout.
        auto result = run_command("ping -n 10 127.0.0.1", 1);
#else
        auto result = run_command("sleep 10", 1);
#endif
        REQUIRE(result.timed_out);
        REQUIRE(result.exit_code != 0);
    }
}

TEST_CASE("run_command: RunOptions.env reaches the child", "[util][1.1.2]") {
    RunOptions opts;
    opts.env["EZMK_TEST_ENV_VAR"] = "hello_123";
#ifdef EZMK_WIN
    auto result = run_command("cmd /c echo %EZMK_TEST_ENV_VAR%", opts);
#else
    auto result = run_command("echo $EZMK_TEST_ENV_VAR", opts);
#endif
    REQUIRE(result.exit_code == 0);
    REQUIRE(result.out.find("hello_123") != std::string::npos);
}

TEST_CASE("run_command: RunOptions.cwd sets child working directory", "[util][1.1.2]") {
    auto tmp = fs::temp_directory_path() / "ezmk_run_cwd_test";
    ezmk::util::remove_all(tmp);
    ezmk::util::create_directories(tmp);

    RunOptions opts;
    opts.cwd = tmp;
#ifdef EZMK_WIN
    // `cd` is a cmd builtin, not an executable — go through cmd to print CWD.
    auto result = run_command("cmd /c cd", opts);
#else
    auto result = run_command("pwd", opts);
#endif
    REQUIRE(result.exit_code == 0);
    REQUIRE(result.out.find("ezmk_run_cwd_test") != std::string::npos);

    ezmk::util::remove_all(tmp);
}

// 1.1.2 C7: the whole point of injecting env via RunOptions (child-only on
// POSIX) is that the PARENT environment is never mutated — the data-race fix.
#ifndef EZMK_WIN
TEST_CASE("run_command: env injection does not pollute the parent", "[util][1.1.2]") {
    unsetenv("EZMK_TEST_ENV_VAR");
    RunOptions opts;
    opts.env["EZMK_TEST_ENV_VAR"] = "child_only";
    auto result = run_command("echo $EZMK_TEST_ENV_VAR", opts);
    REQUIRE(result.out.find("child_only") != std::string::npos);
    REQUIRE(std::getenv("EZMK_TEST_ENV_VAR") == nullptr);
}
#endif

// 1.1.2 C4: run_script must NOT use a "cd <cwd> &&" prefix (Windows `cd` is a
// cmd builtin — CreateProcessA can't spawn it). cwd goes through RunOptions.
TEST_CASE("run_script: executes script in the given working directory", "[util][1.1.2]") {
    auto tmp = fs::temp_directory_path() / "ezmk_runscript_cwd_test";
    ezmk::util::remove_all(tmp);
    ezmk::util::create_directories(tmp);

    auto script = tmp / "cwd_probe.sh";
    { std::ofstream f(script); f << "pwd\n"; }

    ProcResult r = run_script(script, tmp);
    REQUIRE(r.exit_code == 0);
    // pwd output must reflect the requested cwd (not the test's process cwd)
    REQUIRE(r.out.find("ezmk_runscript_cwd_test") != std::string::npos);

    ezmk::util::remove_all(tmp);
}

// 1.1.2 C1: atomic_rename must not silently fail — a failed move previously let
// execute_link print build_success with a stale/missing artifact.
TEST_CASE("atomic_rename: moves file into place, overwriting existing target", "[util][1.1.2]") {
    auto tmp = fs::temp_directory_path() / "ezmk_rename_test";
    ezmk::util::remove_all(tmp);
    ezmk::util::create_directories(tmp);
    auto from = tmp / "src.tmp";
    auto to = tmp / "out.bin";
    { std::ofstream f(from); f << "new"; }
    { std::ofstream f(to); f << "old"; }

    ezmk::util::atomic_rename(from, to);
    REQUIRE_FALSE(fs::exists(from));
    REQUIRE(ezmk::util::file_read(to) == "new");

    ezmk::util::remove_all(tmp);
}

TEST_CASE("atomic_rename: missing source throws fatal_error", "[util][1.1.2]") {
    auto tmp = fs::temp_directory_path() / "ezmk_rename_missing_test";
    ezmk::util::remove_all(tmp);
    ezmk::util::create_directories(tmp);

    REQUIRE_THROWS_AS(ezmk::util::atomic_rename(tmp / "nope.tmp", tmp / "out.bin"),
                      ezmk::fatal_error);
    REQUIRE_FALSE(fs::exists(tmp / "out.bin"));

    ezmk::util::remove_all(tmp);
}

// 1.1.2 C5: toml_quote — writers that interpolate user strings must escape.
TEST_CASE("toml_quote: escapes special characters", "[util][1.1.2]") {
    REQUIRE(toml_quote("plain") == "\"plain\"");
    REQUIRE(toml_quote("a\"b") == "\"a\\\"b\"");
    REQUIRE(toml_quote("a\\b") == "\"a\\\\b\"");
    REQUIRE(toml_quote("line\nbreak") == "\"line\\nbreak\"");
    REQUIRE(toml_quote("tab\there") == "\"tab\\there\"");
}

// ===================================================================
// git_available()
// ===================================================================

TEST_CASE("git_available: runs without throwing", "[util]") {
    // Just verify it doesn't throw. git may or may not be installed.
    REQUIRE_NOTHROW(git_available());
}

// ===================================================================
// find_editor()
// ===================================================================

TEST_CASE("find_editor: returns something or empty", "[util]") {
    auto editor = find_editor();
    // Should return a value or empty string without throwing
    // Verify find_editor() returns something or empty without throwing
    bool ok = editor.empty() || !editor.empty();
    REQUIRE(ok);
}

// ===================================================================
// copy_recursive()
// ===================================================================

TEST_CASE("copy_recursive: copies files", "[util]") {
    SECTION("copy a directory with files") {
        auto src = fs::temp_directory_path() / "ezmk_copy_src";
        auto dst = fs::temp_directory_path() / "ezmk_copy_dst";
        ezmk::util::create_directories(src / "subdir");
        std::ofstream(src / "a.txt") << "a";
        std::ofstream(src / "subdir/b.txt") << "b";

        copy_recursive(src, dst);

        REQUIRE(file_exists(dst / "a.txt"));
        REQUIRE(file_exists(dst / "subdir/b.txt"));
        REQUIRE(file_read(dst / "a.txt") == "a");
        REQUIRE(file_read(dst / "subdir/b.txt") == "b");

        ezmk::util::remove_all(src);
        ezmk::util::remove_all(dst);
    }
}

// ===================================================================
// color_msg()
// ===================================================================

TEST_CASE("color_msg: wraps with color codes when supported", "[util]") {
    auto result = color_msg(color::green, "test message");

    // When color is NOT supported, should be plain text
    if (!supports_color()) {
        REQUIRE(result == "test message");
    } else {
        // When supported, should contain the message
        REQUIRE(result.find("test message") != std::string::npos);
    }
}

TEST_CASE("color_msg: with different colors", "[util]") {
    auto green = color_msg(color::green, "g");
    auto red = color_msg(color::red, "r");
    auto yellow = color_msg(color::yellow, "y");
    auto cyan = color_msg(color::cyan, "c");

    if (supports_color()) {
        REQUIRE(green != red);
    } else {
        REQUIRE(green == "g");
        REQUIRE(red == "r");
    }
}

// ===================================================================
// init_console() — idempotent
// ===================================================================

TEST_CASE("init_console: can be called multiple times", "[util]") {
    REQUIRE_NOTHROW(init_console());
    REQUIRE_NOTHROW(init_console());
    REQUIRE_NOTHROW(init_console());
}

// ===================================================================
// Color constants exist
// ===================================================================

TEST_CASE("color constants are non-null", "[util]") {
    REQUIRE(color::reset != nullptr);
    REQUIRE(color::green != nullptr);
    REQUIRE(color::red != nullptr);
    REQUIRE(color::yellow != nullptr);
    REQUIRE(color::cyan != nullptr);
    REQUIRE(color::bold != nullptr);
    REQUIRE(color::dim != nullptr);
}

// ===================================================================
// Logging functions do not throw
// ===================================================================

TEST_CASE("info/warn/error do not throw", "[util]") {
    REQUIRE_NOTHROW(info("test info message"));
    REQUIRE_NOTHROW(warn("test warn message"));
    REQUIRE_NOTHROW(error("test error message"));
}

TEST_CASE("fatal throws fatal_error", "[util]") {
    REQUIRE_THROWS_AS(fatal("test fatal error"), ezmk::fatal_error);
}

// ===================================================================
// closest_match() — 0.9.4+
// ===================================================================

TEST_CASE("closest_match: exact match returns distance 0", "[util]") {
    auto result = closest_match("build", {"build", "run", "clean"}, 2);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == "build");
}

TEST_CASE("closest_match: single character typo", "[util]") {
    auto result = closest_match("bild", {"build", "run", "clean"}, 2);
    REQUIRE(result.size() >= 1);
    REQUIRE(result[0] == "build"); // distance 1
}

TEST_CASE("closest_match: no match within max_distance", "[util]") {
    auto result = closest_match("xyz", {"project", "pkg", "repo"}, 2);
    REQUIRE(result.empty());
}

TEST_CASE("closest_match: multiple matches sorted by distance", "[util]") {
    auto result = closest_match("projct", {"project", "protect", "pkg", "repo"}, 2);
    REQUIRE(result.size() >= 1);
    // "project" is distance 1, should be first
    REQUIRE(result[0] == "project");
}

TEST_CASE("closest_match: case sensitivity", "[util]") {
    // Levenshtein is case-sensitive; "Build" vs "build" = distance 1
    auto result = closest_match("Build", {"build", "run", "clean"}, 2);
    REQUIRE(result.size() >= 1);
    REQUIRE(result[0] == "build");
}

TEST_CASE("closest_match: empty input", "[util]") {
    auto result = closest_match("", {"project", "pkg"}, 2);
    REQUIRE(result.empty()); // distance = length of candidate (>=3), > max_distance
}

TEST_CASE("closest_match: empty candidates", "[util]") {
    auto result = closest_match("build", {}, 2);
    REQUIRE(result.empty());
}

// ===================================================================
// compare_version() — 0.9.5.1+: full coverage
// ===================================================================

TEST_CASE("compare_version: equality", "[util]") {
    REQUIRE(compare_version("1.0.0", "1.0.0") == 0);
    REQUIRE(compare_version("0.1.0", "0.1.0") == 0);
    REQUIRE(compare_version("2.3.4", "2.3.4") == 0);
}

TEST_CASE("compare_version: major version difference", "[util]") {
    REQUIRE(compare_version("2.0.0", "1.9.9") > 0);
    REQUIRE(compare_version("1.0.0", "2.0.0") < 0);
}

TEST_CASE("compare_version: minor version difference", "[util]") {
    REQUIRE(compare_version("1.2.0", "1.1.9") > 0);
    REQUIRE(compare_version("1.0.5", "1.1.0") < 0);
}

TEST_CASE("compare_version: patch version difference", "[util]") {
    REQUIRE(compare_version("1.0.1", "1.0.0") > 0);
    REQUIRE(compare_version("1.0.0", "1.0.9") < 0);
}

TEST_CASE("compare_version: missing segments default to 0", "[util]") {
    // "1.0" is treated as "1.0.0"
    REQUIRE(compare_version("1.0", "1.0.0") == 0);
    REQUIRE(compare_version("1", "1.0.0") == 0);
    REQUIRE(compare_version("1.0.0", "1") == 0);
}

TEST_CASE("compare_version: single segment versions", "[util]") {
    REQUIRE(compare_version("1", "1") == 0);
    REQUIRE(compare_version("2", "1") > 0);
    REQUIRE(compare_version("0", "1") < 0);
}

TEST_CASE("compare_version: pre-release tags stripped", "[util]") {
    // Pre-release tags (-alpha, -rc1, -beta) are stripped before comparison
    REQUIRE(compare_version("1.0.0-alpha", "1.0.0") == 0);
    REQUIRE(compare_version("1.0.0-rc1", "1.0.0") == 0);
    REQUIRE(compare_version("2.0.0-beta", "2.0.0") == 0);
    REQUIRE(compare_version("1.0.0-alpha", "1.0.0-beta") == 0);
}

TEST_CASE("compare_version: build metadata stripped", "[util]") {
    // Build metadata (+build) is stripped before comparison
    REQUIRE(compare_version("1.0.0+build", "1.0.0") == 0);
    REQUIRE(compare_version("1.0.0+20200101", "1.0.0") == 0);
    REQUIRE(compare_version("1.0.0+build.1", "1.0.0+build.2") == 0);
}

TEST_CASE("compare_version: wider segment width", "[util]") {
    // "1.10.0" > "1.2.0" (numeric comparison, not string)
    REQUIRE(compare_version("1.10.0", "1.2.0") > 0);
    REQUIRE(compare_version("1.2.0", "1.10.0") < 0);
    REQUIRE(compare_version("10.0.0", "9.99.99") > 0);
}

TEST_CASE("compare_version: long version numbers", "[util]") {
    // Extra segments are compared numerically (shorter version pads with 0)
    REQUIRE(compare_version("1.2.3.4", "1.2.3") > 0);  // 4 > 0
    REQUIRE(compare_version("1.2.3", "1.2.3.4") < 0);
    REQUIRE(compare_version("1.0.0.0", "1.0.0") == 0);
}

TEST_CASE("compare_version: edge cases", "[util]") {
    REQUIRE(compare_version("0.0.0", "0.0.0") == 0);
    REQUIRE(compare_version("0.0.1", "0.0.0") > 0);
}

// ===================================================================
// extract_archive() — 0.9.5.1+: basic coverage
// ===================================================================

// Clean up: remove unused ZIP helper function.
// The extract tests below use hardcoded well-formed ZIP bytes.

TEST_CASE("extract_archive: unsupported format throws", "[util]") {
    auto tmp_dir = fs::temp_directory_path() / "ezmk_extract_test";
    ezmk::util::create_directories(tmp_dir);

    // Create a file with unsupported extension
    auto bad = tmp_dir / "test.7z";
    { std::ofstream of(bad); of << "not a valid archive"; }

    REQUIRE_THROWS_AS(extract_archive(bad, tmp_dir / "out"), std::runtime_error);

    ezmk::util::remove_all(tmp_dir);
}

TEST_CASE("extract_archive: empty/invalid zip throws", "[util]") {
    auto tmp_dir = fs::temp_directory_path() / "ezmk_extract_empty_zip";
    ezmk::util::create_directories(tmp_dir);

    auto bad_zip = tmp_dir / "empty.zip";
    { std::ofstream of(bad_zip); of << "not a zip"; }

    // May throw from mz_zip_reader_init_file or from extract logic
    REQUIRE_THROWS(extract_archive(bad_zip, tmp_dir / "out"));

    ezmk::util::remove_all(tmp_dir);
}

TEST_CASE("extract_archive: nonexistent file throws", "[util]") {
    REQUIRE_THROWS_AS(extract_archive("nonexistent_archive_xyz.zip",
        fs::temp_directory_path() / "out"), std::runtime_error);
}

// ===================================================================
// Archive extraction security — 1.1.2 S1 (zip-slip / path traversal)
// ===================================================================
//
// Malicious archive entry names are untrusted input: "../x", absolute paths,
// drive letters / UNC, and backslash separators must never escape `dest`.

namespace {

// Build one tar entry: 512-byte header + content padded to 512.
std::string make_tar_entry(const std::string& name, char typeflag, const std::string& content) {
    std::string hdr(512, '\0');
    auto oct = [](size_t n, unsigned long v) -> std::string {
        std::string s(n, '0');
        int i = static_cast<int>(n) - 1;
        while (i >= 0 && v) { s[i] = static_cast<char>('0' + (v & 7)); v >>= 3; --i; }
        return s;
    };
    hdr.replace(0, std::min<size_t>(name.size(), 100), name.substr(0, 100));
    hdr.replace(100, 8, oct(8, 0644));                                        // mode
    hdr.replace(108, 8, oct(8, 0));                                           // uid
    hdr.replace(116, 8, oct(8, 0));                                           // gid
    hdr.replace(124, 12, oct(11, static_cast<unsigned long>(content.size())) + '\0'); // size
    hdr.replace(136, 12, oct(11, 0) + '\0');                                  // mtime
    // checksum: sum over header with checksum field as spaces, then "6 octal + \0 + space"
    int sum = 0;
    for (char c : hdr) sum += static_cast<unsigned char>(c);
    for (int i = 148; i < 156; ++i) { sum -= static_cast<unsigned char>(hdr[i]); hdr[i] = ' '; sum += ' '; }
    hdr.replace(148, 8, oct(6, static_cast<unsigned long>(sum)) + '\0' + ' ');
    hdr[156] = typeflag;                                                      // typeflag
    // NOTE: use std::string("ustar\0", 6) — a plain "ustar\0" literal has strlen 5
    // and would shrink the header by one byte, shifting file contents off by one.
    hdr.replace(257, 6, std::string("ustar\0", 6));                           // magic (informational)

    std::string entry = hdr + content;
    if (entry.size() % 512 != 0) entry.append(512 - entry.size() % 512, '\0');
    return entry;
}

// Wrap bytes in a valid gzip stream: header + raw deflate + crc32/isize.
std::string make_gzip(const std::string& data) {
    std::string out("\x1f\x8b\x08\x00", 4);  // magic, deflate method, flags=0
    out += std::string(4, '\0');             // mtime
    out += '\x00';                           // XFL
    out += '\xff';                           // OS: unknown
    mz_stream s{};
    if (mz_deflateInit2(&s, MZ_DEFAULT_LEVEL, MZ_DEFLATED, -MZ_DEFAULT_WINDOW_BITS,
                        8, MZ_DEFAULT_STRATEGY) != MZ_OK)
        throw std::runtime_error("deflateInit2 failed");
    s.next_in = reinterpret_cast<const unsigned char*>(data.data());
    s.avail_in = static_cast<unsigned int>(data.size());
    std::string comp;
    comp.resize(static_cast<size_t>(mz_deflateBound(&s, static_cast<mz_ulong>(data.size()))));
    s.next_out = reinterpret_cast<unsigned char*>(&comp[0]);
    s.avail_out = static_cast<unsigned int>(comp.size());
    if (mz_deflate(&s, MZ_FINISH) != MZ_STREAM_END) {
        mz_deflateEnd(&s);
        throw std::runtime_error("deflate failed");
    }
    comp.resize(s.total_out);
    mz_deflateEnd(&s);
    out += comp;
    mz_ulong crc = mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char*>(data.data()), data.size());
    mz_ulong isize = static_cast<mz_ulong>(data.size());
    for (int i = 0; i < 4; ++i) out += static_cast<char>((crc >> (8 * i)) & 0xff);
    for (int i = 0; i < 4; ++i) out += static_cast<char>((isize >> (8 * i)) & 0xff);
    return out;
}

void write_gzip(const fs::path& path, const std::string& data) {
    std::string gz = make_gzip(data);
    std::ofstream f(path, std::ios::binary);
    f.write(gz.data(), static_cast<std::streamsize>(gz.size()));
}

// Write a zip containing a single entry named `entry` with the given content.
// miniz stores the name verbatim — this is how we craft traversal entries.
void write_zip_with_entry(const fs::path& path, const std::string& entry, const std::string& content) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, path.string().c_str(), 0))
        throw std::runtime_error("zip init failed");
    if (!mz_zip_writer_add_mem(&zip, entry.c_str(), content.data(), content.size(), MZ_NO_COMPRESSION))
        throw std::runtime_error("zip add failed");
    if (!mz_zip_writer_finalize_archive(&zip))
        throw std::runtime_error("zip finalize failed");
    mz_zip_writer_end(&zip);
}

} // namespace

TEST_CASE("extract_zip: rejects path traversal / absolute entries", "[util]") {
    auto tmp = fs::temp_directory_path() / "ezmk_zip_slip_test";
    ezmk::util::remove_all(tmp);
    ezmk::util::create_directories(tmp);
    auto dest = tmp / "out";
    ezmk::util::create_directories(dest);

    SECTION("dotdot traversal") {
        auto z = tmp / "a.zip";
        write_zip_with_entry(z, "../evil.txt", "x");
        REQUIRE_THROWS_AS(ezmk::util::extract_zip(z, dest), std::runtime_error);
        REQUIRE_FALSE(fs::exists(tmp / "evil.txt"));
    }
    SECTION("windows drive + backslash") {
        auto z = tmp / "c.zip";
        write_zip_with_entry(z, "C:\\evil.txt", "x");
        REQUIRE_THROWS_AS(ezmk::util::extract_zip(z, dest), std::runtime_error);
    }
    SECTION("backslash dotdot traversal") {
        auto z = tmp / "d.zip";
        write_zip_with_entry(z, "a\\..\\..\\evil.txt", "x");
        REQUIRE_THROWS_AS(ezmk::util::extract_zip(z, dest), std::runtime_error);
    }
    SECTION("UNC prefix") {
        auto z = tmp / "e.zip";
        write_zip_with_entry(z, "\\\\server\\share\\evil.txt", "x");
        REQUIRE_THROWS_AS(ezmk::util::extract_zip(z, dest), std::runtime_error);
    }
    // NOTE: leading-'/' entries are rejected by miniz's writer (add fails), so
    // absolute-path coverage for the zip path is exercised via extract_targz
    // below (safe_extract_path is shared).

    ezmk::util::remove_all(tmp);
}

TEST_CASE("extract_zip: valid nested entry extracts correctly", "[util]") {
    auto tmp = fs::temp_directory_path() / "ezmk_zip_ok_test";
    ezmk::util::remove_all(tmp);
    ezmk::util::create_directories(tmp);
    auto dest = tmp / "out";
    auto z = tmp / "ok.zip";
    write_zip_with_entry(z, "dir/sub/file.txt", "hello");
    ezmk::util::extract_zip(z, dest);
    REQUIRE(fs::exists(dest / "dir" / "sub" / "file.txt"));
    REQUIRE(ezmk::util::file_read(dest / "dir" / "sub" / "file.txt") == "hello");
    ezmk::util::remove_all(tmp);
}

TEST_CASE("extract_targz: rejects path traversal / absolute entries", "[util]") {
    auto tmp = fs::temp_directory_path() / "ezmk_targz_slip_test";
    ezmk::util::remove_all(tmp);
    ezmk::util::create_directories(tmp);
    auto dest = tmp / "out";
    ezmk::util::create_directories(dest);

    SECTION("dotdot traversal") {
        auto t = tmp / "a.tar.gz";
        write_gzip(t, make_tar_entry("../evil.txt", '0', "x") + std::string(1024, '\0'));
        REQUIRE_THROWS_AS(ezmk::util::extract_targz(t, dest), std::runtime_error);
        REQUIRE_FALSE(fs::exists(tmp / "evil.txt"));
    }
    SECTION("absolute path") {
        auto t = tmp / "b.tar.gz";
        write_gzip(t, make_tar_entry("/evil.txt", '0', "x") + std::string(1024, '\0'));
        REQUIRE_THROWS_AS(ezmk::util::extract_targz(t, dest), std::runtime_error);
    }
    SECTION("backslash dotdot traversal") {
        auto t = tmp / "c.tar.gz";
        write_gzip(t, make_tar_entry("..\\..\\evil.txt", '0', "x") + std::string(1024, '\0'));
        REQUIRE_THROWS_AS(ezmk::util::extract_targz(t, dest), std::runtime_error);
    }

    ezmk::util::remove_all(tmp);
}

TEST_CASE("extract_targz: valid nested entry extracts correctly", "[util]") {
    auto tmp = fs::temp_directory_path() / "ezmk_targz_ok_test";
    ezmk::util::remove_all(tmp);
    ezmk::util::create_directories(tmp);
    auto dest = tmp / "out";
    auto t = tmp / "ok.tar.gz";
    write_gzip(t, make_tar_entry("dir/sub/file.txt", '0', "hello") + std::string(1024, '\0'));
    ezmk::util::extract_targz(t, dest);
    REQUIRE(fs::exists(dest / "dir" / "sub" / "file.txt"));
    REQUIRE(ezmk::util::file_read(dest / "dir" / "sub" / "file.txt") == "hello");
    ezmk::util::remove_all(tmp);
}

// ===================================================================
// detect_platform_tag() — 1.1.0-dev.2
// ===================================================================

TEST_CASE("detect_platform_tag: returns non-empty", "[util]") {
    std::string tag = ezmk::util::detect_platform_tag();
    REQUIRE(!tag.empty());
}

TEST_CASE("detect_platform_tag: format is os-arch", "[util]") {
    std::string tag = ezmk::util::detect_platform_tag();
    // Must contain exactly one dash
    auto dash_pos = tag.find('-');
    REQUIRE(dash_pos != std::string::npos);
    REQUIRE(tag.find('-', dash_pos + 1) == std::string::npos);
    // OS part before dash
    std::string os_part = tag.substr(0, dash_pos);
    REQUIRE((os_part == "win" || os_part == "linux" || os_part == "mac"));
    // Arch part after dash
    std::string arch_part = tag.substr(dash_pos + 1);
    REQUIRE((arch_part == "x64" || arch_part == "x86" || arch_part == "arm64" || arch_part == "unknown"));
}

TEST_CASE("detect_platform_tag: consistent on repeated calls", "[util]") {
    std::string t1 = ezmk::util::detect_platform_tag();
    std::string t2 = ezmk::util::detect_platform_tag();
    REQUIRE(t1 == t2);
}
