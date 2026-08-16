// End-to-end integration tests for EazyMake.
//
// These tests call the compiled `ezmk` binary as a subprocess and verify
// complete workflows: project creation 鈫?dependency install 鈫?build 鈫?run.
//
// All tests are tagged [integration] so they can be run (or skipped) selectively:
//   ./build/test_ezmk "[integration]"          # run all integration tests
//   ./build/test_ezmk "~[integration]"         # skip integration tests (unit only)
//
// Prerequisites:
//   1. The ezmk binary must be compiled first (run `bash build.sh`).
//   2. Set EZMK_TEST_BIN to override the binary path (default: build/ezmk[.exe]).
//   3. Some tests need network access (pkg install); they SKIP if offline.
//
// Platforms: Windows/MSYS2, Linux, macOS.

#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "test_helpers.hpp"
#include "ezmk/util.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/toolchain.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::util;

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Test helpers
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
namespace {

// Find the repo root by walking up from the current directory.
// Looks for a directory containing both "build.sh" and "src/main.cpp".
fs::path find_repo_root() {
    fs::path cwd = fs::current_path();
    while (!cwd.empty() && cwd != cwd.root_path()) {
        if (fs::exists(cwd / "build.sh") && fs::exists(cwd / "src" / "main.cpp")) {
            return cwd;
        }
        cwd = cwd.parent_path();
    }
    return fs::current_path(); // fallback
}

// Resolve the ezmk binary path.
// 1. EZMK_TEST_BIN env var (highest priority)
// 2. build/ezmk[.exe] relative to repo root
fs::path find_ezmk_binary() {
    const char* env = std::getenv("EZMK_TEST_BIN");
    if (env && fs::exists(env)) {
        return fs::path(env);
    }

    fs::path repo_root = find_repo_root();
    fs::path candidate = repo_root / "build" / ("ezmk" EZMK_EXE_SUFFIX);
    if (fs::exists(candidate)) return fs::canonical(candidate);

    // Last resort: relative to cwd
    fs::path fallback = fs::current_path() / "build" / ("ezmk" EZMK_EXE_SUFFIX);
    return fallback;
}

// 1.2.0-dev.8: resolve the standalone ezmk-lua runtime binary (built alongside
// ezmk by build.sh: build/ezmk-lua[.exe]). Skipped gracefully if absent.
fs::path find_ezmk_lua_binary() {
    fs::path repo_root = find_repo_root();
    fs::path candidate = repo_root / "build" / ("ezmk-lua" EZMK_EXE_SUFFIX);
    if (fs::exists(candidate)) return fs::canonical(candidate);
    fs::path fallback = fs::current_path() / "build" / ("ezmk-lua" EZMK_EXE_SUFFIX);
    if (fs::exists(fallback)) return fs::canonical(fallback);
    return {};
}

// Build the shell command to run ezmk in a specific working directory.
// Uses "cd <dir> && ezmk <args>" to avoid changing the process CWD.
std::string build_ezmk_cmd(const std::string& args, const fs::path& cwd) {
    std::string ezmk_path = find_ezmk_binary().string();

#ifdef EZMK_WIN
    // cmd.exe: use cd /d to switch drive + directory. Double-quote paths
    // (no bash escaping needed 鈥?cmd.exe doesn't interpret backslashes).
    return "cd /d \"" + cwd.string() + "\" && \"" + ezmk_path + "\" " + args;
#else
    return "cd " + escape_shell_arg(cwd.string()) + " && " +
           escape_shell_arg(ezmk_path) + " " + args;
#endif
}

// Run ezmk with given arguments in the specified working directory.
// Uses "cd <dir> && ezmk ..." so the process CWD is never changed.
ProcResult run_ezmk(const std::string& args, const fs::path& cwd = fs::current_path()) {
    std::string cmd = build_ezmk_cmd(args, cwd);
#ifdef EZMK_WIN
    cmd = "cmd /c " + cmd;
#endif
    return run_command(cmd);
}



// Check if we can reach github.com (lightweight check).
bool network_available() {
#ifdef EZMK_WIN
    ProcResult r = run_command("cmd /c ping -n 1 -w 3000 github.com");
#else
    ProcResult r = run_command("ping -c 1 -W 3 github.com");
#endif
    return r.exit_code == 0;
}

// Detect if EazyMake binary is available (skip tests gracefully if not).
bool ezmk_available() {
    return fs::exists(find_ezmk_binary());
}

} // anonymous namespace

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Scenario 1: From zero to running project (single linear flow)
//   project new 鈫?verify structure 鈫?build 鈫?verify binary 鈫?run 鈫?verify output
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
TEST_CASE("integration: create project, build, and run (end-to-end)", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found 鈥?build it first with: bash build.sh");
    }

    TempDir tmp;
    std::string proj_name = "test_app";

    // Step 1: Create the project
    {
        ProcResult r = run_ezmk(
            "project new " + proj_name + " --disable-git-init --disable-gitignore",
            tmp.path);

        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);

        fs::path proj_dir = tmp.path / proj_name;
        REQUIRE(fs::exists(proj_dir / "ezmk.toml"));
        REQUIRE(fs::exists(proj_dir / "src" / "main.cpp"));
    }

    fs::path proj_dir = tmp.path / proj_name;

    // Step 2: Build
    {
        ProcResult r = run_ezmk("project build", proj_dir);

        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);

        // Verify the executable was produced
        fs::path exe = proj_dir / "build" / ("test_app" EZMK_EXE_SUFFIX);
        REQUIRE(fs::exists(exe));
    }

    // Step 3: Run
    {
        ProcResult r = run_ezmk("project run", proj_dir);

        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);
        REQUIRE(r.out.find("Hello") != std::string::npos);
    }
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Scenario 2: Incremental build 鈥?cache hit on second build
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
TEST_CASE("integration: incremental build cache hit", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found 鈥?build it first with: bash build.sh");
    }

    TempDir tmp;
    std::string proj_name = "cache_test";

    // Create project and do first build
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);

    fs::path proj_dir = tmp.path / proj_name;

    // First build 鈥?full compilation
    ProcResult first = run_ezmk("project build", proj_dir);
    INFO("first build stderr: " << first.err);
    INFO("first build stdout: " << first.out);
    REQUIRE(first.exit_code == 0);

    // Small delay to ensure timestamps differ
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Second build 鈥?should hit cache
    ProcResult second = run_ezmk("project build", proj_dir);
    INFO("second build stderr: " << second.err);
    INFO("second build stdout: " << second.out);
    REQUIRE(second.exit_code == 0);

    // Look for cache hit indicators in output.
    // The combined output (stdout + stderr) should contain indicators that
    // nothing was recompiled.
    std::string combined = second.out + second.err;
    bool cache_hit = (combined.find("cached") != std::string::npos ||
                      combined.find("up to date") != std::string::npos ||
                      (combined.find("Compiling") == std::string::npos &&
                       combined.find("compiling") == std::string::npos &&
                       combined.find("g++") == std::string::npos));
    REQUIRE(cache_hit);
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Scenario 3: Watch mode 鈥?file change triggers rebuild
// NOTE: This test polls the watch log (ezmk logs to unbuffered stderr) until
// the rebuild is detected, with generous timeouts, so it is robust to machine
// speed instead of relying on fixed sleeps.
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
TEST_CASE("integration: watch mode detects file changes", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found 鈥?build it first with: bash build.sh");
    }

    // Force English output so the log-based detection below is independent of
    // the machine's locale (the ezmk messages themselves are localized).
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "watch_test";

    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);

    fs::path proj_dir = tmp.path / proj_name;

    // Build first so watch starts with a clean state.
    ProcResult build_r = run_ezmk("project build", proj_dir);
    REQUIRE(build_r.exit_code == 0);

    // Start watch mode in background, redirecting output to a log file.
    fs::path log_file = tmp.path / "watch_output.txt";
    std::string ezmk_bin = find_ezmk_binary().string();

#ifdef EZMK_WIN
    // Windows: use start /B to run in background. `start` inherits the parent
    // CWD unless /D is given — without it watch can't find ezmk.toml.
    std::string watch_cmd =
        "cmd /c start \"\" /D \"" + proj_dir.string() + "\" /B " +
        escape_shell_arg(ezmk_bin) +
        " project watch --no-build-on-start > \"" +
        escape_shell_arg(log_file.string()) + "\" 2>&1";
#else
    // POSIX: run in background with &
    std::string watch_cmd =
        "cd " + escape_shell_arg(proj_dir.string()) + " && " +
        escape_shell_arg(ezmk_bin) +
        " project watch --no-build-on-start > " +
        escape_shell_arg(log_file.string()) + " 2>&1 &";
