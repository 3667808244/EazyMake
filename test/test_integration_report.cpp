// End-to-end integration tests for EazyMake — report / watch / pack (1.3.2 / 1.3.4 / 1.3.5) (split from
// test_integration.cpp in 1.3.6; shared helpers in test_integration_helpers.hpp).
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "test_helpers.hpp"
#include "test_integration_helpers.hpp"
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

using namespace ezi;
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


// ==============================================================
// 1.3.4: `ezmk watch --run` / `-r` — run the executable after each
// successful rebuild (blocking; watch resumes when the program exits).
// ==============================================================
namespace {

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

// ==============================================================
// 1.4.0-dev.5: pkg install — .sha256 sidecar auto-verification
// ==============================================================
namespace {

// Pack `name` (static-lib source package) into `out` and return the archive +
// its .sha256 sidecar path. Both formats carry a sidecar (1.3.5).
std::pair<fs::path, fs::path> pack_with_sidecar(const fs::path& pkg_dir,
                                                const std::string& name,
                                                const fs::path& out) {
    ProcResult r = run_ezmk("project pack --output \"" + out.string() + "\"", pkg_dir);
    REQUIRE(r.exit_code == 0);
    fs::path archive = out / (name + "-1.0.0.tar.gz");
    REQUIRE(fs::exists(archive));
    fs::path sidecar = fs::path(archive.string() + ".sha256");
    REQUIRE(fs::exists(sidecar));
    return {archive, sidecar};
}

} // anonymous namespace

// Sidecar present + valid → auto-verified (no --sha256 needed).
TEST_CASE("integration: pkg install auto-verifies via .sha256 sidecar (1.4.0-dev.5)", "[integration][1.4.0-dev.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path pkg_dir = tmp.path / "sc135";
    write_pack135_pkg(pkg_dir, "sc135");
    fs::path out = tmp.path / "out";
    fs::create_directories(out);
    auto [archive, sidecar] = pack_with_sidecar(pkg_dir, "sc135", out);

    fs::path proj = tmp.path / "app_sc";
    ProcResult new_r = run_ezmk("project new app_sc --disable-git-init --disable-gitignore", tmp.path);
    REQUIRE(new_r.exit_code == 0);

    // No --sha256: the sibling sidecar is read and verified automatically.
    ProcResult inst = run_ezmk("pkg install \"" + archive.string() + "\" -p -y", proj);
    INFO("sidecar install stderr: " << inst.err);
    REQUIRE(inst.exit_code == 0);
    REQUIRE(inst.err.find("sidecar") != std::string::npos);  // "verifying via .sha256 sidecar"
    REQUIRE(fs::exists(proj / ".ezmk" / "pkg" / "sc135" / "build" / "libsc135.a"));
}

// Sidecar tampered (wrong hash) → verification FAILS (blocks install).
TEST_CASE("integration: pkg install rejects a tampered .sha256 sidecar (1.4.0-dev.5)", "[integration][1.4.0-dev.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path pkg_dir = tmp.path / "sc_bad";
    write_pack135_pkg(pkg_dir, "sc_bad");
    fs::path out = tmp.path / "out";
    fs::create_directories(out);
    auto [archive, sidecar] = pack_with_sidecar(pkg_dir, "sc_bad", out);

    // Corrupt the sidecar hash (keep the "<hash>  <filename>" shape).
    std::string bad_hash(64, '0');
    file_write(sidecar, bad_hash + "  sc_bad-1.0.0.tar.gz\n");

    fs::path proj = tmp.path / "app_sc_bad";
    ProcResult new_r = run_ezmk("project new app_sc_bad --disable-git-init --disable-gitignore", tmp.path);
    REQUIRE(new_r.exit_code == 0);

    ProcResult inst = run_ezmk("pkg install \"" + archive.string() + "\" -p -y", proj);
    INFO("tampered install stderr: " << inst.err);
    REQUIRE(inst.exit_code != 0);
    std::string combined = inst.out + "\n" + inst.err;
    REQUIRE(combined.find("SHA-256 mismatch") != std::string::npos);
    REQUIRE_FALSE(fs::exists(proj / ".ezmk" / "pkg" / "sc_bad"));
}

// Sidecar missing → verification skipped (not blocked); explicit --sha256 wins
// over a (contradictory) sidecar.
TEST_CASE("integration: pkg install sidecar missing or explicit --sha256 precedence (1.4.0-dev.5)", "[integration][1.4.0-dev.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path pkg_dir = tmp.path / "sc_miss";
    write_pack135_pkg(pkg_dir, "sc_miss");
    fs::path out = tmp.path / "out";
    fs::create_directories(out);
    auto [archive, sidecar] = pack_with_sidecar(pkg_dir, "sc_miss", out);

    // NOTE: Catch2 re-runs the whole test body per SECTION — create a fresh
    // consumer project inside each section (project new fails on existing dir).
    auto make_consumer = [&](const std::string& name) {
        fs::path proj = tmp.path / name;
        ProcResult new_r = run_ezmk("project new " + name + " --disable-git-init --disable-gitignore", tmp.path);
        REQUIRE(new_r.exit_code == 0);
        return proj;
    };

    SECTION("no sidecar → install proceeds without verification") {
        fs::remove(sidecar);
        fs::path proj = make_consumer("app_sc_miss");
        ProcResult inst = run_ezmk("pkg install \"" + archive.string() + "\" -p -y", proj);
        INFO("no-sidecar install stderr: " << inst.err);
        REQUIRE(inst.exit_code == 0);
        REQUIRE(fs::exists(proj / ".ezmk" / "pkg" / "sc_miss" / "build" / "libsc_miss.a"));
    }
    SECTION("explicit --sha256 wins over a contradictory sidecar") {
        // Correct hash from the pack (recomputed independently so the test is
        // self-verifying — the sidecar itself is the reference here).
        fs::path proj = make_consumer("app_sc_expl");
        std::string correct = ezmk::crypto::sha256_file(archive);
        ProcResult inst = run_ezmk("pkg install \"" + archive.string() +
                                   "\" --sha256 " + correct + " -p -y", proj);
        INFO("explicit-sha install stderr: " << inst.err);
        REQUIRE(inst.exit_code == 0);
        REQUIRE(fs::exists(proj / ".ezmk" / "pkg" / "sc_miss" / "build" / "libsc_miss.a"));
    }
    SECTION("malformed sidecar (garbage, not 64 hex) → skipped, not blocked") {
        file_write(sidecar, "not-a-hash  sc_miss-1.0.0.tar.gz\n");
        fs::path proj = make_consumer("app_sc_mal");
        ProcResult inst = run_ezmk("pkg install \"" + archive.string() + "\" -p -y", proj);
        INFO("malformed-sidecar install stderr: " << inst.err);
        REQUIRE(inst.exit_code == 0);
        REQUIRE(fs::exists(proj / ".ezmk" / "pkg" / "sc_miss" / "build" / "libsc_miss.a"));
    }
}
