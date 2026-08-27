// Unit tests for the CMake importer (1.2.0, experimental).
// Exercises import_cmake_text(): light parser (quotes/comments/brackets),
// core §3.2 mapping, variable expansion, find_package best-effort,
// conditional compilation best-effort, and transactional rejection.
#include "catch2.hpp"
#include "test_helpers.hpp"
#include "ezmk/import.hpp"
#include "ezmk/util.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path test_root() { return fs::path("import_proj"); }

} // namespace

using ezmk::fatal_error;

TEST_CASE("import: project fields (name/version/type/language)", "[import][1.2.0]") {
    auto t = ezmk::import::import_cmake_text(
        "cmake_minimum_required(VERSION 3.16)\n"
        "project(myapp VERSION 1.2.3 LANGUAGES CXX)\n"
        "add_executable(myapp src/main.cpp)\n",
        test_root());
    CHECK(t.find("name = \"myapp\"") != std::string::npos);
    CHECK(t.find("version = \"1.2.3\"") != std::string::npos);
    CHECK(t.find("type = \"executable\"") != std::string::npos);
    CHECK(t.find("language = \"C++17\"") != std::string::npos);
    CHECK(t.find("src_dirs = [\"src\"]") != std::string::npos);
}

// 1.2.0-dev.11: target_* argument scanning is keyword-aware — the legacy
// no-keyword forms are not lost, and PRIVATE/PUBLIC tokens are skipped instead
// of being collected as paths/options.
TEST_CASE("import: target_* keyword-aware parsing (dev.11)", "[import][1.2.0-dev.11]") {
    // Legacy no-keyword 2-arg form — previously gated out entirely.
    auto t1 = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app src/main.cpp)\n"
        "target_link_libraries(app m)\n",
        test_root());
    CHECK(t1.find("system_target = [\"m\"]") != std::string::npos);

    // PRIVATE/PUBLIC mixed — keywords skipped, content collected.
    auto t2 = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app src/main.cpp)\n"
        "target_include_directories(app PRIVATE inc1 PUBLIC inc2)\n"
        "target_compile_options(app PRIVATE -Wall PUBLIC -O2)\n",
        test_root());
    CHECK(t2.find("include_dirs = [\"inc1\", \"inc2\"]") != std::string::npos);
    CHECK(t2.find("-Wall") != std::string::npos);
    CHECK(t2.find("-O2") != std::string::npos);
    CHECK(t2.find("\"PRIVATE\"") == std::string::npos);
    CHECK(t2.find("\"PUBLIC\"") == std::string::npos);
}

TEST_CASE("import: add_library maps type (STATIC/SHARED)", "[import][1.2.0]") {
    auto shared = ezmk::import::import_cmake_text(
        "project(mylib LANGUAGES CXX)\n"
        "add_library(mylib SHARED src/a.cpp)\n",
        test_root());
    CHECK(shared.find("type = \"shared\"") != std::string::npos);

    auto stat = ezmk::import::import_cmake_text(
        "project(mylib LANGUAGES CXX)\n"
        "add_library(mylib STATIC src/a.cpp)\n",
        test_root());
    CHECK(stat.find("type = \"static\"") != std::string::npos);
}

TEST_CASE("import: ${VAR} single-level expansion", "[import][1.2.0]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "set(SRCS src/a.cpp src/b.cpp)\n"
        "set(INCS include include/extra)\n"
        "add_executable(app ${SRCS})\n"
        "target_include_directories(app PRIVATE ${INCS})\n",
        test_root());
    CHECK(t.find("src_dirs = [\"src\"]") != std::string::npos);
    // include 保留顺序去重
    CHECK(t.find("include_dirs = [\"include\", \"include/extra\"]") != std::string::npos);
}

TEST_CASE("import: quoted strings and inline comments parse", "[import][1.2.0]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app main.cpp)\n"
        "target_include_directories(app PRIVATE \"my inc\")\n"
        "target_compile_options(app PRIVATE -O2 # inline comment\n -Wall)\n",
        test_root());
    CHECK(t.find("include_dirs = [\"my inc\"]") != std::string::npos);
    CHECK(t.find("flags = [\"-O2\", \"-Wall\"]") != std::string::npos);
}

TEST_CASE("import: target_compile_definitions and link libraries", "[import][1.2.0]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app main.cpp)\n"
        "target_compile_definitions(app PRIVATE FOO=1 BAR)\n"
        "target_link_libraries(app PRIVATE pthread)\n",
        test_root());
    CHECK(t.find("\"FOO\" = \"1\"") != std::string::npos);
    CHECK(t.find("\"BAR\" = \"\"") != std::string::npos);
    CHECK(t.find("system_target = [\"pthread\"]") != std::string::npos);
}