#endif

    run_command(watch_cmd);

    // Poll the log file until it contains `needle` or `timeout` elapses.
    // ezmk logs to stderr (unbuffered), so messages appear promptly even when
    // redirected to a file — polling is reliable.
    auto wait_for_log = [&](const fs::path& file, const std::string& needle,
                            std::chrono::seconds timeout) -> bool {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (fs::exists(file)) {
                std::string content = file_read(file);
                if (content.find(needle) != std::string::npos) return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        return false;
    };

    // Wait for watch to actually start monitoring before touching the source.
    // A fixed sleep here is flaky on slow machines: touching before the watcher
    // is ready means the change is missed entirely.
    bool watch_ready = wait_for_log(log_file, "Watching for changes",
                                    std::chrono::seconds(10));
    INFO("watch started: " << (watch_ready ? "yes" : "no"));

    // Touch the main source file to trigger rebuild
    {
        fs::path main_cpp = proj_dir / "src" / "main.cpp";
        std::ofstream f(main_cpp, std::ios::app);
        f << "// touch for watch test\n";
        f.close();
    }

    // Poll for a rebuild indicator instead of a fixed sleep, so the test is
    // robust to machine speed. Break as soon as the rebuild is detected.
    auto is_rebuild_log = [](const std::string& content) -> bool {
        return content.find("changed") != std::string::npos ||
               content.find("detected") != std::string::npos ||
               content.find("rebuild") != std::string::npos ||
               content.find("compil") != std::string::npos ||
               content.find("build succeeded") != std::string::npos ||
               content.find("Build succeeded") != std::string::npos;
    };

    bool detected_change = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline) {
        std::string content =
            fs::exists(log_file) ? file_read(log_file) : std::string();
        detected_change = is_rebuild_log(content);
        if (detected_change) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Kill any running ezmk watch processes
#ifdef EZMK_WIN
    run_command("cmd /c taskkill /F /IM ezmk.exe 2>nul");
#else
    run_command("pkill -f \"ezmk project watch\" 2>/dev/null || true");
#endif

    std::string log_content =
        fs::exists(log_file) ? file_read(log_file) : std::string();
    INFO("watch log:\n" << log_content);

    CHECK(detected_change);
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Scenario 4: compile_commands.json generation (ezmk utils cc)
// NOTE: ezmk-cc is a built-in Lua tool. The development fallback in
// find_utils_script() looks for ./pkg/ezmk-cc/ relative to CWD, so we
// run this test from the repo root.
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
TEST_CASE("integration: utils cc generates compile_commands.json", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found 鈥?build it first with: bash build.sh");
    }

    TempDir tmp;
    std::string proj_name = "cc_test";

    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);

    fs::path proj_dir = tmp.path / proj_name;

    // ezmk-cc is a built-in tool that find_utils_script() discovers via the
    // development fallback: ./pkg/<name>/utils/<name>.lua relative to CWD.
    // Since the test runs from a temp directory, copy the repo's pkg/ezmk-cc/
    // into the project scope to simulate an installed tool package.
    {
        fs::path repo_root = find_repo_root();
        fs::path src_cc = repo_root / "pkg" / "ezmk-cc";
        if (fs::exists(src_cc)) {
            fs::path dst_cc = proj_dir / ".ezmk" / "pkg" / "ezmk-cc";
            fs::create_directories(dst_cc);
            copy_recursive(src_cc, dst_cc);
        }
    }

    ProcResult r = run_ezmk("utils cc", proj_dir);

    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);

    // If the built-in ezmk-cc tool isn't found, skip gracefully
    if (r.exit_code != 0 &&
        (r.err.find("unknown") != std::string::npos ||
         r.err.find("not found") != std::string::npos ||
         r.err.find("鏈煡") != std::string::npos)) {
        SKIP("ezmk-cc built-in tool not found 鈥?skipping (dev env)");
    }

    REQUIRE(r.exit_code == 0);

    // Verify compile_commands.json exists
    fs::path cc_file = proj_dir / "compile_commands.json";
    REQUIRE(fs::exists(cc_file));

    std::string content = file_read(cc_file);
    REQUIRE_FALSE(content.empty());
    REQUIRE(content.find("main.cpp") != std::string::npos);

    // Basic JSON structure check
    bool looks_like_json = (content.find('[') != std::string::npos &&
                            content.find(']') != std::string::npos &&
                            content.find('{') != std::string::npos);
    REQUIRE(looks_like_json);
}

