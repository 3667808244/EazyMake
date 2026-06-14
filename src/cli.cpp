#include "ezmk/cli.hpp"
#include "ezmk/util.hpp"

#include <cstring>
#include <iostream>

namespace ezmk::cli {

CliArgs parse(int argc, char** argv) {
    CliArgs args;

    if (argc < 2) {
        args.cmd = Command::Help;
        return args;
    }

    std::string_view cmd = argv[1];

    if (cmd == "new") {
        args.cmd = Command::New;
        if (argc < 3) {
            util::fatal("'ezmk new' requires a project name");
        }
        args.project_name = argv[2];
        return args;
    }

    if (cmd == "build") {
        args.cmd = Command::Build;
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--disable-cache") == 0) {
                args.build_opts.disable_cache = true;
            } else {
                util::fatal(std::string("unknown flag for build: ") + argv[i]);
            }
        }
        return args;
    }

    if (cmd == "run") {
        args.cmd = Command::Run;
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--disable-cache") == 0) {
                args.build_opts.disable_cache = true;
            } else {
                util::fatal(std::string("unknown flag for run: ") + argv[i]);
            }
        }
        return args;
    }

    if (cmd == "clean") {
        args.cmd = Command::Clean;
        return args;
    }

    if (cmd == "install") {
        args.cmd = Command::Install;
        InstallOptions opts;
        bool scope_set = false;
        bool has_pkg = false;

        for (int i = 2; i < argc; ++i) {
            std::string_view a = argv[i];
            if (a == "-p") {
                opts.scope = Scope::Project;
                scope_set = true;
            } else if (a == "-u") {
                opts.scope = Scope::User;
                scope_set = true;
            } else if (a == "-g") {
                opts.scope = Scope::Global;
                scope_set = true;
            } else {
                // It's the package file/url
                if (has_pkg) {
                    util::fatal("'ezmk install' takes only one package argument");
                }
                opts.pkg_file = argv[i];
                has_pkg = true;
            }
        }

        if (!has_pkg) {
            util::fatal("'ezmk install' requires a package file or URL");
        }
        // Default scope for install is Project (-p)
        if (!scope_set) {
            opts.scope = Scope::Project;
        }
        args.install_opts = opts;
        return args;
    }

    if (cmd == "remove" || cmd == "search" || cmd == "info") {
        if (cmd == "remove") args.cmd = Command::Remove;
        else if (cmd == "search") args.cmd = Command::Search;
        else args.cmd = Command::Info;

        QueryOptions opts;
        bool scope_set = false;
        bool has_name = false;

        for (int i = 2; i < argc; ++i) {
            std::string_view a = argv[i];
            if (a == "-p") {
                opts.scopes.push_back(Scope::Project);
                scope_set = true;
            } else if (a == "-u") {
                opts.scopes.push_back(Scope::User);
                scope_set = true;
            } else if (a == "-g") {
                opts.scopes.push_back(Scope::Global);
                scope_set = true;
            } else if (a == "-pu" || a == "-up") {
                opts.scopes.push_back(Scope::Project);
                opts.scopes.push_back(Scope::User);
                scope_set = true;
            } else if (a == "-pg" || a == "-gp") {
                opts.scopes.push_back(Scope::Project);
                opts.scopes.push_back(Scope::Global);
                scope_set = true;
            } else if (a == "-ug" || a == "-gu") {
                opts.scopes.push_back(Scope::User);
                opts.scopes.push_back(Scope::Global);
                scope_set = true;
            } else if (a == "-pug" || a == "-pgu" || a == "-upg" ||
                       a == "-ugp" || a == "-gpu" || a == "-gup") {
                opts.scopes.push_back(Scope::Project);
                opts.scopes.push_back(Scope::User);
                opts.scopes.push_back(Scope::Global);
                scope_set = true;
            } else {
                if (has_name) {
                    util::fatal(std::string("'ezmk ") + std::string(cmd) +
                                "' takes only one package name");
                }
                opts.pkg_name = argv[i];
                has_name = true;
            }
        }

        if (!has_name) {
            util::fatal(std::string("'ezmk ") + std::string(cmd) + "' requires a package name");
        }
        // Default scopes for remove/search/info: all three (-pug)
        if (!scope_set) {
            opts.scopes = {Scope::Project, Scope::User, Scope::Global};
        }
        args.query_opts = opts;
        return args;
    }

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        args.cmd = Command::Help;
        return args;
    }

    util::error(std::string("unknown command: ") + std::string(cmd));
    args.cmd = Command::Help;
    return args;
}

void print_usage() {
    std::cout << R"(EazyMake - A simple C/C++ build tool

Usage:
  ezmk new <project_name>                  Create a new project
  ezmk build [--disable-cache]             Build the project
  ezmk run [--disable-cache]               Build and run
  ezmk clean                               Clean cache and temp files

Package management:
  ezmk install [-p|-u|-g] <pkg_file_or_url>   Install a package (default: -p)
  ezmk remove  [-p|-u|-g] <pkg>               Remove a package (default: -pug)
  ezmk search  [-p|-u|-g] <pkg>               Search for a package (default: -pug)
  ezmk info    [-p|-u|-g] <pkg>               Show package info (default: -pug)

Scope flags:
  -p    Project scope
  -u    User scope
  -g    Global scope
  Flags can be combined, e.g. -pug
)";
}

} // namespace ezmk::cli