TEST_CASE("import: find_package → commented [depends] entries", "[import][1.2.0]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "find_package(OpenSSL REQUIRED)\n"
        "find_package(Boost 1.82 REQUIRED)\n"
        "add_executable(app main.cpp)\n",
        test_root());
    // 别名表：OpenSSL → openssl；版本约束 boost@1.82
    CHECK(t.find("boost@1.82") != std::string::npos);
    CHECK(t.find("openssl") != std::string::npos);
    CHECK(t.find("# lib = [\"boost@1.82\"]") != std::string::npos);
    // 已识别的 openssl 不应进 system_target
    CHECK(t.find("system_target = []") != std::string::npos);
}

TEST_CASE("import: conditional compilation takes current platform branch", "[import][1.2.0]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app main.cpp)\n"
        "if(WIN32)\n"
        "  target_compile_definitions(app PRIVATE IS_WIN=1)\n"
        "elseif(UNIX)\n"
        "  target_compile_definitions(app PRIVATE IS_UNIX=1)\n"
        "endif()\n",
        test_root());
    CHECK(t.find("IS_WIN") != std::string::npos);
    CHECK(t.find("IS_UNIX") == std::string::npos);  // Windows 平台 → UNIX 分支跳过
}

TEST_CASE("import: unevaluable condition skips block and leaves TODO", "[import][1.2.0]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app main.cpp)\n"
        "if(SOME_CUSTOM_VAR)\n"
        "  target_compile_definitions(app PRIVATE SHOULD_SKIP=1)\n"
        "endif()\n",
        test_root());
    CHECK(t.find("SHOULD_SKIP") == std::string::npos);
    CHECK(t.find("# TODO: 未求值的条件块") != std::string::npos);
}

TEST_CASE("import: reject custom commands transactionally", "[import][1.2.0]") {
    CHECK_THROWS_AS(ezmk::import::import_cmake_text(
                        "project(app LANGUAGES CXX)\n"
                        "add_executable(app main.cpp)\n"
                        "add_custom_command(TARGET app POST_BUILD COMMAND echo hi)\n",
                        test_root()),
                    fatal_error);
}

TEST_CASE("import: reject generator expressions transactionally", "[import][1.2.0]") {
    CHECK_THROWS_AS(ezmk::import::import_cmake_text(
                        "project(app LANGUAGES CXX)\n"
                        "add_executable(app main.cpp)\n"
                        "target_compile_options(app PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Werror>)\n",
                        test_root()),
                    fatal_error);
}

TEST_CASE("import: reject function/macro definitions transactionally", "[import][1.2.0]") {
    CHECK_THROWS_AS(ezmk::import::import_cmake_text(
                        "project(app LANGUAGES CXX)\n"
                        "add_executable(app main.cpp)\n"
                        "function(foo) add_executable(bar main.cpp) endfunction()\n",
                        test_root()),
                    fatal_error);
}

// ===================================================================
// 1.4.0-dev.4: import reads CXX_STANDARD / C_STANDARD / target_compile_features
// → range language (">=CPP<N>", semantics A)
// ===================================================================

TEST_CASE("import: CXX_STANDARD maps to a >=CPP<N> range (dev.4)", "[import][1.4.0-dev.4]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app src/main.cpp)\n"
        "set_target_properties(app PROPERTIES CXX_STANDARD 17)\n",
        test_root());
    CHECK(t.find("language = \">=CPP17\"") != std::string::npos);
    CHECK(t.find("language = \"C++17\"") == std::string::npos);  // not the hardcode
}

TEST_CASE("import: CXX_STANDARD with CXX_EXTENSIONS OFF stays non-GNU (dev.4)", "[import][1.4.0-dev.4]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app src/main.cpp)\n"
        "set_target_properties(app PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)\n",
        test_root());
    CHECK(t.find("language = \">=CPP20\"") != std::string::npos);
    CHECK(t.find("GNUCPP") == std::string::npos);
}

TEST_CASE("import: CXX_EXTENSIONS ON maps to a GNU prefix (dev.4)", "[import][1.4.0-dev.4]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app src/main.cpp)\n"
        "set_target_properties(app PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)\n",
        test_root());
    CHECK(t.find("language = \">=GNUCPP17\"") != std::string::npos);
}

TEST_CASE("import: target_compile_features cxx_std_N is scanned (dev.4)", "[import][1.4.0-dev.4]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app src/main.cpp)\n"
        "target_compile_features(app PRIVATE cxx_std_20)\n",
        test_root());
    CHECK(t.find("language = \">=CPP20\"") != std::string::npos);
}

TEST_CASE("import: C_STANDARD maps a C project (dev.4)", "[import][1.4.0-dev.4]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES C)\n"
        "add_executable(app src/main.c)\n"
        "set_target_properties(app PROPERTIES C_STANDARD 11)\n",
        test_root());
    CHECK(t.find("language = \">=C11\"") != std::string::npos);
}

TEST_CASE("import: no standard keeps the C++17 fallback (dev.4)", "[import][1.4.0-dev.4]") {
    auto t = ezmk::import::import_cmake_text(
        "project(app LANGUAGES CXX)\n"
        "add_executable(app src/main.cpp)\n",
        test_root());
    CHECK(t.find("language = \"C++17\"") != std::string::npos);
    CHECK(t.find(">=") == std::string::npos);
}
