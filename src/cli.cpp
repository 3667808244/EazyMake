#include "ezmk/cli.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <cstring>
#include <iostream>

namespace ezmk::cli
{

    // ===================================================================
    // Shared helpers
    // ===================================================================

    // Parse combined scope flags like "-pug", "-pu", etc.
    // Returns true if scope flags were parsed.
    static bool parse_scope_flags(std::string_view a,
                                  std::vector<Scope> &scopes,
                                  bool allow_single)
    {
        if (a.empty() || a[0] != '-')
            return false;

        if (allow_single)
        {
            if (a == "-p")
            {
                scopes = {Scope::Project};
                return true;
            }
            if (a == "-u")
            {
                scopes = {Scope::User};
                return true;
            }
            if (a == "-g")
            {
                scopes = {Scope::Global};
                return true;
            }
            return false;
        }
        else
        {
            // Must be a short flag like -pug (single dash, not --long-flag)
            if (a.size() < 2 || a[0] != '-' || a[1] == '-')
                return false;

            bool found = false;
            if (a.find('p') != std::string::npos)
            {
                scopes.push_back(Scope::Project);
                found = true;
            }
            if (a.find('u') != std::string::npos)
            {
                scopes.push_back(Scope::User);
                found = true;
            }
            if (a.find('g') != std::string::npos)
            {
                scopes.push_back(Scope::Global);
                found = true;
            }
            return found;
        }
    }

    // Helper for subcommands that take scopes + one positional argument.
    // Sets scopes if default_scopes is non-empty, sets value_name to the positional arg.
    // single_scope: if true, only one scope flag is allowed (like install).
    static void parse_scope_and_value(int argc, char **argv,
                                      std::vector<Scope> &scopes,
                                      std::string &value,
                                      bool single_scope,
                                      const char *cmd_name)
    {
        bool scope_set = false;
        bool has_value = false;

        for (int i = 3; i < argc; ++i)
        {
            std::vector<Scope> parsed;
            if (parse_scope_flags(argv[i], parsed, single_scope))
            {
                if (single_scope && scope_set)
                {
                    util::fatal(std::string("'") + cmd_name + "' accepts only one scope flag");
                }
                for (auto s : parsed)
                    scopes.push_back(s);
                scope_set = true;
            }
            else
            {
                if (has_value)
                {
                    util::fatal(std::string("'") + cmd_name + "' takes only one argument");
                }
                value = argv[i];
                has_value = true;
            }
        }
    }

    // ===================================================================
    // Command-group parsers
    // ===================================================================

