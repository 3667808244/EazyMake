// Unit tests for cli.cpp (argument parsing)
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace ezmk::cli;

// ===================================================================
// Helper: build argv from strings
// ===================================================================

struct TestArgs {
    std::vector<std::string> strings;
    std::vector<char*> argv;

    explicit TestArgs(std::initializer_list<std::string> args) {
        // argv[0] is always the program name
        strings.push_back("ezmk");
        for (auto& s : args) {
            strings.push_back(s);
        }
        for (auto& s : strings) {
            argv.push_back(s.data());
        }
    }

    int argc() const { return static_cast<int>(argv.size()); }
    char** argv_data() { return argv.data(); }

    CliArgs parse() { return ezmk::cli::parse(argc(), argv_data()); }
};

// ===================================================================
// Help / Version
// ===================================================================

TEST_CASE("cli parse: no arguments → Help", "[cli]") {
    TestArgs ta({});
    // We need to mock argc < 2
    const char* av0 = "ezmk";
    char* av[] = { const_cast<char*>(av0) };
    auto args = ezmk::cli::parse(1, av);
    REQUIRE(args.cmd == Command::Help);
}

TEST_CASE("cli parse: explicit --help", "[cli]") {
    auto args = TestArgs({"--help"}).parse();
    REQUIRE(args.cmd == Command::Help);
}

TEST_CASE("cli parse: -h shortcut", "[cli]") {
    auto args = TestArgs({"-h"}).parse();
    REQUIRE(args.cmd == Command::Help);
}

TEST_CASE("cli parse: help subcommand", "[cli]") {
    auto args = TestArgs({"help"}).parse();
    REQUIRE(args.cmd == Command::Help);
}

TEST_CASE("cli parse: --version", "[cli]") {
    auto args = TestArgs({"--version"}).parse();
    REQUIRE(args.cmd == Command::Version);
}

TEST_CASE("cli parse: -V shortcut", "[cli]") {
    auto args = TestArgs({"-V"}).parse();
    REQUIRE(args.cmd == Command::Version);
}

TEST_CASE("cli parse: version subcommand", "[cli]") {
    auto args = TestArgs({"version"}).parse();
    REQUIRE(args.cmd == Command::Version);
}

// ===================================================================
// project new
// ===================================================================

TEST_CASE("cli parse: project new basic", "[cli]") {
    auto args = TestArgs({"project", "new", "myapp"}).parse();
    REQUIRE(args.cmd == Command::ProjectNew);
    REQUIRE(args.project_name == "myapp");
    REQUIRE(args.project_type == "executable");
    REQUIRE(args.disable_git_init == false);
    REQUIRE(args.disable_gitignore == false);
}

TEST_CASE("cli parse: project new with --type static", "[cli]") {
    auto args = TestArgs({"project", "new", "mylib", "--type", "static"}).parse();
    REQUIRE(args.cmd == Command::ProjectNew);
    REQUIRE(args.project_name == "mylib");
    REQUIRE(args.project_type == "static");
}

TEST_CASE("cli parse: project new with --type shared", "[cli]") {
    auto args = TestArgs({"project", "new", "myshared", "--type", "shared"}).parse();
    REQUIRE(args.cmd == Command::ProjectNew);
    REQUIRE(args.project_type == "shared");
}

TEST_CASE("cli parse: project new with --disable-git-init", "[cli]") {
    auto args = TestArgs({"project", "new", "myapp", "--disable-git-init"}).parse();
    REQUIRE(args.cmd == Command::ProjectNew);
    REQUIRE(args.disable_git_init == true);
}

TEST_CASE("cli parse: project new with --disable-gitignore", "[cli]") {
    auto args = TestArgs({"project", "new", "myapp", "--disable-gitignore"}).parse();
    REQUIRE(args.cmd == Command::ProjectNew);
    REQUIRE(args.disable_gitignore == true);
}

TEST_CASE("cli parse: project new with both disable flags", "[cli]") {
    auto args = TestArgs({"project", "new", "myapp",
                          "--disable-git-init", "--disable-gitignore"}).parse();
    REQUIRE(args.disable_git_init == true);
    REQUIRE(args.disable_gitignore == true);
}

// ===================================================================
// project build / run / clean
// ===================================================================

TEST_CASE("cli parse: project build", "[cli]") {
    auto args = TestArgs({"project", "build"}).parse();
    REQUIRE(args.cmd == Command::ProjectBuild);
    REQUIRE(args.build_opts.disable_cache == false);
    REQUIRE(args.build_opts.verbose == false);
}

TEST_CASE("cli parse: project build --disable-cache", "[cli]") {
    auto args = TestArgs({"project", "build", "--disable-cache"}).parse();
    REQUIRE(args.cmd == Command::ProjectBuild);
    REQUIRE(args.build_opts.disable_cache == true);
}

TEST_CASE("cli parse: project build --verbose", "[cli]") {
    auto args = TestArgs({"project", "build", "--verbose"}).parse();
    REQUIRE(args.build_opts.verbose == true);
}

TEST_CASE("cli parse: project build -v", "[cli]") {
    auto args = TestArgs({"project", "build", "-v"}).parse();
    REQUIRE(args.build_opts.verbose == true);
}

TEST_CASE("cli parse: project build with both flags", "[cli]") {
    auto args = TestArgs({"project", "build", "--disable-cache", "-v"}).parse();
    REQUIRE(args.build_opts.disable_cache == true);
    REQUIRE(args.build_opts.verbose == true);
}

TEST_CASE("cli parse: project run", "[cli]") {
    auto args = TestArgs({"project", "run"}).parse();
    REQUIRE(args.cmd == Command::ProjectRun);
}