// 1.1.1: [compile].compile_commands — build auto-generates compile_commands.json.
TEST_CASE("integration: compile_commands auto-generation", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found 鈥?build it first with: bash build.sh");
    }

    SECTION("via --compile-commands flag") {
        TempDir tmp;
        std::string proj_name = "auto_cc_flag";

        ProcResult new_r = run_ezmk(
            "project new " + proj_name + " --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(new_r.exit_code == 0);

        ProcResult r = run_ezmk("build --compile-commands",
                                tmp.path / proj_name);
        REQUIRE(r.exit_code == 0);

        fs::path cc_file = tmp.path / proj_name / "compile_commands.json";
        REQUIRE(fs::exists(cc_file));
    }

    SECTION("via [compile].compile_commands = true") {
        TempDir tmp;
        std::string proj_name = "auto_cc_cfg";

        ProcResult new_r = run_ezmk(
            "project new " + proj_name + " --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(new_r.exit_code == 0);

        fs::path proj_dir = tmp.path / proj_name;
        {
            std::ofstream of(proj_dir / "ezmk.toml");
            of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
                  "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
                  "[compile]\nflags = [\"-Wall\"]\ncompile_commands = true\n\n"
                  "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
                  "[depends]\nlib = []\n";
        }

        ProcResult r = run_ezmk("build", proj_dir);
        REQUIRE(r.exit_code == 0);

        fs::path cc_file = proj_dir / "compile_commands.json";
        REQUIRE(fs::exists(cc_file));

        // The generated index reflects the build's flags.
        auto j = nlohmann::json::parse(file_read(cc_file));
        REQUIRE(j.is_array());
        REQUIRE(j.size() >= 1);
        std::vector<std::string> args = j[0]["arguments"].get<std::vector<std::string>>();
        REQUIRE(std::find(args.begin(), args.end(), "-Wall") != args.end());
    }
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Scenario 5: project new creates expected directory layout
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
TEST_CASE("integration: project new creates expected directory layout", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found 鈥?build it first with: bash build.sh");
    }

    TempDir tmp;

    // Use default behavior (git init enabled) to verify the full layout
    ProcResult r = run_ezmk(
        "project new layout_test --disable-git-init --disable-gitignore",
        tmp.path);

    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);

    fs::path proj_dir = tmp.path / "layout_test";

    // Verify expected files and directories exist.
    // .gitignore is not created because --disable-gitignore was passed.
    std::vector<std::string> expected = {
        "ezmk.toml",
        "src/main.cpp",
        "README.md",
        "include",
        "src",
        "build",
        ".ezmk/pkg",
        ".ezmk/temp",
        ".ezmk/cache"
    };

    for (const auto& item : expected) {
        INFO("Checking: " << (proj_dir / item).string());
        REQUIRE(fs::exists(proj_dir / item));
    }

    // Verify ezmk.toml has correct content
    std::string toml = file_read(proj_dir / "ezmk.toml");
    REQUIRE(toml.find("layout_test") != std::string::npos);
    REQUIRE(toml.find("executable") != std::string::npos);
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Scenario 6: Package install with network (requires network)
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
TEST_CASE("integration: pkg install downloads and installs a package", "[integration][network]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found 鈥?build it first with: bash build.sh");
    }

    if (!network_available()) {
        SKIP("Network not available 鈥?skipping package install test");
    }

    TempDir tmp;
    std::string proj_name = "pkg_test";

    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);

    fs::path proj_dir = tmp.path / proj_name;

    // Register the official repo (user scope) so pkg install works.
    // This is a no-op if already registered.
    ProcResult repo_r = run_ezmk(
        "repo add -u https://github.com/3667808244/ezmk-repo.git --name official",
        proj_dir);
    INFO("repo add: " << repo_r.out << " / " << repo_r.err);

    // Try to install a small package
    ProcResult r = run_ezmk("pkg install catch2 -p -y", proj_dir);

    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);

    // pkg install may fail if the repo isn't set up or network issues.
    if (r.exit_code != 0) {
        SKIP("pkg install failed (network or repo issue) 鈥?skipping");
    }

    // Verify the package was installed
    REQUIRE(fs::exists(proj_dir / ".ezmk" / "pkg"));
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Scenario 7: version and help commands work
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
TEST_CASE("integration: basic CLI commands", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found 鈥?build it first with: bash build.sh");
    }

    // --version
    {
        ProcResult r = run_ezmk("--version");
        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);
        REQUIRE_FALSE(r.out.empty());
    }

    // --help
    {
        ProcResult r = run_ezmk("--help");
        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);
        REQUIRE(r.out.find("ezmk") != std::string::npos);
    }

    // help command via shorthand
    {
        ProcResult r = run_ezmk("h");
        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);
        REQUIRE(r.out.find("ezmk") != std::string::npos);
    }
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Scenario 8: Dependency version constraint validation (0.9.6+)
//   create mock pkg → set constraint → verify build accepts/rejects correctly
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
TEST_CASE("integration: version constraint validation in build", "[integration][0.9.6]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }

    TempDir tmp;
    std::string proj_name = "vconstraint_test";

    // Step 1: Create the project
    {
        ProcResult r = run_ezmk(
            "project new " + proj_name + " --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(r.exit_code == 0);
    }

    fs::path proj_dir = tmp.path / proj_name;

    // Step 2: Create a mock installed package "mylib" v2.0.0 with include/ and src/
    {
        fs::path pkg_root = proj_dir / ".ezmk" / "pkg" / "mylib";
        fs::create_directories(pkg_root / "include");
        fs::create_directories(pkg_root / "src");
        std::string pkg_toml =
            "[project]\n"
            "name = \"mylib\"\n"
            "type = \"static\"\n"
            "version = \"2.0.0\"\n"
            "language = \"C++17\"\n";
        file_write(pkg_root / "ezmk.toml", pkg_toml);
        file_write(pkg_root / "include" / "mylib.hpp", "// mylib header\n");
        file_write(pkg_root / "src" / "mylib.cpp", "// mylib source\n");
    }

    // Step 3a: Exact constraint satisfied — @2.0.0 matches installed 2.0.0
    {
        std::string toml =
            "[project]\n"
            "name = \"" + proj_name + "\"\n"
            "type = \"executable\"\n"
            "version = \"0.1.0\"\n"
            "language = \"C++17\"\n"
            "\n"
            "[compile]\n"
            "flags = [\"-Wall\"]\n"
            "include_dirs = [\"include\"]\n"
            "\n"
            "[link]\n"
            "flags = []\n"
            "link_dirs = []\n"
            "system_target = []\n"
            "\n"
            "[depends]\n"
            "lib = [\"mylib@2.0.0\"]\n";
        file_write(proj_dir / "ezmk.toml", toml);

        ProcResult r = run_ezmk("project build", proj_dir);
        INFO("stdout: " << r.out);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
    }

    // Step 3b: Exact constraint violated — @1.0.0 does NOT match installed 2.0.0
    {
        std::string toml =
            "[project]\n"
            "name = \"" + proj_name + "\"\n"
            "type = \"executable\"\n"
            "version = \"0.1.0\"\n"
            "language = \"C++17\"\n"
            "\n"
            "[compile]\n"
            "flags = [\"-Wall\"]\n"
            "include_dirs = [\"include\"]\n"
            "\n"
            "[link]\n"
            "flags = []\n"
            "link_dirs = []\n"
            "system_target = []\n"
            "\n"
            "[depends]\n"
            "lib = [\"mylib@1.0.0\"]\n";
        file_write(proj_dir / "ezmk.toml", toml);

        {
            std::error_code ec;
            fs::remove_all(proj_dir / ".ezmk" / "cache", ec);
        }

        ProcResult r = run_ezmk("project build", proj_dir);
        INFO("stdout: " << r.out);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code != 0);
        std::string combined = r.out + r.err;
        REQUIRE(combined.find("mylib") != std::string::npos);
    }

    // Step 3c: Compatible constraint satisfied — ^2.0.0 matches installed 2.0.0
    {
        std::string toml =
            "[project]\n"
            "name = \"" + proj_name + "\"\n"
            "type = \"executable\"\n"
            "version = \"0.1.0\"\n"
            "language = \"C++17\"\n"
            "\n"
            "[compile]\n"
            "flags = [\"-Wall\"]\n"
            "include_dirs = [\"include\"]\n"
            "\n"
            "[link]\n"
            "flags = []\n"
            "link_dirs = []\n"
            "system_target = []\n"
            "\n"
            "[depends]\n"
            "lib = [\"mylib^2.0.0\"]\n";
        file_write(proj_dir / "ezmk.toml", toml);

        {
            std::error_code ec;
            fs::remove_all(proj_dir / ".ezmk" / "cache", ec);
        }

        ProcResult r = run_ezmk("project build", proj_dir);
        INFO("stdout: " << r.out);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
    }

    // Step 3d: Compatible constraint violated — ^3.0.0 does NOT match 2.0.0
    {
        std::string toml =
            "[project]\n"
            "name = \"" + proj_name + "\"\n"
            "type = \"executable\"\n"
            "version = \"0.1.0\"\n"
            "language = \"C++17\"\n"
            "\n"
            "[compile]\n"
            "flags = [\"-Wall\"]\n"
            "include_dirs = [\"include\"]\n"
            "\n"
            "[link]\n"
            "flags = []\n"
            "link_dirs = []\n"
            "system_target = []\n"
            "\n"
            "[depends]\n"
            "lib = [\"mylib^3.0.0\"]\n";
        file_write(proj_dir / "ezmk.toml", toml);

        {
            std::error_code ec;
            fs::remove_all(proj_dir / ".ezmk" / "cache", ec);
        }

        ProcResult r = run_ezmk("project build", proj_dir);
        INFO("stdout: " << r.out);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code != 0);
    }

    // Step 3e: No constraint — backward compatible, any version works
    {
        std::string toml =
            "[project]\n"
            "name = \"" + proj_name + "\"\n"
            "type = \"executable\"\n"
            "version = \"0.1.0\"\n"
            "language = \"C++17\"\n"
            "\n"
            "[compile]\n"
            "flags = [\"-Wall\"]\n"
            "include_dirs = [\"include\"]\n"
            "\n"
            "[link]\n"
            "flags = []\n"
            "link_dirs = []\n"
            "system_target = []\n"
            "\n"
            "[depends]\n"
            "lib = [\"mylib\"]\n";
        file_write(proj_dir / "ezmk.toml", toml);

        {
            std::error_code ec;
            fs::remove_all(proj_dir / ".ezmk" / "cache", ec);
        }

        ProcResult r = run_ezmk("project build", proj_dir);
        INFO("stdout: " << r.out);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
    }
}

// 1.1.2 C7: SOURCE_DATE_EPOCH is injected via child env (not process-global
// setenv), so deterministic builds must reproduce identical objects under
// parallel jobs. Two clean -j4 builds → identical .o hashes.
TEST_CASE("integration: deterministic build is reproducible with parallel jobs", "[integration][1.1.2]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }

    TempDir tmp;
    fs::path proj_dir = tmp.path / "det_proj";
    fs::create_directories(proj_dir / "src");

    file_write(proj_dir / "ezmk.toml",
        "[project]\n"
        "name = \"det_proj\"\n"
        "type = \"executable\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[compile]\n"
        "deterministic = true\n"
        "source_date_epoch = 1700000000\n");

    // deterministic builds require an existing ezmk.lock (strict mode);
    // a project with no deps needs only a minimal metadata section.
    file_write(proj_dir / "ezmk.lock",
        "[metadata]\n"
        "version = 1\n");

    // 3 sources so -j4 actually runs workers in parallel
    for (int i = 0; i < 3; ++i) {
        file_write(proj_dir / "src" / ("a" + std::to_string(i) + ".cpp"),
                   "int f" + std::to_string(i) + "() { return " + std::to_string(i) + "; }\n");
    }
    file_write(proj_dir / "src" / "main.cpp",
               "int f0(); int f1(); int f2(); int main() { return f0() + f1() + f2(); }\n");

    // project .o files land in .ezmk/temp/src/ (recursive)
    auto hash_objs = [&]() -> std::vector<std::string> {
        std::vector<std::string> hashes;
        std::error_code ec;
        for (auto& e : fs::recursive_directory_iterator(proj_dir / ".ezmk" / "temp", ec)) {
            if (e.path().extension() == ".o") {
                hashes.push_back(ezmk::crypto::sha256_file(e.path()));
            }
        }
        std::sort(hashes.begin(), hashes.end());
        return hashes;
    };

    {   // Build #1
        ProcResult r = run_ezmk("project build -j4", proj_dir);
        INFO("stdout: " << r.out);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
    }
    auto first = hash_objs();
    REQUIRE(first.size() == 4);  // a0..a2 + main

    {   // Force a full recompile, then build #2
        std::error_code ec;
        fs::remove_all(proj_dir / ".ezmk" / "cache", ec);
        fs::remove_all(proj_dir / ".ezmk" / "temp", ec);
        fs::remove_all(proj_dir / "build", ec);
        ProcResult r = run_ezmk("project build -j4", proj_dir);
        INFO("stdout: " << r.out);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
    }
    auto second = hash_objs();

    REQUIRE(first == second);  // reproducible under -j
}

