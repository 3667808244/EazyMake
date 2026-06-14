#pragma once

#include <string>
#include <vector>
#include <optional>

namespace ezmk::cli {

enum class Command {
    New,
    Build,
    Run,
    Clean,
    Install,
    Remove,
    Search,
    Info,
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

struct CliArgs {
    Command cmd = Command::Help;

    // Only valid for New
    std::optional<std::string> project_name;

    // Only valid for Build / Run
    BuildOptions build_opts;

    // Only valid for Install
    std::optional<InstallOptions> install_opts;

    // Only valid for Remove / Search / Info
    std::optional<QueryOptions> query_opts;
};

CliArgs parse(int argc, char** argv);
void print_usage();

} // namespace ezmk::cli
