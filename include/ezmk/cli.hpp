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
    Utils,
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

    // Only valid for RepoAdd / RepoRemove / RepoUpdate / RepoList
    RepoOptions repo_opts;

    // Only valid for Utils
    std::string utils_name;
    std::vector<std::string> utils_args;
};

CliArgs parse(int argc, char** argv);
void print_usage();

} // namespace ezmk::cli
