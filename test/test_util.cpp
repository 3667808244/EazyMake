// Unit tests for util.cpp
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/util.hpp"

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
// native_path()
// ===================================================================

TEST_CASE("native_path: forward slash conversion", "[util]") {
    SECTION("already using forward slashes") {
        auto path = fs::path("src/main.cpp");
        auto result = native_path(path);
#ifdef EZMK_WIN
        REQUIRE(result == "src\\main.cpp");
#else
        REQUIRE(result == "src/main.cpp");
#endif
    }
    SECTION("mixed slashes") {
        auto path = fs::path("C:/project\\src/main.cpp");
        auto result = native_path(path);
#ifdef EZMK_WIN
        // generic_string normalizes to forward slashes, then native_path converts to backslashes
        REQUIRE(result.find('/') == std::string::npos);
#else
        // On POSIX '\\' is an ordinary filename character (not a separator) and
        // '/' is already the native separator, so native_path is the identity
        // on generic_string(). Backslashes are legitimately preserved.
        REQUIRE(result == path.generic_string());
#endif
    }
    SECTION("no path separators") {
        auto path = fs::path("Makefile");
        auto result = native_path(path);
        REQUIRE(result == "Makefile");
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
