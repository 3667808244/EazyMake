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
    // 1.3.0-dev.2: `ezmk workspace` command group helpers
    //   workspace list
    //   workspace build [-j N] [--stop-on-error] [--member <name>...]
    //   workspace test  [-j N] [--stop-on-error] [--member <name>...]
    //   workspace clean [--member <name>...]
    // Declared before parse_project_args so the -w redirect (build/test/clean)
    // can reuse them.
    // ===================================================================

    // Option spec shared by workspace build/test (clean has its own reduced
    // spec — no -j, no --stop-on-error). `-w` is accepted and ignored: the
    // redirect (`ezmk build -w`) re-parses the same argv with this spec, so
    // the -w token must not be an "unknown option".
    static std::vector<OptionSpec> workspace_cmd_spec()
    {
        std::vector<OptionSpec> spec = {
            {'w', "workspace", false},
            {'j', "jobs", true},
            {'\0', "stop-on-error", false},
            {'\0', "member", true},
            {'\0', "report", true},   // 1.3.2: test only — forwarded to members
        };
        // 1.3.3: accept -v/--verbose so the shorthand expansion hint
        // (`ezmk wb -v` → "wb → workspace build") can display — consistent
        // with every other command group (0.9.8 design). Workspace has no
        // per-command verbose semantics; the flag is accepted and ignored.
        add_verbose_spec(spec);
        return spec;
    }

    // 1.3.2: --report is test-only; reject it on workspace build/clean/list.
    static void reject_report_on_non_test(const WorkspaceOptions& w,
                                          std::string_view cmd) {
        if (!w.test_report.empty())
            util::fatal(ezmk::i18n::fmt(ezmk::i18n::I18nKey::cli_report_test_only,
                                        {{"cmd", std::string(cmd)}}));
    }

    // Read workspace-level options from a parsed option set — -j/--jobs
    // (0 = auto), --stop-on-error, repeatable --member.
    static WorkspaceOptions parse_workspace_opts(const ParsedOptions &p)
    {
        WorkspaceOptions w;
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
            w.jobs = j;
        }
        if (p.has("stop-on-error"))
            w.stop_on_error = true;
        if (auto v = p.value("report")) {
            // 1.3.2: <fmt>[:<path>] — same shape as `ezmk test --report`.
            std::string val = *v;
            auto colon = val.find(':');
            std::string fmt = colon == std::string::npos ? val : val.substr(0, colon);
            if (fmt.empty() ||
                fmt.find_first_of(" \t\r\n") != std::string::npos)
                util::fatal(ezmk::i18n::I18nKey::cli_err_invalid_report,
                            {{"val", val}});
            w.test_report = std::move(val);
        }
        return w;
    }

    // Collect every --member value in order of appearance
    // (ParsedOptions.options keeps occurrences in order).
    static void fill_members(const ParsedOptions &p, WorkspaceOptions &w)
    {
        for (const auto &[k, v] : p.options)
        {
            if (k == "member")
                w.members.push_back(v);
        }
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
                // 1.3.0-dev.2: -w/--workspace redirect → `ezmk workspace build`
                // (not for run/watch — only build/test/clean per the design).
                spec.push_back({'w', "workspace", false});
                spec.push_back({'\0', "stop-on-error", false});
                spec.push_back({'\0', "member", true});
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
                spec.push_back({'r', "run", false});   // 1.3.4: run after each successful rebuild
            }

            auto p = parse_options(argc, argv, 3, spec, cmd_name);
            fill_build_opts(p, args.build_opts);

            if (action == "build" && p.has("workspace"))
            {
                // 1.3.0-dev.2: `ezmk build -w` ≡ `ezmk workspace build`. Re-parse
                // with the authoritative workspace spec so workspace-foreign
                // flags (--profile, --disable-cache, ...) are rejected.
                args.cmd = Command::WorkspaceBuild;
                auto wp = parse_options(argc, argv, 3, workspace_cmd_spec(),
                                        "ezmk workspace build");
                WorkspaceOptions w = parse_workspace_opts(wp);
                fill_members(wp, w);
                reject_positionals(wp, "ezmk workspace build");
                reject_report_on_non_test(w, "ezmk workspace build");
                args.workspace_opts = std::move(w);
                return args;
            }
            if (action == "build" &&
                (p.has("stop-on-error") || p.has("member")))
            {
                // 1.3.0-dev.2: workspace-only flags without -w are a usage error.
                std::string flag = p.has("stop-on-error") ? "--stop-on-error"
                                                          : "--member";
                util::fatal(ezmk::i18n::I18nKey::cli_flag_needs_workspace,
                            {{"flag", flag}});
            }

            if (action == "run")
            {
                // Positionals (typically after "--") are passed to the program.
                args.program_args = p.positionals;
            }
            else if (action == "watch")
            {
                if (p.has("no-build-on-start"))
                    args.watch_no_build_on_start = true;
                if (p.has("run"))
                    args.watch_run = true;   // 1.3.4: --run / -r
                // 1.4.0-dev.5: positionals (typically after "--") are passed to
                // the watched executable on each run (--run), like `project run`.
                args.program_args = p.positionals;
            }
            else // build
            {
                reject_positionals(p, "ezmk project build");
            }
            return args;
        }

        if (action == "clean")
        {
            // 1.2.0-dev.11: clean previously accepted any garbage silently
            // (--bogus / positionals) — parse it like every other subcommand.
            args.cmd = Command::ProjectClean;
            std::vector<OptionSpec> spec = {
                // 1.3.0-dev.2: -w/--workspace redirect → `ezmk workspace clean`;
                // --member is workspace-only (rejected without -w below).
                {'w', "workspace", false},
                {'\0', "member", true},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project clean");
            reject_positionals(p, "ezmk project clean");

            if (p.has("workspace"))
            {
                args.cmd = Command::WorkspaceClean;
                auto wp = parse_options(argc, argv, 3, workspace_cmd_spec(),
                                        "ezmk workspace clean");
                WorkspaceOptions w = parse_workspace_opts(wp);
                fill_members(wp, w);
                reject_positionals(wp, "ezmk workspace clean");
                reject_report_on_non_test(w, "ezmk workspace clean");
                if (w.stop_on_error)
                    util::fatal(ezmk::i18n::I18nKey::workspace_err_clean_stop_on_error);
                args.workspace_opts = std::move(w);
                return args;
            }
            if (p.has("member"))
                util::fatal(ezmk::i18n::I18nKey::cli_flag_needs_workspace,
                            {{"flag", "--member"}});
            return args;
        }

        // 1.1.0: project install
        if (action == "install")
        {
            args.cmd = Command::ProjectInstall;
            ProjectInstallOptions opts;
            std::vector<OptionSpec> spec = {
                {'v', "verbose", false},
                {'\0', "prefix", true},
                {'\0', "dry-run", false},
                {'\0', "no-headers", false},
                {'\0', "no-data", false},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project install");
            if (p.has("verbose"))       opts.verbose = true;
            if (p.has("dry-run"))       opts.dry_run = true;
            if (p.has("no-headers"))    opts.no_headers = true;
            if (p.has("no-data"))       opts.no_data = true;
            if (auto v = p.value("prefix"))
                opts.prefix = *v;
            args.project_install_opts = opts;
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
                {'\0', "precompiled", false},  // 1.2.5
                {'\0', "format", true},        // 1.3.5: <tar.gz|zip>
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project pack");
            if (p.has("verbose"))     opts.verbose = true;
            if (p.has("precompiled")) opts.precompiled = true;  // 1.2.5
            if (auto v = p.value("output"))
                opts.output_dir = *v;
            else
                opts.output_dir = ".";
            if (auto v = p.value("format")) {
                // 1.3.5: tar.gz / zip (case-insensitive); anything else → fatal.
                std::string fmt = *v;
                for (auto& c : fmt) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (fmt != "tar.gz" && fmt != "zip") {
                    util::fatal(ezmk::i18n::fmt(ezmk::i18n::I18nKey::cli_err_invalid_format,
                                                {{"val", *v}}));
                }
                opts.format = std::move(fmt);
            }
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

        // 1.2.0: project export <target> — generate CMakeLists.txt (cmake first)
        // 1.4.0-dev.1: + vscode — generate .vscode/ launch/tasks/settings
        if (action == "export")
        {
            args.cmd = Command::ProjectExport;
            ProjectExportOptions opts;
            std::vector<OptionSpec> spec = {
                {'o', "output", true},
                {'\0', "overwrite", false},
                {'\0', "profile", true},
                {'\0', "resolve", false},
                {'\0', "glob", false},
                {'\0', "no-glob", false},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project export");
            opts.target = require_positional(
                p, "ezmk project export",
                ezmk::i18n::get(ezmk::i18n::I18nKey::arg_export_target));
            if (opts.target != "cmake" && opts.target != "vscode")
                util::fatal(ezmk::i18n::I18nKey::export_unknown_target,
                            {{"target", opts.target}});
            if (auto v = p.value("output"))
                opts.output = *v;
            if (p.has("overwrite"))
                opts.overwrite = true;
            if (auto v = p.value("profile"))
                opts.profile = *v;
            if (p.has("resolve"))
                opts.resolve = true;
            if (p.has("no-glob"))
                opts.use_glob = false;
            // 1.4.0-dev.1: cmake-only flags on the vscode target — explicit
            // refusal beats silently ignoring them (design doc §3.1).
            if (opts.target == "vscode")
            {
                auto refuse = [&](const char* flag) {
                    util::fatal(ezmk::i18n::I18nKey::export_flag_target_mismatch,
                                {{"flag", flag}, {"target", opts.target}});
                };
                if (!opts.output.empty()) refuse("--output");
                if (opts.resolve)             refuse("--resolve");
                if (p.has("glob"))            refuse("--glob");
                if (p.has("no-glob"))         refuse("--no-glob");
            }
            args.project_export_opts = opts;
            return args;
        }

        // 1.2.0: project import [--from <format>] [--overwrite] — import a
        // CMake project into ezmk.toml (experimental, single-direction snapshot)
        if (action == "import")
        {
            args.cmd = Command::ProjectImport;
            ProjectImportOptions opts;
            std::vector<OptionSpec> spec = {
                {'\0', "from", true},
                {'\0', "overwrite", false},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project import");
            reject_positionals(p, "ezmk project import");
            if (auto v = p.value("from"))
            {
                // 大小写不敏感：--from CMAKE / CMake / cmake 等价
                std::string fmt = *v;
                for (auto& c : fmt)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (fmt != "cmake")
                    util::fatal(ezmk::i18n::I18nKey::import_unknown_format,
                                {{"format", *v}});
                opts.from = fmt;
            }
            if (p.has("overwrite"))
                opts.overwrite = true;
            args.project_import_opts = opts;
            return args;
        }

        // 1.1.0-dev.6: project test
        if (action == "test")
        {
            args.cmd = Command::ProjectTest;
            std::vector<OptionSpec> spec = {
                {'f', "framework", true},
                {'\0', "filter", true},
                // 1.2.0-dev.11: -v accepted as an alias for -V (other commands
                // use -v for verbose; -V kept for backward compatibility).
                {'v', "verbose", false},
                {'V', "verbose", false},
                {'\0', "profile", true},   // 1.2.0-dev.12
                {'\0', "report", true},    // 1.3.2: <fmt>[:<path>] machine-readable report
                // 1.3.0-dev.2: -w redirect + workspace-only flags (rejected
                // without -w below).
                {'w', "workspace", false},
                {'j', "jobs", true},
                {'\0', "stop-on-error", false},
                {'\0', "member", true},
            };
            auto p = parse_options(argc, argv, 3, spec, "ezmk project test");
            if (p.has("workspace"))
            {
                args.cmd = Command::WorkspaceTest;
                auto wp = parse_options(argc, argv, 3, workspace_cmd_spec(),
                                        "ezmk workspace test");
                WorkspaceOptions w = parse_workspace_opts(wp);
                fill_members(wp, w);
                reject_positionals(wp, "ezmk workspace test");
                args.workspace_opts = std::move(w);
                return args;
            }
            if (p.has("jobs") || p.has("stop-on-error") || p.has("member"))
            {
                std::string flag = p.has("jobs") ? "-j/--jobs"
                                   : p.has("stop-on-error") ? "--stop-on-error"
                                                            : "--member";
                util::fatal(ezmk::i18n::I18nKey::cli_flag_needs_workspace,
                            {{"flag", flag}});
            }
            if (auto v = p.value("framework"))
                args.test_framework = *v;
            if (auto v = p.value("filter"))
                args.test_filter = *v;
            if (p.has("verbose"))
                args.test_verbose = true;
            if (auto v = p.value("profile"))
                args.test_profile = *v;
            if (auto v = p.value("report")) {
                // 1.3.2: <fmt>[:<path>] — validate the format segment now;
                // framework-specific support is checked in run_tests.
                std::string val = *v;
                auto colon = val.find(':');
                std::string fmt = colon == std::string::npos ? val : val.substr(0, colon);
                if (fmt.empty() ||
                    fmt.find_first_of(" \t\r\n") != std::string::npos)
                    util::fatal(ezmk::i18n::I18nKey::cli_err_invalid_report,
                                {{"val", val}});
                args.test_report = std::move(val);
            }
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
            if (auto s = p.value("sha256")) {
                // 1.2.0-dev.11: validate the hash format up front — a typo'd
                // value used to fail only after a full download+install.
                if (s->size() != 64 ||
                    s->find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                    util::fatal(ezmk::i18n::I18nKey::cli_invalid_sha256,
                                {{"value", *s}});
                }
                opts.sha256 = *s;
            }
            if (p.has("yes"))
                opts.assume_yes = true;
            // 1.2.0-dev.11: --locked and --no-lock are mutually exclusive.
            if (p.has("locked") && p.has("no-lock")) {
                util::fatal(ezmk::i18n::I18nKey::cli_conflicting_flags,
                            {{"cmd", "ezmk pkg install"},
                             {"flags", "--locked and --no-lock"}});
            }
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
    // 1.3.0-dev.2: `ezmk workspace` command group parser
    // ===================================================================

    static CliArgs parse_workspace_args(int argc, char **argv)
    {
        CliArgs args;
        std::string_view action = argv[2];

        if (action == "list")
        {
            args.cmd = Command::WorkspaceList;
            // 1.3.3: accept -v/--verbose (accepted + ignored) so the shorthand
            // expansion hint (`ezmk wl -v` → "wl → workspace list") displays.
            std::vector<OptionSpec> spec;
            add_verbose_spec(spec);
            auto p = parse_options(argc, argv, 3, spec, "ezmk workspace list");
            reject_positionals(p, "ezmk workspace list");
            return args;
        }

        if (action == "build" || action == "test" || action == "clean")
        {
            std::string cmd_name = "ezmk workspace " + std::string(action);
            if (action == "build")
                args.cmd = Command::WorkspaceBuild;
            else if (action == "test")
                args.cmd = Command::WorkspaceTest;
            else
                args.cmd = Command::WorkspaceClean;

            auto p = parse_options(argc, argv, 3, workspace_cmd_spec(), cmd_name);
            WorkspaceOptions w = parse_workspace_opts(p);
            fill_members(p, w);
            reject_positionals(p, cmd_name);

            // 1.3.0-dev.2: clean is a plain batch operation — no dependency
            // semantics, so --stop-on-error is rejected explicitly.
            if (action == "clean" && w.stop_on_error)
                util::fatal(ezmk::i18n::I18nKey::workspace_err_clean_stop_on_error);
            // 1.3.2: --report is test-only.
            if (action != "test")
                reject_report_on_non_test(w, cmd_name);

            args.workspace_opts = std::move(w);
            return args;
        }

        util::fatal(ezmk::i18n::I18nKey::cli_unknown_subcommand,
                    {{"group", "workspace"}, {"sub", std::string(action)}});
    }

    // 1.2.3: `ezmk example` — list built-in examples or scaffold one:
    //   ezmk example                    → list
    //   ezmk example list               → list
    //   ezmk example <name> [-o <dir>]  → scaffold ./<name>/ (or <dir>/<name>/)
    static CliArgs parse_example_args(int argc, char **argv)
    {
        CliArgs args;
        args.cmd = Command::Example;

        if (argc < 3 || std::string(argv[2]) == "list")
        {
            ExampleOptions opts;
            opts.list = true;
            args.example_opts = std::move(opts);
            return args;
        }

        std::vector<OptionSpec> spec = {
            {'o', "output", true},
        };
        auto p = parse_options(argc, argv, 3, spec, "ezmk example");
        reject_positionals(p, "ezmk example");

        ExampleOptions opts;
        opts.name = argv[2];  // fixed positional at index 2
        if (auto o = p.value("output")) opts.output_dir = *o;
        args.example_opts = std::move(opts);
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
                // 1.3.3: workspace two-letter shorthands (组首字母 + 子命令首字母)
                {"wl", {"workspace", "list"}},  {"wb", {"workspace", "build"}},
                {"wt", {"workspace", "test"}},  {"wc", {"workspace", "clean"}},
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
            // 1.2.3: `ezmk example` (no subcommand) means `example list`.
            if (arg1 == "example") {
                auto result = parse_example_args(argc, argv);
                result.shorthand_expansion = std::move(args.shorthand_expansion);
                return result;
            }
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
        if (group == "example") {
            auto result = parse_example_args(argc, argv);
            result.shorthand_expansion = std::move(args.shorthand_expansion);
            return result;
        }
        if (group == "workspace") {   // 1.3.0-dev.2
            auto result = parse_workspace_args(argc, argv);
            result.shorthand_expansion = std::move(args.shorthand_expansion);
            return result;
        }

        util::error(ezmk::i18n::fmt(ezmk::i18n::I18nKey::cli_unknown_command,
                                    {{"cmd", std::string(group)}}));
        // 0.9.4+: suggest closest matching command
        {
            std::vector<std::string> cmds = {
                "project", "pkg", "repo", "utils", "example", "workspace",
                "help", "version"
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
        row("ezmk project pack [--output <dir>] [--precompiled]", I18nKey::help_project_pack);
        row("ezmk project cc   [-o <path>] [--profile <p>]", I18nKey::help_project_cc);
        row("ezmk project export <cmake|vscode> [flags]", I18nKey::help_project_export);
        row("ezmk project import [--from <fmt>] [--overwrite]", I18nKey::help_project_import);
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
        row("ezmk example [<name>] [-o <dir>]", I18nKey::help_example);
        row("ezmk utils   <name> [-- args]", I18nKey::help_utils);
        row("ezmk help", I18nKey::help_help);
        row("ezmk version", I18nKey::help_version);
        std::cout << "\n";

        // ── §4.5: Workspace (1.3.0-dev.2) ─────────────────────────
        std::cout << get(I18nKey::help_section_workspace) << "\n";
        row("ezmk workspace list", I18nKey::help_workspace_list);
        row("ezmk workspace build [-j N] [--stop-on-error] [--member <n>]", I18nKey::help_workspace_build);
        row("ezmk workspace test  [-j N] [--stop-on-error] [--member <n>]", I18nKey::help_workspace_test);
        row("ezmk workspace clean [--member <n>]", I18nKey::help_workspace_clean);
        sub(get(I18nKey::help_full_form) + ": build/test/clean accept -w to redirect");
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
        row("-w, --workspace", I18nKey::help_flag_workspace);   // 1.3.0-dev.2
        row("--stop-on-error", I18nKey::help_flag_stop_on_error);   // 1.3.0-dev.2
        row("--member <name>", I18nKey::help_flag_member);     // 1.3.0-dev.2
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