// 1.2.0: ezmk project cc — formal compile_commands generation command.
TEST_CASE("integration: project cc generates compile_commands.json", "[integration][1.2.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "projcc_test";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    SECTION("default output path") {
        ProcResult r = run_ezmk("project cc", proj_dir);
        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);

        fs::path cc_file = proj_dir / "compile_commands.json";
        REQUIRE(fs::exists(cc_file));
        auto j = nlohmann::json::parse(file_read(cc_file));
        REQUIRE(j.is_array());
        REQUIRE(j.size() >= 1);
        REQUIRE(j[0]["file"] == "src/main.cpp");
    }

    SECTION("-o custom path") {
        ProcResult r = run_ezmk("project cc -o custom.json", proj_dir);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
        REQUIRE(fs::exists(proj_dir / "custom.json"));
        REQUIRE(!fs::exists(proj_dir / "compile_commands.json"));
    }

    SECTION("--profile applies profile flags") {
        // 1.2.0-dev.3: the default template now ships [compile.profile.debug],
        // so a custom profile must use a distinct name to avoid redefinition.
        {
            std::ofstream of(proj_dir / "ezmk.toml", std::ios::app);
            of << "\n[compile.profile.custom]\nflags = [\"-g\", \"-DDEBUG=1\"]\n";
        }
        ProcResult r = run_ezmk("project cc --profile custom", proj_dir);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);

        auto j = nlohmann::json::parse(file_read(proj_dir / "compile_commands.json"));
        std::vector<std::string> args = j[0]["arguments"].get<std::vector<std::string>>();
        REQUIRE(std::find(args.begin(), args.end(), "-g") != args.end());
        REQUIRE(std::find(args.begin(), args.end(), "-DDEBUG=1") != args.end());
    }

    SECTION("--profile=value form") {
        {
            std::ofstream of(proj_dir / "ezmk.toml", std::ios::app);
            of << "\n[compile.profile.rel]\nflags = [\"-O3\"]\n";
        }
        ProcResult r = run_ezmk("project cc --profile=rel", proj_dir);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);

        auto j = nlohmann::json::parse(file_read(proj_dir / "compile_commands.json"));
        std::vector<std::string> args = j[0]["arguments"].get<std::vector<std::string>>();
        REQUIRE(std::find(args.begin(), args.end(), "-O3") != args.end());
    }
}

// 1.2.0: ezmk utils cc is deprecated — emits a warning, still redirects.
TEST_CASE("integration: utils cc prints deprecation warning", "[integration][1.2.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "cc_deprec";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    ProcResult r = run_ezmk("utils cc", proj_dir);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);

    // Deprecation notice + still functional.
    std::string combined = r.out + r.err;
    REQUIRE(combined.find("deprecated") != std::string::npos);
    REQUIRE(combined.find("ezmk project cc") != std::string::npos);
    REQUIRE(r.exit_code == 0);
    REQUIRE(fs::exists(proj_dir / "compile_commands.json"));
}

// 1.2.0: ezmk project export cmake — generate CMakeLists.txt from ezmk.toml.
TEST_CASE("integration: project export cmake generates CMakeLists.txt", "[integration][1.2.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "exp_cmake";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    SECTION("default export") {
        ProcResult r = run_ezmk("project export cmake", proj_dir);
        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);

        fs::path out = proj_dir / "CMakeLists.txt";
        REQUIRE(fs::exists(out));
        std::string content = file_read(out);
        REQUIRE(content.find("project(" + proj_name) != std::string::npos);
        REQUIRE(content.find("add_executable") != std::string::npos);
        REQUIRE(content.find("file(GLOB_RECURSE") != std::string::npos);
    }

    SECTION("refuses overwrite without --overwrite") {
        ProcResult first = run_ezmk("project export cmake", proj_dir);
        REQUIRE(first.exit_code == 0);

        ProcResult second = run_ezmk("project export cmake", proj_dir);
        INFO("stderr: " << second.err);
        REQUIRE(second.exit_code != 0);
        std::string combined = second.out + second.err;
        REQUIRE(combined.find("overwrite") != std::string::npos);

        ProcResult third = run_ezmk("project export cmake --overwrite", proj_dir);
        REQUIRE(third.exit_code == 0);
    }

    SECTION("-o custom output path") {
        ProcResult r = run_ezmk("project export cmake -o build/CMakeLists.txt", proj_dir);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
        REQUIRE(fs::exists(proj_dir / "build" / "CMakeLists.txt"));
        REQUIRE(!fs::exists(proj_dir / "CMakeLists.txt"));
    }
}

// 1.2.0: ezmk project import — convert a CMake project into ezmk.toml.
TEST_CASE("integration: project import converts CMake to ezmk.toml", "[integration][1.2.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    fs::create_directories(tmp.path / "src");
    file_write(tmp.path / "CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.16)\n"
        "project(imp_app VERSION 0.5.0 LANGUAGES CXX)\n"
        "set(SRCS src/main.cpp)\n"
        "add_executable(imp_app ${SRCS})\n"
        "target_compile_definitions(imp_app PRIVATE FOO=1)\n");
    file_write(tmp.path / "src" / "main.cpp",
        "#include <cstdio>\nint main() { std::printf(\"%d\", FOO); return 0; }\n");

    SECTION("basic import produces ezmk.toml") {
        ProcResult r = run_ezmk("project import --from cmake", tmp.path);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
        fs::path out = tmp.path / "ezmk.toml";
        REQUIRE(fs::exists(out));
        std::string content = file_read(out);
        REQUIRE(content.find("name = \"imp_app\"") != std::string::npos);
        REQUIRE(content.find("version = \"0.5.0\"") != std::string::npos);
        REQUIRE(content.find("src_dirs = [\"src\"]") != std::string::npos);
        REQUIRE(content.find("\"FOO\" = \"1\"") != std::string::npos);
    }

    SECTION("case-insensitive --from") {
        ProcResult r = run_ezmk("project import --from CMAKE", tmp.path);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
        REQUIRE(fs::exists(tmp.path / "ezmk.toml"));
    }

    SECTION("refuses overwrite without --overwrite") {
        ProcResult first = run_ezmk("project import", tmp.path);
        REQUIRE(first.exit_code == 0);

        ProcResult second = run_ezmk("project import", tmp.path);
        INFO("stderr: " << second.err);
        REQUIRE(second.exit_code != 0);
        std::string combined = second.out + second.err;
        REQUIRE(combined.find("overwrite") != std::string::npos);

        ProcResult third = run_ezmk("project import --overwrite", tmp.path);
        REQUIRE(third.exit_code == 0);
    }

    SECTION("imported project builds and runs") {
        REQUIRE(run_ezmk("project import", tmp.path).exit_code == 0);
        ProcResult build = run_ezmk("build", tmp.path);
        INFO("stderr: " << build.err);
        REQUIRE(build.exit_code == 0);
        std::string run_out = run_ezmk("run", tmp.path).out;
        REQUIRE(run_out.find("1") != std::string::npos);
    }

    SECTION("rejects non-standard custom commands transactionally") {
        file_write(tmp.path / "CMakeLists.txt",
            "project(x LANGUAGES CXX)\n"
            "add_executable(x main.cpp)\n"
            "add_custom_command(TARGET x POST_BUILD COMMAND echo hi)\n");
        ProcResult r = run_ezmk("project import", tmp.path);
        INFO("stderr: " << r.err);
        REQUIRE(r.exit_code != 0);
        REQUIRE(!fs::exists(tmp.path / "ezmk.toml"));  // 不产出半成品
    }
}

