#include "ezmk/cli.hpp"
#include "ezmk/config.hpp"
#include "ezmk/project.hpp"
#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/pkg.hpp"
#include "ezmk/util.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        auto args = ezmk::cli::parse(argc, argv);

        switch (args.cmd) {

        case ezmk::cli::Command::Help:
            ezmk::cli::print_usage();
            break;

        case ezmk::cli::Command::ProjectNew:
            ezmk::project::create_project(*args.project_name, args.project_type);
            break;

        case ezmk::cli::Command::Version:
            std::cout << "EazyMake 0.1.2" << std::endl;
            break;

        case ezmk::cli::Command::ProjectBuild: {
            auto cfg = ezmk::config::parse_config("ezmk.toml");
            ezmk::build::build_project(cfg, args.build_opts);
            break;
        }

        case ezmk::cli::Command::ProjectRun: {
            auto cfg = ezmk::config::parse_config("ezmk.toml");
            auto exe = ezmk::build::build_project(cfg, args.build_opts);
            ezmk::util::info("Running " + exe.filename().string() + "...");
            auto res = ezmk::util::run_command("\"" + exe.string() + "\"");
            if (!res.out.empty()) std::cout << res.out;
            if (!res.err.empty()) std::cerr << res.err;
            if (res.exit_code != 0) {
                return res.exit_code;
            }
            break;
        }

        case ezmk::cli::Command::ProjectClean:
            ezmk::cache::clear_cache();
            ezmk::util::remove_all(".ezmk/temp");
            ezmk::util::info("Cleaned cache and temp files");
            break;

        case ezmk::cli::Command::PkgInstall: {
            auto& opts = *args.install_opts;
            ezmk::pkg::install(opts.pkg_file, opts.scope);
            break;
        }

        case ezmk::cli::Command::PkgRemove: {
            auto& opts = *args.query_opts;
            ezmk::pkg::remove(opts.pkg_name, opts.scopes);
            break;
        }

        case ezmk::cli::Command::PkgSearch: {
            auto& opts = *args.query_opts;
            auto results = ezmk::pkg::search(opts.pkg_name, opts.scopes);
            if (results.empty()) {
                ezmk::util::info("package not found: " + opts.pkg_name);
            } else {
                for (auto& p : results) {
                    std::cout << p.string() << "\n";
                }
            }
            break;
        }

        case ezmk::cli::Command::PkgInfo: {
            auto& opts = *args.query_opts;
            ezmk::pkg::info(opts.pkg_name, opts.scopes);
            break;
        }

        case ezmk::cli::Command::RepoAdd:
        case ezmk::cli::Command::RepoUpdate:
        case ezmk::cli::Command::RepoRemove:
        case ezmk::cli::Command::RepoList:
            ezmk::util::info("repo subcommand not yet implemented (placeholder)");
            break;

        } // switch

    } catch (const std::exception& e) {
        ezmk::util::fatal(e.what());
    }

    return 0;
}