    static CliArgs parse_project_args(int argc, char **argv)
    {
        CliArgs args;
        std::string_view action = argv[2];

        if (action == "new")
        {
            args.cmd = Command::ProjectNew;
            if (argc < 4)
            {
                util::fatal("'ezmk project new' requires a project name");
            }
            args.project_name = argv[3];

            for (int i = 4; i < argc; ++i)
            {
                if (std::strcmp(argv[i], "--type") == 0)
                {
                    if (i + 1 >= argc)
                    {
                        util::fatal("'--type' requires a value: executable, static, shared, or utils");
                    }
                    std::string_view t = argv[++i];
                    if (t != "executable" && t != "static" && t != "shared" && t != "utils")
                    {
                        util::fatal(std::string("unknown project type: ") + std::string(t) +
                                    ". Expected: executable, static, shared, or utils");
                    }
                    args.project_type = t;
                }
                else if (std::strcmp(argv[i], "--disable-git-init") == 0)
                {
                    args.disable_git_init = true;
                }
                else if (std::strcmp(argv[i], "--disable-gitignore") == 0)
                {
                    args.disable_gitignore = true;
                }
                else
                {
                    util::fatal(std::string("unknown flag for new: ") + argv[i]);
                }
            }
            return args;
        }

        if (action == "build")
        {
            args.cmd = Command::ProjectBuild;
            for (int i = 3; i < argc; ++i)
            {
                if (std::strcmp(argv[i], "--disable-cache") == 0)
                {
                    args.build_opts.disable_cache = true;
                }
                else if (std::strcmp(argv[i], "--verbose") == 0 ||
                         std::strcmp(argv[i], "-v") == 0)
                {
                    args.build_opts.verbose = true;
                }
                else if (std::strcmp(argv[i], "-j") == 0 ||
                         std::strcmp(argv[i], "--jobs") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'-j' / '--jobs' requires a number (e.g. -j 4)");
                    std::string val = argv[++i];
                    try {
                        args.build_opts.jobs = std::stoi(val);
                        if (args.build_opts.jobs < 0)
                            util::fatal("'-j' value must be >= 0");
                    } catch (...) {
                        util::fatal(std::string("invalid -j value: ") + val);
                    }
                }
                else if (std::strcmp(argv[i], "--profile") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'--profile' requires a name (e.g. --profile debug)");
                    args.build_opts.profile = argv[++i];
                }
                else
                {
                    util::fatal(std::string("unknown flag for build: ") + argv[i]);
                }
            }
            return args;
        }

        if (action == "run")
        {
            args.cmd = Command::ProjectRun;
            for (int i = 3; i < argc; ++i)
            {
                if (std::strcmp(argv[i], "--disable-cache") == 0)
                {
                    args.build_opts.disable_cache = true;
                }
                else if (std::strcmp(argv[i], "--verbose") == 0 ||
                         std::strcmp(argv[i], "-v") == 0)
                {
                    args.build_opts.verbose = true;
                }
                else if (std::strcmp(argv[i], "-j") == 0 ||
                         std::strcmp(argv[i], "--jobs") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'-j' / '--jobs' requires a number (e.g. -j 4)");
                    std::string val = argv[++i];
                    try {
                        args.build_opts.jobs = std::stoi(val);
                        if (args.build_opts.jobs < 0)
                            util::fatal("'-j' value must be >= 0");
                    } catch (...) {
                        util::fatal(std::string("invalid -j value: ") + val);
                    }
                }
                else if (std::strcmp(argv[i], "--profile") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'--profile' requires a name (e.g. --profile debug)");
                    args.build_opts.profile = argv[++i];
                }
                else
                {
                    util::fatal(std::string("unknown flag for run: ") + argv[i]);
                }
            }
            return args;
        }

        if (action == "clean")
        {
            args.cmd = Command::ProjectClean;
            return args;
        }

        if (action == "watch")
        {
            args.cmd = Command::ProjectWatch;
            for (int i = 3; i < argc; ++i)
            {
                if (std::strcmp(argv[i], "--disable-cache") == 0)
                {
                    args.build_opts.disable_cache = true;
                }
                else if (std::strcmp(argv[i], "--verbose") == 0 ||
                         std::strcmp(argv[i], "-v") == 0)
                {
                    args.build_opts.verbose = true;
                }
                else if (std::strcmp(argv[i], "-j") == 0 ||
                         std::strcmp(argv[i], "--jobs") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'-j' / '--jobs' requires a number (e.g. -j 4)");
                    std::string val = argv[++i];
                    try {
                        args.build_opts.jobs = std::stoi(val);
                        if (args.build_opts.jobs < 0)
                            util::fatal("'-j' value must be >= 0");
                    } catch (...) {
                        util::fatal(std::string("invalid -j value: ") + val);
                    }
                }
                else if (std::strcmp(argv[i], "--profile") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'--profile' requires a name (e.g. --profile debug)");
                    args.build_opts.profile = argv[++i];
                }
                else if (std::strcmp(argv[i], "--no-build-on-start") == 0)
                {
                    args.watch_no_build_on_start = true;
                }
                else
                {
                    util::fatal(std::string("unknown flag for watch: ") + argv[i]);
                }
            }
            return args;
        }

        util::fatal(std::string("unknown project subcommand: ") + std::string(action));
    }

