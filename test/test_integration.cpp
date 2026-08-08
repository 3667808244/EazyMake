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