TEST_CASE("cli parse: project run with flags", "[cli]") {
    auto args = TestArgs({"project", "run", "--disable-cache", "--verbose"}).parse();
    REQUIRE(args.cmd == Command::ProjectRun);
    REQUIRE(args.build_opts.disable_cache == true);
    REQUIRE(args.build_opts.verbose == true);
}

TEST_CASE("cli parse: project clean", "[cli]") {
    auto args = TestArgs({"project", "clean"}).parse();
    REQUIRE(args.cmd == Command::ProjectClean);
}

// 1.2.0-dev.11: project clean previously swallowed unknown options/positionals.
TEST_CASE("cli parse: project clean rejects unknown options and positionals", "[cli][1.2.0-dev.11]") {
    REQUIRE_THROWS_AS(TestArgs({"project", "clean", "--bogus"}).parse(),
                      ezmk::fatal_error);
    REQUIRE_THROWS_AS(TestArgs({"project", "clean", "extra"}).parse(),
                      ezmk::fatal_error);
}

// ===================================================================
// pkg install
// ===================================================================

TEST_CASE("cli parse: pkg install basic", "[cli]") {
    auto args = TestArgs({"pkg", "install", "./foo.zip"}).parse();
    REQUIRE(args.cmd == Command::PkgInstall);
    REQUIRE(args.install_opts.has_value());
    REQUIRE(args.install_opts->pkg_file == "./foo.zip");
    REQUIRE(args.install_opts->scope == Scope::Project); // default
}

TEST_CASE("cli parse: pkg install -u", "[cli]") {
    auto args = TestArgs({"pkg", "install", "-u", "./foo.zip"}).parse();
    REQUIRE(args.cmd == Command::PkgInstall);
    REQUIRE(args.install_opts->scope == Scope::User);
}

TEST_CASE("cli parse: pkg install -g", "[cli]") {
    auto args = TestArgs({"pkg", "install", "-g", "./foo.zip"}).parse();
    REQUIRE(args.install_opts->scope == Scope::Global);
}

TEST_CASE("cli parse: pkg install --sha256", "[cli]") {
    // 1.2.0-dev.11: --sha256 values are validated (64 hex chars) at parse time.
    std::string valid_sha(64, 'a');
    auto args = TestArgs({"pkg", "install", "./foo.zip",
                          "--sha256", valid_sha}).parse();
    REQUIRE(args.install_opts.has_value());
    REQUIRE(args.install_opts->sha256 == valid_sha);
}