    static CliArgs parse_pkg_args(int argc, char **argv)
    {
        CliArgs args;
        std::string_view action = argv[2];

        if (action == "install")
        {
            args.cmd = Command::PkgInstall;
            InstallOptions opts;
            std::string pkg_file;
            std::vector<Scope> scopes;

            // Parse scope flags, positional arg, and optional flags
            for (int i = 3; i < argc; ++i)
            {
                std::vector<Scope> parsed;
                if (parse_scope_flags(argv[i], parsed, true))
                {
                    for (auto s : parsed)
                        scopes.push_back(s);
                }
                else if (argv[i][0] == '-' && argv[i][1] != '-' &&
                         (std::strchr(argv[i], 'p') || std::strchr(argv[i], 'u') || std::strchr(argv[i], 'g')))
                {
                    util::fatal("'ezmk pkg install' accepts only one scope flag "
                                "(e.g. -p, -u, or -g), not combined flags like -pug");
                }
                else if (std::strcmp(argv[i], "--sha256") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'--sha256' requires a value");
                    opts.sha256 = argv[++i];
                }
                else if (std::strcmp(argv[i], "-y") == 0 || std::strcmp(argv[i], "--yes") == 0)
                {
                    opts.assume_yes = true;
                }
                else
                {
                    if (!pkg_file.empty())
                    {
                        util::fatal("'ezmk pkg install' takes only one package argument");
                    }
                    pkg_file = argv[i];
                }
            }

            if (pkg_file.empty())
            {
                util::fatal("'ezmk pkg install' requires a package file or URL");
            }
            opts.pkg_file = pkg_file;
            opts.scope = scopes.empty() ? Scope::Project : scopes[0];
            args.install_opts = opts;
            return args;
        }

        if (action == "remove" || action == "search" || action == "info")
        {
            if (action == "remove")
                args.cmd = Command::PkgRemove;
            else if (action == "search")
                args.cmd = Command::PkgSearch;
            else
                args.cmd = Command::PkgInfo;

            QueryOptions opts;
            std::string pkg_name;
            parse_scope_and_value(argc, argv, opts.scopes, pkg_name, false,
                                  ("ezmk pkg " + std::string(action)).c_str());

            if (pkg_name.empty())
            {
                util::fatal(std::string("'ezmk pkg ") + std::string(action) +
                            "' requires a package name");
            }
            if (opts.scopes.empty())
            {
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            }
            opts.pkg_name = pkg_name;
            args.query_opts = opts;
            return args;
        }

        if (action == "list")
        {
            args.cmd = Command::PkgList;
            QueryOptions opts;
            std::string dummy;
            parse_scope_and_value(argc, argv, opts.scopes, dummy, false,
                                  "ezmk pkg list");
            if (opts.scopes.empty())
            {
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            }
            args.query_opts = opts;
            return args;
        }

        if (action == "update")
        {
            args.cmd = Command::PkgUpdate;
            QueryOptions opts;
            std::string pkg_name;
            parse_scope_and_value(argc, argv, opts.scopes, pkg_name, false,
                                  "ezmk pkg update");

            if (pkg_name.empty())
            {
                util::fatal("'ezmk pkg update' requires a package name");
            }
            if (opts.scopes.empty())
            {
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            }
            opts.pkg_name = pkg_name;
            args.query_opts = opts;
            return args;
        }

        util::fatal(std::string("unknown pkg subcommand: ") + std::string(action));
    }

    static CliArgs parse_repo_args(int argc, char **argv)
    {
        CliArgs args;
        std::string_view action = argv[2];

        if (action == "add")
        {
            args.cmd = Command::RepoAdd;
            RepoOptions opts;
            bool scope_set = false;
            bool has_url = false;

            for (int i = 3; i < argc; ++i)
            {
                std::vector<Scope> parsed;
                if (parse_scope_flags(argv[i], parsed, true))
                {
                    if (scope_set)
                    {
                        util::fatal("'ezmk repo add' accepts only one scope flag");
                    }
                    opts.scopes = {parsed[0]};
                    scope_set = true;
                }
                else if (std::strcmp(argv[i], "--name") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'--name' requires a value");
                    opts.name = argv[++i];
                }
                else if (std::strcmp(argv[i], "--branch") == 0)
                {
                    if (i + 1 >= argc)
                        util::fatal("'--branch' requires a value");
                    opts.branch = argv[++i];
                }
                else
                {
                    if (has_url)
                        util::fatal("'ezmk repo add' takes only one URL or path");
                    opts.url = argv[i];
                    has_url = true;
                }
            }

            if (!has_url)
                util::fatal("'ezmk repo add' requires a git URL or local path");
            if (!scope_set)
                opts.scopes = {Scope::Project};
            args.repo_opts = std::move(opts);
            return args;
        }

        if (action == "remove" || action == "update")
        {
            bool is_remove = (action == "remove");
            args.cmd = is_remove ? Command::RepoRemove : Command::RepoUpdate;
            RepoOptions opts;
            std::string name;
            parse_scope_and_value(argc, argv, opts.scopes, name, false,
                                  ("ezmk repo " + std::string(action)).c_str());

            if (is_remove && name.empty())
            {
                util::fatal("'ezmk repo remove' requires a repository name");
            }
            if (opts.scopes.empty())
            {
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            }
            opts.name = name;
            args.repo_opts = std::move(opts);
            return args;
        }

        if (action == "list")
        {
            args.cmd = Command::RepoList;
            RepoOptions opts;
            bool scope_set = false;

            for (int i = 3; i < argc; ++i)
            {
                std::vector<Scope> parsed;
                if (parse_scope_flags(argv[i], parsed, false))
                {
                    for (auto s : parsed)
                        opts.scopes.push_back(s);
                    scope_set = true;
                }
                else
                {
                    util::fatal(std::string("unknown argument for repo list: ") + argv[i]);
                }
            }

            if (!scope_set)
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            args.repo_opts = std::move(opts);
            return args;
        }

        util::fatal(std::string("unknown repo subcommand: ") + std::string(action));
    }

