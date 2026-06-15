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
};

struct InstallOptions {
    Scope scope = Scope::Project;
    std::string pkg_file; // local path or URL
};

struct QueryOptions {
    std::vector<Scope> scopes;
    std::string pkg_name;
};

struct RepoOptions {
    std::string repo_path;   // for add
    std::string repo_name;   // for remove
};

struct CliArgs {
    Command cmd = Command::Help;

    // Only valid for ProjectNew
    std::optional<std::string> project_name;
    std::string project_type = "executable";   // --type flag

    // Only valid for ProjectBuild / ProjectRun
    BuildOptions build_opts;

    // Only valid for PkgInstall
    std::optional<InstallOptions> install_opts;

    // Only valid for PkgRemove / PkgSearch / PkgInfo
    std::optional<QueryOptions> query_opts;

    // Only valid for RepoAdd / RepoRemove
    std::optional<RepoOptions> repo_opts;
};

CliArgs parse(int argc, char** argv);
void print_usage();

} // namespace ezmk::cli