// 1.2.0-dev.3: default template embeds debug/release profiles + default_profile = "debug";
// no --profile build falls back to the default profile.
TEST_CASE("integration: default template profiles + default_profile fallback", "[integration][1.2.0-dev.3]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "dev3_tpl";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    SECTION("generated ezmk.toml embeds profiles + default_profile") {
        std::string toml = file_read(proj_dir / "ezmk.toml");
        REQUIRE(toml.find("default_profile = \"debug\"") != std::string::npos);
        REQUIRE(toml.find("[compile.profile.debug]") != std::string::npos);
        REQUIRE(toml.find("[compile.profile.release]") != std::string::npos);
        // base [compile].flags is warnings-only — no -O* in the base line
        REQUIRE(toml.find("flags = [\"-Wall\", \"-Wextra\"]") != std::string::npos);
    }

    SECTION("default build (no --profile) applies debug flags") {
        ProcResult r = run_ezmk("build -v", proj_dir);
        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);
        std::string combined = r.out + r.err;
        REQUIRE(combined.find("-g") != std::string::npos);
        REQUIRE(combined.find("-O0") != std::string::npos);
    }

    SECTION("--profile debug and --profile release both succeed") {
        ProcResult dbg = run_ezmk("build --profile debug -v", proj_dir);
        INFO("stderr: " << dbg.err);
        REQUIRE(dbg.exit_code == 0);
        REQUIRE((dbg.out + dbg.err).find("-O0") != std::string::npos);

        ProcResult rel = run_ezmk("build --profile release -v", proj_dir);
        INFO("stderr: " << rel.err);
        REQUIRE(rel.exit_code == 0);
        std::string rel_out = rel.out + rel.err;
        REQUIRE(rel_out.find("-O2") != std::string::npos);
        REQUIRE(rel_out.find("-DNDEBUG") != std::string::npos);
    }
}

// 1.2.0-dev.5: ezmk test links catch2 v3 (multi-header). The project has no
// include/vendor/catch2.hpp single-header, so run_tests takes the multi-header
// path and emits a v3-compatible main (`Catch::Session().run(argc, argv)`)
// instead of the removed `CATCH_CONFIG_MAIN`.
TEST_CASE("integration: ezmk test works with catch2 v3", "[integration][1.2.0-dev.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "catch2v3_test";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // v3 multi-header test source.
    fs::create_directories(proj_dir / "test");
    file_write(proj_dir / "test" / "test_math.cpp",
        "#include <catch2/catch_test_macros.hpp>\n"
        "\n"
        "TEST_CASE(\"addition works\", \"[math]\") {\n"
        "    REQUIRE(1 + 1 == 2);\n"
        "}\n");

    // [depends] catch2 (hard dep) + explicit [test] config.
    {
        std::ofstream of(proj_dir / "ezmk.toml");
        of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
              "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
              "[compile]\nflags = [\"-Wall\"]\ninclude_dirs = [\"include\"]\n\n"
              "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
              "[depends]\nlib = [\"catch2\"]\n\n"
              "[test]\ndirs = [\"test\"]\nframework = \"catch2\"\n";
    }

    // Install catch2 into project scope (needs a registered repo; skip offline).
    ProcResult inst = run_ezmk("pkg install catch2 -p -y", proj_dir);
    if (inst.exit_code != 0) {
        SKIP("catch2 install failed (no repo / offline) — skipping");
    }

    ProcResult r = run_ezmk("test", proj_dir);
    INFO("stdout: " << r.out);
    INFO("stderr: " << r.err);

    std::string combined = r.out + r.err;
    // Defensive: if catch2 still isn't resolvable, skip rather than fail.
    if (r.exit_code != 0 && combined.find("Catch2 not found") != std::string::npos) {
        SKIP("catch2 not available — skipping");
    }

    REQUIRE(r.exit_code == 0);
    REQUIRE(combined.find("failed: 0") != std::string::npos);
}

// 1.2.0-dev.6: per-file build timing detail.
// `-v` build prints the "slowest compile units" breakdown; a small default
// build (well under the 5s threshold) does not spam the detail. Timing values
// are non-deterministic, so only the header's presence/absence is asserted.
// `-j4` pins the parallel path so the timing logic (not the serial fallback)
// is what gets exercised.
TEST_CASE("integration: build timing detail (dev.6)", "[integration][1.2.0-dev.6]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "timing_test";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // Add extra sources so the parallel path (num_jobs>1 && >1 source) runs.
    for (int i = 0; i < 3; ++i) {
        file_write(proj_dir / "src" / ("extra" + std::to_string(i) + ".cpp"),
                   "int extra" + std::to_string(i) + "() { return " +
                       std::to_string(i) + "; }\n");
    }

    // -v build → full detail header appears.
    ProcResult v = run_ezmk("build -v -j4", proj_dir);
    INFO("stderr: " << v.err);
    REQUIRE(v.exit_code == 0);
    REQUIRE((v.out + v.err).find("slowest compile units") != std::string::npos);

    // default small build (forced recompile, but fast) → no detail spam.
    ProcResult d = run_ezmk("build --disable-cache -j4", proj_dir);
    INFO("stderr: " << d.err);
    REQUIRE(d.exit_code == 0);
    REQUIRE((d.out + d.err).find("slowest compile units") == std::string::npos);
}

// 1.2.0-dev.7: pkg install <dir> — install a package straight from a source
// directory (no archive, no SHA-256). The installed package must be visible
// to `pkg list -p`.
TEST_CASE("integration: pkg install from a source directory (project scope)", "[integration][1.2.0-dev.7]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "dirinstall_app";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // A source-directory package: include/ + src/ + ezmk.toml
    fs::path pkg_dir = tmp.path / "greet_pkg";
    fs::create_directories(pkg_dir / "include" / "greet");
    fs::create_directories(pkg_dir / "src");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"greet\"\ntype = \"static\"\nversion = \"1.0.0\"\n");
    file_write(pkg_dir / "include" / "greet" / "greet.hpp",
        "#pragma once\nint greet();\n");
    file_write(pkg_dir / "src" / "greet.cpp",
        "#include \"greet/greet.hpp\"\nint greet() { return 42; }\n");

    // Install from the directory into project scope
    ProcResult r = run_ezmk("pkg install \"" + pkg_dir.string() + "\" -p", proj_dir);
    INFO("install stderr: " << r.err);
    INFO("install stdout: " << r.out);
    REQUIRE(r.exit_code == 0);

    // The package must be installed under the project's .ezmk/pkg and listed
    // (pkg list prints to stderr — inspect the combined output)
    ProcResult l = run_ezmk("pkg list -p", proj_dir);
    INFO("list stderr: " << l.err);
    INFO("list stdout: " << l.out);
    REQUIRE(l.exit_code == 0);
    REQUIRE((l.out + l.err).find("greet") != std::string::npos);
    REQUIRE(fs::exists(proj_dir / ".ezmk" / "pkg" / "greet" / "ezmk.toml"));
}

// 1.2.0-dev.7: a directory that is not a valid package (ezmk.toml but no
// src/ and no include/) must be rejected.
TEST_CASE("integration: pkg install from an invalid directory is rejected", "[integration][1.2.0-dev.7]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "dirinstall_bad";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    fs::path bad_dir = tmp.path / "badpkg";
    fs::create_directories(bad_dir);
    file_write(bad_dir / "ezmk.toml",
        "[project]\nname = \"badpkg\"\ntype = \"static\"\nversion = \"0.1.0\"\n");

    ProcResult r = run_ezmk("pkg install \"" + bad_dir.string() + "\" -p", proj_dir);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code != 0);
    std::string combined = r.out + r.err;
    REQUIRE(combined.find("src/") != std::string::npos);
}

// 1.2.0-dev.7: upward project-root search — ezmk build works from a nested
// subdirectory, and artifacts land in the located project root's build/.
TEST_CASE("integration: build works from a subdirectory (upward search)", "[integration][1.2.0-dev.7]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "upward_app";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // Build from a subdirectory (2 levels below the project root)
    fs::path subdir = proj_dir / "docs" / "guide";
    fs::create_directories(subdir);
    ProcResult r = run_ezmk("build", subdir);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);

    // Artifacts land in the project root's build/, not the subdirectory
    fs::path exe = proj_dir / "build" / (proj_name + EZMK_EXE_SUFFIX);
    REQUIRE(fs::exists(exe));
}

// 1.2.0-dev.7: beyond the 5-level upward-search boundary, the command must
// fail rather than silently use a wrong directory.
TEST_CASE("integration: build beyond the 5-level upward search fails", "[integration][1.2.0-dev.7]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "deep_app";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // 6 levels deep → the project root is beyond the search limit
    fs::path deep = proj_dir;
    for (int i = 0; i < 6; ++i) deep = deep / "d";
    fs::create_directories(deep);

    ProcResult r = run_ezmk("build", deep);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code != 0);
}