    static CliArgs parse_utils_args(int argc, char **argv) {
        CliArgs args;
        args.cmd = Command::Utils;
        args.utils_name = (argc >= 3) ? argv[2] : "";
        // Store extra args for future use
        for (int i = 3; i < argc; ++i) {
            args.utils_args.push_back(argv[i]);
        }
        return args;
    }

    // ===================================================================
    // Main parse entry point
    // ===================================================================

    CliArgs parse(int argc, char **argv)
    {
        CliArgs args;

        if (argc < 2)
        {
            args.cmd = Command::Help;
            return args;
        }

        std::string_view arg1 = argv[1];
        if (arg1 == "help" || arg1 == "--help" || arg1 == "-h")
        {
            args.cmd = Command::Help;
            return args;
        }
        if (arg1 == "version" || arg1 == "--version" || arg1 == "-V")
        {
            args.cmd = Command::Version;
            return args;
        }

        if (argc < 3)
        {
            util::fatal(std::string("'ezmk ") + std::string(arg1) +
                        "' requires a subcommand. Use 'ezmk help' for usage.");
        }

        std::string_view group = argv[1];

        if (group == "project")
            return parse_project_args(argc, argv);
        if (group == "pkg")
            return parse_pkg_args(argc, argv);
        if (group == "repo")
            return parse_repo_args(argc, argv);
        if (group == "utils")
            return parse_utils_args(argc, argv);

        util::error(std::string("unknown command group: ") + std::string(group) +
                    ". Use 'ezmk help' for usage.");
        args.cmd = Command::Help;
        return args;
    }

    void print_usage()
    {
        using namespace ezmk::i18n;
        std::cout << get(I18nKey::cli_usage_header) << "\n\n"
                  << get(I18nKey::cli_usage_usage) << ":\n\n";
        std::cout << get(I18nKey::cli_usage_project) << "\n"
                  << "  ezmk project new <project_name>             Create a new project\n"
                  << "  ezmk project build [--disable-cache] [--verbose|-v] [-j <N>] [--profile <name>]\n"
                  << "  ezmk project run [--disable-cache] [--verbose|-v] [-j <N>] [--profile <name>]\n"
                  << "  ezmk project clean                          Clean cache and temp files\n"
                  << "  ezmk project watch [--profile <name>] [--no-build-on-start] [-j <N>]\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_pkg) << "\n"
                  << "  ezmk pkg install [-p|-u|-g] [--sha256 <hash>] [-y] <pkg_file_or_url>\n"
                  << "  ezmk pkg remove  [-p|-u|-g] <pkg>              Remove a package (default: -pug)\n"
                  << "  ezmk pkg search  [-p|-u|-g] <pkg>              Search for a package (default: -pug)\n"
                  << "  ezmk pkg info    [-p|-u|-g] <pkg>              Show package info (default: -pug)\n"
                  << "  ezmk pkg list    [-p|-u|-g]                    List installed packages (default: -pug)\n"
                  << "  ezmk pkg update  [-p|-u|-g] <pkg>              Update an installed package (default: -pug)\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_repo) << "\n"
                  << "  ezmk repo add [-p|-u|-g] <git_url_or_path> [--name <name>] [--branch <branch>]\n"
                  << "  ezmk repo remove [-p|-u|-g] <name>\n"
                  << "  ezmk repo update [-p|-u|-g] [<name>]\n"
                  << "  ezmk repo list [-p|-u|-g]\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_utils) << "\n"
                  << "  ezmk utils <util_name> [args]              Run a Lua-based utility tool\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_scopes) << "\n"
                  << "  -p    Project scope\n"
                  << "  -u    User scope\n"
                  << "  -g    Global scope\n"
                  << "  Flags can be combined, e.g. -pug\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_build_flags) << "\n"
                  << "  --disable-cache      Force recompilation of all sources\n"
                  << "  --verbose, -v        Print detailed compile commands and cache diagnostics\n"
                  << "  -j, --jobs <N>       Max parallel compile jobs (0 = auto, default: auto)\n"
                  << "  --profile <name>     Build profile (e.g. debug, release)\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_install_flags) << "\n"
                  << "  --sha256 <hash>      Verify package archive against expected SHA-256\n"
                  << "  -y, --yes            Skip all interactive prompts (for CI/scripts)\n";
    }

} // namespace ezmk::cli
