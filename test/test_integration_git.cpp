// 1.4.1: `pkg install <git-url>` end-to-end tests. A local git repository is
// built as an offline fixture (no network), then installed via file:// URLs.
// Covers the four ref forms: default branch, "#tag", "#<sha>", "--branch".
//
// Skipped when git is unavailable (fixture cannot be built).

#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "test_helpers.hpp"
#include "test_integration_helpers.hpp"
#include "ezmk/util.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ezi;
using namespace ezmk::util;

namespace {

// Run a git command in a working directory (run_command's cwd option is
// cross-platform — no shell cd wrapper needed).
ProcResult run_git(const std::string& args, const fs::path& cwd) {
    RunOptions opts;
    opts.cwd = cwd;
    return run_command("git " + args, opts);
}

// Build a file:// URL from a Windows/Unix path.
std::string file_url(const fs::path& p) {
    std::string s = p.lexically_normal().string();
    for (auto& c : s) if (c == '\\') c = '/';
    return "file:///" + s;
}

// git init with a deterministic default branch ("main").
bool git_init(const fs::path& dir) {
    fs::create_directories(dir);
    auto r = run_git("init -q -b main", dir);
    if (r.exit_code != 0) return false;   // old git: fall back to plain init
    return true;
}

bool git_commit_all(const fs::path& dir, const std::string& msg) {
    run_git("config user.email test@example.com", dir);
    run_git("config user.name test", dir);
    if (run_git("add -A", dir).exit_code != 0) return false;
    return run_git("commit -q -m \"" + msg + "\"", dir).exit_code == 0;
}

// Create a source package commit with the given version in ezmk.toml.
void write_pkg_version(const fs::path& repo, const std::string& version) {
    file_write(repo / "ezmk.toml",
        "[project]\nname = \"gitlib\"\ntype = \"static\"\nversion = \"" + version + "\"\n");
    file_write(repo / "include" / "gitlib.hpp", "#pragma once\n");
    file_write(repo / "src" / "gitlib.cpp",
        "#include \"gitlib.hpp\"\nint gitlib_value() { return 0; }\n");
}

// Read the installed package's version from ezmk.toml ("" if not installed).
std::string installed_version(const fs::path& proj_dir) {
    fs::path toml = proj_dir / ".ezmk" / "pkg" / "gitlib" / "ezmk.toml";
    if (!fs::exists(toml)) return "";
    std::string content = file_read(toml);
    auto pos = content.find("version = \"");
    if (pos == std::string::npos) return "";
    pos += 11;
    auto end = content.find('"', pos);
    return content.substr(pos, end - pos);
}

} // namespace

// Fixture: a git repo with two commits + a tag on the first + a "dev" branch.
struct GitFixture {
    TempDir tmp;
    fs::path repo;
    std::string sha_a;   // commit A (version 1.0.0, tagged v1.0)
    std::string sha_b;   // commit B (version 2.0.0, on main)

    GitFixture() : repo(tmp.path / "gitlib-repo") {
        REQUIRE(git_init(repo));
        fs::create_directories(repo / "include");
        fs::create_directories(repo / "src");
        write_pkg_version(repo, "1.0.0");
        REQUIRE(git_commit_all(repo, "v1"));
        sha_a = run_git("rev-parse HEAD", repo).out;
        while (!sha_a.empty() && (sha_a.back() == '\n' || sha_a.back() == '\r')) sha_a.pop_back();
        REQUIRE(run_git("tag v1.0", repo).exit_code == 0);
        write_pkg_version(repo, "2.0.0");
        REQUIRE(git_commit_all(repo, "v2"));
        sha_b = run_git("rev-parse HEAD", repo).out;
        while (!sha_b.empty() && (sha_b.back() == '\n' || sha_b.back() == '\r')) sha_b.pop_back();
        REQUIRE(run_git("branch dev " + sha_a, repo).exit_code == 0);
    }
};

