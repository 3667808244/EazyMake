#include "ezmk/project.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"

#include <string>

namespace ezmk::project {

void create_project(const std::string& name) {
    fs::path root = fs::current_path() / name;

    if (util::file_exists(root)) {
        util::fatal("directory already exists: " + root.string());
    }

    util::info("Creating project: " + name);

    // Directory structure
    fs::create_directories(root / "include");
    fs::create_directories(root / "src");
    fs::create_directories(root / "build");
    fs::create_directories(root / ".ezmk/pkg");
    fs::create_directories(root / ".ezmk/temp");
    fs::create_directories(root / ".ezmk/cache");

    // src/main.cpp
    std::string main_cpp = R"(#include <iostream>

int main(int argc, char **argv){
    std::cout << "Hello world!" << std::endl;
    return 0;
}
)";
    util::file_write(root / "src/main.cpp", main_cpp);

    // ezmk.toml
    config::write_default_config(root / "ezmk.toml", name);

    // README.md (empty)
    util::file_write(root / "README.md", "");

    util::info("Project created at: " + root.string());
}

} // namespace ezmk::project