// 1.2.0-dev.7: no ezmk.toml anywhere within the search boundary → a clear
// fatal error (rather than the old generic "config file not found").
TEST_CASE("integration: build with no ezmk.toml fails with clear message", "[integration][1.2.0-dev.7]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    fs::path work = tmp.path / "work";
    fs::create_directories(work);

    ProcResult r = run_ezmk("build", work);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code != 0);
    REQUIRE((r.out + r.err).find("ezmk.toml") != std::string::npos);
}

// 1.2.0-dev.8: the standalone ezmk-lua runtime runs a hook script, injects ctx
// from CLI flags, and propagates run(ctx)'s return value as the exit code.
TEST_CASE("integration: ezmk-lua runs a sample hook with ctx + return code", "[integration][1.2.0-dev.8]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    fs::path lua_bin = find_ezmk_lua_binary();
    if (lua_bin.empty()) {
        SKIP("ezmk-lua binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    fs::path proj = tmp.path / "hookproj";
    fs::create_directories(proj);
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"hookproj\"\ntype = \"executable\"\nversion = \"1.0.0\"\n");
    file_write(proj / "hook.lua", R"(
function run(ctx)
    assert(ctx.project_root:find("hookproj"), "project_root: " .. tostring(ctx.project_root))
    assert(ctx.output:find("app"), "output: " .. tostring(ctx.output))
    assert(ctx.profile == "release", "profile: " .. tostring(ctx.profile))
    -- ezmk.* API resolves config from --project-root
    assert(ezmk.project_name() == "hookproj", "project_name: " .. tostring(ezmk.project_name()))
    return 9
end
)");

    std::string bin = "\"" + lua_bin.string() + "\"";
    std::string script = "\"" + (proj / "hook.lua").string() + "\"";
    std::string cmd = bin + " " + script +
        " --project-root \"" + proj.string() + "\"" +
        " --profile release" +
        " --output \"" + (proj / "build" / "app" EZMK_EXE_SUFFIX).string() + "\"";
    ProcResult r = run_command(cmd);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 9);
}

// 1.2.0-dev.8: `ezmk build` still runs hooks via the sandboxed runtime —
// the sandbox path must be unchanged (hard regression gate for dev.8).
TEST_CASE("integration: ezmk build runs hooks (sandbox path unchanged)", "[integration][1.2.0-dev.8]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "hookbuild";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // A pre_build hook that writes a marker file via ezmk.file_write, AND
    // attempts to escape the project root (../escaped.marker) — the write hard
    // limit must deny it (1.2.0-dev.11: real assertion, not a vacuous check).
    fs::create_directories(proj_dir / "scripts");
    file_write(proj_dir / "scripts" / "pre.lua", R"(
function run(ctx)
    ezmk.file_write(".ezmk/prehook.marker", "ran")
    local ok, err = ezmk.file_write("../escaped.marker", "x")
    if ok then
        error("escape write unexpectedly succeeded: " .. tostring(err))
    end
    return 0
end
)");
    // Append the hooks section to the generated ezmk.toml.
    {
        std::ifstream in(proj_dir / "ezmk.toml");
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        content += "\n[hooks]\npre_build = \"scripts/pre.lua\"\n";
        file_write(proj_dir / "ezmk.toml", content);
    }

    ProcResult r = run_ezmk("build", proj_dir);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(fs::exists(proj_dir / ".ezmk" / "prehook.marker"));
    // The hook's escape attempt was denied — the marker must not exist.
    REQUIRE(!fs::exists(proj_dir.parent_path() / "escaped.marker"));
}

// 1.2.0-dev.8: `ezmk project export cmake` emits the ezmk-lua hook commands and
// prints the hooks note.
TEST_CASE("integration: export cmake emits ezmk-lua hook commands + note", "[integration][1.2.0-dev.8]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "hookexport";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    fs::create_directories(proj_dir / "scripts");
    file_write(proj_dir / "scripts" / "pre.lua", "function run(ctx) return 0 end\n");
    {
        std::ifstream in(proj_dir / "ezmk.toml");
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        content += "\n[hooks]\npre_build = \"scripts/pre.lua\"\n";
        file_write(proj_dir / "ezmk.toml", content);
    }

    ProcResult r = run_ezmk("project export cmake --overwrite", proj_dir);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    std::string cmake = file_read(proj_dir / "CMakeLists.txt");
    REQUIRE(cmake.find("find_program(EZMK_LUA ezmk-lua)") != std::string::npos);
    REQUIRE(cmake.find("add_custom_command(TARGET " + proj_name + " PRE_BUILD")
            != std::string::npos);
    REQUIRE(cmake.find("${CMAKE_CURRENT_SOURCE_DIR}/scripts/pre.lua") != std::string::npos);
    // The export-time note about the ezmk-lua dependency.
    REQUIRE((r.out + r.err).find("hooks exported") != std::string::npos);
}

// 1.2.0-dev.9: a package with custom [compile].src_dirs + include_dirs is
// installed from a source directory, compiled from BOTH src dirs, and linked
// into a consumer project. The package is type = "executable" with NO main.cpp
// — packages are always static libs, so require_main=false must apply. pkg info
// shows the src_dirs; the consumer's compile_commands.json carries the
// package's custom include dir.
TEST_CASE("integration: custom src_dirs+include_dirs package builds and links (dev.9)", "[integration][1.2.0-dev.9]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "convapp";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // Source-directory package: src_dirs = ["src","generated"] and
    // include_dirs = ["include","extra"]. type = "executable" + no main.cpp
    // exercises the require_main=false path end-to-end.
    fs::path pkg_dir = tmp.path / "conv_pkg";
    fs::create_directories(pkg_dir / "include" / "conv");
    fs::create_directories(pkg_dir / "extra");
    fs::create_directories(pkg_dir / "src");
    fs::create_directories(pkg_dir / "generated");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"conv\"\ntype = \"executable\"\nversion = \"1.0.0\"\n\n"
        "[compile]\nsrc_dirs = [\"src\", \"generated\"]\ninclude_dirs = [\"include\", \"extra\"]\n");
    file_write(pkg_dir / "include" / "conv" / "conv.hpp",
        "#pragma once\nint conv_add(int a, int b);\nint conv_mul(int a, int b);\n");
    // gen.cpp sits in the non-default src_dir "generated" and includes a header
    // from the CUSTOM include dir "extra" — proves self-compile resolves
    // src_dirs + include_dirs relative to the package root.
    file_write(pkg_dir / "extra" / "conv_extra.hpp",
        "#pragma once\n#define CONV_SCALE 2\n");
    file_write(pkg_dir / "src" / "conv.cpp",
        "#include \"conv/conv.hpp\"\nint conv_add(int a, int b) { return a + b; }\n");
    file_write(pkg_dir / "generated" / "gen.cpp",
        "#include \"conv_extra.hpp\"\nint conv_mul(int a, int b) { return a * b * CONV_SCALE; }\n");

    // Install from the directory into project scope.
    ProcResult r = run_ezmk("pkg install \"" + pkg_dir.string() + "\" -p", proj_dir);
    INFO("install stderr: " << r.err);
    INFO("install stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    // The static library must have been built (GCC/Clang: .a, MSVC: .lib).
    bool lib_exists =
        fs::exists(proj_dir / ".ezmk" / "pkg" / "conv" / "build" / "libconv.a") ||
        fs::exists(proj_dir / ".ezmk" / "pkg" / "conv" / "build" / "libconv.lib");
    REQUIRE(lib_exists);

    // pkg info displays the configured src_dirs (en locale).
    ProcResult info_r = run_ezmk("pkg info conv", proj_dir);
    INFO("info stderr: " << info_r.err);
    INFO("info stdout: " << info_r.out);
    REQUIRE(info_r.exit_code == 0);
    REQUIRE((info_r.out + info_r.err).find("Source dirs: src generated")
            != std::string::npos);

    // Consumer project: depends on conv and uses both functions.
    {
        std::ofstream of(proj_dir / "ezmk.toml");
        of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
              "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
              "[compile]\nflags = [\"-Wall\"]\ncompile_commands = true\n\n"
              "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
              "[depends]\nlib = [\"conv\"]\n";
    }
    file_write(proj_dir / "src" / "main.cpp",
        "#include \"conv/conv.hpp\"\n#include <cstdio>\n"
        "int main() { std::printf(\"%d %d\\n\", conv_add(2, 3), conv_mul(2, 3)); return 0; }\n");

    ProcResult b = run_ezmk("build", proj_dir);
    INFO("build stderr: " << b.err);
    INFO("build stdout: " << b.out);
    REQUIRE(b.exit_code == 0);

    // Run the executable: conv_add(2,3)=5, conv_mul(2,3)=2*3*2=12.
    fs::path exe = proj_dir / "build" / (proj_name + EZMK_EXE_SUFFIX);
    REQUIRE(fs::exists(exe));
    ProcResult run_r = run_command("\"" + exe.string() + "\"");
    INFO("run stderr: " << run_r.err);
    INFO("run stdout: " << run_r.out);
    REQUIRE(run_r.exit_code == 0);
    REQUIRE(run_r.out.find("5 12") != std::string::npos);

    // compile_commands.json carries the package's custom include dir
    // (installed location) — proves the consumer-side include_dirs wiring.
    fs::path cc_file = proj_dir / "compile_commands.json";
    REQUIRE(fs::exists(cc_file));
    auto j = nlohmann::json::parse(file_read(cc_file));
    REQUIRE(j.is_array());
    std::string expected_inc = "-I" +
        (proj_dir / ".ezmk" / "pkg" / "conv" / "extra").string();
    // Windows fs::path may mix '/' and '\' (e.g. proj_root / ".ezmk/pkg" in
    // build.cpp), so compare with normalized separators.
    auto norm_sep = [](const std::string& s) {
        std::string out = s;
#ifdef EZMK_WIN
        std::replace(out.begin(), out.end(), '/', '\\');
#endif
        return out;
    };
    bool found_extra_inc = false;
    for (auto& entry : j) {
        auto args = entry["arguments"].get<std::vector<std::string>>();
        for (auto& a : args) {
            if (norm_sep(a) == norm_sep(expected_inc)) { found_extra_inc = true; break; }
        }
        if (found_extra_inc) break;
    }
    REQUIRE(found_extra_inc);
}

