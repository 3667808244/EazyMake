#include "ezmk/cli.hpp"
#include "ezmk/argparse.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace ezmk::cli
{

    // ===================================================================
    // Shared helpers
    // ===================================================================

    // Scope flags -p / -u / -g are ordinary short options; the generic
    // tokenizer splits "-pug" into "-p -u -g" for free. Collect them from a
    // ParsedOptions in the order they appeared.
    static std::vector<Scope> collect_scopes(const ParsedOptions &p)
    {
        std::vector<Scope> scopes;
        for (const auto &[k, v] : p.options)
        {
            (void)v;
            if (k == "p")
                scopes.push_back(Scope::Project);
            else if (k == "u")
                scopes.push_back(Scope::User);
            else if (k == "g")
                scopes.push_back(Scope::Global);
        }
        return scopes;
    }

    // The three scope option specs, shared by every scoped subcommand.
    static void add_scope_specs(std::vector<OptionSpec> &spec)
    {
        spec.push_back({'p', "", false});
        spec.push_back({'u', "", false});
        spec.push_back({'g', "", false});
    }

    // Read build-related options (build / run / watch share these).
    static void fill_build_opts(const ParsedOptions &p, BuildOptions &b)
    {
        if (p.has("disable-cache"))
            b.disable_cache = true;
        if (p.has("verbose"))
            b.verbose = true;
        if (auto v = p.value("jobs"))
        {
            int j = 0;
            try
            {
                size_t pos = 0;
                j = std::stoi(*v, &pos);
                if (pos != v->size())
                    throw std::invalid_argument("");
            }
            catch (...)
            {
                util::fatal(std::string("invalid -j value: ") + *v);
            }
            if (j < 0)
                util::fatal("'-j' value must be >= 0");
            b.jobs = j;
        }
        if (auto v = p.value("profile"))
            b.profile = *v;
        if (p.has("auto-update"))          // 0.2.5+
            b.auto_update = true;
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
            std::vector<OptionSpec> spec = {
                {'\0', "type", true},
                {'\0', "disable-git-init", false},
                {'\0', "disable-gitignore", false},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project new");

            if (p.positionals.empty())
                util::fatal("'ezmk project new' requires a project name");
            if (p.positionals.size() > 1)
                util::fatal("'ezmk project new' takes only one project name");
            args.project_name = p.positionals[0];

            if (auto t = p.value("type"))
            {
                if (*t != "executable" && *t != "static" && *t != "shared" && *t != "utils")
                    util::fatal(std::string("unknown project type: ") + *t +
                                ". Expected: executable, static, shared, or utils");
                args.project_type = *t;
            }
            if (p.has("disable-git-init"))
                args.disable_git_init = true;
            if (p.has("disable-gitignore"))
                args.disable_gitignore = true;
            return args;
        }

        if (action == "build" || action == "run" || action == "watch")
        {
            std::vector<OptionSpec> spec = {
                {'v', "verbose", false},
                {'j', "jobs", true},
                {'\0', "disable-cache", false},
                {'\0', "profile", true},
                {'\0', "auto-update", false},    // 0.2.5+
            };
            std::string cmd_name;
            if (action == "build")
            {
                args.cmd = Command::ProjectBuild;
                cmd_name = "ezmk project build";
            }
            else if (action == "run")
            {
                args.cmd = Command::ProjectRun;
                cmd_name = "ezmk project run";
            }
            else
            {
                args.cmd = Command::ProjectWatch;
                cmd_name = "ezmk project watch";
                spec.push_back({'\0', "no-build-on-start", false});
            }

            auto p = parse_options(argc, argv, 3, spec, cmd_name);
            fill_build_opts(p, args.build_opts);

            if (action == "run")
            {
                // Positionals (typically after "--") are passed to the program.
                args.program_args = p.positionals;
            }
            else if (action == "watch")
            {
                if (p.has("no-build-on-start"))
                    args.watch_no_build_on_start = true;
                if (!p.positionals.empty())
                    util::fatal(std::string("unexpected argument for watch: ") +
                                p.positionals[0]);
            }
            else // build
            {
                if (!p.positionals.empty())
                    util::fatal(std::string("unexpected argument for build: ") +
                                p.positionals[0]);
            }
            return args;
        }

        if (action == "clean")
        {
            args.cmd = Command::ProjectClean;
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
            std::vector<OptionSpec> spec = {
                {'\0', "sha256", true},
                {'y', "yes", false},
            };
            add_scope_specs(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk pkg install");

            auto scopes = collect_scopes(p);
            if (scopes.size() > 1)
                util::fatal("'ezmk pkg install' accepts only one scope flag "
                            "(e.g. -p, -u, or -g), not combined flags like -pug");

            if (p.positionals.empty())
                util::fatal("'ezmk pkg install' requires a package file or URL");
            if (p.positionals.size() > 1)
                util::fatal("'ezmk pkg install' takes only one package argument");

            InstallOptions opts;
            opts.pkg_file = p.positionals[0];
            opts.scope = scopes.empty() ? Scope::Project : scopes[0];
            if (auto s = p.value("sha256"))
                opts.sha256 = *s;
            if (p.has("yes"))
                opts.assume_yes = true;
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

            std::vector<OptionSpec> spec;
            add_scope_specs(spec);
            auto p = parse_options(argc, argv, 3, spec,
                                   "ezmk pkg " + std::string(action));

            if (p.positionals.empty())
                util::fatal(std::string("'ezmk pkg ") + std::string(action) +
                            "' requires a package name");
            if (p.positionals.size() > 1)
                util::fatal(std::string("'ezmk pkg ") + std::string(action) +
                            "' takes only one package name");

            QueryOptions opts;
            opts.pkg_name = p.positionals[0];
            opts.scopes = collect_scopes(p);
            if (opts.scopes.empty())
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            args.query_opts = opts;
            return args;
        }

        if (action == "list")
        {
            args.cmd = Command::PkgList;
            std::vector<OptionSpec> spec;
            add_scope_specs(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk pkg list");
            if (!p.positionals.empty())
                util::fatal(std::string("'ezmk pkg list' takes no arguments"));

            QueryOptions opts;
            opts.scopes = collect_scopes(p);
            if (opts.scopes.empty())
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            args.query_opts = opts;
            return args;
        }

        if (action == "update")
        {
            args.cmd = Command::PkgUpdate;
            std::vector<OptionSpec> spec = {
                {'\0', "all", false},
            };
            add_scope_specs(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk pkg update");

            QueryOptions opts;
            opts.update_all = p.has("all");
            if (p.positionals.size() > 1)
                util::fatal("'ezmk pkg update' takes only one package name");

            std::string pkg_name = p.positionals.empty() ? "" : p.positionals[0];
            if (opts.update_all)
            {
                if (!pkg_name.empty())
                    util::warn("'--all' ignores the explicit package name '" + pkg_name + "'");
                opts.pkg_name.clear();
            }
            else if (pkg_name.empty())
            {
                util::fatal("'ezmk pkg update' requires a package name or --all");
            }
            else
            {
                opts.pkg_name = pkg_name;
            }
            opts.scopes = collect_scopes(p);
            if (opts.scopes.empty())
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
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
            std::vector<OptionSpec> spec = {
                {'\0', "name", true},
                {'\0', "branch", true},
            };
            add_scope_specs(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk repo add");

            auto scopes = collect_scopes(p);
            if (scopes.size() > 1)
                util::fatal("'ezmk repo add' accepts only one scope flag");

            if (p.positionals.empty())
                util::fatal("'ezmk repo add' requires a git URL or local path");
            if (p.positionals.size() > 1)
                util::fatal("'ezmk repo add' takes only one URL or path");

            RepoOptions opts;
            opts.url = p.positionals[0];
            opts.scopes = scopes.empty() ? std::vector<Scope>{Scope::Project} : scopes;
            if (auto n = p.value("name"))
                opts.name = *n;
            if (auto b = p.value("branch"))
                opts.branch = *b;
            args.repo_opts = std::move(opts);
            return args;
        }

        if (action == "remove" || action == "update")
        {
            bool is_remove = (action == "remove");
            args.cmd = is_remove ? Command::RepoRemove : Command::RepoUpdate;

            std::vector<OptionSpec> spec;
            add_scope_specs(spec);
            auto p = parse_options(argc, argv, 3, spec,
                                   "ezmk repo " + std::string(action));

            if (p.positionals.size() > 1)
                util::fatal(std::string("'ezmk repo ") + std::string(action) +
                            "' takes only one repository name");
            std::string name = p.positionals.empty() ? "" : p.positionals[0];
            if (is_remove && name.empty())
                util::fatal("'ezmk repo remove' requires a repository name");

            RepoOptions opts;
            opts.name = name;
            opts.scopes = collect_scopes(p);
            if (opts.scopes.empty())
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            args.repo_opts = std::move(opts);
            return args;
        }

        if (action == "list")
        {
            args.cmd = Command::RepoList;
            std::vector<OptionSpec> spec;
            add_scope_specs(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk repo list");
            if (!p.positionals.empty())
                util::fatal(std::string("'ezmk repo list' takes no arguments"));

            RepoOptions opts;
            opts.scopes = collect_scopes(p);
            if (opts.scopes.empty())
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            args.repo_opts = std::move(opts);
            return args;
        }

        if (action == "info")     // 0.2.5+
        {
            args.cmd = Command::RepoInfo;
            std::vector<OptionSpec> spec;
            add_scope_specs(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk repo info");

            if (p.positionals.empty())
                util::fatal("'ezmk repo info' requires a repository name");
            if (p.positionals.size() > 1)
                util::fatal("'ezmk repo info' takes only one repository name");

            RepoOptions opts;
            opts.name = p.positionals[0];
            opts.scopes = collect_scopes(p);
            if (opts.scopes.empty())
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            args.repo_opts = std::move(opts);
            return args;
        }

        util::fatal(std::string("unknown repo subcommand: ") + std::string(action));
    }

    static CliArgs parse_utils_args(int argc, char **argv)
    {
        CliArgs args;
        args.cmd = Command::Utils;
        args.utils_name = (argc >= 3) ? argv[2] : "";
        // Everything after the tool name is passed to the tool verbatim.
        // A single leading "--" separator is consumed so callers can write
        // `ezmk utils fmt -- --help` to forward flags that would otherwise
        // look like ezmk options.
        bool dropped_separator = false;
        for (int i = 3; i < argc; ++i)
        {
            if (!dropped_separator && std::string(argv[i]) == "--")
            {
                dropped_separator = true;
                continue;
            }
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
                  << "  ezmk project new <project_name>                             Create a new project\n"
                  << "  ezmk project build [--disable-cache] [--verbose|-v] [-j <N>] [--profile <name>] [--auto-update]\n"
                  << "  ezmk project run [--disable-cache] [--verbose|-v] [-j <N>] [--profile <name>] [--auto-update] [-- <program args>]\n"
                  << "  ezmk project clean                                          Clean cache and temp files\n"
                  << "  ezmk project watch [--profile <name>] [--no-build-on-start] [-j <N>] [--auto-update]\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_pkg) << "\n"
                  << "  ezmk pkg install [-p|-u|-g] [--sha256 <hash>] [-y] <pkg_file_or_url>\n"
                  << "  ezmk pkg remove  [-p|-u|-g] <pkg>              Remove a package (default: -pug)\n"
                  << "  ezmk pkg search  [-p|-u|-g] <pkg>              Search for a package (default: -pug)\n"
                  << "  ezmk pkg info    [-p|-u|-g] <pkg>              Show package info (default: -pug)\n"
                  << "  ezmk pkg list    [-p|-u|-g]                    List installed packages (default: -pug)\n"
                  << "  ezmk pkg update  [-p|-u|-g] [--all] [<pkg>]        Update installed package(s) (default: -pug)\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_repo) << "\n"
                  << "  ezmk repo add [-p|-u|-g] <git_url_or_path> [--name <name>] [--branch <branch>]\n"
                  << "  ezmk repo remove [-p|-u|-g] <name>\n"
                  << "  ezmk repo update [-p|-u|-g] [<name>]\n"
                  << "  ezmk repo list [-p|-u|-g]\n"
                  << "  ezmk repo info [-p|-u|-g] <name>                          Show repository details\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_utils) << "\n"
                  << "  ezmk utils <util_name> [-- args]           Run a Lua-based utility tool\n"
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
                  << "  --auto-update        Auto-update all registered repo indices before building\n"
                  << "\n";
        std::cout << get(I18nKey::cli_usage_install_flags) << "\n"
                  << "  --sha256 <hash>      Verify package archive against expected SHA-256\n"
                  << "  -y, --yes            Skip all interactive prompts (for CI/scripts)\n"
                  << "\n";
        // 0.2.5+: GNU-style option syntax note.
        std::cout << "Option syntax:\n"
                  << "  Long options accept --flag=value or --flag value.\n"
                  << "  Short options can be grouped (-pug) and take attached values (-j4).\n"
                  << "  Use -- to end option parsing and pass the rest through (utils, project run).\n";
    }

} // namespace ezmk::cli
