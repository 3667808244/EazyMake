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

    // 0.9.8+: add -v/--verbose as an accepted (ignored) flag to commands
    // that don't otherwise use it, so it doesn't cause "unknown option" errors.
    static void add_verbose_spec(std::vector<OptionSpec> &spec)
    {
        spec.push_back({'v', "verbose", false});
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
                    util::fatal(ezmk::i18n::I18nKey::cli_invalid_jobs, {{"val", *v}});
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
        if (p.has("compile-commands"))     // 1.1.1
            b.compile_commands = true;
    }

    // ===================================================================
    // Command-group parsers
    // ===================================================================

    // 1.1.3 Q3: positional 数量校验 helper —— 替换三个 parse_* 中的重复块。
    // 恰好一个 positional（空 → required 报错；>1 → 太多报错）。
    static std::string require_positional(const ParsedOptions &p,
                                          std::string_view cmd,
                                          std::string_view what)
    {
        if (p.positionals.empty())
            util::fatal(ezmk::i18n::I18nKey::cli_arg_required,
                        {{"cmd", std::string(cmd)},
                         {"what", std::string(what)}});
        if (p.positionals.size() > 1)
            util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                        {{"cmd", std::string(cmd)},
                         {"what", std::string(what)}});
        return p.positionals[0];
    }

    // 至多一个 positional（允许为空，>1 报错）。
    static std::string optional_positional(const ParsedOptions &p,
                                           std::string_view cmd,
                                           std::string_view what)
    {
        if (p.positionals.size() > 1)
            util::fatal(ezmk::i18n::I18nKey::cli_too_many_args,
                        {{"cmd", std::string(cmd)},
                         {"what", std::string(what)}});
        return p.positionals.empty() ? "" : p.positionals[0];
    }

    // 不允许任何 positional。
    static void reject_positionals(const ParsedOptions &p, std::string_view cmd)
    {
        if (!p.positionals.empty())
            util::fatal(ezmk::i18n::I18nKey::cli_unexpected_arg,
                        {{"cmd", std::string(cmd)}, {"arg", p.positionals[0]}});
    }

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

            args.project_name = require_positional(
                p, "ezmk project new",
                ezmk::i18n::get(ezmk::i18n::I18nKey::arg_project_name));

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
                {'\0', "compile-commands", false}, // 1.1.1
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
                reject_positionals(p, "ezmk project watch");
            }
            else // build
            {
                reject_positionals(p, "ezmk project build");
            }
            return args;
        }

        if (action == "clean")
        {
            args.cmd = Command::ProjectClean;
            return args;
        }

        // 1.1.0: project install
        if (action == "install")
        {
            args.cmd = Command::ProjectInstall;
            std::vector<OptionSpec> spec = {
                {'v', "verbose", false},
                {'\0', "prefix", true},
                {'\0', "dry-run", false},
                {'\0', "no-headers", false},
                {'\0', "no-data", false},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project install");
            if (p.has("verbose"))       args.project_install_opts.verbose = true;
            if (p.has("dry-run"))       args.project_install_opts.dry_run = true;
            if (p.has("no-headers"))    args.project_install_opts.no_headers = true;
            if (p.has("no-data"))       args.project_install_opts.no_data = true;
            if (auto v = p.value("prefix"))
                args.project_install_opts.prefix = *v;
            return args;
        }

        // 1.1.0-dev.2: project pack
        if (action == "pack")
        {
            args.cmd = Command::ProjectPack;
            ProjectPackOptions opts;
            std::vector<OptionSpec> spec = {
                {'v', "verbose", false},
                {'\0', "output", true},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project pack");
            if (p.has("verbose"))     opts.verbose = true;
            if (auto v = p.value("output"))
                opts.output_dir = *v;
            else
                opts.output_dir = ".";
            args.project_pack_opts = opts;
            return args;
        }

        // 1.2.0: project cc — generate compile_commands.json
        if (action == "cc")
        {
            args.cmd = Command::ProjectCc;
            ProjectCcOptions opts;
            std::vector<OptionSpec> spec = {
                {'o', "output", true},
                {'\0', "profile", true},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project cc");
            if (auto v = p.value("output"))
                opts.output = *v;
            if (auto v = p.value("profile"))
                opts.profile = *v;
            reject_positionals(p, "ezmk project cc");
            args.project_cc_opts = opts;
            return args;
        }

        // 1.1.0-dev.6: project test
        if (action == "test")
        {
            args.cmd = Command::ProjectTest;
            std::vector<OptionSpec> spec = {
                {'f', "framework", true},
                {'\0', "filter", true},
                {'V', "verbose", false},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project test");
            if (auto v = p.value("framework"))
                args.test_framework = *v;
            if (auto v = p.value("filter"))
                args.test_filter = *v;
            if (p.has("verbose"))
                args.test_verbose = true;
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
                {'\0', "locked", false},    // 1.1.0
                {'\0', "no-lock", false},   // 1.1.0
            };
            add_scope_specs(spec);
            add_verbose_spec(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk pkg install");

            auto scopes = collect_scopes(p);
            if (scopes.size() > 1)
                util::fatal(ezmk::i18n::I18nKey::cli_one_scope,
                            {{"cmd", "ezmk pkg install"}});

            InstallOptions opts;
            opts.pkg_file = require_positional(
                p, "ezmk pkg install",
                ezmk::i18n::get(ezmk::i18n::I18nKey::arg_package_arg));
            opts.scope = scopes.empty() ? Scope::Project : scopes[0];
            if (auto s = p.value("sha256"))
                opts.sha256 = *s;
            if (p.has("yes"))
                opts.assume_yes = true;
            if (p.has("locked"))       // 1.1.0
                opts.locked = true;
            if (p.has("no-lock"))      // 1.1.0
                opts.no_lock = true;
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
            add_verbose_spec(spec);
            auto p = parse_options(argc, argv, 3, spec,
                                   "ezmk pkg " + std::string(action));

            QueryOptions opts;
            opts.pkg_name = require_positional(
                p, "ezmk pkg " + std::string(action),
                ezmk::i18n::get(ezmk::i18n::I18nKey::arg_package_name));
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
            add_verbose_spec(spec);
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
            add_verbose_spec(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk pkg update");

            QueryOptions opts;
            opts.update_all = p.has("all");
            std::string pkg_name = optional_positional(
                p, "ezmk pkg update",
                ezmk::i18n::get(ezmk::i18n::I18nKey::arg_package_name));
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
            add_verbose_spec(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk repo add");

            auto scopes = collect_scopes(p);
            if (scopes.size() > 1)
                util::fatal(ezmk::i18n::I18nKey::cli_one_scope, {{"cmd", "ezmk repo add"}});

            RepoOptions opts;
            opts.url = require_positional(
                p, "ezmk repo add",
                ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_url));
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
            add_verbose_spec(spec);
            auto p = parse_options(argc, argv, 3, spec,
                                   "ezmk repo " + std::string(action));

            std::string name = optional_positional(
                p, "ezmk repo " + std::string(action),
                ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_name));
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
            add_verbose_spec(spec);
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
            add_verbose_spec(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk repo info");

            RepoOptions opts;
            opts.name = require_positional(
                p, "ezmk repo info",
                ezmk::i18n::get(ezmk::i18n::I18nKey::arg_repo_name));
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
            // 1.1.3 C5: argv 项若含嵌入 NUL，std::string(argv[i]) 会在 NUL 处截断，
            // 与 C 字符串语义一致（操作系统不会传入真正含 NUL 的 argv）。已知限制：
            // 完整防御（检测截断/显式拒绝）归 1.2.0（TODO）。
            toks.emplace_back(argv[i]);
        strip_color_option(toks);

        if (toks.size() < 2)
        {
            args.cmd = Command::Help;
            return args;
        }

        // 0.9.8+: pre-scan for --verbose/-v before shorthand expansion,
        // so we can record the expansion hint when verbose mode is active.
        // Stop at "--" (end-of-options marker — what follows is positional args).
        bool any_verbose = false;
        for (int i = 1; i < argc; ++i) {
            std::string_view a(argv[i]);
            if (a == "--") break;
            if (a == "-v" || a == "--verbose") {
                any_verbose = true;
                break;
            }
        }

        // 0.2.6+: top-level command shorthands. Expand toks[1] into its full
        // group[/action] form BEFORE any further parsing, so downstream logic
        // and error messages all see the canonical command names. Aliases only
        // apply at the command position (e.g. `ezmk project pn` is NOT an alias
        // and correctly reports an unknown project subcommand).
        static const std::map<std::string_view,
                               std::pair<const char *, const char *>>
            kAliases = {
                // 1.1.0-pre.1: top-level aliases (natural language commands)
                {"build",  {"project", "build"}},
                {"run",    {"project", "run"}},
                {"clean",  {"project", "clean"}},
                {"watch",  {"project", "watch"}},
                {"install",{"project", "install"}},
                {"test",   {"project", "test"}},
                {"pack",   {"project", "pack"}},
                // 0.2.6+: two-letter shorthands
                {"pn", {"project", "new"}},   {"pb", {"project", "build"}},
                {"pr", {"project", "run"}},   {"pc", {"project", "clean"}},
                {"pi", {"project", "install"}}, {"pp", {"project", "pack"}},
                {"pw", {"project", "watch"}},
                {"pt", {"project", "test"}},       // 1.1.0-dev.6
                {"ki", {"pkg", "install"}},
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
            // 0.9.8+: record expansion for --verbose display
            if (any_verbose) {
                std::string alias(toks[1]);
                std::string full = it->second.first;
                if (it->second.second)
                    full += std::string(" ") + it->second.second;
                args.shorthand_expansion = alias + " → " + full;
            }

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

        if (group == "project") {
            auto result = parse_project_args(argc, argv);
            result.shorthand_expansion = std::move(args.shorthand_expansion);
            return result;
        }
        if (group == "pkg") {
            auto result = parse_pkg_args(argc, argv);
            result.shorthand_expansion = std::move(args.shorthand_expansion);
            return result;
        }
        if (group == "repo") {
            auto result = parse_repo_args(argc, argv);
            result.shorthand_expansion = std::move(args.shorthand_expansion);
            return result;
        }
        if (group == "utils") {
            auto result = parse_utils_args(argc, argv);
            result.shorthand_expansion = std::move(args.shorthand_expansion);
            return result;
        }

        util::error(ezmk::i18n::fmt(ezmk::i18n::I18nKey::cli_unknown_command,
                                    {{"cmd", std::string(group)}}));
        // 0.9.4+: suggest closest matching command
        {
            std::vector<std::string> cmds = {
                "project", "pkg", "repo", "utils", "help", "version"
            };
            auto matches = util::closest_match(std::string(group), cmds, 2);
            if (!matches.empty()) {
                std::string suggestion = matches[0];
                for (size_t i = 1; i < matches.size() && i < 3; ++i)
                    suggestion += ", " + matches[i];
                util::error(ezmk::i18n::I18nKey::cli_did_you_mean,
                            {{"suggestion", suggestion}});
            }
            std::string avail;
            for (size_t i = 0; i < cmds.size(); ++i) {
                if (i > 0) avail += ", ";
                avail += cmds[i];
            }
            util::error(ezmk::i18n::I18nKey::cli_available_commands,
                        {{"commands", avail}});
        }
        args.cmd = Command::Help;
        return args;
    }

    void print_usage()
    {
        using namespace ezmk::i18n;

        // Render one command row: a literal usage string left-padded to a fixed
        // column, followed by the localized description.
        auto row = [](const std::string &usage, I18nKey desc) {
            std::cout << "  " << std::left << std::setw(52) << usage
                      << get(desc) << "\n";
        };
        // Render an indented continuation line (e.g. full command form).
        auto sub = [](const std::string &text) {
            std::cout << "       " << text << "\n";
        };

        std::cout << get(I18nKey::cli_usage_header) << "\n\n"
                  << get(I18nKey::cli_usage_usage) << ":\n\n";

        // ── §1: Daily build commands ──────────────────────────────
        std::cout << get(I18nKey::help_section_daily) << "\n";
        row("ezmk build    [flags]", I18nKey::help_project_build);
        sub(get(I18nKey::help_full_form) + ": ezmk project build");
        row("ezmk run      [flags] [-- args]", I18nKey::help_project_run);
        sub(get(I18nKey::help_full_form) + ": ezmk project run");
        row("ezmk clean", I18nKey::help_project_clean);
        sub(get(I18nKey::help_full_form) + ": ezmk project clean");
        row("ezmk watch    [flags]", I18nKey::help_project_watch);
        sub(get(I18nKey::help_full_form) + ": ezmk project watch");
        row("ezmk install  [flags]", I18nKey::help_project_install);
        sub(get(I18nKey::help_full_form) + ": ezmk project install");
        row("ezmk test     [flags]", I18nKey::help_project_test);
        sub(get(I18nKey::help_full_form) + ": ezmk project test");
        std::cout << "\n";

        // ── §2: Project init ─────────────────────────────────────
        std::cout << get(I18nKey::help_section_init) << "\n";
        row("ezmk project new  <name> [--type <t>]", I18nKey::help_project_new);
        row("ezmk project pack [--output <dir>]", I18nKey::help_project_pack);
        row("ezmk project cc   [-o <path>] [--profile <p>]", I18nKey::help_project_cc);
        std::cout << "\n";

        // ── §3: Package & repo management (advanced) ──────────────
        std::cout << get(I18nKey::help_section_advanced) << "\n";
        row("ezmk pkg install  [flags] <pkg>", I18nKey::help_pkg_install);
        row("ezmk pkg remove   [-p|-u|-g] <pkg>", I18nKey::help_pkg_remove);
        row("ezmk pkg search   [-p|-u|-g] <pkg>", I18nKey::help_pkg_search);
        row("ezmk pkg info     [-p|-u|-g] <pkg>", I18nKey::help_pkg_info);
        row("ezmk pkg list     [-p|-u|-g]", I18nKey::help_pkg_list);
        row("ezmk pkg update   [-p|-u|-g] [--all] [<pkg>]", I18nKey::help_pkg_update);
        std::cout << "\n";
        row("ezmk repo add     [flags] <url_or_path>", I18nKey::help_repo_add);
        row("ezmk repo remove  [-p|-u|-g] <name>", I18nKey::help_repo_remove);
        row("ezmk repo update  [-p|-u|-g] [<name>]", I18nKey::help_repo_update);
        row("ezmk repo list    [-p|-u|-g]", I18nKey::help_repo_list);
        row("ezmk repo info    [-p|-u|-g] <name>", I18nKey::help_repo_info);
        std::cout << "\n";

        // ── §4: Other ────────────────────────────────────────────
        std::cout << get(I18nKey::help_section_other) << "\n";
        row("ezmk utils   <name> [-- args]", I18nKey::help_utils);
        row("ezmk help", I18nKey::help_help);
        row("ezmk version", I18nKey::help_version);
        std::cout << "\n";

        // ── §5: Common options ────────────────────────────────────
        std::cout << get(I18nKey::help_section_options) << "\n";
        std::cout << get(I18nKey::cli_usage_scopes) << "\n";
        row("-p", I18nKey::help_scope_project);
        row("-u", I18nKey::help_scope_user);
        row("-g", I18nKey::help_scope_global);
        std::cout << "  " << get(I18nKey::help_scope_combined) << "\n\n";

        row("--disable-cache", I18nKey::help_flag_disable_cache);
        row("--verbose, -v", I18nKey::help_flag_verbose);
        row("-j, --jobs <N>", I18nKey::help_flag_jobs);
        row("--profile <name>", I18nKey::help_flag_profile);
        row("--auto-update", I18nKey::help_flag_auto_update);
        row("--sha256 <hash>", I18nKey::help_flag_sha256);
        row("-y, --yes", I18nKey::help_flag_yes);
        row("--color=<mode>", I18nKey::help_flag_color);
        std::cout << "\n";

        // GNU-style option syntax note.
        std::cout << get(I18nKey::help_option_syntax_title) << "\n"
                  << "  " << get(I18nKey::help_option_syntax_long) << "\n"
                  << "  " << get(I18nKey::help_option_syntax_short) << "\n"
                  << "  " << get(I18nKey::help_option_syntax_dashdash) << "\n";
    }

} // namespace ezmk::cli
