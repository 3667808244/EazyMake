#include "ezmk/project.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include <sstream>
#include <string>

namespace ezmk::project {

void create_project(const std::string& name, const std::string& project_type,
                    bool disable_git_init, bool disable_gitignore) {
    fs::path root = fs::current_path() / name;

    if (util::file_exists(root)) {
        util::fatal("directory already exists: " + root.string());
    }

    util::info(ezmk::i18n::I18nKey::creating_project,
               {{"name", name}, {"type", project_type}});

    // Directory structure
    fs::create_directories(root / "include");
    fs::create_directories(root / "src");
    fs::create_directories(root / "build");
    fs::create_directories(root / ".ezmk/pkg");
    fs::create_directories(root / ".ezmk/temp");
    fs::create_directories(root / ".ezmk/cache");

    // src/main.cpp (only for executable — library projects may not need it,
    // but we still create it as a starting point)
    std::string main_cpp = R"(#include <iostream>

int main(int argc, char **argv){
    std::cout << "Hello world!" << std::endl;
    return 0;
}
)";
    util::file_write(root / "src/main.cpp", main_cpp);

    // ezmk.toml
    config::write_default_config(root / "ezmk.toml", name, project_type);

    // README.md (empty)
    util::file_write(root / "README.md", "");

    // .gitignore (can be disabled)
    if (!disable_gitignore) {
        std::string gitignore = R"(# EazyMake build artifacts
build/
.ezmk/
*.o
*.obj
*.tmp.o
*.tmp.obj
)";
        util::file_write(root / ".gitignore", gitignore);
    }

    // git init (can be disabled, only runs if git is available)
    if (!disable_git_init) {
        if (util::git_available()) {
            util::info(ezmk::i18n::I18nKey::init_git);
            std::ostringstream cmd;
            cmd << "git init \""
                << util::escape_shell_arg(root.string()) << "\"";
            auto res = util::run_command(cmd.str());
            if (res.exit_code == 0) {
                util::info(ezmk::i18n::I18nKey::git_initialized);
            } else {
                util::warn(ezmk::i18n::I18nKey::git_init_failed,
                           {{"code", std::to_string(res.exit_code)}});
                if (!res.err.empty()) util::warn(res.err);
            }
        } else {
            util::info(ezmk::i18n::I18nKey::git_not_found);
        }
    }

    util::info(ezmk::i18n::I18nKey::project_created, {{"path", root.string()}});
}

} // namespace ezmk::project