TEST_CASE("cli parse: pkg install rejects malformed --sha256", "[cli][1.2.0-dev.11]") {
    REQUIRE_THROWS_AS(
        TestArgs({"pkg", "install", "./foo.zip", "--sha256", "abc123"}).parse(),
        ezmk::fatal_error
    );
    REQUIRE_THROWS_AS(
        TestArgs({"pkg", "install", "./foo.zip",
                  "--sha256", std::string(64, 'g')}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: pkg install rejects --locked + --no-lock together", "[cli][1.2.0-dev.11]") {
    REQUIRE_THROWS_AS(
        TestArgs({"pkg", "install", "foo.zip", "--locked", "--no-lock"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: pkg install -y", "[cli]") {
    auto args = TestArgs({"pkg", "install", "-y", "./foo.zip"}).parse();
    REQUIRE(args.install_opts->assume_yes == true);
}

TEST_CASE("cli parse: pkg install --yes", "[cli]") {
    auto args = TestArgs({"pkg", "install", "--yes", "./foo.zip"}).parse();
    REQUIRE(args.install_opts->assume_yes == true);
}

TEST_CASE("cli parse: pkg install URL", "[cli]") {
    auto args = TestArgs({"pkg", "install",
                          "https://example.com/packages/foo.zip"}).parse();
    REQUIRE(args.cmd == Command::PkgInstall);
    REQUIRE(args.install_opts->pkg_file == "https://example.com/packages/foo.zip");
}

TEST_CASE("cli parse: pkg install by name (repo search)", "[cli]") {
    auto args = TestArgs({"pkg", "install", "foo"}).parse();
    REQUIRE(args.cmd == Command::PkgInstall);
    REQUIRE(args.install_opts->pkg_file == "foo");
}

// ===================================================================
// pkg remove / search / info
// ===================================================================

TEST_CASE("cli parse: pkg remove", "[cli]") {
    auto args = TestArgs({"pkg", "remove", "foo"}).parse();
    REQUIRE(args.cmd == Command::PkgRemove);
    REQUIRE(args.query_opts.has_value());
    REQUIRE(args.query_opts->pkg_name == "foo");
    REQUIRE(args.query_opts->scopes.size() == 3); // default: -pug
}

TEST_CASE("cli parse: pkg remove -p", "[cli]") {
    auto args = TestArgs({"pkg", "remove", "-p", "foo"}).parse();
    REQUIRE(args.query_opts->scopes.size() == 1);
    REQUIRE(args.query_opts->scopes[0] == Scope::Project);
}

TEST_CASE("cli parse: pkg search", "[cli]") {
    auto args = TestArgs({"pkg", "search", "foo"}).parse();
    REQUIRE(args.cmd == Command::PkgSearch);
    REQUIRE(args.query_opts->pkg_name == "foo");
}

TEST_CASE("cli parse: pkg search -pug", "[cli]") {
    auto args = TestArgs({"pkg", "search", "-pug", "foo"}).parse();
    REQUIRE(args.query_opts->scopes.size() == 3);
}

TEST_CASE("cli parse: pkg search -pu", "[cli]") {
    auto args = TestArgs({"pkg", "search", "-pu", "foo"}).parse();
    REQUIRE(args.query_opts->scopes.size() == 2);
    REQUIRE(args.query_opts->scopes[0] == Scope::Project);
    REQUIRE(args.query_opts->scopes[1] == Scope::User);
}

TEST_CASE("cli parse: pkg info", "[cli]") {
    auto args = TestArgs({"pkg", "info", "foo"}).parse();
    REQUIRE(args.cmd == Command::PkgInfo);
    REQUIRE(args.query_opts->pkg_name == "foo");
}

TEST_CASE("cli parse: pkg info -g", "[cli]") {
    auto args = TestArgs({"pkg", "info", "-g", "foo"}).parse();
    REQUIRE(args.query_opts->scopes.size() == 1);
    REQUIRE(args.query_opts->scopes[0] == Scope::Global);
}

// ===================================================================
// repo add
// ===================================================================

TEST_CASE("cli parse: repo add basic", "[cli]") {
    auto args = TestArgs({"repo", "add",
                          "https://github.com/user/repo.git"}).parse();
    REQUIRE(args.cmd == Command::RepoAdd);
    REQUIRE(args.repo_opts.url == "https://github.com/user/repo.git");
    REQUIRE(args.repo_opts.scopes.size() == 1);
    REQUIRE(args.repo_opts.scopes[0] == Scope::Project); // default
}

TEST_CASE("cli parse: repo add with --name", "[cli]") {
    auto args = TestArgs({"repo", "add",
                          "https://github.com/user/repo.git",
                          "--name", "myrepo"}).parse();
    REQUIRE(args.repo_opts.name == "myrepo");
}

TEST_CASE("cli parse: repo add with --branch", "[cli]") {
    auto args = TestArgs({"repo", "add",
                          "https://github.com/user/repo.git",
                          "--branch", "stable"}).parse();
    REQUIRE(args.repo_opts.branch == "stable");
}

TEST_CASE("cli parse: repo add -u", "[cli]") {
    auto args = TestArgs({"repo", "add", "-u",
                          "https://github.com/user/repo.git"}).parse();
    REQUIRE(args.repo_opts.scopes[0] == Scope::User);
}

TEST_CASE("cli parse: repo add -g with all options", "[cli]") {
    auto args = TestArgs({"repo", "add", "-g",
                          "https://github.com/user/repo.git",
                          "--name", "global-repo",
                          "--branch", "develop"}).parse();
    REQUIRE(args.repo_opts.scopes[0] == Scope::Global);
    REQUIRE(args.repo_opts.name == "global-repo");
    REQUIRE(args.repo_opts.branch == "develop");
}

TEST_CASE("cli parse: repo add local path", "[cli]") {
    auto args = TestArgs({"repo", "add", "E:/packages/my-repo"}).parse();
    REQUIRE(args.cmd == Command::RepoAdd);
    REQUIRE(args.repo_opts.url == "E:/packages/my-repo");
}

// ===================================================================
// repo remove / update
// ===================================================================

TEST_CASE("cli parse: repo remove", "[cli]") {
    auto args = TestArgs({"repo", "remove", "myrepo"}).parse();
    REQUIRE(args.cmd == Command::RepoRemove);
    REQUIRE(args.repo_opts.name == "myrepo");
    REQUIRE(args.repo_opts.scopes.size() == 3); // default: -pug
}

TEST_CASE("cli parse: repo remove -p", "[cli]") {
    auto args = TestArgs({"repo", "remove", "-p", "myrepo"}).parse();
    REQUIRE(args.repo_opts.scopes.size() == 1);
    REQUIRE(args.repo_opts.scopes[0] == Scope::Project);
}

TEST_CASE("cli parse: repo update all", "[cli]") {
    auto args = TestArgs({"repo", "update"}).parse();
    REQUIRE(args.cmd == Command::RepoUpdate);
    REQUIRE(args.repo_opts.name.empty());
    REQUIRE(args.repo_opts.scopes.size() == 3);
}

TEST_CASE("cli parse: repo update by name", "[cli]") {
    auto args = TestArgs({"repo", "update", "myrepo"}).parse();
    REQUIRE(args.repo_opts.name == "myrepo");
}

TEST_CASE("cli parse: repo update -g", "[cli]") {
    auto args = TestArgs({"repo", "update", "-g", "myrepo"}).parse();
    REQUIRE(args.repo_opts.scopes.size() == 1);
    REQUIRE(args.repo_opts.scopes[0] == Scope::Global);
}

// ===================================================================
// repo list
// ===================================================================

TEST_CASE("cli parse: repo list default", "[cli]") {
    auto args = TestArgs({"repo", "list"}).parse();
    REQUIRE(args.cmd == Command::RepoList);
    REQUIRE(args.repo_opts.scopes.size() == 3); // default: -pug
}

TEST_CASE("cli parse: repo list -pu", "[cli]") {
    auto args = TestArgs({"repo", "list", "-pu"}).parse();
    REQUIRE(args.repo_opts.scopes.size() == 2);
    REQUIRE(args.repo_opts.scopes[0] == Scope::Project);
    REQUIRE(args.repo_opts.scopes[1] == Scope::User);
}

TEST_CASE("cli parse: repo list -g", "[cli]") {
    auto args = TestArgs({"repo", "list", "-g"}).parse();
    REQUIRE(args.repo_opts.scopes.size() == 1);
    REQUIRE(args.repo_opts.scopes[0] == Scope::Global);
}

// ===================================================================
// utils
// ===================================================================

TEST_CASE("cli parse: utils basic", "[cli]") {
    auto args = TestArgs({"utils", "cc"}).parse();
    REQUIRE(args.cmd == Command::Utils);
    REQUIRE(args.utils_name == "cc");
    REQUIRE(args.utils_args.empty());
}

TEST_CASE("cli parse: utils with arguments", "[cli]") {
    auto args = TestArgs({"utils", "cc", "-o", "custom.json"}).parse();
    REQUIRE(args.cmd == Command::Utils);
    REQUIRE(args.utils_name == "cc");
    REQUIRE(args.utils_args.size() == 2);
    REQUIRE(args.utils_args[0] == "-o");
    REQUIRE(args.utils_args[1] == "custom.json");
}

TEST_CASE("cli parse: utils with no subcommand throws", "[cli]") {
    // "ezmk utils" without a subcommand should throw (requires a util name)
    REQUIRE_THROWS_AS(
        TestArgs({"utils"}).parse(),
        ezmk::fatal_error
    );
}

// ===================================================================
// unknown commands
// ===================================================================

TEST_CASE("cli parse: unknown command group", "[cli]") {
    // "ezmk unknown_cmd sub" → falls through to help
    // But the function calls util::error which we can't easily test without
    // redirecting stderr. We just verify the returned struct.
    // The code sets cmd = Help for unknown groups
    char arg0[] = "ezmk";
    char arg1[] = "unknown_group";
    char arg2[] = "sub";
    char* av[] = { arg0, arg1, arg2 };
    auto args = ezmk::cli::parse(3, av);
    REQUIRE(args.cmd == Command::Help);
}

// ===================================================================
// print_usage
// ===================================================================

TEST_CASE("print_usage: does not throw", "[cli]") {
    // Redirect stdout to suppress output during test
    REQUIRE_NOTHROW(print_usage());
}

// ===================================================================
// Edge cases
// ===================================================================

TEST_CASE("cli parse: project new with --type missing value should throw", "[cli]") {
    // --type requires a value; if missing, fatal() throws fatal_error
    REQUIRE_THROWS_AS(
        TestArgs({"project", "new", "myapp", "--type"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: pkg install with no file should throw", "[cli]") {
    REQUIRE_THROWS_AS(
        TestArgs({"pkg", "install"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: project new with no name should throw", "[cli]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "new"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: pkg remove with no name should throw", "[cli]") {
    REQUIRE_THROWS_AS(
        TestArgs({"pkg", "remove"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: repo add with no URL should throw", "[cli]") {
    REQUIRE_THROWS_AS(
        TestArgs({"repo", "add"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: repo remove with no name should throw", "[cli]") {
    REQUIRE_THROWS_AS(
        TestArgs({"repo", "remove"}).parse(),
        ezmk::fatal_error
    );
}

// ===================================================================
// 0.2.5 — GNU option syntax (argparse layer)
// ===================================================================

TEST_CASE("cli parse: long option --flag=value equals --flag value", "[cli][gnu]") {
    auto a = TestArgs({"project", "build", "--profile=debug"}).parse();
    auto b = TestArgs({"project", "build", "--profile", "debug"}).parse();
    REQUIRE(a.build_opts.profile == "debug");
    REQUIRE(b.build_opts.profile == "debug");
}

TEST_CASE("cli parse: short option attached value -j4 equals -j 4", "[cli][gnu]") {
    auto a = TestArgs({"project", "build", "-j4"}).parse();
    auto b = TestArgs({"project", "build", "-j", "4"}).parse();
    REQUIRE(a.build_opts.jobs == 4);
    REQUIRE(b.build_opts.jobs == 4);
}

TEST_CASE("cli parse: --jobs=8 long attached value", "[cli][gnu]") {
    auto a = TestArgs({"project", "build", "--jobs=8"}).parse();
    REQUIRE(a.build_opts.jobs == 8);
}

TEST_CASE("cli parse: grouped scope flags -pug split", "[cli][gnu]") {
    auto args = TestArgs({"pkg", "search", "-pug", "foo"}).parse();
    REQUIRE(args.query_opts->scopes.size() == 3);
    REQUIRE(args.query_opts->scopes[0] == Scope::Project);
    REQUIRE(args.query_opts->scopes[1] == Scope::User);
    REQUIRE(args.query_opts->scopes[2] == Scope::Global);
}

TEST_CASE("cli parse: grouped short with trailing value -vj4", "[cli][gnu]") {
    auto args = TestArgs({"project", "build", "-vj4"}).parse();
    REQUIRE(args.build_opts.verbose == true);
    REQUIRE(args.build_opts.jobs == 4);
}

TEST_CASE("cli parse: options and positionals interleave", "[cli][gnu]") {
    auto a = TestArgs({"pkg", "install", "-g", "foo.zip"}).parse();
    auto b = TestArgs({"pkg", "install", "foo.zip", "-g"}).parse();
    REQUIRE(a.install_opts->pkg_file == "foo.zip");
    REQUIRE(a.install_opts->scope == Scope::Global);
    REQUIRE(b.install_opts->pkg_file == "foo.zip");
    REQUIRE(b.install_opts->scope == Scope::Global);
}

TEST_CASE("cli parse: -- forwards args to utils tool", "[cli][gnu]") {
    auto args = TestArgs({"utils", "fmt", "--", "--help"}).parse();
    REQUIRE(args.cmd == Command::Utils);
    REQUIRE(args.utils_name == "fmt");
    REQUIRE(args.utils_args.size() == 1);
    REQUIRE(args.utils_args[0] == "--help");
}

TEST_CASE("cli parse: -- forwards args to project run", "[cli][gnu]") {
    auto args = TestArgs({"project", "run", "--", "--verbose", "input.txt"}).parse();
    REQUIRE(args.cmd == Command::ProjectRun);
    REQUIRE(args.program_args.size() == 2);
    REQUIRE(args.program_args[0] == "--verbose");
    REQUIRE(args.program_args[1] == "input.txt");
}

TEST_CASE("cli parse: lone dash is a positional", "[cli][gnu]") {
    auto args = TestArgs({"pkg", "install", "-"}).parse();
    REQUIRE(args.install_opts->pkg_file == "-");
}

TEST_CASE("cli parse: unknown long option throws", "[cli][gnu]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "build", "--nonsense"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: unknown short option in group throws", "[cli][gnu]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "build", "-xj4"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: missing value for long option throws", "[cli][gnu]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "build", "--profile"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: pkg install rejects combined scope flags", "[cli][gnu]") {
    REQUIRE_THROWS_AS(
        TestArgs({"pkg", "install", "-pu", "foo.zip"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: value beginning with dash via = form", "[cli][gnu]") {
    auto args = TestArgs({"project", "cc", "--output=-custom.json"}).parse();
    REQUIRE(args.project_cc_opts->output == "-custom.json");
}

TEST_CASE("cli parse: invalid -j value throws", "[cli][gnu]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "build", "-jabc"}).parse(),
        ezmk::fatal_error
    );
}

// ===================================================================
// 0.2.5 — repo info CLI
// ===================================================================

TEST_CASE("cli parse: repo info basic", "[cli][gnu]") {
    auto args = TestArgs({"repo", "info", "myrepo"}).parse();
    REQUIRE(args.cmd == Command::RepoInfo);
    REQUIRE(args.repo_opts.name == "myrepo");
    REQUIRE(args.repo_opts.scopes.size() == 3); // default: -pug
}

TEST_CASE("cli parse: repo info -p", "[cli][gnu]") {
    auto args = TestArgs({"repo", "info", "-p", "myrepo"}).parse();
    REQUIRE(args.repo_opts.scopes.size() == 1);
    REQUIRE(args.repo_opts.scopes[0] == Scope::Project);
}

TEST_CASE("cli parse: repo info -g", "[cli][gnu]") {
    auto args = TestArgs({"repo", "info", "-g", "myrepo"}).parse();
    REQUIRE(args.repo_opts.scopes.size() == 1);
    REQUIRE(args.repo_opts.scopes[0] == Scope::Global);
}

TEST_CASE("cli parse: repo info with no name throws", "[cli][gnu]") {
    REQUIRE_THROWS_AS(
        TestArgs({"repo", "info"}).parse(),
        ezmk::fatal_error
    );
}

// ===================================================================
// 0.2.5 — --auto-update flag
// ===================================================================

TEST_CASE("cli parse: project build --auto-update", "[cli][gnu]") {
    auto args = TestArgs({"project", "build", "--auto-update"}).parse();
    REQUIRE(args.cmd == Command::ProjectBuild);
    REQUIRE(args.build_opts.auto_update == true);
}

TEST_CASE("cli parse: project build without --auto-update defaults false", "[cli][gnu]") {
    auto args = TestArgs({"project", "build"}).parse();
    REQUIRE(args.build_opts.auto_update == false);
}

// ===================================================================
// 0.2.6 — command shorthands (aliases)
// ===================================================================

TEST_CASE("cli parse: project aliases expand", "[cli][0.2.6][alias]") {
    REQUIRE(TestArgs({"pn", "myapp"}).parse().cmd == Command::ProjectNew);
    REQUIRE(TestArgs({"pn", "myapp"}).parse().project_name.value() == "myapp");
    REQUIRE(TestArgs({"pb"}).parse().cmd == Command::ProjectBuild);
    REQUIRE(TestArgs({"pr"}).parse().cmd == Command::ProjectRun);
    REQUIRE(TestArgs({"pc"}).parse().cmd == Command::ProjectClean);
    REQUIRE(TestArgs({"pw"}).parse().cmd == Command::ProjectWatch);
}

TEST_CASE("cli parse: pkg aliases expand", "[cli][0.2.6][alias]") {
    REQUIRE(TestArgs({"ki", "foo.zip"}).parse().cmd == Command::PkgInstall);
    REQUIRE(TestArgs({"kr", "foo"}).parse().cmd == Command::PkgRemove);
    REQUIRE(TestArgs({"ks", "foo"}).parse().cmd == Command::PkgSearch);
    REQUIRE(TestArgs({"kn", "foo"}).parse().cmd == Command::PkgInfo);
    REQUIRE(TestArgs({"kl"}).parse().cmd == Command::PkgList);
    REQUIRE(TestArgs({"ku", "foo"}).parse().cmd == Command::PkgUpdate);
}

TEST_CASE("cli parse: repo aliases expand", "[cli][0.2.6][alias]") {
    REQUIRE(TestArgs({"ra", "http://x/y.git"}).parse().cmd == Command::RepoAdd);
    REQUIRE(TestArgs({"rr", "foo"}).parse().cmd == Command::RepoRemove);
    REQUIRE(TestArgs({"rl"}).parse().cmd == Command::RepoList);
    REQUIRE(TestArgs({"ru"}).parse().cmd == Command::RepoUpdate);
    REQUIRE(TestArgs({"ri", "foo"}).parse().cmd == Command::RepoInfo);
}

TEST_CASE("cli parse: single-letter aliases u/h/v", "[cli][0.2.6][alias]") {
    auto u = TestArgs({"u", "fmt"}).parse();
    REQUIRE(u.cmd == Command::Utils);
    REQUIRE(u.utils_name == "fmt");
    REQUIRE(TestArgs({"h"}).parse().cmd == Command::Help);
    REQUIRE(TestArgs({"v"}).parse().cmd == Command::Version);
}

TEST_CASE("cli parse: alias preserves flags and positionals", "[cli][0.2.6][alias]") {
    auto args = TestArgs({"ki", "-g", "foo.zip"}).parse();
    REQUIRE(args.cmd == Command::PkgInstall);
    REQUIRE(args.install_opts->pkg_file == "foo.zip");
    REQUIRE(args.install_opts->scope == Scope::Global);
}

TEST_CASE("cli parse: alias with missing arg still errors", "[cli][0.2.6][alias]") {
    // `pn` expands to `project new`, which requires a project name.
    REQUIRE_THROWS_AS(TestArgs({"pn"}).parse(), ezmk::fatal_error);
}

TEST_CASE("cli parse: alias only applies at command position", "[cli][0.2.6][alias]") {
    // `project pn` is NOT an alias — pn is an unknown project subcommand.
    REQUIRE_THROWS_AS(TestArgs({"project", "pn"}).parse(), ezmk::fatal_error);
}

// ===================================================================
// 1.3.3 — workspace two-letter shorthands (wl/wb/wt/wc)
// ===================================================================

TEST_CASE("cli parse: workspace aliases expand (1.3.3)", "[cli][1.3.3][alias]") {
    REQUIRE(TestArgs({"wl"}).parse().cmd == Command::WorkspaceList);
    REQUIRE(TestArgs({"wb"}).parse().cmd == Command::WorkspaceBuild);
    REQUIRE(TestArgs({"wt"}).parse().cmd == Command::WorkspaceTest);
    REQUIRE(TestArgs({"wc"}).parse().cmd == Command::WorkspaceClean);
}

TEST_CASE("cli parse: workspace alias preserves flags (1.3.3)", "[cli][1.3.3][alias]") {
    // `wt -w` ≡ `workspace test -w` (the -w token is accepted+ignored by the
    // workspace spec); `wb --member x -j 2` forwards member/jobs options.
    auto args = TestArgs({"wt", "-w", "--report", "junit"}).parse();
    REQUIRE(args.cmd == Command::WorkspaceTest);
    REQUIRE(args.workspace_opts.has_value());
    REQUIRE(args.workspace_opts->test_report == "junit");

    auto b = TestArgs({"wb", "--member", "libs/a", "-j", "2"}).parse();
    REQUIRE(b.cmd == Command::WorkspaceBuild);
    REQUIRE(b.workspace_opts.has_value());
    REQUIRE(b.workspace_opts->members.size() == 1);
    REQUIRE(b.workspace_opts->members[0] == "libs/a");
    REQUIRE(b.workspace_opts->jobs == 2);
}

TEST_CASE("cli parse: workspace alias only at command position (1.3.3)", "[cli][1.3.3][alias]") {
    // `workspace wb` is NOT an alias — wb is an unknown workspace subcommand.
    REQUIRE_THROWS_AS(TestArgs({"workspace", "wb"}).parse(), ezmk::fatal_error);
    REQUIRE_THROWS_AS(TestArgs({"workspace", "wl"}).parse(), ezmk::fatal_error);
}

TEST_CASE("cli parse: --verbose records workspace alias expansion (1.3.3)", "[cli][1.3.3][alias]") {
    auto args = TestArgs({"wb", "-v"}).parse();
    REQUIRE(args.cmd == Command::WorkspaceBuild);
    REQUIRE(args.shorthand_expansion == "wb → workspace build");
    auto l = TestArgs({"wl", "--verbose"}).parse();
    REQUIRE(l.cmd == Command::WorkspaceList);
    REQUIRE(l.shorthand_expansion == "wl → workspace list");
}

// ===================================================================
// 0.2.6 — --color=<mode> global option
// ===================================================================

namespace {
// Restore color mode to Auto after each color test to avoid cross-test leakage.
struct ColorModeGuard {
    ~ColorModeGuard() { ezmk::util::set_color_mode(ezmk::util::ColorMode::Auto); }
};
}

TEST_CASE("cli parse: --color=<mode> sets color mode", "[cli][0.2.6][color]") {
    ColorModeGuard g;
    TestArgs({"--color=always", "help"}).parse();
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Always);
    TestArgs({"--color=never", "help"}).parse();
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Never);
    TestArgs({"--color=auto", "help"}).parse();
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Auto);
}

TEST_CASE("cli parse: --color aliases enable/disable/default", "[cli][0.2.6][color]") {
    ColorModeGuard g;
    TestArgs({"--color=enable", "help"}).parse();
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Always);
    TestArgs({"--color=disable", "help"}).parse();
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Never);
    TestArgs({"--color=default", "help"}).parse();
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Auto);
}

TEST_CASE("cli parse: --color is case-insensitive", "[cli][0.2.6][color]") {
    ColorModeGuard g;
    TestArgs({"--color=ALWAYS", "help"}).parse();
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Always);
}

TEST_CASE("cli parse: --color <mode> separate value form", "[cli][0.2.6][color]") {
    ColorModeGuard g;
    TestArgs({"--color", "never", "help"}).parse();
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Never);
}

TEST_CASE("cli parse: invalid --color value throws", "[cli][0.2.6][color]") {
    ColorModeGuard g;
    REQUIRE_THROWS_AS(TestArgs({"--color=bogus", "help"}).parse(), ezmk::fatal_error);
}

TEST_CASE("cli parse: --color mid-subcommand is stripped, rest parses", "[cli][0.2.6][color]") {
    ColorModeGuard g;
    auto args = TestArgs({"pkg", "install", "--color=never", "foo.zip"}).parse();
    REQUIRE(args.cmd == Command::PkgInstall);
    REQUIRE(args.install_opts->pkg_file == "foo.zip");
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Never);
}

TEST_CASE("cli parse: --color after -- is NOT consumed (utils passthrough)", "[cli][0.2.6][color]") {
    ColorModeGuard g;
    ezmk::util::set_color_mode(ezmk::util::ColorMode::Auto);
    auto args = TestArgs({"utils", "fmt", "--", "--color=never"}).parse();
    REQUIRE(args.cmd == Command::Utils);
    REQUIRE(args.utils_args.size() == 1);
    REQUIRE(args.utils_args[0] == "--color=never");
    // Color mode untouched because the token was after "--".
    REQUIRE(ezmk::util::get_color_mode() == ezmk::util::ColorMode::Auto);
}

TEST_CASE("cli parse: project run --auto-update", "[cli][gnu]") {
    auto args = TestArgs({"project", "run", "--auto-update"}).parse();
    REQUIRE(args.cmd == Command::ProjectRun);
    REQUIRE(args.build_opts.auto_update == true);
}

TEST_CASE("cli parse: project watch --auto-update", "[cli][gnu]") {
    auto args = TestArgs({"project", "watch", "--auto-update"}).parse();
    REQUIRE(args.cmd == Command::ProjectWatch);
    REQUIRE(args.build_opts.auto_update == true);
}

// ===================================================================
// 1.2.0 — project cc (generate compile_commands.json)
// ===================================================================

TEST_CASE("cli parse: project cc basic", "[cli][1.2.0]") {
    auto args = TestArgs({"project", "cc"}).parse();
    REQUIRE(args.cmd == Command::ProjectCc);
    REQUIRE(args.project_cc_opts.has_value());
    REQUIRE(args.project_cc_opts->output.empty());
    REQUIRE(args.project_cc_opts->profile.empty());
}

TEST_CASE("cli parse: project cc -o custom.json", "[cli][1.2.0]") {
    auto args = TestArgs({"project", "cc", "-o", "custom.json"}).parse();
    REQUIRE(args.cmd == Command::ProjectCc);
    REQUIRE(args.project_cc_opts->output == "custom.json");
}

TEST_CASE("cli parse: project cc --output custom.json", "[cli][1.2.0]") {
    auto args = TestArgs({"project", "cc", "--output", "custom.json"}).parse();
    REQUIRE(args.cmd == Command::ProjectCc);
    REQUIRE(args.project_cc_opts->output == "custom.json");
}

TEST_CASE("cli parse: project cc --output=custom.json", "[cli][1.2.0][gnu]") {
    auto args = TestArgs({"project", "cc", "--output=custom.json"}).parse();
    REQUIRE(args.cmd == Command::ProjectCc);
    REQUIRE(args.project_cc_opts->output == "custom.json");
}

TEST_CASE("cli parse: project cc --profile debug", "[cli][1.2.0]") {
    auto args = TestArgs({"project", "cc", "--profile", "debug"}).parse();
    REQUIRE(args.cmd == Command::ProjectCc);
    REQUIRE(args.project_cc_opts->profile == "debug");
}

TEST_CASE("cli parse: project cc --profile=debug", "[cli][1.2.0][gnu]") {
    auto args = TestArgs({"project", "cc", "--profile=debug"}).parse();
    REQUIRE(args.cmd == Command::ProjectCc);
    REQUIRE(args.project_cc_opts->profile == "debug");
}

TEST_CASE("cli parse: project cc rejects positionals", "[cli][1.2.0]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "cc", "foo.json"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: project cc missing -o value throws", "[cli][1.2.0][gnu]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "cc", "-o"}).parse(),
        ezmk::fatal_error
    );
}

// ===================================================================
// 1.2.0 — project export <target>
// ===================================================================

TEST_CASE("cli parse: project export cmake basic", "[cli][1.2.0]") {
    auto args = TestArgs({"project", "export", "cmake"}).parse();
    REQUIRE(args.cmd == Command::ProjectExport);
    REQUIRE(args.project_export_opts.has_value());
    REQUIRE(args.project_export_opts->target == "cmake");
    REQUIRE(args.project_export_opts->output.empty());
    REQUIRE(args.project_export_opts->overwrite == false);
    REQUIRE(args.project_export_opts->use_glob == true);
}

TEST_CASE("cli parse: project export cmake flags", "[cli][1.2.0]") {
    auto args = TestArgs({"project", "export", "cmake",
                          "-o", "build/CMakeLists.txt",
                          "--overwrite", "--resolve", "--no-glob",
                          "--profile", "debug"}).parse();
    REQUIRE(args.cmd == Command::ProjectExport);
    auto& o = *args.project_export_opts;
    REQUIRE(o.target == "cmake");
    REQUIRE(o.output == "build/CMakeLists.txt");
    REQUIRE(o.overwrite == true);
    REQUIRE(o.resolve == true);
    REQUIRE(o.use_glob == false);
    REQUIRE(o.profile == "debug");
}

TEST_CASE("cli parse: project export --glob explicit", "[cli][1.2.0]") {
    auto args = TestArgs({"project", "export", "cmake", "--glob"}).parse();
    REQUIRE(args.project_export_opts->use_glob == true);
}

TEST_CASE("cli parse: project export unknown target throws", "[cli][1.2.0]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "export", "make"}).parse(),
        ezmk::fatal_error
    );
}

TEST_CASE("cli parse: project export with no target throws", "[cli][1.2.0]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "export"}).parse(),
        ezmk::fatal_error
    );
}

// ===================================================================
// 1.2.0-dev.12 — project test options
// ===================================================================

TEST_CASE("cli parse: project test --profile debug", "[cli][1.2.0-dev.12]") {
    auto args = TestArgs({"project", "test", "--profile", "debug"}).parse();
    REQUIRE(args.cmd == Command::ProjectTest);
    REQUIRE(args.test_profile == "debug");
}

TEST_CASE("cli parse: project test --profile=debug", "[cli][1.2.0-dev.12]") {
    auto args = TestArgs({"project", "test", "--profile=release"}).parse();
    REQUIRE(args.cmd == Command::ProjectTest);
    REQUIRE(args.test_profile == "release");
}

TEST_CASE("cli parse: project test without --profile keeps empty", "[cli][1.2.0-dev.12]") {
    auto args = TestArgs({"project", "test", "--framework", "ezmk"}).parse();
    REQUIRE(args.cmd == Command::ProjectTest);
    REQUIRE(args.test_profile.empty());
}

// 1.2.0-dev.11: -v is accepted as an alias for -V (verbose) on project test.
TEST_CASE("cli parse: project test -v is verbose alias", "[cli][1.2.0-dev.11]") {
    auto args = TestArgs({"project", "test", "-v"}).parse();
    REQUIRE(args.cmd == Command::ProjectTest);
    REQUIRE(args.test_verbose == true);
    auto args2 = TestArgs({"project", "test", "-V"}).parse();
    REQUIRE(args2.test_verbose == true);
}

TEST_CASE("cli parse: project test missing --profile value throws", "[cli][1.2.0-dev.12]") {
    REQUIRE_THROWS_AS(
        TestArgs({"project", "test", "--profile"}).parse(),
        ezmk::fatal_error
    );
}

// ===================================================================
// 1.3.2 — project test --report
// ===================================================================

TEST_CASE("cli parse: project test --report junit", "[cli][1.3.2]") {
    auto args = TestArgs({"project", "test", "--report", "junit"}).parse();
    REQUIRE(args.cmd == Command::ProjectTest);
    REQUIRE(args.test_report == "junit");
}

TEST_CASE("cli parse: project test --report junit:custom path", "[cli][1.3.2]") {
    auto args = TestArgs({"project", "test", "--report", "junit:out/custom.xml"}).parse();
    REQUIRE(args.cmd == Command::ProjectTest);
    REQUIRE(args.test_report == "junit:out/custom.xml");
}

TEST_CASE("cli parse: project test --report empty format throws", "[cli][1.3.2]") {
    // ":out.xml" has an empty format segment; whitespace-only is invalid too.
    REQUIRE_THROWS_AS(
        TestArgs({"project", "test", "--report", ":out.xml"}).parse(),
        ezmk::fatal_error);
    REQUIRE_THROWS_AS(
        TestArgs({"project", "test", "--report", " "}).parse(),
        ezmk::fatal_error);
}

TEST_CASE("cli parse: project test -w --report → WorkspaceTest passthrough", "[cli][1.3.2]") {
    auto args = TestArgs({"project", "test", "-w", "--report", "junit"}).parse();
    REQUIRE(args.cmd == Command::WorkspaceTest);
    REQUIRE(args.workspace_opts.has_value());
    REQUIRE(args.workspace_opts->test_report == "junit");
}

TEST_CASE("cli parse: workspace test --report junit", "[cli][1.3.2]") {
    auto args = TestArgs({"workspace", "test", "--report", "junit"}).parse();
    REQUIRE(args.cmd == Command::WorkspaceTest);
    REQUIRE(args.workspace_opts.has_value());
    REQUIRE(args.workspace_opts->test_report == "junit");
}

TEST_CASE("cli parse: --report rejected on non-test workspace commands", "[cli][1.3.2]") {
    REQUIRE_THROWS_AS(
        TestArgs({"workspace", "build", "--report", "junit"}).parse(),
        ezmk::fatal_error);
    REQUIRE_THROWS_AS(
        TestArgs({"workspace", "clean", "--report", "junit"}).parse(),
        ezmk::fatal_error);
    REQUIRE_THROWS_AS(
        TestArgs({"project", "build", "-w", "--report", "junit"}).parse(),
        ezmk::fatal_error);
    REQUIRE_THROWS_AS(
        TestArgs({"project", "clean", "-w", "--report", "junit"}).parse(),
        ezmk::fatal_error);
}

// ===================================================================
// 1.3.4 — project watch --run / -r
// ===================================================================

TEST_CASE("cli parse: project watch --run / -r", "[cli][1.3.4]") {
    auto a = TestArgs({"watch", "--run"}).parse();
    REQUIRE(a.cmd == Command::ProjectWatch);
    REQUIRE(a.watch_run == true);

    auto b = TestArgs({"project", "watch", "-r"}).parse();
    REQUIRE(b.cmd == Command::ProjectWatch);
    REQUIRE(b.watch_run == true);

    // Default: no --run → flag stays off (behavior unchanged).
    auto c = TestArgs({"watch"}).parse();
    REQUIRE(c.cmd == Command::ProjectWatch);
    REQUIRE(c.watch_run == false);

    // Orthogonal to --no-build-on-start.
    auto d = TestArgs({"watch", "--no-build-on-start", "--run"}).parse();
    REQUIRE(d.watch_no_build_on_start == true);
    REQUIRE(d.watch_run == true);
}

// ===================================================================
// 1.3.5 — project pack --format <tar.gz|zip>
// ===================================================================

TEST_CASE("cli parse: project pack --format", "[cli][1.3.5]") {
    // Default: tar.gz (unchanged behavior).
    auto d = TestArgs({"project", "pack"}).parse();
    REQUIRE(d.cmd == Command::ProjectPack);
    REQUIRE(d.project_pack_opts.has_value());
    REQUIRE(d.project_pack_opts->format == "tar.gz");

    auto z = TestArgs({"project", "pack", "--format", "zip"}).parse();
    REQUIRE(z.project_pack_opts->format == "zip");

    auto up = TestArgs({"project", "pack", "--format", "ZIP"}).parse();
    REQUIRE(up.project_pack_opts->format == "zip");  // case-insensitive

    auto t = TestArgs({"project", "pack", "--format", "tar.gz"}).parse();
    REQUIRE(t.project_pack_opts->format == "tar.gz");

    // Invalid formats are rejected at parse time.
    REQUIRE_THROWS_AS(
        TestArgs({"project", "pack", "--format", "deb"}).parse(),
        ezmk::fatal_error);
    REQUIRE_THROWS_AS(
        TestArgs({"project", "pack", "--format", "tgz"}).parse(),
        ezmk::fatal_error);
}