// 1.2.0-dev.12: [test].default_profile / include_dirs / link_targets take
// effect, --profile overrides default_profile, and the deprecated [test].flags
// fires a warning while still working. Uses the ezmk built-in framework (no
// catch2 / network needed) and -V so the test stdout + compile command surface.
TEST_CASE("integration: [test] default_profile + include_dirs + link_targets (dev.12)", "[integration][1.2.0-dev.12]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    TempDir tmp;
    std::string proj_name = "testprof";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // Test source in test/ that needs BOTH the profile macro (TEST_PROFILE,
    // defined by the selected compile profile) and the custom include dir
    // test/helpers (via [test].include_dirs — NOT the source file's dir).
    fs::create_directories(proj_dir / "test" / "helpers");
    file_write(proj_dir / "test" / "helpers" / "helper.hpp",
        "#pragma once\n#define HELP 7\n");
    file_write(proj_dir / "test" / "t.cpp",
        "#include \"helper.hpp\"\n#include <cstdio>\n"
        "int main() {\n"
        "    std::printf(\"PROFILE=%d HELP=%d\\n\", TEST_PROFILE, HELP);\n"
        "    return 0;\n"
        "}\n");

    {
        std::ofstream of(proj_dir / "ezmk.toml");
        of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
              "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
              "[compile]\nflags = [\"-Wall\"]\n\n"
              "[compile.profile.tsan]\n"
              "macros.TEST_PROFILE = 1\n\n"
              "[compile.profile.debug]\n"
              "macros.TEST_PROFILE = 2\n\n"
              "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
              "[depends]\nlib = []\n\n"
              "[test]\ndirs = [\"test\"]\nframework = \"ezmk\"\n"
              "default_profile = \"tsan\"\n"
              "include_dirs = [\"test/helpers\"]\n"
              "link_targets = [\"m\"]\n"
              "flags = [\"-DTEST_FLAG\"]\n";   // deprecated — must warn
    }

    // 1) default_profile = "tsan" applies the profile macro (PROFILE=1) and the
    //    include_dirs header resolves (HELP=7); deprecated flags warn.
    ProcResult r1 = run_ezmk("test -V", proj_dir);
    INFO("run1 stderr: " << r1.err);
    INFO("run1 stdout: " << r1.out);
    std::string c1 = r1.out + r1.err;
    REQUIRE(r1.exit_code == 0);
    REQUIRE(c1.find("PROFILE=1") != std::string::npos);
    REQUIRE(c1.find("HELP=7") != std::string::npos);
    REQUIRE(c1.find("deprecated") != std::string::npos);

    // 2) link_targets: the verbose compile command carries -lm. On MSVC the
    //    ezmk-framework link command never took -l flags (pre-existing
    //    GCC/Clang-only behavior), so that assertion is skipped there.
    bool is_msvc = (ezmk::toolchain::detect_toolchain().family ==
                    ezmk::toolchain::CompilerFamily::Msvc);
    if (!is_msvc) {
        REQUIRE(c1.find("-lm") != std::string::npos);
    }

    // 3) --profile overrides default_profile → debug profile macro (PROFILE=2).
    ProcResult r2 = run_ezmk("test --profile debug -V", proj_dir);
    INFO("run2 stderr: " << r2.err);
    INFO("run2 stdout: " << r2.out);
    std::string c2 = r2.out + r2.err;
    REQUIRE(r2.exit_code == 0);
    REQUIRE(c2.find("PROFILE=2") != std::string::npos);
}

// 1.2.0-dev.10: a precompiled package with a toolchain-tagged archive
// (lib<name>.<os>-<arch>-<compiler>.a, L3) is selected over a bare decoy with
// DIFFERENT symbols — if the wrong archive were chosen the link would fail.
// The installed package's `pkg info` lists the variants.
TEST_CASE("integration: precompiled toolchain-tagged archive selected + linked (dev.10)", "[integration][1.2.0-dev.10]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    auto tc = ezmk::toolchain::detect_toolchain();
    if (tc.family == ezmk::toolchain::CompilerFamily::Msvc) {
        SKIP("archive creation via ar is GCC/Clang-only — skipping");
    }
    std::string plat = ezmk::util::detect_platform_tag();
    std::string comp = ezmk::toolchain::compiler_tag(tc);
    REQUIRE_FALSE(comp.empty());
    std::string tag = plat + "-" + comp;

    std::string proj_name = "precapp";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // Precompiled package: lib/<tag>.a provides prec_answer(); the bare decoy
    // libprec.a provides only prec_decoy() — linking must use the tagged one.
    fs::path pkg_dir = tmp.path / "prec_pkg";
    fs::create_directories(pkg_dir / "include");
    fs::create_directories(pkg_dir / "lib");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"prec\"\ntype = \"static\"\nversion = \"1.0.0\"\nprecompiled = true\n");
    std::string cxx = tc.cxx_compiler.string();
    fs::path good_src = tmp.path / "good.cpp";
    file_write(good_src, "int prec_answer() { return 42; }\n");
    fs::path good_obj = tmp.path / "good.o";
    REQUIRE(run_command("\"" + cxx + "\" -c \"" + good_src.string() +
                        "\" -o \"" + good_obj.string() + "\"").exit_code == 0);
    fs::path good_ar = pkg_dir / "lib" / ("libprec." + tag + ".a");
    REQUIRE(run_command("ar rcs \"" + good_ar.string() + "\" \"" +
                        good_obj.string() + "\"").exit_code == 0);
    fs::path bad_src = tmp.path / "bad.cpp";
    file_write(bad_src, "int prec_decoy() { return 1; }\n");
    fs::path bad_obj = tmp.path / "bad.o";
    REQUIRE(run_command("\"" + cxx + "\" -c \"" + bad_src.string() +
                        "\" -o \"" + bad_obj.string() + "\"").exit_code == 0);
    fs::path bad_ar = pkg_dir / "lib" / "libprec.a";
    REQUIRE(run_command("ar rcs \"" + bad_ar.string() + "\" \"" +
                        bad_obj.string() + "\"").exit_code == 0);

    ProcResult inst = run_ezmk("pkg install \"" + pkg_dir.string() + "\" -p", proj_dir);
    INFO("install stderr: " << inst.err);
    INFO("install stdout: " << inst.out);
    REQUIRE(inst.exit_code == 0);

    // pkg info lists the variants (sorted; bare + toolchain tag).
    ProcResult info_r = run_ezmk("pkg info prec", proj_dir);
    INFO("info stderr: " << info_r.err);
    REQUIRE(info_r.exit_code == 0);
    REQUIRE((info_r.out + info_r.err).find("Precompiled variants") != std::string::npos);
    REQUIRE((info_r.out + info_r.err).find(tag) != std::string::npos);

    // Consumer depends on prec and calls prec_answer().
    {
        std::ofstream of(proj_dir / "ezmk.toml");
        of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
              "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
              "[compile]\nflags = [\"-Wall\"]\n\n"
              "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
              "[depends]\nlib = [\"prec\"]\n";
    }
    file_write(proj_dir / "src" / "main.cpp",
        "#include <cstdio>\nint prec_answer();\n"
        "int main() { std::printf(\"%d\\n\", prec_answer()); return 0; }\n");

    ProcResult b = run_ezmk("build", proj_dir);
    INFO("build stderr: " << b.err);
    INFO("build stdout: " << b.out);
    REQUIRE(b.exit_code == 0);
    // Toolchain-tagged selection is safe — no ABI fallback warning.
    REQUIRE((b.out + b.err).find("may be ABI-incompatible") == std::string::npos);

    // prec_answer() = 42 proves the tagged archive (not the bare decoy) linked.
    fs::path exe = proj_dir / "build" / (proj_name + EZMK_EXE_SUFFIX);
    REQUIRE(fs::exists(exe));
    ProcResult run_r = run_command("\"" + exe.string() + "\"");
    INFO("run stdout: " << run_r.out);
    REQUIRE(run_r.exit_code == 0);
    REQUIRE(run_r.out.find("42") != std::string::npos);
}