TEST_CASE("integration: pkg install git URL — default branch clones HEAD (1.4.1)", "[integration][1.4.1]") {
    if (!ezmk_available() || !git_available()) {
        SKIP("ezmk binary or git not available");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    GitFixture fx;
    TempDir proj_tmp;
    fs::path proj = proj_tmp.path / "proj";
    fs::create_directories(proj / "src");
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
    file_write(proj / "src" / "main.cpp", "int main() { return 0; }\n");

    ProcResult r = run_ezmk("pkg install \"" + file_url(fx.repo) + "\" -p -y", proj);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(installed_version(proj) == "2.0.0");   // main HEAD
}

TEST_CASE("integration: pkg install git URL — #tag clones the tagged commit (1.4.1)", "[integration][1.4.1]") {
    if (!ezmk_available() || !git_available()) {
        SKIP("ezmk binary or git not available");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    GitFixture fx;
    TempDir proj_tmp;
    fs::path proj = proj_tmp.path / "proj";
    fs::create_directories(proj / "src");
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
    file_write(proj / "src" / "main.cpp", "int main() { return 0; }\n");

    ProcResult r = run_ezmk("pkg install \"" + file_url(fx.repo) + "#v1.0\" -p -y", proj);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(installed_version(proj) == "1.0.0");   // tagged commit
}

TEST_CASE("integration: pkg install git URL — #<sha> pins the exact commit (1.4.1)", "[integration][1.4.1]") {
    if (!ezmk_available() || !git_available()) {
        SKIP("ezmk binary or git not available");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    GitFixture fx;
    TempDir proj_tmp;
    fs::path proj = proj_tmp.path / "proj";
    fs::create_directories(proj / "src");
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
    file_write(proj / "src" / "main.cpp", "int main() { return 0; }\n");

    // Pin the OLD commit by full SHA (a full clone + detached checkout is used).
    ProcResult r = run_ezmk("pkg install \"" + file_url(fx.repo) + "#" + fx.sha_a + "\" -p -y", proj);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(installed_version(proj) == "1.0.0");
}

TEST_CASE("integration: pkg install git URL — --branch selects a non-default branch (1.4.1)", "[integration][1.4.1]") {
    if (!ezmk_available() || !git_available()) {
        SKIP("ezmk binary or git not available");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    GitFixture fx;
    TempDir proj_tmp;
    fs::path proj = proj_tmp.path / "proj";
    fs::create_directories(proj / "src");
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
    file_write(proj / "src" / "main.cpp", "int main() { return 0; }\n");

    ProcResult r = run_ezmk(
        "pkg install \"" + file_url(fx.repo) + "\" --branch dev -p -y", proj);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(installed_version(proj) == "1.0.0");   // dev branch == commit A
}

TEST_CASE("integration: pkg install git URL — --branch beats the URL #ref (1.4.1)", "[integration][1.4.1]") {
    if (!ezmk_available() || !git_available()) {
        SKIP("ezmk binary or git not available");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    GitFixture fx;
    TempDir proj_tmp;
    fs::path proj = proj_tmp.path / "proj";
    fs::create_directories(proj / "src");
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
    file_write(proj / "src" / "main.cpp", "int main() { return 0; }\n");

    // URL says #v1.0 (→ 1.0.0) but --branch main must win (→ 2.0.0).
    ProcResult r = run_ezmk(
        "pkg install \"" + file_url(fx.repo) + "#v1.0\" --branch main -p -y", proj);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);
    REQUIRE(installed_version(proj) == "2.0.0");
}

TEST_CASE("integration: pkg install git URL — lockfile records source=git + commit (1.4.1)", "[integration][1.4.1]") {
    if (!ezmk_available() || !git_available()) {
        SKIP("ezmk binary or git not available");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    GitFixture fx;
    TempDir proj_tmp;
    fs::path proj = proj_tmp.path / "proj";
    fs::create_directories(proj / "src");
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
    file_write(proj / "src" / "main.cpp", "int main() { return 0; }\n");

    ProcResult r = run_ezmk("pkg install \"" + file_url(fx.repo) + "#v1.0\" -p -y", proj);
    INFO("stderr: " << r.err);
    INFO("stdout: " << r.out);
    REQUIRE(r.exit_code == 0);

    // ezmk.lock records the git provenance: source = "git" + the pinned commit.
    fs::path lock = proj / "ezmk.lock";
    REQUIRE(fs::exists(lock));
    std::string lock_text = file_read(lock);
    REQUIRE(lock_text.find("source = \"git\"") != std::string::npos);
    REQUIRE(lock_text.find("commit = \"" + fx.sha_a + "\"") != std::string::npos);
}

TEST_CASE("integration: pkg install git URL — --locked re-clones the recorded commit (1.4.1)", "[integration][1.4.1]") {
    if (!ezmk_available() || !git_available()) {
        SKIP("ezmk binary or git not available");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    GitFixture fx;
    TempDir proj_tmp;
    fs::path proj = proj_tmp.path / "proj";
    fs::create_directories(proj / "src");
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
    file_write(proj / "src" / "main.cpp", "int main() { return 0; }\n");

    // First install from #v1.0 (records commit A in ezmk.lock).
    ProcResult r1 = run_ezmk("pkg install \"" + file_url(fx.repo) + "#v1.0\" -p -y", proj);
    REQUIRE(r1.exit_code == 0);
    REQUIRE(installed_version(proj) == "1.0.0");

    // --locked: matches the lockfile entry by source_url, re-clones at commit A.
    ProcResult r2 = run_ezmk(
        "pkg install \"" + file_url(fx.repo) + "\" --locked -p -y", proj);
    INFO("locked stderr: " << r2.err);
    INFO("locked stdout: " << r2.out);
    REQUIRE(r2.exit_code == 0);
    REQUIRE(installed_version(proj) == "1.0.0");   // still the pinned commit
}

TEST_CASE("integration: pkg install git URL — --locked rejects a tampered commit (1.4.1)", "[integration][1.4.1]") {
    if (!ezmk_available() || !git_available()) {
        SKIP("ezmk binary or git not available");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    GitFixture fx;
    TempDir proj_tmp;
    fs::path proj = proj_tmp.path / "proj";
    fs::create_directories(proj / "src");
    file_write(proj / "ezmk.toml",
        "[project]\nname = \"proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
    file_write(proj / "src" / "main.cpp", "int main() { return 0; }\n");

    ProcResult r1 = run_ezmk("pkg install \"" + file_url(fx.repo) + "#v1.0\" -p -y", proj);
    REQUIRE(r1.exit_code == 0);

    // Tamper: replace the recorded commit with a bogus SHA — simulates a
    // force-pushed branch/tag whose recorded commit no longer exists upstream.
    fs::path lock = proj / "ezmk.lock";
    std::string lock_text = file_read(lock);
    std::string bogus(40, '0');
    auto pos = lock_text.find("commit = \"" + fx.sha_a + "\"");
    REQUIRE(pos != std::string::npos);
    lock_text.replace(pos + 10, fx.sha_a.size(), bogus);
    file_write(lock, lock_text);

    ProcResult r2 = run_ezmk(
        "pkg install \"" + file_url(fx.repo) + "\" --locked -p -y", proj);
    INFO("locked stderr: " << r2.err);
    INFO("locked stdout: " << r2.out);
    REQUIRE(r2.exit_code != 0);
    REQUIRE((r2.out + r2.err).find("ezmk.lock") != std::string::npos);
}
