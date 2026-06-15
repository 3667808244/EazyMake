#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

#include <cstring>
#include <iostream>

namespace ezmk::cli {

// Helper: parse combined scope flags like "-pug", "-pu", etc.
// Returns true if scopes were set.
static bool parse_scope_flags(std::string_view a,
                               std::vector<Scope>& scopes,
                               bool allow_single) {
    // Must start with '-' to be a scope flag
    if (a.empty() || a[0] != '-') return false;

    // Single-scope mode (for install): only one flag allowed
    // Multi-scope mode (for remove/search/info): combination allowed
    if (allow_single) {
        if (a == "-p") { scopes = {Scope::Project}; return true; }
        if (a == "-u") { scopes = {Scope::User};    return true; }
        if (a == "-g") { scopes = {Scope::Global};  return true; }
        return false;
    } else {
        // Multi-scope: support -p, -u, -g and combinations like -pug
        bool found = false;
        if (a.find('p') != std::string::npos) { scopes.push_back(Scope::Project); found = true; }
        if (a.find('u') != std::string::npos) { scopes.push_back(Scope::User);    found = true; }
        if (a.find('g') != std::string::npos) { scopes.push_back(Scope::Global);  found = true; }
        return found;
    }
}

CliArgs parse(int argc, char** argv) {
    CliArgs args;

    if (argc < 2) {
        args.cmd = Command::Help;
        return args;
    }

    // Handle bare help
    std::string_view arg1 = argv[1];
    if (arg1 == "help" || arg1 == "--help" || arg1 == "-h") {
        args.cmd = Command::Help;
        return args;
    }

    // Handle version
    if (arg1 == "version" || arg1 == "--version" || arg1 == "-V") {
        args.cmd = Command::Version;
        return args;
    }

    if (argc < 3) {
        util::fatal(std::string("'ezmk ") + std::string(arg1) +
                    "' requires a subcommand. Use 'ezmk help' for usage.");
    }

    std::string_view group = argv[1];
    std::string_view action = argv[2];

    // ================================================================
    // project subcommands
    // ================================================================
    if (group == "project") {
        if (action == "new") {
            args.cmd = Command::ProjectNew;
            if (argc < 4) {
                util::fatal("'ezmk project new' requires a project name");
            }
            args.project_name = argv[3];

            // Parse --type flag
            for (int i = 4; i < argc; ++i) {
                if (std::strcmp(argv[i], "--type") == 0) {
                    if (i + 1 >= argc) {
                        util::fatal("'--type' requires a value: executable, static, or shared");
                    }
                    std::string_view t = argv[++i];
                    if (t != "executable" && t != "static" && t != "shared") {
                        util::fatal(std::string("unknown project type: ") + std::string(t) +
                                    ". Expected: executable, static, or shared");
                    }
                    args.project_type = t;
                } else {
                    util::fatal(std::string("unknown flag for new: ") + argv[i]);
                }
            }
            return args;
        }

        if (action == "build") {
            args.cmd = Command::ProjectBuild;
            for (int i = 3; i < argc; ++i) {
                if (std::strcmp(argv[i], "--disable-cache") == 0) {
                    args.build_opts.disable_cache = true;
                } else {
                    util::fatal(std::string("unknown flag for build: ") + argv[i]);
                }
            }
            return args;
        }

        if (action == "run") {
            args.cmd = Command::ProjectRun;
            for (int i = 3; i < argc; ++i) {
                if (std::strcmp(argv[i], "--disable-cache") == 0) {
                    args.build_opts.disable_cache = true;
                } else {
                    util::fatal(std::string("unknown flag for run: ") + argv[i]);
                }
            }
            return args;
        }

        if (action == "clean") {
            args.cmd = Command::ProjectClean;
            return args;
        }

        util::fatal(std::string("unknown project subcommand: ") + std::string(action));
    }

    // ================================================================
    // pkg subcommands
    // ================================================================
    if (group == "pkg") {
        if (action == "install") {
            args.cmd = Command::PkgInstall;
            InstallOptions opts;
            bool scope_set = false;
            bool has_pkg = false;

            for (int i = 3; i < argc; ++i) {
                std::vector<Scope> scopes;
                if (parse_scope_flags(argv[i], scopes, true)) {
                    opts.scope = scopes[0];
                    scope_set = true;
                } else {
                    if (has_pkg) {
                        util::fatal("'ezmk pkg install' takes only one package argument");
                    }
                    opts.pkg_file = argv[i];
                    has_pkg = true;
                }
            }

            if (!has_pkg) {
                util::fatal("'ezmk pkg install' requires a package file or URL");
            }
            if (!scope_set) {
                opts.scope = Scope::Project;
            }
            args.install_opts = opts;
            return args;
        }

        if (action == "remove" || action == "search" || action == "info") {
            if (action == "remove") args.cmd = Command::PkgRemove;
            else if (action == "search") args.cmd = Command::PkgSearch;
            else args.cmd = Command::PkgInfo;

            QueryOptions opts;
            bool scope_set = false;
            bool has_name = false;

            for (int i = 3; i < argc; ++i) {
                std::vector<Scope> scopes;
                if (parse_scope_flags(argv[i], scopes, false)) {
                    for (auto s : scopes) opts.scopes.push_back(s);
                    scope_set = true;
                } else {
                    if (has_name) {
                        util::fatal(std::string("'ezmk pkg ") + std::string(action) +
                                    "' takes only one package name");
                    }
                    opts.pkg_name = argv[i];
                    has_name = true;
                }
            }

            if (!has_name) {
                util::fatal(std::string("'ezmk pkg ") + std::string(action) +
                            "' requires a package name");
            }
            if (!scope_set) {
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            }
            args.query_opts = opts;
            return args;
        }

        util::fatal(std::string("unknown pkg subcommand: ") + std::string(action));
    }

    // ================================================================
    // repo subcommands (placeholder)
    // ================================================================
    if (group == "repo") {
        if (action == "add") {
            args.cmd = Command::RepoAdd;
            if (argc < 4) {
                util::fatal("'ezmk repo add' requires a repository path or URL");
            }
            RepoOptions opts;
            opts.repo_path = argv[3];
            args.repo_opts = opts;
            return args;
        }
        if (action == "update") {
            args.cmd = Command::RepoUpdate;
            return args;
        }
        if (action == "remove") {
            args.cmd = Command::RepoRemove;
            if (argc < 4) {
                util::fatal("'ezmk repo remove' requires a repository name");
            }
            RepoOptions opts;
            opts.repo_name = argv[3];
            args.repo_opts = opts;
            return args;
        }
        if (action == "list") {
            args.cmd = Command::RepoList;
            return args;
        }
        util::fatal(std::string("unknown repo subcommand: ") + std::string(action));
    }

    util::error(std::string("unknown command group: ") + std::string(group) +
                ". Use 'ezmk help' for usage.");
    args.cmd = Command::Help;
    return args;
}

void print_usage() {
    std::cout << R"(EazyMake - A simple C/C++ build tool

Usage:
  ezmk project new <project_name>             Create a new project
  ezmk project build [--disable-cache]        Build the project
  ezmk project run [--disable-cache]          Build and run
  ezmk project clean                          Clean cache and temp files

Package management:
  ezmk pkg install [-p|-u|-g] <pkg_file_or_url>  Install a package (default: -p)
  ezmk pkg remove  [-p|-u|-g] <pkg>              Remove a package (default: -pug)
  ezmk pkg search  [-p|-u|-g] <pkg>              Search for a package (default: -pug)
  ezmk pkg info    [-p|-u|-g] <pkg>              Show package info (default: -pug)

Repository management (coming soon):
  ezmk repo add <path>                            Register a repository
  ezmk repo remove <name>                         Unregister a repository
  ezmk repo update                                Refresh repository index
  ezmk repo list                                  List registered repositories

Scope flags:
  -p    Project scope
  -u    User scope
  -g    Global scope
  Flags can be combined, e.g. -pug
)";
}

} // namespace ezmk::cli
