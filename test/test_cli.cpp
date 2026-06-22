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
    auto args = TestArgs({"pkg", "install", "./foo.zip",
                          "--sha256", "abc123"}).parse();
    REQUIRE(args.install_opts.has_value());
    REQUIRE(args.install_opts->sha256 == "abc123");
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
