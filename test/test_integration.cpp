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
#include "test_helpers.hpp"
#include "ezmk/util.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/toolchain.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::util;

// Test helpers
// ==============================================================
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



// Detect if EazyMake binary is available (skip tests gracefully if not).
bool ezmk_available() {
    return fs::exists(find_ezmk_binary());
}

// 1.2.0-dev.11: independent oracle for the precompiled-tag tests. Derives the
// expected compiler tag from the RAW version string instead of calling the
// production toolchain::compiler_tag() — a regression in tag derivation would
// otherwise pass silently (the test's expectation was derived from the very
// code under test). Mirrors the documented rule:
//   "g++ (GCC) 13.2.0"        → gcc13
//   "clang version 18.1.8"    → clang18
// Returns "" when unparseable (MSVC is skipped by the precompiled tests).
std::string independent_compiler_tag(const ezmk::toolchain::Toolchain& tc) {
    std::string prefix;
    switch (tc.family) {
    case ezmk::toolchain::CompilerFamily::Gcc:   prefix = "gcc"; break;
    case ezmk::toolchain::CompilerFamily::Clang: prefix = "clang"; break;
    default: return "";  // MSVC — not covered by these tests
    }
    // First "<digits>.<digit" sequence (a real major.minor) → leading number.
    const std::string& v = tc.version;
    for (size_t i = 0; i + 2 < v.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(v[i])) &&
            v[i + 1] == '.' &&
            std::isdigit(static_cast<unsigned char>(v[i + 2]))) {
            size_t start = i;
            while (start > 0 &&
                   std::isdigit(static_cast<unsigned char>(v[start - 1]))) --start;
            return prefix + v.substr(start, i - start + 1);
        }
    }
    return "";
}

} // anonymous namespace

// Scenario 1: From zero to running project (single linear flow)
//   project new → verify structure → build → verify binary → run → verify output
// ==============================================================
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

// Scenario 2: Incremental build — cache hit on second build
// ==============================================================
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