// 1.2.0-dev.10: a precompiled package with only an os-arch archive (no toolchain
// tag) still builds — with an explicit ABI fallback warning.
TEST_CASE("integration: precompiled os-arch fallback warns (dev.10)", "[integration][1.2.0-dev.10]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    auto tc = ezmk::toolchain::detect_toolchain();
    if (tc.family == ezmk::toolchain::CompilerFamily::Msvc) {
        SKIP("archive creation via ar is GCC/Clang-only — skipping");
    }
    std::string plat = ezmk::util::detect_platform_tag();

    std::string proj_name = "precwarn";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    fs::path pkg_dir = tmp.path / "prec2_pkg";
    fs::create_directories(pkg_dir / "include");
    fs::create_directories(pkg_dir / "lib");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"prec2\"\ntype = \"static\"\nversion = \"1.0.0\"\nprecompiled = true\n");
    fs::path srcf = tmp.path / "p.cpp";
    file_write(srcf, "int prec2_answer() { return 7; }\n");
    fs::path objf = tmp.path / "p.o";
    REQUIRE(run_command("\"" + tc.cxx_compiler.string() + "\" -c \"" + srcf.string() +
                        "\" -o \"" + objf.string() + "\"").exit_code == 0);
    fs::path ar = pkg_dir / "lib" / ("libprec2." + plat + ".a");
    REQUIRE(run_command("ar rcs \"" + ar.string() + "\" \"" + objf.string() + "\"").exit_code == 0);

    ProcResult inst = run_ezmk("pkg install \"" + pkg_dir.string() + "\" -p", proj_dir);
    INFO("install stderr: " << inst.err);
    REQUIRE(inst.exit_code == 0);
    // The install-time selection also degrades → warning expected.
    REQUIRE((inst.out + inst.err).find("may be ABI-incompatible") != std::string::npos);

    {
        std::ofstream of(proj_dir / "ezmk.toml");
        of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
              "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
              "[compile]\nflags = [\"-Wall\"]\n\n"
              "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
              "[depends]\nlib = [\"prec2\"]\n";
    }
    file_write(proj_dir / "src" / "main.cpp",
        "#include <cstdio>\nint prec2_answer();\n"
        "int main() { std::printf(\"%d\\n\", prec2_answer()); return 0; }\n");

    ProcResult b = run_ezmk("build", proj_dir);
    INFO("build stderr: " << b.err);
    INFO("build stdout: " << b.out);
    REQUIRE(b.exit_code == 0);
    REQUIRE((b.out + b.err).find("may be ABI-incompatible") != std::string::npos);

    fs::path exe = proj_dir / "build" / (proj_name + EZMK_EXE_SUFFIX);
    REQUIRE(fs::exists(exe));
    ProcResult run_r = run_command("\"" + exe.string() + "\"");
    REQUIRE(run_r.exit_code == 0);
    REQUIRE(run_r.out.find("7") != std::string::npos);
}

// 1.2.0-dev.10: [project].precompiled_strict = true turns the toolchain
// fallback into a fail-fast error (install path).
TEST_CASE("integration: precompiled_strict refuses toolchain fallback (dev.10)", "[integration][1.2.0-dev.10]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    auto tc = ezmk::toolchain::detect_toolchain();
    if (tc.family == ezmk::toolchain::CompilerFamily::Msvc) {
        SKIP("archive creation via ar is GCC/Clang-only — skipping");
    }
    std::string plat = ezmk::util::detect_platform_tag();

    std::string proj_name = "precstrict";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    fs::path pkg_dir = tmp.path / "prec3_pkg";
    fs::create_directories(pkg_dir / "include");
    fs::create_directories(pkg_dir / "lib");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"prec3\"\ntype = \"static\"\nversion = \"1.0.0\"\n"
        "precompiled = true\nprecompiled_strict = true\n");
    fs::path srcf = tmp.path / "p.cpp";
    file_write(srcf, "int prec3_answer() { return 1; }\n");
    fs::path objf = tmp.path / "p.o";
    REQUIRE(run_command("\"" + tc.cxx_compiler.string() + "\" -c \"" + srcf.string() +
                        "\" -o \"" + objf.string() + "\"").exit_code == 0);
    fs::path ar = pkg_dir / "lib" / ("libprec3." + plat + ".a");  // no toolchain tag
    REQUIRE(run_command("ar rcs \"" + ar.string() + "\" \"" + objf.string() + "\"").exit_code == 0);

    // Install must fail fast with the strict mismatch error.
    ProcResult inst = run_ezmk("pkg install \"" + pkg_dir.string() + "\" -p", proj_dir);
    INFO("install stderr: " << inst.err);
    INFO("install stdout: " << inst.out);
    REQUIRE(inst.exit_code != 0);
    REQUIRE((inst.out + inst.err).find("precompiled_strict") != std::string::npos);
}

// 1.2.0-dev.11: run_tests derives project objects from the project's sources
// (object at .ezmk/temp/<rel>.o), so tests referencing project functions link.
// Regression: the old top-level scan of .ezmk/temp missed "src/helper.cpp" —
// its object lives at .ezmk/temp/src/helper.o — so the runner linked with no
// project objects and any reference was an undefined symbol.
TEST_CASE("integration: ezmk test links project objects (dev.11)", "[integration][1.2.0-dev.11]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    std::string proj_name = "projobj";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // A second project source directly under src/ — its object lands at
    // .ezmk/temp/src/helper.o, nested under temp/.
    file_write(proj_dir / "src" / "helper.cpp", "int test_helper() { return 5; }\n");

    // EZMK-framework test that calls the project function.
    fs::create_directories(proj_dir / "test");
    file_write(proj_dir / "test" / "t.cpp",
        "int test_helper();\nint main() { return test_helper() == 5 ? 0 : 1; }\n");

    {
        std::ofstream of(proj_dir / "ezmk.toml");
        of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
              "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
              "[compile]\nflags = [\"-Wall\"]\n\n"
              "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
              "[depends]\nlib = []\n\n"
              "[test]\ndirs = [\"test\"]\nframework = \"ezmk\"\n";
    }

    ProcResult r = run_ezmk("test", proj_dir);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE((r.out + r.err).find("[PASS]") != std::string::npos);
}

// 1.2.0-dev.11: the test compile path goes through build_compile_args +
// join_shell_args (1.1.3 S4 blacklist) — a shell-expansion flag like
// -DTESTINJ=$(touch ...) must be passed to the compiler literally, not executed.
// (On Windows CreateProcessA never interprets $(...) — the assertion is
// trivially true there, but harmless; on POSIX it is a real regression gate.)
TEST_CASE("integration: ezmk test compile path is injection-safe (dev.11)", "[integration][1.2.0-dev.11]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    std::string proj_name = "injtest";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    fs::create_directories(proj_dir / "test");
    file_write(proj_dir / "test" / "t.cpp", "int main() { return 0; }\n");
    {
        std::ofstream of(proj_dir / "ezmk.toml");
        of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
              "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
              "[compile]\nflags = [\"-DTESTINJ=$(touch injected.marker)\"]\n\n"
              "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
              "[depends]\nlib = []\n\n"
              "[test]\ndirs = [\"test\"]\nframework = \"ezmk\"\n";
    }

    ProcResult r = run_ezmk("test", proj_dir);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(!fs::exists(proj_dir / "injected.marker"));
}
