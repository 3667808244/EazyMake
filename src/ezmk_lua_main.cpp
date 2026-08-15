// ezmk-lua — EazyMake standalone, unrestricted Lua hook runtime.
//
// 1.2.0-dev.8: runs a single `[hooks]` script (pre_build/post_build) in the
// FULL Lua global environment — a strict superset of the sandboxed `ezmk`
// runtime. The `ezmk.*` API is registered with the injected project root so
// config-reading functions work without a full build context.
//
// The exported CMakeLists.txt finds this binary via `find_program(EZMK_LUA
// ezmk-lua)` and calls it from add_custom_command to reproduce `ezmk build`'s
// hook post-processing.
//
// Usage:
//   ezmk-lua <hook.lua> [--project-root <dir>] [--profile <name>] [--output <path>]

#include "ezmk/lua_api.hpp"
#include "ezmk/util.hpp"
#include "ezmk/i18n.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static void print_usage() {
    std::cout << ezmk::i18n::get(ezmk::i18n::I18nKey::ezmk_lua_usage) << "\n";
}

int main(int argc, char** argv) {
    // Init console + locale first so usage/error output (below) is localized.
    ezmk::util::init_console();
    ezmk::i18n::init();  // detect language, load locale data

    std::string script_path;
    std::string project_root;
    std::string profile;
    std::string output;

    auto cli_err = [](ezmk::i18n::I18nKey key, const std::string& arg) {
        return "ezmk-lua: " + ezmk::i18n::fmt(key, {{"option", arg}, {"arg", arg}});
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        }
        auto need_value = [&](const char* name) -> bool {
            if (i + 1 < argc) return true;
            std::cerr << cli_err(ezmk::i18n::I18nKey::ezmk_lua_need_value, name) << "\n";
            return false;
        };
        if (arg == "--project-root") {
            if (!need_value("--project-root")) return 2;
            project_root = argv[++i];
        } else if (arg == "--profile") {
            if (!need_value("--profile")) return 2;
            profile = argv[++i];
        } else if (arg == "--output") {
            if (!need_value("--output")) return 2;
            output = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << cli_err(ezmk::i18n::I18nKey::ezmk_lua_unknown_option, arg) << "\n";
            return 2;
        } else if (script_path.empty()) {
            script_path = arg;
        } else {
            std::cerr << cli_err(ezmk::i18n::I18nKey::ezmk_lua_extra_arg, arg) << "\n";
            return 2;
        }
    }

    if (script_path.empty()) {
        std::cerr << "ezmk-lua: "
                  << ezmk::i18n::get(ezmk::i18n::I18nKey::ezmk_lua_missing_script)
                  << "\n\n";
        print_usage();
        return 2;
    }

    ezmk::lua::init();   // initialize the shared Lua state

    int rc = ezmk::lua::run_script_unrestricted(
        ezmk::lua::state(), fs::path(script_path), project_root, profile, output);

    ezmk::lua::shutdown();
    return rc;
}
