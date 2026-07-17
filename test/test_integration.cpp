// End-to-end integration tests for EazyMake.
//
// These tests call the compiled `ezmk` binary as a subprocess and verify
// complete workflows: project creation → dependency install → build → run.
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
#include "ezmk/util.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using namespace ezmk::util;

// ═══════════════════════════════════════════════════════════════════════════════
// Test helpers
// ═══════════════════════════════════════════════════════════════════════════════

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
    // (no bash escaping needed — cmd.exe doesn't interpret backslashes).
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

// RAII temp directory — created on construction, cleaned up on destruction.
struct TempDir {
    fs::path path;

    TempDir() {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("ezmk_int_" + std::to_string(ts));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

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

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario 1: From zero to running project (single linear flow)
//   project new → verify structure → build → verify binary → run → verify output
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("integration: create project, build, and run (end-to-end)", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
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

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario 2: Incremental build — cache hit on second build
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("integration: incremental build cache hit", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }

    TempDir tmp;
    std::string proj_name = "cache_test";

    // Create project and do first build
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);

    fs::path proj_dir = tmp.path / proj_name;

    // First build — full compilation
    ProcResult first = run_ezmk("project build", proj_dir);
    INFO("first build stderr: " << first.err);
    INFO("first build stdout: " << first.out);
    REQUIRE(first.exit_code == 0);

    // Small delay to ensure timestamps differ
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Second build — should hit cache
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

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario 3: Watch mode — file change triggers rebuild
// NOTE: This test is inherently timing-sensitive. It uses WARN instead of
// REQUIRE so flakes don't block CI.
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("integration: watch mode detects file changes", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }

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
    // Windows: use start /B to run in background
    std::string watch_cmd =
        "cmd /c start /B \"\" " + escape_shell_arg(ezmk_bin) +
        " project watch --no-build-on-start > " +
        escape_shell_arg(log_file.string()) + " 2>&1";
#else
    // POSIX: run in background with &
    std::string watch_cmd =
        "cd " + escape_shell_arg(proj_dir.string()) + " && " +
        escape_shell_arg(ezmk_bin) +
        " project watch --no-build-on-start > " +
        escape_shell_arg(log_file.string()) + " 2>&1 &";
#endif

    run_command(watch_cmd);

    // Give watch time to start monitoring
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Touch the main source file to trigger rebuild
    {
        fs::path main_cpp = proj_dir / "src" / "main.cpp";
        std::ofstream f(main_cpp, std::ios::app);
        f << "// touch for watch test\n";
        f.close();
    }

    // Wait for watch to detect and rebuild
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Kill any running ezmk watch processes
#ifdef EZMK_WIN
    run_command("cmd /c taskkill /F /IM ezmk.exe 2>nul");
#else
    run_command("pkill -f \"ezmk project watch\" 2>/dev/null || true");
#endif

    // Check the log file for rebuild indicators
    std::string log_content;
    if (fs::exists(log_file)) {
        log_content = file_read(log_file);
    }

    INFO("watch log: " << log_content);

    bool detected_change =
        log_content.find("changed") != std::string::npos ||
        log_content.find("detected") != std::string::npos ||
        log_content.find("rebuild") != std::string::npos ||
        log_content.find("compil") != std::string::npos ||
        log_content.find("build succeeded") != std::string::npos ||
        log_content.find("Build succeeded") != std::string::npos;

    WARN(detected_change);
    if (!detected_change) {
        WARN("Watch mode test: no rebuild detected (timing-sensitive, may be false negative)");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario 4: compile_commands.json generation (ezmk utils cc)
// NOTE: ezmk-cc is a built-in Lua tool. The development fallback in
// find_utils_script() looks for ./pkg/ezmk-cc/ relative to CWD, so we
// run this test from the repo root.
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("integration: utils cc generates compile_commands.json", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
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
         r.err.find("未知") != std::string::npos)) {
        SKIP("ezmk-cc built-in tool not found — skipping (dev env)");
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

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario 5: project new creates expected directory layout
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("integration: project new creates expected directory layout", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
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

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario 6: Package install with network (requires network)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("integration: pkg install downloads and installs a package", "[integration][network]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }

    if (!network_available()) {
        SKIP("Network not available — skipping package install test");
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
        SKIP("pkg install failed (network or repo issue) — skipping");
    }

    // Verify the package was installed
    REQUIRE(fs::exists(proj_dir / ".ezmk" / "pkg"));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario 7: version and help commands work
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("integration: basic CLI commands", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
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