// Scenario 3: Watch mode — file change triggers rebuild
// NOTE: This test polls the watch log (ezmk logs to unbuffered stderr) until
// the rebuild is detected, with generous timeouts, so it is robust to machine
// speed instead of relying on fixed sleeps.
// ==============================================================
TEST_CASE("integration: watch mode detects file changes", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
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

// Scenario 4: compile_commands.json generation (ezmk utils cc)
// NOTE: ezmk-cc is a built-in Lua tool. The development fallback in
// find_utils_script() looks for ./pkg/ezmk-cc/ relative to CWD, so we
// run this test from the repo root.
// ==============================================================
TEST_CASE("integration: utils cc generates compile_commands.json", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    // 1.2.0-dev.11: pin the locale so the "tool not found" SKIP detection below
    // matches the actual error output regardless of the host language.
    EnvGuard lang_guard("EZMK_LANG", "en");

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

// 1.1.1: [compile].compile_commands — build auto-generates compile_commands.json.
TEST_CASE("integration: compile_commands auto-generation", "[integration]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
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

// Scenario 5: project new creates expected directory layout
// ==============================================================
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

// 1.2.1: project new templates differ by type — static/shared get a library
// skeleton (include/<name>.hpp + src/<name>.cpp, no main.cpp), executable keeps
// Hello world main.cpp, utils gets no C++ code at all.
TEST_CASE("integration: project new templates differ by type (1.2.1)", "[integration][1.2.1]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }

    TempDir tmp;

    // executable: main.cpp present, no header
    {
        ProcResult r = run_ezmk(
            "project new exe_121 --type executable --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(r.exit_code == 0);
        fs::path p = tmp.path / "exe_121";
        REQUIRE(fs::exists(p / "src" / "main.cpp"));
        REQUIRE_FALSE(fs::exists(p / "include" / "exe_121.hpp"));
        REQUIRE_FALSE(fs::exists(p / "src" / "exe_121.cpp"));
    }

    // static: hpp + cpp library skeleton, no main.cpp
    {
        ProcResult r = run_ezmk(
            "project new st_121 --type static --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(r.exit_code == 0);
        fs::path p = tmp.path / "st_121";
        REQUIRE(fs::exists(p / "include" / "st_121.hpp"));
        REQUIRE(fs::exists(p / "src" / "st_121.cpp"));
        REQUIRE_FALSE(fs::exists(p / "src" / "main.cpp"));
        std::string hpp = file_read(p / "include" / "st_121.hpp");
        REQUIRE(hpp.find("#pragma once") != std::string::npos);
        REQUIRE(hpp.find("namespace st_121 {") != std::string::npos);
    }

    // shared: same library skeleton, no main.cpp
    {
        ProcResult r = run_ezmk(
            "project new sh_121 --type shared --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(r.exit_code == 0);
        fs::path p = tmp.path / "sh_121";
        REQUIRE(fs::exists(p / "include" / "sh_121.hpp"));
        REQUIRE(fs::exists(p / "src" / "sh_121.cpp"));
        REQUIRE_FALSE(fs::exists(p / "src" / "main.cpp"));
    }

    // utils: no C++ code, utils/ directory only
    {
        ProcResult r = run_ezmk(
            "project new ut_121 --type utils --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(r.exit_code == 0);
        fs::path p = tmp.path / "ut_121";
        REQUIRE(fs::is_directory(p / "utils"));
        REQUIRE_FALSE(fs::exists(p / "src" / "main.cpp"));
        REQUIRE(fs::is_empty(p / "src"));
    }

    // dashed name: file names keep the dash, namespace is sanitized
    {
        ProcResult r = run_ezmk(
            "project new my-lib --type static --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(r.exit_code == 0);
        fs::path p = tmp.path / "my-lib";
        REQUIRE(fs::exists(p / "include" / "my-lib.hpp"));
        REQUIRE(fs::exists(p / "src" / "my-lib.cpp"));
        std::string cpp = file_read(p / "src" / "my-lib.cpp");
        REQUIRE(cpp.find("namespace my_lib {") != std::string::npos);
    }
}

// 1.2.1: a newly created static/shared library skeleton compiles out of the
// box (library compile + archive/link), no manual edits needed.
TEST_CASE("integration: library skeleton builds (1.2.1)", "[integration][1.2.1]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }

    TempDir tmp;

    // static → archive (lib<name>.a / <name>.lib)
    {
        ProcResult new_r = run_ezmk(
            "project new libst_121 --type static --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(new_r.exit_code == 0);
        fs::path p = tmp.path / "libst_121";

        ProcResult b = run_ezmk("project build", p);
        INFO("static build stderr: " << b.err);
        INFO("static build stdout: " << b.out);
        REQUIRE(b.exit_code == 0);
    }

    // shared → lib<name>.dll / lib<name>.so
    {
        ProcResult new_r = run_ezmk(
            "project new libsh_121 --type shared --disable-git-init --disable-gitignore",
            tmp.path);
        REQUIRE(new_r.exit_code == 0);
        fs::path p = tmp.path / "libsh_121";

        ProcResult b = run_ezmk("project build", p);
        INFO("shared build stderr: " << b.err);
        INFO("shared build stdout: " << b.out);
        REQUIRE(b.exit_code == 0);
    }
}

// 1.2.0-dev.11: pkg install from a locally packed archive — real assertions
// (exit code 0 + installed artifacts + pkg list), no network required. The old
// test SKIP'd whenever the network install failed, silently swallowing every
// assertion (vacuous) on machines without GitHub access.
TEST_CASE("integration: pkg install from a local packed archive (dev.11)", "[integration][1.2.0-dev.11]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    // 1) A static package project (include/ + src/ + ezmk.toml) to pack.
    fs::path pkg_dir = tmp.path / "arc_pkg";
    fs::create_directories(pkg_dir / "include" / "arc");
    fs::create_directories(pkg_dir / "src");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"arc\"\ntype = \"static\"\nversion = \"1.0.0\"\n");
    file_write(pkg_dir / "include" / "arc" / "arc.hpp",
        "#pragma once\nint arc_answer();\n");
    file_write(pkg_dir / "src" / "arc.cpp",
        "#include \"arc/arc.hpp\"\nint arc_answer() { return 11; }\n");

    // 2) Pack it into a .tar.gz in a clean output dir.
    fs::path out_dir = tmp.path / "arc_out";
    fs::create_directories(out_dir);
    ProcResult pack_r = run_ezmk(
        "project pack --output \"" + out_dir.string() + "\"", pkg_dir);
    INFO("pack stderr: " << pack_r.err);
    INFO("pack stdout: " << pack_r.out);
    REQUIRE(pack_r.exit_code == 0);
    fs::path archive = out_dir / "arc-1.0.0.tar.gz";
    REQUIRE(fs::exists(archive));

    // 3) A consumer project installs the archive (project scope, -y).
    std::string proj_name = "arc_app";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    ProcResult r = run_ezmk(
        "pkg install \"" + archive.string() + "\" -p -y", proj_dir);
    INFO("install stderr: " << r.err);
    INFO("install stdout: " << r.out);
    REQUIRE(r.exit_code == 0);

    // 4) Artifacts installed + visible to pkg list.
    REQUIRE(fs::exists(proj_dir / ".ezmk" / "pkg" / "arc" / "ezmk.toml"));
    REQUIRE(fs::exists(proj_dir / ".ezmk" / "pkg" / "arc" / "include" / "arc" / "arc.hpp"));
    ProcResult l = run_ezmk("pkg list -p", proj_dir);
    INFO("list stderr: " << l.err);
    REQUIRE(l.exit_code == 0);
    REQUIRE((l.out + l.err).find("arc") != std::string::npos);
}

// 1.2.5: project pack default = source package (src/ + include/ + ezmk.toml,
// no precompiled marker — compiled on the consumer side); --precompiled keeps
// the legacy prebuilt archive (include/ + lib/ + precompiled marker).
TEST_CASE("integration: project pack source default + --precompiled (1.2.5)", "[integration][1.2.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    // A static library project (include/ + src/ + ezmk.toml) to pack.
    fs::path pkg_dir = tmp.path / "pkg125";
    fs::create_directories(pkg_dir / "include" / "pkg125");
    fs::create_directories(pkg_dir / "src");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"pkg125\"\ntype = \"static\"\nversion = \"1.0.0\"\n");
    file_write(pkg_dir / "include" / "pkg125" / "pkg125.hpp",
        "#pragma once\nint pkg125_answer();\n");
    file_write(pkg_dir / "src" / "pkg125.cpp",
        "#include \"pkg125/pkg125.hpp\"\nint pkg125_answer() { return 125; }\n");

    fs::path out_dir = tmp.path / "out";
    fs::create_directories(out_dir);

    // 1) Default pack → source package; installing it compiles on the consumer
    //    side (no precompiled marker shipped).
    ProcResult pack_r = run_ezmk("project pack --output \"" + out_dir.string() + "\"", pkg_dir);
    INFO("pack stderr: " << pack_r.err);
    REQUIRE(pack_r.exit_code == 0);
    fs::path archive = out_dir / "pkg125-1.0.0.tar.gz";
    REQUIRE(fs::exists(archive));

    std::string proj_name = "app125";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore", tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    ProcResult inst_r = run_ezmk("pkg install \"" + archive.string() + "\" -p -y", proj_dir);
    INFO("source install stderr: " << inst_r.err);
    REQUIRE(inst_r.exit_code == 0);
    std::string installed_toml = file_read(proj_dir / ".ezmk" / "pkg" / "pkg125" / "ezmk.toml");
    REQUIRE(installed_toml.find("precompiled") == std::string::npos);
    REQUIRE(fs::exists(proj_dir / ".ezmk" / "pkg" / "pkg125" / "include" / "pkg125" / "pkg125.hpp"));
    REQUIRE(fs::exists(proj_dir / ".ezmk" / "pkg" / "pkg125" / "build" / "libpkg125.a"));

    // 2) --precompiled → legacy prebuilt archive (marker + lib/ artifacts).
    fs::path out2 = tmp.path / "out2";
    fs::create_directories(out2);
    ProcResult pc_r = run_ezmk(
        "project pack --precompiled --output \"" + out2.string() + "\"", pkg_dir);
    INFO("--precompiled stderr: " << pc_r.err);
    REQUIRE(pc_r.exit_code == 0);
    fs::path archive2 = out2 / "pkg125-1.0.0.tar.gz";
    REQUIRE(fs::exists(archive2));

    std::string proj2 = "app125b";
    ProcResult new2 = run_ezmk(
        "project new " + proj2 + " --disable-git-init --disable-gitignore", tmp.path);
    REQUIRE(new2.exit_code == 0);
    fs::path proj2_dir = tmp.path / proj2;
    ProcResult inst2 = run_ezmk("pkg install \"" + archive2.string() + "\" -p -y", proj2_dir);
    INFO("--precompiled install stderr: " << inst2.err);
    REQUIRE(inst2.exit_code == 0);
    std::string installed2_toml = file_read(proj2_dir / ".ezmk" / "pkg" / "pkg125" / "ezmk.toml");
    REQUIRE(installed2_toml.find("precompiled") != std::string::npos);
    REQUIRE(fs::exists(proj2_dir / ".ezmk" / "pkg" / "pkg125" / "lib" / "libpkg125.a"));

    // 3) --precompiled on a non-static project → fatal.
    fs::path exe_dir = tmp.path / "exe125";
    fs::create_directories(exe_dir / "src");
    file_write(exe_dir / "ezmk.toml",
        "[project]\nname = \"exe125\"\ntype = \"executable\"\nversion = \"1.0.0\"\n");
    file_write(exe_dir / "src" / "main.cpp", "int main() { return 0; }\n");
    ProcResult bad_r = run_ezmk(
        "project pack --precompiled --output \"" + out2.string() + "\"", exe_dir);
    REQUIRE(bad_r.exit_code != 0);

    // 4) Source pack of an executable project is allowed (type relaxed) and
    //    never triggers a build-first step.
    ProcResult exe_pack = run_ezmk(
        "project pack --output \"" + out2.string() + "\"", exe_dir);
    INFO("exe source pack stderr: " << exe_pack.err);
    REQUIRE(exe_pack.exit_code == 0);
    REQUIRE(fs::exists(out2 / "exe125-1.0.0.tar.gz"));
}

// Scenario 7: version and help commands work
// ==============================================================
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

// Scenario 8: Dependency version constraint validation (0.9.6+)
//   create mock pkg → set constraint → verify build accepts/rejects correctly
// ==============================================================
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
    // 1.2.0-dev.11: independent oracle — see independent_compiler_tag().
    std::string comp = independent_compiler_tag(tc);
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

// 1.2.0-dev.11: mutual hard dependencies (A⇄B) must not recurse unboundedly
// during auto-install — the recursion guard turns it into a clear error.
// Uses a local repo (project scope) + `project pack` archives.
TEST_CASE("integration: mutual deps stop auto-install recursion (dev.11)", "[integration][1.2.0-dev.11]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    std::string proj_name = "circapp";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    auto make_lib = [&](const std::string& name, const std::string& deps) {
        fs::path d = tmp.path / name;
        fs::create_directories(d / "src");
        fs::create_directories(d / "include");
        std::ofstream(d / "src" / (name + ".cpp"))
            << "int " << name << "_f() { return 1; }\n";
        {
            std::ofstream of(d / "ezmk.toml");
            of << "[project]\nname = \"" << name << "\"\ntype = \"static\"\n"
                  "version = \"1.0.0\"\nlanguage = \"C++17\"\n\n"
                  "[compile]\nflags = []\ninclude_dirs = [\"include\"]\n\n"
                  "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
                  "[depends]\nlib = [" << deps << "]\n";
        }
        return d;
    };
    // A stub installed dep so `project pack`'s build-first step passes the
    // hard-dep pre-check (static libs never link against their deps, so the
    // stub's archive content is irrelevant). The PACKED archive only carries
    // ezmk.toml + include/ + lib/, never the stub.
    auto make_stub = [&](const fs::path& pkg_proj, const std::string& dep_name) {
        fs::path stub = pkg_proj / ".ezmk" / "pkg" / dep_name;
        fs::create_directories(stub / "build");
        std::ofstream(stub / "ezmk.toml")
            << "[project]\nname = \"" << dep_name
            << "\"\ntype = \"static\"\nversion = \"1.0.0\"\n";
        std::ofstream(stub / "build" / ("lib" + dep_name + ".a"),
                      std::ios::binary) << "stub";
    };

    fs::path repo_dir = tmp.path / "repo";
    fs::create_directories(repo_dir);

    auto pack_lib = [&](const std::string& name, const std::string& deps,
                        const std::string& stub_dep) {
        auto d = make_lib(name, deps);
        if (!stub_dep.empty()) make_stub(d, stub_dep);
        ProcResult p = run_ezmk("project pack --output \"" + repo_dir.string() + "\"", d);
        INFO(name << " pack stderr: " << p.err);
        REQUIRE(p.exit_code == 0);
    };
    pack_lib("a_circ", "\"b_circ\"", "b_circ");
    pack_lib("b_circ", "\"a_circ\"", "a_circ");

    file_write(repo_dir / "index.toml",
        "[repo]\nname = \"localdev11\"\n\n"
        "[[packages]]\nname = \"a_circ\"\nversion = \"1.0.0\"\n"
        "file = \"a_circ-1.0.0.tar.gz\"\n\n"
        "[[packages]]\nname = \"b_circ\"\nversion = \"1.0.0\"\n"
        "file = \"b_circ-1.0.0.tar.gz\"\n");

    ProcResult ra = run_ezmk("repo add -p \"" + repo_dir.string() + "\"", proj_dir);
    INFO("repo add stderr: " << ra.err);
    REQUIRE(ra.exit_code == 0);

    ProcResult r = run_ezmk("pkg install a_circ -p -y", proj_dir);
    INFO("install stderr: " << r.err);
    INFO("install stdout: " << r.out);
    REQUIRE(r.exit_code != 0);
    REQUIRE((r.out + r.err).find("circular") != std::string::npos);
}

// 1.2.0-dev.11: auto-install re-validates the freshly installed version against
// the caller's constraint — B@^1.0 must not silently get repo B 2.0.0.
TEST_CASE("integration: auto-install enforces version constraint (dev.11)", "[integration][1.2.0-dev.11]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    std::string proj_name = "consapp";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    auto make_lib = [&](const std::string& name, const std::string& version,
                        const std::string& deps) {
        fs::path d = tmp.path / name;
        fs::create_directories(d / "src");
        fs::create_directories(d / "include");
        std::ofstream(d / "src" / (name + ".cpp"))
            << "int " << name << "_f() { return 1; }\n";
        {
            std::ofstream of(d / "ezmk.toml");
            of << "[project]\nname = \"" << name << "\"\ntype = \"static\"\n"
                  "version = \"" << version << "\"\nlanguage = \"C++17\"\n\n"
                  "[compile]\nflags = []\ninclude_dirs = [\"include\"]\n\n"
                  "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
                  "[depends]\nlib = [" << deps << "]\n";
        }
        return d;
    };

    fs::path repo_dir = tmp.path / "repo";
    fs::create_directories(repo_dir);

    auto pack_lib = [&](const std::string& name, const std::string& version,
                        const std::string& deps, const std::string& stub_dep) {
        auto d = make_lib(name, version, deps);
        if (!stub_dep.empty()) {
            fs::path stub = d / ".ezmk" / "pkg" / stub_dep;
            fs::create_directories(stub / "build");
            std::ofstream(stub / "ezmk.toml")
                << "[project]\nname = \"" << stub_dep
                << "\"\ntype = \"static\"\nversion = \"1.0.0\"\n";
            std::ofstream(stub / "build" / ("lib" + stub_dep + ".a"),
                          std::ios::binary) << "stub";
        }
        ProcResult p = run_ezmk("project pack --output \"" + repo_dir.string() + "\"", d);
        INFO(name << " pack stderr: " << p.err);
        REQUIRE(p.exit_code == 0);
    };
    // A wants B^1.0 (compatible); the repo only carries B 2.0.0 → auto-install
    // must fail.
    pack_lib("b_cons", "2.0.0", "", "");
    pack_lib("a_cons", "1.0.0", "\"b_cons^1.0\"", "b_cons");

    file_write(repo_dir / "index.toml",
        "[repo]\nname = \"localdev11b\"\n\n"
        "[[packages]]\nname = \"b_cons\"\nversion = \"2.0.0\"\n"
        "file = \"b_cons-2.0.0.tar.gz\"\n\n"
        "[[packages]]\nname = \"a_cons\"\nversion = \"1.0.0\"\n"
        "file = \"a_cons-1.0.0.tar.gz\"\n");

    ProcResult ra = run_ezmk("repo add -p \"" + repo_dir.string() + "\"", proj_dir);
    INFO("repo add stderr: " << ra.err);
    REQUIRE(ra.exit_code == 0);

    ProcResult r = run_ezmk("pkg install a_cons -p -y", proj_dir);
    INFO("install stderr: " << r.err);
    INFO("install stdout: " << r.out);
    REQUIRE(r.exit_code != 0);
    REQUIRE((r.out + r.err).find("constraint") != std::string::npos);
}

// =============================================================
// 1.2.3: ezmk example — list / scaffold / errors / build
// =============================================================

// Repo-root examples/ dir — the source of truth for the embedded table.
static fs::path example_source_dir() {
    return find_repo_root() / "examples";
}

TEST_CASE("integration: ezmk example lists all built-in examples (1.2.3)", "[integration][1.2.3]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    TempDir tmp;

    // Bare `ezmk example` and explicit `ezmk example list` both list 6 examples.
    for (const std::string& args : {"example", "example list"}) {
        ProcResult r = run_ezmk(args, tmp.path);
        INFO("stderr: " << r.err);
        INFO("stdout: " << r.out);
        REQUIRE(r.exit_code == 0);
        for (const std::string& name :
             {"hello", "greeter", "with-packages", "with-tests", "with-hooks", "cmake-interop"}) {
            INFO("list must contain: " << name);
            REQUIRE((r.out + r.err).find(name) != std::string::npos);
        }
    }
}

TEST_CASE("integration: ezmk example scaffolds match source tree (1.2.3)", "[integration][1.2.3]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    TempDir tmp;

    // Generate `hello` and `with-hooks`; every file must byte-match the source.
    for (const std::string& name : {"hello", "with-hooks"}) {
        ProcResult g = run_ezmk("example " + name, tmp.path);
        INFO(name << " stderr: " << g.err);
        REQUIRE(g.exit_code == 0);
        fs::path out_dir = tmp.path / name;
        fs::path src_dir = example_source_dir() / name;
        for (auto& e : fs::recursive_directory_iterator(src_dir)) {
            if (!e.is_regular_file()) continue;
            auto rel = fs::relative(e.path(), src_dir).generic_string();
            if (rel == "description.txt") continue;  // metadata, not scaffolded
            INFO(name << ": " << rel);
            REQUIRE(fs::exists(out_dir / rel));
            REQUIRE(file_read(out_dir / rel) == file_read(e.path()));
        }
    }

    // --output <dir> scaffolds to <dir>/<name>/.
    fs::path out_root = tmp.path / "out";
    ProcResult o = run_ezmk("example greeter --output \"" + out_root.string() + "\"", tmp.path);
    INFO("greeter -o stderr: " << o.err);
    REQUIRE(o.exit_code == 0);
    REQUIRE(fs::exists(out_root / "greeter" / "ezmk.toml"));
    REQUIRE(fs::exists(out_root / "greeter" / "include" / "greeter.hpp"));
}

TEST_CASE("integration: ezmk example error paths (1.2.3)", "[integration][1.2.3]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");  // error messages are localized
    TempDir tmp;

    // Existing directory → fatal.
    ProcResult g1 = run_ezmk("example hello", tmp.path);
    REQUIRE(g1.exit_code == 0);
    ProcResult g2 = run_ezmk("example hello", tmp.path);
    INFO("exists stderr: " << g2.err);
    REQUIRE(g2.exit_code != 0);
    REQUIRE((g2.out + g2.err).find("exist") != std::string::npos);

    // Unknown example name → fatal listing available names.
    ProcResult u = run_ezmk("example no_such_example", tmp.path);
    INFO("unknown stderr: " << u.err);
    REQUIRE(u.exit_code != 0);
    REQUIRE((u.out + u.err).find("hello") != std::string::npos);
    REQUIRE((u.out + u.err).find("no_such_example") != std::string::npos);
}

TEST_CASE("integration: ezmk example generated projects build (1.2.3)", "[integration][1.2.3]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    TempDir tmp;

    // Dependency-free examples: hello / greeter / with-hooks / cmake-interop build
    // out of the box.
    for (const std::string& name :
         {"hello", "greeter", "with-hooks", "cmake-interop"}) {
        ProcResult g = run_ezmk("example " + name, tmp.path);
        INFO(name << " gen stderr: " << g.err);
        REQUIRE(g.exit_code == 0);
        fs::path proj = tmp.path / name;

        ProcResult b = run_ezmk("build", proj);
        INFO(name << " build stderr: " << b.err);
        INFO(name << " build stdout: " << b.out);
        REQUIRE(b.exit_code == 0);
    }

    // with-tests: Catch2 framework — install catch2 (network/repo), then
    // `ezmk test` passes the two TEST_CASEs. Skip if the install cannot succeed.
    {
        ProcResult g = run_ezmk("example with-tests", tmp.path);
        REQUIRE(g.exit_code == 0);
        fs::path proj = tmp.path / "with-tests";
        std::string toml = file_read(proj / "ezmk.toml");
        REQUIRE(toml.find("catch2") != std::string::npos);

        ProcResult inst = run_ezmk("pkg install catch2 -p -y", proj);
        INFO("catch2 install stderr: " << inst.err);
        if (inst.exit_code != 0) {
            SKIP("catch2 install unavailable (network/repo) — test verification skipped");
        }
        ProcResult t = run_ezmk("test", proj);
        INFO("with-tests stderr: " << t.err);
        INFO("with-tests stdout: " << t.out);
        REQUIRE(t.exit_code == 0);
        REQUIRE((t.out + t.err).find("passed") != std::string::npos);
    }

    // with-packages: needs fmt — install (network/repo), then build. Skip the
    // build verification when the install cannot succeed (offline / broken repo),
    // but always verify the scaffold content above.
    {
        ProcResult g = run_ezmk("example with-packages", tmp.path);
        REQUIRE(g.exit_code == 0);
        fs::path proj = tmp.path / "with-packages";
        // fmt constraint is declared in the example's ezmk.toml.
        std::string toml = file_read(proj / "ezmk.toml");
        REQUIRE(toml.find("fmt") != std::string::npos);

        ProcResult inst = run_ezmk("pkg install fmt -p -y", proj);
        INFO("fmt install stderr: " << inst.err);
        if (inst.exit_code == 0) {
            ProcResult b = run_ezmk("build", proj);
            INFO("with-packages build stderr: " << b.err);
            REQUIRE(b.exit_code == 0);
        } else {
            SKIP("fmt install unavailable (network/repo) — build verification skipped");
        }
    }
}

// =============================================================
// 1.2.4: repo-hosted directory packages
// =============================================================

TEST_CASE("integration: repo directory package installs (1.2.4)", "[integration][1.2.4]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    TempDir tmp;

    // A local repo with a `type = "dir"` package (no sha256 — dir packages have
    // no archive hash). `pkg install <name>` must reuse the directory-install path.
    fs::path repo_dir = tmp.path / "dirrepo";
    fs::path pkg_dir = repo_dir / "packages" / "greetdir";
    fs::create_directories(pkg_dir / "include");
    fs::create_directories(pkg_dir / "src");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"greetdir\"\ntype = \"static\"\nversion = \"1.0.0\"\n"
        "language = \"C++17\"\n\n[compile]\nflags = []\ninclude_dirs = [\"include\"]\n");
    file_write(pkg_dir / "include" / "greetdir.hpp",
        "#pragma once\nconst char* greetdir_hi();\n");
    file_write(pkg_dir / "src" / "greetdir.cpp",
        "#include \"greetdir.hpp\"\nconst char* greetdir_hi() { return \"hi\"; }\n");
    file_write(repo_dir / "index.toml",
        "[repo]\nname = \"dirrepo\"\n\n"
        "[[packages]]\nname = \"greetdir\"\nversion = \"1.0.0\"\n"
        "type = \"dir\"\nfile = \"packages/greetdir\"\n");

    std::string proj_name = "dir_app";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    ProcResult ra = run_ezmk("repo add -p \"" + repo_dir.string() + "\" --name dirrepo", proj_dir);
    INFO("repo add stderr: " << ra.err);
    REQUIRE(ra.exit_code == 0);

    ProcResult r = run_ezmk("pkg install greetdir -p -y", proj_dir);
    INFO("install stderr: " << r.err);
    INFO("install stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(fs::exists(proj_dir / ".ezmk/pkg/greetdir/ezmk.toml"));
    REQUIRE(fs::exists(proj_dir / ".ezmk/pkg/greetdir/include/greetdir.hpp"));
    // The dir package was compiled into an archive (not just copied).
    REQUIRE(fs::exists(proj_dir / ".ezmk/pkg/greetdir/build/libgreetdir.a"));
}

TEST_CASE("integration: repo directory package missing dir errors (1.2.4)", "[integration][1.2.4]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    TempDir tmp;

    // index `file` points at a directory that does not exist → repo validation
    // fails at `repo add` (same friendly missing-file path as archive packages).
    fs::path repo_dir = tmp.path / "dirrepo_missing";
    fs::create_directories(repo_dir);
    file_write(repo_dir / "index.toml",
        "[repo]\nname = \"dirrepo_missing\"\n\n"
        "[[packages]]\nname = \"ghostdir\"\nversion = \"1.0.0\"\n"
        "type = \"dir\"\nfile = \"packages/ghostdir\"\n");

    std::string proj_name = "dir_miss_app";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    ProcResult ra = run_ezmk("repo add -p \"" + repo_dir.string() + "\" --name dirrepo_missing", proj_dir);
    INFO("repo add stderr: " << ra.err);
    REQUIRE(ra.exit_code != 0);
    REQUIRE((ra.out + ra.err).find("ghostdir") != std::string::npos);
}

// ==============================================================
// 1.3.0-dev.3: workspace integration tests
// (fixture: 3-member workspace — libs/strutil static lib + apps/tool-a
//  and apps/tool-b executables; tool-a also has [test] ezmk framework)
// ==============================================================
namespace {

// Write the 3-member workspace fixture at `root` (root may already exist).
//   root/apps/tool-a   executable; [depends] workspace=["strutil"]; [test] ezmk
//   root/apps/tool-b   executable; [depends] workspace=["strutil"]
//   root/libs/strutil  static; include/strutil.hpp + src/strutil.cpp
// The app output is "sum = add(2,3) + OFFSET": 5 (baseline) → 6 (source
// change) → 106 (header OFFSET change) — used to prove relink/recompile.
void write_ws_fixture(const fs::path& root) {
    fs::create_directories(root / "libs/strutil" / "include");
    fs::create_directories(root / "libs/strutil" / "src");
    fs::create_directories(root / "apps/tool-a" / "src");
    fs::create_directories(root / "apps/tool-a" / "test");
    fs::create_directories(root / "apps/tool-b" / "src");

    file_write(root / "ezmk-workspace.toml",
        "[workspace]\n"
        "members = [\"apps/tool-a\", \"apps/tool-b\", \"libs/strutil\"]\n\n"
        "[workspace.options]\ndefault_jobs = 2\n");

    file_write(root / "libs/strutil" / "ezmk.toml",
        "[project]\nname = \"strutil\"\ntype = \"static\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n");
    file_write(root / "libs/strutil" / "include" / "strutil.hpp",
        "#pragma once\nnamespace strutil {\ninline constexpr int OFFSET = 0;\n"
        "int add(int a, int b);\n}\n");
    file_write(root / "libs/strutil" / "src" / "strutil.cpp",
        "#include \"strutil.hpp\"\nnamespace strutil {\n"
        "int add(int a, int b) { return a + b; }\n}\n");

    file_write(root / "apps/tool-a" / "ezmk.toml",
        "[project]\nname = \"tool-a\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[depends]\nworkspace = [\"strutil\"]\n\n"
        "[test]\nframework = \"ezmk\"\ndirs = [\"test\"]\n");
    file_write(root / "apps/tool-a" / "src" / "main.cpp",
        "#include \"strutil.hpp\"\n#include <cstdio>\n"
        "int main() { std::printf(\"sum=%d\\n\", strutil::add(2, 3) + strutil::OFFSET); return 0; }\n");
    file_write(root / "apps/tool-a" / "test" / "test_smoke.cpp",
        "#include <cstdio>\nint main() { std::printf(\"tool-a tests ok\\n\"); return 0; }\n");

    file_write(root / "apps/tool-b" / "ezmk.toml",
        "[project]\nname = \"tool-b\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[depends]\nworkspace = [\"strutil\"]\n");
    file_write(root / "apps/tool-b" / "src" / "main.cpp",
        "#include \"strutil.hpp\"\n#include <cstdio>\n"
        "int main() { std::printf(\"b=%d\\n\", strutil::add(10, 20) + strutil::OFFSET); return 0; }\n");
}

// Built executable of an app member (root/apps/<name>/build/<name>[.exe]).
fs::path ws_app_exe(const fs::path& root, const std::string& name) {
    return root / "apps" / name / "build" / (name + EZMK_EXE_SUFFIX);
}

// Combined stdout+stderr of a run (workspace output goes to stderr; member
// prefixed child output is echoed by the orchestrator).
std::string ws_output(const ProcResult& r) {
    return r.out + "\n" + r.err;
}

} // anonymous namespace

// Case 1: workspace list
TEST_CASE("integration: workspace list shows members + types + deps (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";   // space in the root exercises quoting
    write_ws_fixture(root);

    ProcResult r = run_ezmk("workspace list", root);
    INFO("list output: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);
    REQUIRE(out.find("apps/tool-a") != std::string::npos);
    REQUIRE(out.find("apps/tool-b") != std::string::npos);
    REQUIRE(out.find("libs/strutil") != std::string::npos);
    // Member type column and dependency listing.
    REQUIRE(out.find("[executable]") != std::string::npos);
    REQUIRE(out.find("[static]") != std::string::npos);
    REQUIRE(out.find("strutil") != std::string::npos);
}

// Cases 2+3: build all + dependency build order (lib layer before apps)
TEST_CASE("integration: workspace build succeeds and respects dependency order (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);

    // All three members succeeded.
    REQUIRE(out.find("3 succeeded") != std::string::npos);
    // Artifacts landed (executables + static archive).
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
    REQUIRE(fs::exists(ws_app_exe(root, "tool-b")));
    REQUIRE(fs::exists(root / "libs/strutil/build/libstrutil.a"));
    // The app runs and links the sibling library via self-discovery injection.
    auto app = run_command("\"" + ws_app_exe(root, "tool-a").string() + "\"");
    REQUIRE(app.exit_code == 0);
    REQUIRE(app.out.find("sum=5") != std::string::npos);

    // Dependency order: the strutil layer's output precedes BOTH app layers
    // (layers execute sequentially; intra-layer order is not asserted).
    auto lib_pos = out.find("[libs/strutil]");
    auto a_pos = out.find("[apps/tool-a]");
    auto b_pos = out.find("[apps/tool-b]");
    REQUIRE(lib_pos != std::string::npos);
    REQUIRE(a_pos != std::string::npos);
    REQUIRE(b_pos != std::string::npos);
    REQUIRE(lib_pos < a_pos);
    REQUIRE(lib_pos < b_pos);
}

// Case 4: cross-member incremental — lib .cpp change → dependents RELINK only
TEST_CASE("integration: workspace incremental — lib .cpp change relinks dependents (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);

    // Change the lib implementation (not the header).
    file_write(root / "libs/strutil/src/strutil.cpp",
        "#include \"strutil.hpp\"\nnamespace strutil {\n"
        "int add(int a, int b) { return a + b + 1; }\n}\n");
    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("rebuild stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);

    // strutil recompiled ("0 cached, 1 compiled"); the apps were NOT
    // recompiled ("1 cached, 0 compiled" — source + headers unchanged).
    REQUIRE(out.find("0 cached, 1 compiled") != std::string::npos);
    REQUIRE(out.find("1 cached, 0 compiled") != std::string::npos);
    // The apps relinked against the new archive → new runtime output.
    auto app = run_command("\"" + ws_app_exe(root, "tool-a").string() + "\"");
    REQUIRE(app.exit_code == 0);
    REQUIRE(app.out.find("sum=6") != std::string::npos);
}

// Case 5: cross-member incremental — lib .h change → dependents RECOMPILE
TEST_CASE("integration: workspace incremental — lib .h change recompiles dependents (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);

    // Change the header (OFFSET constant) — the apps include it via the
    // injected -I, so their depfile tracks it and they must recompile.
    file_write(root / "libs/strutil/include/strutil.hpp",
        "#pragma once\nnamespace strutil {\ninline constexpr int OFFSET = 100;\n"
        "int add(int a, int b);\n}\n");
    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("rebuild stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);

    // All three members recompile (lib source + both app mains).
    size_t recompiles = 0;
    for (size_t pos = out.find("0 cached, 1 compiled"); pos != std::string::npos;
         pos = out.find("0 cached, 1 compiled", pos + 1)) {
        ++recompiles;
    }
    REQUIRE(recompiles >= 3);
    // Header change reached the consumers → new runtime output (5 + 100).
    auto app = run_command("\"" + ws_app_exe(root, "tool-a").string() + "\"");
    REQUIRE(app.exit_code == 0);
    REQUIRE(app.out.find("sum=105") != std::string::npos);
}

// Case 6: workspace test — members with tests run; others are skipped
TEST_CASE("integration: workspace test runs tested members and skips the rest (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    // Build first so tool-a's test runner links against the sibling lib.
    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);

    ProcResult r = run_ezmk("workspace test -j 2", root);
    INFO("test stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);
    // tool-a ran its [test] (ezmk framework, zero dependencies) → PASS.
    REQUIRE(out.find("[PASS]") != std::string::npos);
    // tool-b and strutil have no tests → skipped (not an error).
    REQUIRE(out.find("no tests configured") != std::string::npos);
    REQUIRE(out.find("1 succeeded") != std::string::npos);
}

// Case 7: workspace clean clears member caches (same semantics as single
// project `ezmk clean` — .ezmk/cache and .ezmk/temp, build/ artifacts kept).
TEST_CASE("integration: workspace clean clears member caches (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);
    REQUIRE(fs::exists(root / "apps/tool-a/.ezmk/cache"));

    ProcResult r = run_ezmk("workspace clean", root);
    INFO("clean stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    REQUIRE_FALSE(fs::exists(root / "apps/tool-a/.ezmk/cache"));
    REQUIRE_FALSE(fs::exists(root / "apps/tool-b/.ezmk/cache"));
    REQUIRE_FALSE(fs::exists(root / "libs/strutil/.ezmk/cache"));
}

// Case 8: failure summary — one member fails, the rest complete (no flag)
TEST_CASE("integration: workspace build failure summary without --stop-on-error (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);
    // Break tool-b's source (deterministic compile failure).
    file_write(root / "apps/tool-b/src/main.cpp",
        "int broken( { this does not compile\n");
    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code != 0);
    std::string out = ws_output(r);
    // strutil + tool-a completed; tool-b failed; nothing skipped.
    REQUIRE(out.find("2 succeeded, 1 failed") != std::string::npos);
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
}

// Case 9: --stop-on-error — dependency-layer failure skips all dependents
TEST_CASE("integration: workspace build --stop-on-error skips dependents of the failing member (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    // Break the LIB (layer 0) — both apps depend on it, so the whole layer 1
    // must be skipped deterministically (nothing dispatched after the failure).
    file_write(root / "libs/strutil/src/strutil.cpp",
        "int broken( { this does not compile\n");
    ProcResult r = run_ezmk("workspace build --stop-on-error -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code != 0);
    std::string out = ws_output(r);
    REQUIRE(out.find("0 succeeded, 1 failed, 2 skipped") != std::string::npos);
    // Dependents never started → no artifacts.
    REQUIRE_FALSE(fs::exists(root / "apps/tool-a/build"));
    REQUIRE_FALSE(fs::exists(root / "apps/tool-b/build"));
}

// Case 10: --member subset + dependency closure; unknown member → fatal
TEST_CASE("integration: workspace build --member selects member + dependency closure (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    // --member tool-a → tool-a + its dependency strutil; tool-b untouched.
    ProcResult r = run_ezmk("workspace build --member tool-a -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    REQUIRE(fs::exists(root / "libs/strutil/build/libstrutil.a"));
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
    REQUIRE_FALSE(fs::exists(root / "apps/tool-b/build"));

    // Full relative path is equivalent.
    TempDir tmp2;
    fs::path root2 = tmp2.path / "ws dir";
    write_ws_fixture(root2);
    ProcResult r2 = run_ezmk("workspace build --member apps/tool-a -j 2", root2);
    REQUIRE(r2.exit_code == 0);
    REQUIRE(fs::exists(ws_app_exe(root2, "tool-a")));
    REQUIRE_FALSE(fs::exists(root2 / "apps/tool-b/build"));

    // Unknown member → fatal with a clear message.
    ProcResult r3 = run_ezmk("workspace build --member nope", root2);
    REQUIRE(r3.exit_code != 0);
    REQUIRE(ws_output(r3).find("unknown workspace member") != std::string::npos);
}

// Case 11: member-internal standalone build — injects EXISTING siblings, does
// not trigger the dependency closure.
TEST_CASE("integration: member-internal build injects existing siblings without closure (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);

    // cd apps/tool-a && ezmk build — only tool-a builds; strutil is NOT
    // rebuilt (its artifact already exists and is injected via self-discovery).
    ProcResult r = run_ezmk("build", root / "apps/tool-a");
    INFO("member build stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);
    REQUIRE(out.find("Archiving libstrutil.a") == std::string::npos);
    REQUIRE(out.find("Linking tool-a") != std::string::npos);
    REQUIRE(out.find("Build successful") != std::string::npos);
}

// Case 12: injection is self-discovery — EZK_WS_* environment variables are
// never read (pit 1 locked at the integration level).
TEST_CASE("integration: workspace injection ignores EZK_WS_* environment variables (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    // Garbage values that would break the build IF the env were consulted.
    EnvGuard env_deps("EZK_WS_DEPS", "/nonexistent/deps");
    EnvGuard env_root("EZK_WS_ROOT", "/nonexistent/root");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    // The build still links both apps against the sibling lib → the injection
    // came from the workspace file (member self-discovery), not the env.
    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
    auto app = run_command("\"" + ws_app_exe(root, "tool-a").string() + "\"");
    REQUIRE(app.exit_code == 0);
    REQUIRE(app.out.find("sum=5") != std::string::npos);
}

// Cases 13–15: configuration rejection — path escape / cycle / non-static dep
TEST_CASE("integration: workspace validation rejects escape/cycle/non-static (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    SECTION("path escape (../outside)") {
        TempDir tmp;
        fs::path root = tmp.path / "ws";
        write_ws_fixture(root);
        file_write(root / "ezmk-workspace.toml",
            "[workspace]\nmembers = [\"../outside\"]\n");
        ProcResult r = run_ezmk("workspace list", root);
        REQUIRE(r.exit_code != 0);
        REQUIRE(ws_output(r).find("outside") != std::string::npos);
    }
    SECTION("dependency cycle (tool-a <-> strutil)") {
        TempDir tmp;
        fs::path root = tmp.path / "ws";
        write_ws_fixture(root);
        file_write(root / "libs/strutil/ezmk.toml",
            "[project]\nname = \"strutil\"\ntype = \"static\"\nversion = \"0.1.0\"\n\n"
            "[depends]\nworkspace = [\"tool-a\"]\n");
        ProcResult r = run_ezmk("workspace list", root);
        REQUIRE(r.exit_code != 0);
        REQUIRE(ws_output(r).find("cycle") != std::string::npos);
    }
    SECTION("non-static dependency (tool-a depends on executable tool-b)") {
        TempDir tmp;
        fs::path root = tmp.path / "ws";
        write_ws_fixture(root);
        file_write(root / "apps/tool-a/ezmk.toml",
            "[project]\nname = \"tool-a\"\ntype = \"executable\"\nversion = \"0.1.0\"\n\n"
            "[depends]\nworkspace = [\"tool-b\"]\n");
        ProcResult r = run_ezmk("workspace list", root);
        REQUIRE(r.exit_code != 0);
        REQUIRE(ws_output(r).find("static") != std::string::npos);
    }
}

// Case 16: nested workspace file marks the member invalid; execution skips it
TEST_CASE("integration: nested ezmk-workspace.toml invalidates the member (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);
    // strutil gains its own workspace file → invalid; tool-a depends on it →
    // tool-a invalid too (referencer of an invalid member).
    file_write(root / "libs/strutil/ezmk-workspace.toml",
        "[workspace]\nmembers = [\"x\"]\n");
    // Drop tool-b's dependency so one member stays valid (tool-b and tool-a
    // both depend on strutil in the fixture — keep only tool-a invalid).
    file_write(root / "apps/tool-b/ezmk.toml",
        "[project]\nname = \"tool-b\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n");
    file_write(root / "apps/tool-b/src/main.cpp",
        "int main() { return 0; }\n");

    ProcResult l = run_ezmk("workspace list", root);
    REQUIRE(l.exit_code == 0);
    REQUIRE(ws_output(l).find("invalid") != std::string::npos);

    // Build: only tool-b remains valid → 1 succeeded, invalid members skipped.
    ProcResult b = run_ezmk("workspace build -j 2", root);
    INFO("build stderr: " << b.err);
    REQUIRE(b.exit_code == 0);
    REQUIRE(ws_output(b).find("1 succeeded") != std::string::npos);
    REQUIRE(fs::exists(ws_app_exe(root, "tool-b")));
    REQUIRE_FALSE(fs::exists(root / "apps/tool-a/build"));
}

// Case 17: missing member directory marks it invalid, does not block the rest
TEST_CASE("integration: missing member directory is skipped, others build (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);
    file_write(root / "ezmk-workspace.toml",
        "[workspace]\nmembers = [\"apps/tool-a\", \"apps/tool-b\", \"libs/strutil\", \"ghost\"]\n");

    ProcResult l = run_ezmk("workspace list", root);
    REQUIRE(l.exit_code == 0);
    REQUIRE(ws_output(l).find("ghost") != std::string::npos);
    REQUIRE(ws_output(l).find("invalid") != std::string::npos);

    ProcResult b = run_ezmk("workspace build -j 2", root);
    REQUIRE(b.exit_code == 0);
    REQUIRE(ws_output(b).find("3 succeeded") != std::string::npos);
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
    REQUIRE(fs::exists(ws_app_exe(root, "tool-b")));
}

// Case 18: pure container root hint; project root with workspace file unchanged
TEST_CASE("integration: pure container root hints at workspace build (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    SECTION("workspace file only (no ezmk.toml) → hint") {
        // Fixture root has ezmk-workspace.toml but no ezmk.toml → pure
        // container root; `ezmk build` must hint at the workspace command.
        fs::path root = tmp.path / "ws";
        write_ws_fixture(root);
        ProcResult r = run_ezmk("build", root);
        REQUIRE(r.exit_code != 0);
        REQUIRE(ws_output(r).find("workspace build") != std::string::npos);
    }
    SECTION("root is BOTH a project and a workspace → normal build") {
        fs::path root = tmp.path / "both";
        write_ws_fixture(root);
        // A real project at the container root: ezmk.toml + src/main.cpp.
        file_write(root / "ezmk.toml",
            "[project]\nname = \"root-proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
        fs::create_directories(root / "src");
        file_write(root / "src/main.cpp", "int main() { return 0; }\n");
        ProcResult r = run_ezmk("build", root);
        INFO("build stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
        REQUIRE(fs::exists(root / "build/root-proj" EZMK_EXE_SUFFIX));
    }
}

// ==============================================================
// 1.3.0-dev.5: consumer commands always build fresh artifacts first —
// `ezmk test` and `ezmk project pack --precompiled` no longer gate on
// artifact existence (stale-artifact trap lock).
// ==============================================================

// 1.3.0-dev.5: `ezmk test` must ALWAYS build first (incremental). The lock:
// change the project source + the test expectation, then run `ezmk test`
// directly — the fresh build recompiles the project object, so the test links
// the NEW value and passes. The old existence-only gate skipped the build,
// linked the stale object and the test FAILED (the stale-artifact trap).
TEST_CASE("integration: ezmk test always builds fresh artifacts (1.3.0-dev.5)", "[integration][1.3.0-dev.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    std::string proj_name = "staletest";
    ProcResult new_r = run_ezmk(
        "project new " + proj_name + " --disable-git-init --disable-gitignore",
        tmp.path);
    REQUIRE(new_r.exit_code == 0);
    fs::path proj_dir = tmp.path / proj_name;

    // Project source with a value the EZMK-framework test checks (the test
    // links the project's own objects minus main.o).
    file_write(proj_dir / "src" / "answer.cpp", "int answer() { return 1; }\n");
    fs::create_directories(proj_dir / "test");
    file_write(proj_dir / "test" / "t.cpp",
        "int answer();\nint main() { return answer() == 1 ? 0 : 1; }\n");
    {
        std::ofstream of(proj_dir / "ezmk.toml");
        of << "[project]\nname = \"" << proj_name << "\"\ntype = \"executable\"\n"
              "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
              "[compile]\nflags = [\"-Wall\"]\n\n"
              "[depends]\nlib = []\n\n"
              "[test]\ndirs = [\"test\"]\nframework = \"ezmk\"\n";
    }

    // 1) Baseline: test passes against answer() == 1.
    ProcResult r1 = run_ezmk("test", proj_dir);
    INFO("baseline stderr: " << r1.err);
    REQUIRE(r1.exit_code == 0);
    REQUIRE((r1.out + r1.err).find("[PASS]") != std::string::npos);

    // 2) Change the PROJECT source AND the test expectation, then run
    //    `ezmk test` DIRECTLY (no `ezmk build` in between). Always-build
    //    recompiles answer.o → the test links the new value → passes.
    file_write(proj_dir / "src" / "answer.cpp", "int answer() { return 2; }\n");
    file_write(proj_dir / "test" / "t.cpp",
        "int answer();\nint main() { return answer() == 2 ? 0 : 1; }\n");
    ProcResult r2 = run_ezmk("test", proj_dir);
    INFO("post-change stderr: " << r2.err);
    REQUIRE(r2.exit_code == 0);
    std::string out2 = r2.out + "\n" + r2.err;
    REQUIRE(out2.find("building project before tests") != std::string::npos);

    // 3) Fresh artifacts → the always-build is incremental (cache hit, no
    //    recompile) and the test still passes. The project build (2 sources:
    //    main.cpp + answer.cpp) reports a full cache hit.
    ProcResult r3 = run_ezmk("test", proj_dir);
    REQUIRE(r3.exit_code == 0);
    std::string out3 = r3.out + "\n" + r3.err;
    REQUIRE(out3.find("0 cached, 1 compiled") == std::string::npos);  // no recompile
    REQUIRE(out3.find("2 cached, 0 compiled") != std::string::npos);
    REQUIRE(out3.find("[PASS]") != std::string::npos);
}

// 1.3.0-dev.5: `pack --precompiled` must ALWAYS build first (incremental).
// The lock: change the static-lib source, then pack directly — the fresh
// build embeds the NEW lib in the archive (different sha256). The old
// existence-only gate packed the stale archive (identical hash).
TEST_CASE("integration: pack --precompiled always builds fresh artifacts (1.3.0-dev.5)", "[integration][1.3.0-dev.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path pkg_dir = tmp.path / "stale-pack";
    fs::create_directories(pkg_dir / "src");
    fs::create_directories(pkg_dir / "include");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"stale-pack\"\ntype = \"static\"\nversion = \"1.0.0\"\n");
    file_write(pkg_dir / "include" / "sp.hpp", "#pragma once\nint sp_answer();\n");
    file_write(pkg_dir / "src" / "sp.cpp", "#include \"sp.hpp\"\nint sp_answer() { return 1; }\n");
    fs::path out = tmp.path / "out";
    fs::create_directories(out);

    // 1) Baseline pack.
    ProcResult p1 = run_ezmk(
        "project pack --precompiled --output \"" + out.string() + "\"", pkg_dir);
    INFO("pack1 stderr: " << p1.err);
    REQUIRE(p1.exit_code == 0);
    fs::path arch = out / "stale-pack-1.0.0.tar.gz";
    REQUIRE(fs::exists(arch));
    std::string h1 = ezmk::crypto::sha256_file(arch);

    // 2) Change the source, then pack DIRECTLY (no `ezmk build` in between).
    //    Always-build recompiles sp.o → the archive embeds the NEW lib →
    //    different hash (the old gate produced an identical archive).
    file_write(pkg_dir / "src" / "sp.cpp", "#include \"sp.hpp\"\nint sp_answer() { return 2; }\n");
    ProcResult p2 = run_ezmk(
        "project pack --precompiled --output \"" + out.string() + "\"", pkg_dir);
    INFO("pack2 stderr: " << p2.err);
    REQUIRE(p2.exit_code == 0);
    REQUIRE(fs::exists(arch));
    std::string h2 = ezmk::crypto::sha256_file(arch);
    REQUIRE(h2 != h1);  // fresh artifacts packed

    // 3) Fresh artifacts → incremental build (no recompile) before packing.
    ProcResult p3 = run_ezmk(
        "project pack --precompiled --output \"" + out.string() + "\"", pkg_dir);
    REQUIRE(p3.exit_code == 0);
    std::string out3 = p3.out + "\n" + p3.err;
    REQUIRE(out3.find("0 cached, 1 compiled") == std::string::npos);  // no recompile
    REQUIRE(out3.find("1 cached, 0 compiled") != std::string::npos);
}

// ==============================================================
// 1.3.2: `ezmk test --report <fmt>[:<path>]` — machine-readable reports
// ==============================================================

namespace {

// EZMK-framework project with one passing and one failing test file.
void write_ezmk_test_proj(const fs::path& proj_dir, const std::string& proj_name) {
    fs::create_directories(proj_dir / "src");
    fs::create_directories(proj_dir / "test");
    file_write(proj_dir / "src" / "main.cpp", "int main() { return 0; }\n");
    file_write(proj_dir / "test" / "pass_test.cpp",
        "#include <cstdio>\nint main() { std::printf(\"pass-test-ok\\n\"); return 0; }\n");
    file_write(proj_dir / "test" / "fail_test.cpp",
        "#include <cstdio>\nint main() { std::printf(\"fail-test-ran\\n\"); return 1; }\n");
    file_write(proj_dir / "ezmk.toml",
        "[project]\nname = \"" + proj_name + "\"\ntype = \"executable\"\n"
        "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[compile]\nflags = [\"-Wall\"]\ninclude_dirs = [\"include\"]\n\n"
        "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
        "[depends]\nlib = []\n\n"
        "[test]\ndirs = [\"test\"]\nframework = \"ezmk\"\n");
}

// Minimal EZMK-framework project: a single passing test + src/main.cpp.
void write_ezmk_pass_proj(const fs::path& proj_dir, const std::string& proj_name) {
    fs::create_directories(proj_dir / "src");
    fs::create_directories(proj_dir / "test");
    file_write(proj_dir / "src" / "main.cpp", "int main() { return 0; }\n");
    file_write(proj_dir / "test" / "ok_test.cpp", "int main() { return 0; }\n");
    file_write(proj_dir / "ezmk.toml",
        "[project]\nname = \"" + proj_name + "\"\ntype = \"executable\"\n"
        "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[compile]\ninclude_dirs = [\"include\"]\n\n"
        "[test]\ndirs = [\"test\"]\nframework = \"ezmk\"\n");
}

} // anonymous namespace

// EZMK framework: report file written BEFORE the failure gate, so CI sees the
// failures even though `ezmk test` exits non-zero. Console summary intact.
TEST_CASE("integration: ezmk test --report junit with failures (1.3.2)", "[integration][1.3.2]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "rpt_fail";
    write_ezmk_test_proj(proj_dir, "rpt_fail");

    ProcResult r = run_ezmk("test --report junit", proj_dir);
    INFO("stderr: " << r.err);
    std::string combined = r.out + "\n" + r.err;
    // Console summary unchanged (坑 1: report never clobbers stdout).
    REQUIRE(combined.find("2 tests: 1 passed, 1 failed") != std::string::npos);
    REQUIRE(combined.find("[PASS] pass_test.cpp") != std::string::npos);
    REQUIRE(combined.find("[FAIL] fail_test.cpp") != std::string::npos);
    // Failure gate preserved.
    REQUIRE(r.exit_code != 0);

    // Report file exists at the default path with the failure recorded.
    fs::path report = proj_dir / ".ezmk" / "test-results" / "junit.xml";
    REQUIRE(fs::exists(report));
    std::string xml = file_read(report);
    REQUIRE(xml.find("<testsuites tests=\"2\" failures=\"1\" errors=\"0\"") != std::string::npos);
    REQUIRE(xml.find("<failure message=\"test failed\">") != std::string::npos);
    REQUIRE(xml.find("fail-test-ran") != std::string::npos);
    REQUIRE(xml.find("<testcase name=\"pass_test.cpp\"") != std::string::npos);
    REQUIRE(xml.find("<failure") != std::string::npos);
}

TEST_CASE("integration: ezmk test --report junit all-pass (1.3.2)", "[integration][1.3.2]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "rpt_pass";
    write_ezmk_pass_proj(proj_dir, "rpt_pass");

    ProcResult r = run_ezmk("test --report junit", proj_dir);
    INFO("stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    fs::path report = proj_dir / ".ezmk" / "test-results" / "junit.xml";
    REQUIRE(fs::exists(report));
    std::string xml = file_read(report);
    REQUIRE(xml.find("<testsuites tests=\"1\" failures=\"0\" errors=\"0\"") != std::string::npos);
    REQUIRE(xml.find("<failure") == std::string::npos);
}

TEST_CASE("integration: ezmk test --report custom path (1.3.2)", "[integration][1.3.2]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "rpt_path";
    write_ezmk_pass_proj(proj_dir, "rpt_path");

    // Relative path resolves against the project root.
    ProcResult r = run_ezmk("test --report junit:custom/results.xml", proj_dir);
    INFO("stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    fs::path report = proj_dir / "custom" / "results.xml";
    REQUIRE(fs::exists(report));
    REQUIRE(file_read(report).find("<testsuites") != std::string::npos);
    // The default-path file must NOT exist (custom path won).
    REQUIRE_FALSE(fs::exists(proj_dir / ".ezmk" / "test-results" / "junit.xml"));
}

TEST_CASE("integration: ezmk framework rejects non-junit report formats (1.3.2)", "[integration][1.3.2]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "rpt_json";
    write_ezmk_pass_proj(proj_dir, "rpt_json");

    ProcResult r = run_ezmk("test --report json", proj_dir);
    std::string combined = r.out + "\n" + r.err;
    REQUIRE(r.exit_code != 0);
    // Explicit hint to use Catch2 for non-junit formats (坑 3).
    REQUIRE(combined.find("not supported") != std::string::npos);
    REQUIRE(combined.find("Catch2") != std::string::npos);
}

// Catch2 path: `-r junit::out=<file>` — the console reporter stays default and
// the summary parse still works; the JUnit file carries the same result.
TEST_CASE("integration: catch2 test --report junit writes report (1.3.2)", "[integration][1.3.2]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "rpt_catch2";
    fs::create_directories(proj_dir / "src");
    fs::create_directories(proj_dir / "test");
    file_write(proj_dir / "src" / "main.cpp", "int main() { return 0; }\n");
    file_write(proj_dir / "test" / "test_math.cpp",
        "#include <catch2/catch_test_macros.hpp>\n"
        "TEST_CASE(\"addition works\", \"[math]\") { REQUIRE(1 + 1 == 2); }\n");
    file_write(proj_dir / "ezmk.toml",
        "[project]\nname = \"rpt_catch2\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[compile]\ninclude_dirs = [\"include\"]\n\n"
        "[depends]\nlib = [\"catch2\"]\n\n"
        "[test]\ndirs = [\"test\"]\nframework = \"catch2\"\n");

    ProcResult inst = run_ezmk("pkg install catch2 -p -y", proj_dir);
    if (inst.exit_code != 0) {
        SKIP("catch2 install failed (no repo / offline) — skipping");
    }

    ProcResult r = run_ezmk("test --report junit", proj_dir);
    INFO("stderr: " << r.err);
    std::string combined = r.out + "\n" + r.err;
    if (r.exit_code != 0 && combined.find("Catch2 not found") != std::string::npos) {
        SKIP("catch2 not available — skipping");
    }
    REQUIRE(r.exit_code == 0);
    // Console summary parse untouched (坑 1).
    REQUIRE(combined.find("failed: 0") != std::string::npos);
    fs::path report = proj_dir / ".ezmk" / "test-results" / "junit.xml";
    REQUIRE(fs::exists(report));
    std::string xml = file_read(report);
    REQUIRE(xml.find("<testsuites") != std::string::npos);
    REQUIRE(xml.find("addition works") != std::string::npos);
}

// 1.3.2: `workspace test --report` forwards the flag to every member — each
// member writes its OWN report file (default path under its project root).
TEST_CASE("integration: workspace test --report writes per-member reports (1.3.2)", "[integration][workspace][1.3.2]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws_rpt";
    write_ws_fixture(root);  // tool-a has [test] ezmk + test dir

    // Give tool-b a test dir + [test] section so TWO members write reports.
    fs::create_directories(root / "apps/tool-b" / "test");
    file_write(root / "apps/tool-b" / "test" / "b_test.cpp",
        "#include <cstdio>\nint main() { std::printf(\"b-ok\\n\"); return 0; }\n");
    file_write(root / "apps/tool-b" / "ezmk.toml",
        "[project]\nname = \"tool-b\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[depends]\nworkspace = [\"strutil\"]\n\n"
        "[test]\ndirs = [\"test\"]\nframework = \"ezmk\"\n");

    // Members link against sibling artifacts — build the workspace first.
    ProcResult b = run_ezmk("workspace build -j 2", root);
    REQUIRE(b.exit_code == 0);

    ProcResult r = run_ezmk("workspace test --report junit -j 2", root);
    INFO("stderr: " << r.err);
    REQUIRE(r.exit_code == 0);

    // Each tested member wrote its own report (default per-member path).
    fs::path rpt_a = root / "apps/tool-a" / ".ezmk" / "test-results" / "junit.xml";
    fs::path rpt_b = root / "apps/tool-b" / ".ezmk" / "test-results" / "junit.xml";
    REQUIRE(fs::exists(rpt_a));
    REQUIRE(fs::exists(rpt_b));
    REQUIRE(file_read(rpt_a).find("<testcase name=\"test_smoke.cpp\"") != std::string::npos);
    REQUIRE(file_read(rpt_b).find("<testcase name=\"b_test.cpp\"") != std::string::npos);
    // The workspace root itself has no report (members are the units).
    REQUIRE_FALSE(fs::exists(root / ".ezmk" / "test-results" / "junit.xml"));
}

// ==============================================================
// 1.3.4: `ezmk watch --run` / `-r` — run the executable after each
// successful rebuild (blocking; watch resumes when the program exits).
// ==============================================================
namespace {

// Poll `file` until it contains `needle` (or timeout). ezmk logs to stderr
// unbuffered, so redirected output appears promptly — polling is reliable.
bool poll_log(const fs::path& file, const std::string& needle,
              std::chrono::seconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (fs::exists(file)) {
            std::string content = file_read(file);
            if (content.find(needle) != std::string::npos) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return false;
}

// Kill all ezmk watch processes (same approach as the existing watch test).
void kill_watch_processes() {
#ifdef EZMK_WIN
    run_command("cmd /c taskkill /F /IM ezmk.exe 2>nul");
#else
    run_command("pkill -f \"ezmk project watch\" 2>/dev/null || true");
#endif
}

// An executable project whose main.cpp prints a unique marker each run.
void write_watch_run_proj(const fs::path& proj_dir, const std::string& name) {
    fs::create_directories(proj_dir / "src");
    file_write(proj_dir / "src" / "main.cpp",
        "#include <cstdio>\n"
        "int main() { std::printf(\"WATCH-RUN-MARKER\\n\"); return 0; }\n");
    file_write(proj_dir / "ezmk.toml",
        "[project]\nname = \"" + name + "\"\ntype = \"executable\"\n"
        "version = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[compile]\ninclude_dirs = [\"include\"]\n\n"
        "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
        "[depends]\nlib = []\n");
}

// Start `ezmk project watch <flags>` in proj_dir (background), wait until the
// watcher is ready, then return. Caller must kill_watch_processes() at the end.
void start_watch(const fs::path& proj_dir, const std::string& flags,
                 const fs::path& log_file) {
    std::string ezmk_bin = find_ezmk_binary().string();
    std::string watch_cmd;
#ifdef EZMK_WIN
    watch_cmd = "cmd /c start \"\" /D \"" + proj_dir.string() + "\" /B " +
                escape_shell_arg(ezmk_bin) +
                " project watch " + flags + " > \"" +
                escape_shell_arg(log_file.string()) + "\" 2>&1";
#else
    watch_cmd = "cd " + escape_shell_arg(proj_dir.string()) + " && " +
                escape_shell_arg(ezmk_bin) +
                " project watch " + flags + " > " +
                escape_shell_arg(log_file.string()) + " 2>&1 &";
#endif
    run_command(watch_cmd);
    bool ready = poll_log(log_file, "Watching for changes", std::chrono::seconds(10));
    INFO("watch started: " << (ready ? "yes" : "no"));
}

// Append a marker-bearing source change to trigger a rebuild.
void touch_source(const fs::path& proj_dir, const std::string& marker) {
    std::ofstream f(proj_dir / "src" / "main.cpp", std::ios::app);
    f << "// " << marker << "\n";
    f.close();
}

// Count occurrences of a needle in the log (for "ran N times" assertions).
size_t count_in_log(const fs::path& log_file, const std::string& needle) {
    if (!fs::exists(log_file)) return 0;
    std::string content = file_read(log_file);
    size_t count = 0, pos = 0;
    while ((pos = content.find(needle, pos)) != std::string::npos) {
        count++;
        pos += needle.size();
    }
    return count;
}

} // anonymous namespace

// Executable + --run: after each successful rebuild the program runs (marker
// appears); the initial build does NOT run (first run after first change).
TEST_CASE("integration: watch --run runs executable after rebuilds (1.3.4)", "[integration][1.3.4]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "wr_run";
    write_watch_run_proj(proj_dir, "wr_run");

    fs::path log_file = tmp.path / "watch_run.log";
    start_watch(proj_dir, "--run", log_file);

    // Initial build succeeded → no marker yet (first run happens after a change).
    CHECK(count_in_log(log_file, "WATCH-RUN-MARKER") == 0);

    // Change 1 → rebuild → run → marker.
    touch_source(proj_dir, "change-1");
    CHECK(poll_log(log_file, "WATCH-RUN-MARKER", std::chrono::seconds(30)));

    // Change 2 → rebuild → run again (marker count grows to ≥ 2).
    touch_source(proj_dir, "change-2");
    bool ran_twice = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline) {
        if (count_in_log(log_file, "WATCH-RUN-MARKER") >= 2) { ran_twice = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    INFO("watch log:\n" << (fs::exists(log_file) ? file_read(log_file) : ""));
    kill_watch_processes();
    REQUIRE(ran_twice);
    REQUIRE(count_in_log(log_file, "WATCH-RUN-MARKER") >= 2);
    REQUIRE(count_in_log(log_file, "Running wr_run.exe") +
            count_in_log(log_file, "Running wr_run") >= 2);
}

// Build failure → NO run (never run stale artifacts); watch survives and runs
// again after the source is fixed.
TEST_CASE("integration: watch --run skips on build failure (1.3.4)", "[integration][1.3.4]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "wr_fail";
    write_watch_run_proj(proj_dir, "wr_fail");

    fs::path log_file = tmp.path / "watch_fail.log";
    start_watch(proj_dir, "--run", log_file);

    // Introduce a compile error → rebuild fails → no run output.
    file_write(proj_dir / "src" / "main.cpp",
        "#include <cstdio>\nint main() { this is not valid c++ }\n");
    bool failed = poll_log(log_file, "build failed", std::chrono::seconds(30));
    INFO("watch log:\n" << (fs::exists(log_file) ? file_read(log_file) : ""));
    REQUIRE(failed);
    REQUIRE(count_in_log(log_file, "WATCH-RUN-MARKER") == 0);

    // Fix the source → rebuild succeeds → program runs (watch stayed alive).
    file_write(proj_dir / "src" / "main.cpp",
        "#include <cstdio>\nint main() { std::printf(\"WATCH-RUN-MARKER\\n\"); return 0; }\n");
    bool ran_after_fix = poll_log(log_file, "WATCH-RUN-MARKER", std::chrono::seconds(30));
    kill_watch_processes();
    REQUIRE(ran_after_fix);
}

// Non-executable + --run → startup fatal (config-time gate).
TEST_CASE("integration: watch --run rejected for non-executable projects (1.3.4)", "[integration][1.3.4]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "wr_static";
    fs::create_directories(proj_dir / "src");
    file_write(proj_dir / "src" / "lib.cpp", "int f() { return 1; }\n");
    file_write(proj_dir / "ezmk.toml",
        "[project]\nname = \"wr_static\"\ntype = \"static\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n");

    ProcResult r = run_ezmk("watch --run", proj_dir);
    INFO("stderr: " << r.err);
    std::string combined = r.out + "\n" + r.err;
    REQUIRE(r.exit_code != 0);
    REQUIRE(combined.find("requires an executable") != std::string::npos);
    REQUIRE(combined.find("static") != std::string::npos);
}

// No --run → behavior unchanged: source change rebuilds but never runs.
TEST_CASE("integration: watch without --run never runs (1.3.4)", "[integration][1.3.4]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path proj_dir = tmp.path / "wr_plain";
    write_watch_run_proj(proj_dir, "wr_plain");

    fs::path log_file = tmp.path / "watch_plain.log";
    start_watch(proj_dir, "", log_file);

    touch_source(proj_dir, "change-1");
    // Rebuild output appears, but the program never runs.
    bool rebuilt = poll_log(log_file, "Build succeeded", std::chrono::seconds(30));
    INFO("watch log:\n" << (fs::exists(log_file) ? file_read(log_file) : ""));
    kill_watch_processes();
    REQUIRE(rebuilt);
    REQUIRE(count_in_log(log_file, "WATCH-RUN-MARKER") == 0);
    REQUIRE(count_in_log(log_file, "Running wr_plain") == 0);
}

// ==============================================================
// 1.3.5: `ezmk project pack --format <tar.gz|zip>` + .sha256 sidecar
// ==============================================================
namespace {

// A static-lib package (include/ + src/ + ezmk.toml) for pack tests.
void write_pack135_pkg(const fs::path& pkg_dir, const std::string& name) {
    fs::create_directories(pkg_dir / "include");
    fs::create_directories(pkg_dir / "src");
    file_write(pkg_dir / "include" / (name + ".hpp"),
        "#pragma once\nint " + name + "_answer();\n");
    file_write(pkg_dir / "src" / (name + ".cpp"),
        "#include \"" + name + ".hpp\"\nint " + name + "_answer() { return 135; }\n");
    file_write(pkg_dir / "ezmk.toml",
        "[project]\nname = \"" + name + "\"\ntype = \"static\"\nversion = \"1.0.0\"\n");
}

// Recursive (relative path → sha256) map of an extracted directory.
std::map<std::string, std::string> tree_hashes(const fs::path& dir) {
    std::map<std::string, std::string> out;
    for (auto& e : fs::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        auto rel = fs::relative(e.path(), dir).generic_string();
        out[rel] = ezmk::crypto::sha256_file(e.path());
    }
    return out;
}

} // anonymous namespace

// zip E2E: pack --format zip → pkg install <zip> compiles on the consumer side.
TEST_CASE("integration: pack --format zip installs end-to-end (1.3.5)", "[integration][1.3.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path pkg_dir = tmp.path / "pkg135";
    write_pack135_pkg(pkg_dir, "pkg135");
    fs::path out = tmp.path / "out";
    fs::create_directories(out);

    ProcResult r = run_ezmk("project pack --format zip --output \"" + out.string() + "\"", pkg_dir);
    INFO("pack stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    fs::path zip = out / "pkg135-1.0.0.zip";
    REQUIRE(fs::exists(zip));

    // Consumer installs the zip → compiled on the consumer side (source pkg).
    fs::path proj = tmp.path / "app135";
    ProcResult new_r = run_ezmk("project new app135 --disable-git-init --disable-gitignore", tmp.path);
    REQUIRE(new_r.exit_code == 0);
    ProcResult inst = run_ezmk("pkg install \"" + zip.string() + "\" -p -y", proj);
    INFO("zip install stderr: " << inst.err);
    REQUIRE(inst.exit_code == 0);
    REQUIRE(fs::exists(proj / ".ezmk" / "pkg" / "pkg135" / "build" / "libpkg135.a"));
    REQUIRE(fs::exists(proj / ".ezmk" / "pkg" / "pkg135" / "include" / "pkg135.hpp"));
}

// Equivalence: tar.gz and zip extract to the same file set + content hashes.
TEST_CASE("integration: pack zip/tar.gz contents are equivalent (1.3.5)", "[integration][1.3.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path pkg_dir = tmp.path / "eq135";
    write_pack135_pkg(pkg_dir, "eq135");
    fs::path out = tmp.path / "out";
    fs::create_directories(out);

    ProcResult rt = run_ezmk("project pack --output \"" + out.string() + "\"", pkg_dir);
    REQUIRE(rt.exit_code == 0);
    ProcResult rz = run_ezmk("project pack --format zip --output \"" + out.string() + "\"", pkg_dir);
    REQUIRE(rz.exit_code == 0);
    fs::path tgz = out / "eq135-1.0.0.tar.gz";
    fs::path zip = out / "eq135-1.0.0.zip";
    REQUIRE(fs::exists(tgz));
    REQUIRE(fs::exists(zip));

    // Extract both and compare the recursive file set + content hashes.
    fs::path d_tgz = tmp.path / "x_tgz", d_zip = tmp.path / "x_zip";
    fs::create_directories(d_tgz);
    fs::create_directories(d_zip);
    REQUIRE_NOTHROW(extract_archive(tgz, d_tgz));
    REQUIRE_NOTHROW(extract_archive(zip, d_zip));
    auto h1 = tree_hashes(d_tgz);
    auto h2 = tree_hashes(d_zip);
    REQUIRE(h1 == h2);
    // Spot-check the expected entries (same layout as the tarball).
    REQUIRE(h1.count("include/eq135.hpp") == 1);
    REQUIRE(h1.count("src/eq135.cpp") == 1);
    REQUIRE(h1.count("ezmk.toml") == 1);
}

// Default format regression + .sha256 sidecar + invalid format rejection.
TEST_CASE("integration: pack default format and .sha256 sidecar (1.3.5)", "[integration][1.3.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path pkg_dir = tmp.path / "def135";
    write_pack135_pkg(pkg_dir, "def135");
    fs::path out = tmp.path / "out";
    fs::create_directories(out);

    // Default (no --format) → .tar.gz, no .zip (behavior unchanged).
    ProcResult r = run_ezmk("project pack --output \"" + out.string() + "\"", pkg_dir);
    REQUIRE(r.exit_code == 0);
    fs::path tgz = out / "def135-1.0.0.tar.gz";
    REQUIRE(fs::exists(tgz));
    REQUIRE_FALSE(fs::exists(out / "def135-1.0.0.zip"));

    // --format zip → .zip + sidecar.
    ProcResult rz = run_ezmk("project pack --format zip --output \"" + out.string() + "\"", pkg_dir);
    REQUIRE(rz.exit_code == 0);
    fs::path zip = out / "def135-1.0.0.zip";
    REQUIRE(fs::exists(zip));

    // Sidecar: both formats write <archive>.sha256 matching sha256_file,
    // in "<hash>  <filename>" form.
    std::string s_tgz = file_read(tgz.string() + ".sha256");
    std::string s_zip = file_read(zip.string() + ".sha256");
    REQUIRE(s_tgz.find(ezmk::crypto::sha256_file(tgz)) != std::string::npos);
    REQUIRE(s_zip.find(ezmk::crypto::sha256_file(zip)) != std::string::npos);
    REQUIRE(s_tgz.find("def135-1.0.0.tar.gz") != std::string::npos);
    REQUIRE(s_zip.find("def135-1.0.0.zip") != std::string::npos);

    // Invalid format → fatal with a hint.
    ProcResult bad = run_ezmk("project pack --format deb --output \"" + out.string() + "\"", pkg_dir);
    REQUIRE(bad.exit_code != 0);
    std::string combined = bad.out + "\n" + bad.err;
    REQUIRE(combined.find("invalid --format") != std::string::npos);
    REQUIRE(combined.find("tar.gz") != std::string::npos);
}
