#include "ezmk/project.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include <sstream>
#include <string>

namespace ezmk::project {

// 1.2.1: '-', '.', ' ' → '_' so the project name is a valid C++ namespace
// identifier. File names keep the original name (filesystem allows these
// characters); only the C++ identifier needs sanitizing.
std::string sanitize_namespace(const std::string& name) {
    std::string ns = name;
    for (char& c : ns) {
        if (c == '-' || c == '.' || c == ' ') c = '_';
    }
    return ns;
}

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

    // 1.2.1: source templates branch by project type —
    //   executable → src/main.cpp (Hello world entry, unchanged)
    //   static/shared → include/<name>.hpp + src/<name>.cpp library skeleton
    //                   (no main.cpp — a library never links one)
    //   utils → no C++ code at all (utils/ holds Lua scripts)
    if (project_type == "static" || project_type == "shared") {
        const std::string ns = sanitize_namespace(name);

        std::string hpp = "#pragma once\n"
                          "\n"
                          "// " + name + " — 示例公共 API。\n"
                          "// 替换为你的库接口：头文件放 include/，实现放 src/。\n"
                          "\n"
                          "namespace " + ns + " {\n"
                          "\n"
                          "// 示例函数：返回一条问候消息。\n"
                          "const char* greeting();\n"
                          "\n"
                          "} // namespace " + ns + "\n";

        std::string cpp = "#include \"" + name + ".hpp\"\n"
                          "\n"
                          "namespace " + ns + " {\n"
                          "\n"
                          "const char* greeting() {\n"
                          "    return \"Hello from " + name + "!\";\n"
                          "}\n"
                          "\n"
                          "} // namespace " + ns + "\n";

        util::file_write(root / "include" / (name + ".hpp"), hpp);
        util::file_write(root / "src" / (name + ".cpp"), cpp);
    } else if (project_type != "utils") {
        // executable (default) and any other non-utils type: Hello world entry.
        std::string main_cpp = R"(#include <iostream>

int main(int argc, char **argv){
    std::cout << "Hello world!" << std::endl;
    return 0;
}
)";
        util::file_write(root / "src/main.cpp", main_cpp);
    }
    // utils: no C++ code — only the utils/ directory created below.

    // For utils projects, create the utils/ directory for Lua scripts
    if (project_type == "utils") {
        fs::create_directories(root / "utils");
    }

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
