#pragma once

#include <string>
#include <vector>
#include <optional>

namespace ezmk::cli {

enum class Command {
    ProjectNew,
    ProjectBuild,
    ProjectRun,
    ProjectClean,
    PkgInstall,
    PkgRemove,
    PkgSearch,
    PkgInfo,
    RepoAdd,
    RepoUpdate,
    RepoRemove,
    RepoList,
    RepoInfo,        // 0.2.5+
    Utils,
    ProjectWatch,    // 0.2.3+
    PkgList,         // 0.2.3+
    PkgUpdate,       // 0.2.3+
    Version,
    Help,
};

enum class Scope {
    Project,
    User,
    Global,
};

struct BuildOptions {
    bool disable_cache = false;
    bool verbose = false;
    int jobs = 0;              // 0.2.3+ (0 = auto-detect via hardware_concurrency)
    std::string profile;        // 0.2.3+ build profile name (debug, release, etc.)
    bool auto_update = false;   // 0.2.5+: auto-update repo indices before build
};

struct InstallOptions {
    Scope scope = Scope::Project;
    std::string pkg_file;    // local path or URL
    std::string sha256;      // optional: expected SHA-256 for verification
    bool assume_yes = false; // -y: skip all interactive prompts
};

struct QueryOptions {
    std::vector<Scope> scopes;
    std::string pkg_name;
    bool update_all = false;       // 0.2.4+: --all flag for pkg update
};

struct RepoOptions {
    std::vector<Scope> scopes;     // scopes to operate on
    std::string url;               // for add: git URL or local path
    std::string name;              // for add (--name override) / for remove
    std::string branch = "main";   // for add (--branch)
};

struct CliArgs {
    Command cmd = Command::Help;

    // Only valid for ProjectNew
    std::optional<std::string> project_name;
    std::string project_type = "executable";   // --type flag
    bool disable_git_init = false;             // --disable-git-init
    bool disable_gitignore = false;            // --disable-gitignore

    // Only valid for ProjectBuild / ProjectRun
    BuildOptions build_opts;

    // Only valid for PkgInstall
    std::optional<InstallOptions> install_opts;

    // Only valid for PkgRemove / PkgSearch / PkgInfo
    std::optional<QueryOptions> query_opts;

    // Only valid for RepoAdd / RepoRemove / RepoUpdate / RepoList / RepoInfo
    RepoOptions repo_opts;

    // Only valid for Utils
    std::string utils_name;
    std::vector<std::string> utils_args;

    // 0.2.5+: Positional args after "--" for `project run` (passed to the program)
    std::vector<std::string> program_args;

    // 0.2.3+: Watch mode flags
    bool watch_no_build_on_start = false;   // --no-build-on-start
};

CliArgs parse(int argc, char** argv);
void print_usage();

} // namespace ezmk::cli
