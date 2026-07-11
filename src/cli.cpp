#include "ezmk/cli.hpp"
#include "ezmk/argparse.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <iostream>
#include <iomanip>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
                util::fatal(ezmk::i18n::I18nKey::cli_invalid_jobs, {{"val", *v}});
            }
            if (j < 0)
                util::fatal(ezmk::i18n::I18nKey::cli_jobs_negative);
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
                util::fatal(ezmk::i18n::I18nKey::cli_arg_required,
                            {{"cmd", "ezmk project new"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_project_name)}});
            if (p.positionals.size() > 1)
                util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                            {{"cmd", "ezmk project new"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_project_name)}});
            args.project_name = p.positionals[0];

            if (auto t = p.value("type"))
            {
                if (*t != "executable" && *t != "static" && *t != "shared" && *t != "utils")
                    util::fatal(ezmk::i18n::I18nKey::cli_unknown_project_type, {{"type", *t}});
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
                    util::fatal(ezmk::i18n::I18nKey::cli_unexpected_arg,
                                {{"cmd", "ezmk project watch"}, {"arg", p.positionals[0]}});
            }
            else // build
            {
                if (!p.positionals.empty())
                    util::fatal(ezmk::i18n::I18nKey::cli_unexpected_arg,
                                {{"cmd", "ezmk project build"}, {"arg", p.positionals[0]}});
            }
            return args;
        }

        if (action == "clean")
        {
            args.cmd = Command::ProjectClean;
            return args;
        }

        util::fatal(ezmk::i18n::I18nKey::cli_unknown_subcommand,
                    {{"group", "project"}, {"sub", std::string(action)}});
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
                util::fatal(ezmk::i18n::I18nKey::cli_one_scope,
                            {{"cmd", "ezmk pkg install"}});

            if (p.positionals.empty())
                util::fatal(ezmk::i18n::I18nKey::cli_arg_required,
                            {{"cmd", "ezmk pkg install"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_package_arg)}});
            if (p.positionals.size() > 1)
                util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                            {{"cmd", "ezmk pkg install"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_package_arg)}});

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
                util::fatal(ezmk::i18n::I18nKey::cli_arg_required,
                            {{"cmd", "ezmk pkg " + std::string(action)},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_package_name)}});
            if (p.positionals.size() > 1)
                util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                            {{"cmd", "ezmk pkg " + std::string(action)},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_package_name)}});

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
                util::fatal(ezmk::i18n::I18nKey::cli_takes_no_args, {{"cmd", "ezmk pkg list"}});

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
                util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                            {{"cmd", "ezmk pkg update"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_package_name)}});

            std::string pkg_name = p.positionals.empty() ? "" : p.positionals[0];
            if (opts.update_all)
            {
                if (!pkg_name.empty())
                    util::warn(ezmk::i18n::fmt(ezmk::i18n::I18nKey::cli_all_ignores_name,
                                               {{"name", pkg_name}}));
                opts.pkg_name.clear();
            }
            else if (pkg_name.empty())
            {
                util::fatal(ezmk::i18n::I18nKey::cli_update_needs_name_or_all);
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

        util::fatal(ezmk::i18n::I18nKey::cli_unknown_subcommand,
                    {{"group", "pkg"}, {"sub", std::string(action)}});
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
                util::fatal(ezmk::i18n::I18nKey::cli_one_scope, {{"cmd", "ezmk repo add"}});

            if (p.positionals.empty())
                util::fatal(ezmk::i18n::I18nKey::cli_arg_required,
                            {{"cmd", "ezmk repo add"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_url)}});
            if (p.positionals.size() > 1)
                util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                            {{"cmd", "ezmk repo add"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_url)}});

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
                util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                            {{"cmd", "ezmk repo " + std::string(action)},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_name)}});
            std::string name = p.positionals.empty() ? "" : p.positionals[0];
            if (is_remove && name.empty())
                util::fatal(ezmk::i18n::I18nKey::cli_arg_required,
                            {{"cmd", "ezmk repo remove"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_name)}});

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
                util::fatal(ezmk::i18n::I18nKey::cli_takes_no_args, {{"cmd", "ezmk repo list"}});

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
                util::fatal(ezmk::i18n::I18nKey::cli_arg_required,
                            {{"cmd", "ezmk repo info"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_name)}});
            if (p.positionals.size() > 1)
                util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                            {{"cmd", "ezmk repo info"},
                             {"what", ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_name)}});

            RepoOptions opts;
            opts.name = p.positionals[0];
            opts.scopes = collect_scopes(p);
            if (opts.scopes.empty())
                opts.scopes = {Scope::Project, Scope::User, Scope::Global};
            args.repo_opts = std::move(opts);
            return args;
        }

        util::fatal(ezmk::i18n::I18nKey::cli_unknown_subcommand,
                    {{"group", "repo"}, {"sub", std::string(action)}});
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

    // 0.2.6+: parse a --color value (case-insensitive). Aborts (fatal) on an
    // unrecognized mode. `always/enable`, `auto/default`, `never/disable`.
    static util::ColorMode parse_color_mode(const std::string &raw)
    {
        std::string v;
        v.reserve(raw.size());
        for (char c : raw)
            v.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        if (v == "always" || v == "enable")
            return util::ColorMode::Always;
        if (v == "never" || v == "disable")
            return util::ColorMode::Never;
        if (v == "auto" || v == "default")
            return util::ColorMode::Auto;
        util::fatal(ezmk::i18n::I18nKey::cli_invalid_color, {{"val", raw}});
    }

    // 0.2.6+: consume the global --color=<mode> / --color <mode> option from the
    // token list wherever it appears (before "--"), applying it via
    // util::set_color_mode so per-command parsers never see it. Tokens after a
    // "--" separator are left untouched (they belong to utils / project run).
    static void strip_color_option(std::vector<std::string> &toks)
    {
        std::vector<std::string> out;
        out.reserve(toks.size());
        bool applied = false;
        bool passthrough = false;
        util::ColorMode mode = util::ColorMode::Auto;

        if (!toks.empty())
            out.push_back(toks[0]); // program name

        for (size_t i = 1; i < toks.size(); ++i)
        {
            const std::string &t = toks[i];
            if (passthrough)
            {
                out.push_back(t);
                continue;
            }
            if (t == "--")
            {
                passthrough = true;
                out.push_back(t);
                continue;
            }
            if (t == "--color")
            {
                if (i + 1 >= toks.size())
                    util::fatal(ezmk::i18n::I18nKey::cli_invalid_color, {{"val", ""}});
                mode = parse_color_mode(toks[++i]);
                applied = true;
                continue;
            }
            if (t.rfind("--color=", 0) == 0)
            {
                mode = parse_color_mode(t.substr(8));
                applied = true;
                continue;
            }
            out.push_back(t);
        }

        toks.swap(out);
        if (applied)
            util::set_color_mode(mode);
    }

    CliArgs parse(int argc, char **argv)
    {
        CliArgs args;

        // 0.2.6+: consume the global --color option first (wherever it appears),
        // so command-specific parsers below never see it. Backing storage kept
        // alive for the whole function (parse_*_args read from argv).
        std::vector<std::string> toks;
        toks.reserve(argc);
        for (int i = 0; i < argc; ++i)
            toks.emplace_back(argv[i]);
        strip_color_option(toks);

        if (toks.size() < 2)
        {
            args.cmd = Command::Help;
            return args;
        }

        // 0.2.6+: top-level command shorthands. Expand toks[1] into its full
        // group[/action] form BEFORE any further parsing, so downstream logic
        // and error messages all see the canonical command names. Aliases only
        // apply at the command position (e.g. `ezmk project pn` is NOT an alias
        // and correctly reports an unknown project subcommand).
        static const std::map<std::string_view,
                               std::pair<const char *, const char *>>
            kAliases = {
                {"pn", {"project", "new"}},   {"pb", {"project", "build"}},
                {"pr", {"project", "run"}},   {"pc", {"project", "clean"}},
                {"pw", {"project", "watch"}}, {"ki", {"pkg", "install"}},
                {"kr", {"pkg", "remove"}},    {"ks", {"pkg", "search"}},
                {"kn", {"pkg", "info"}},      {"kl", {"pkg", "list"}},
                {"ku", {"pkg", "update"}},    {"ra", {"repo", "add"}},
                {"rr", {"repo", "remove"}},   {"rl", {"repo", "list"}},
                {"ru", {"repo", "update"}},   {"ri", {"repo", "info"}},
                {"u", {"utils", nullptr}},    {"h", {"help", nullptr}},
                {"v", {"version", nullptr}},
            };

        if (auto it = kAliases.find(std::string_view(toks[1])); it != kAliases.end())
        {
            std::vector<std::string> expanded;
            expanded.emplace_back(toks[0]);
            expanded.emplace_back(it->second.first);
            if (it->second.second)
                expanded.emplace_back(it->second.second);
            for (size_t i = 2; i < toks.size(); ++i)
                expanded.emplace_back(toks[i]);
            toks = std::move(expanded);
        }

        // Rebuild a char** view over toks for the existing argc/argv parsers.
        std::vector<char *> argv_buf;
        argv_buf.reserve(toks.size());
        for (auto &s : toks)
            argv_buf.push_back(const_cast<char *>(s.c_str()));
        argc = static_cast<int>(argv_buf.size());
        argv = argv_buf.data();

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
            util::fatal(ezmk::i18n::I18nKey::cli_requires_subcommand,
                        {{"group", std::string(arg1)}});
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

        util::error(ezmk::i18n::fmt(ezmk::i18n::I18nKey::cli_unknown_group,
                                    {{"group", std::string(group)}}));
        args.cmd = Command::Help;
        return args;
    }

    void print_usage()
    {
        using namespace ezmk::i18n;

        // Render one command row: a literal usage string (never translated —
        // it is a verbatim command) left-padded to a fixed column, followed by
        // the localized description. The parenthesized shorthand (0.2.6+) is
        // baked into the usage string so it lines up with the command.
        auto row = [](const std::string &usage, I18nKey desc) {
            std::cout << "  " << std::left << std::setw(52) << usage
                      << get(desc) << "\n";
        };

        std::cout << get(I18nKey::cli_usage_header) << "\n\n"
                  << get(I18nKey::cli_usage_usage) << ":\n\n";

        std::cout << get(I18nKey::cli_usage_project) << "\n";
        row("ezmk project new    (pn)  <name>", I18nKey::help_project_new);
        row("ezmk project build  (pb)  [flags]", I18nKey::help_project_build);
        row("ezmk project run    (pr)  [flags] [-- args]", I18nKey::help_project_run);
        row("ezmk project clean  (pc)", I18nKey::help_project_clean);
        row("ezmk project watch  (pw)  [flags]", I18nKey::help_project_watch);
        std::cout << "\n";

        std::cout << get(I18nKey::cli_usage_pkg) << "\n";
        row("ezmk pkg install  (ki)  [flags] <pkg>", I18nKey::help_pkg_install);
        row("ezmk pkg remove   (kr)  [-p|-u|-g] <pkg>", I18nKey::help_pkg_remove);
        row("ezmk pkg search   (ks)  [-p|-u|-g] <pkg>", I18nKey::help_pkg_search);
        row("ezmk pkg info     (kn)  [-p|-u|-g] <pkg>", I18nKey::help_pkg_info);
        row("ezmk pkg list     (kl)  [-p|-u|-g]", I18nKey::help_pkg_list);
        row("ezmk pkg update   (ku)  [-p|-u|-g] [--all] [<pkg>]", I18nKey::help_pkg_update);
        std::cout << "\n";

        std::cout << get(I18nKey::cli_usage_repo) << "\n";
        row("ezmk repo add     (ra)  [flags] <url_or_path>", I18nKey::help_repo_add);
        row("ezmk repo remove  (rr)  [-p|-u|-g] <name>", I18nKey::help_repo_remove);
        row("ezmk repo update  (ru)  [-p|-u|-g] [<name>]", I18nKey::help_repo_update);
        row("ezmk repo list    (rl)  [-p|-u|-g]", I18nKey::help_repo_list);
        row("ezmk repo info    (ri)  [-p|-u|-g] <name>", I18nKey::help_repo_info);
        std::cout << "\n";

        std::cout << get(I18nKey::cli_usage_utils) << "\n";
        row("ezmk utils        (u)   <name> [-- args]", I18nKey::help_utils);
        row("ezmk help         (h)", I18nKey::help_help);
        row("ezmk version      (v)", I18nKey::help_version);
        std::cout << "\n";

        std::cout << get(I18nKey::cli_usage_scopes) << "\n";
        row("-p", I18nKey::help_scope_project);
        row("-u", I18nKey::help_scope_user);
        row("-g", I18nKey::help_scope_global);
        std::cout << "  " << get(I18nKey::help_scope_combined) << "\n\n";

        std::cout << get(I18nKey::cli_usage_build_flags) << "\n";
        row("--disable-cache", I18nKey::help_flag_disable_cache);
        row("--verbose, -v", I18nKey::help_flag_verbose);
        row("-j, --jobs <N>", I18nKey::help_flag_jobs);
        row("--profile <name>", I18nKey::help_flag_profile);
        row("--auto-update", I18nKey::help_flag_auto_update);
        std::cout << "\n";

        std::cout << get(I18nKey::cli_usage_install_flags) << "\n";
        row("--sha256 <hash>", I18nKey::help_flag_sha256);
        row("-y, --yes", I18nKey::help_flag_yes);
        std::cout << "\n";

        std::cout << get(I18nKey::help_global_options) << "\n";
        row("--color=<mode>", I18nKey::help_flag_color);
        std::cout << "\n";

        // GNU-style option syntax note.
        std::cout << get(I18nKey::help_option_syntax_title) << "\n"
                  << "  " << get(I18nKey::help_option_syntax_long) << "\n"
                  << "  " << get(I18nKey::help_option_syntax_short) << "\n"
                  << "  " << get(I18nKey::help_option_syntax_dashdash) << "\n";
    }

} // namespace ezmk::cli
