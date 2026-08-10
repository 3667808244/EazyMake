// Unit tests for compile_db.cpp + cache::build_compile_args()/join_shell_args().
// 1.1.1: single-source compile command construction + compile_commands.json.
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "test_helpers.hpp"
#include "ezmk/compile_db.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/config.hpp"
#include "ezmk/toolchain.hpp"
#include "ezmk/util.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::cache;
using namespace ezmk::config;
using namespace ezmk::toolchain;
using namespace ezmk::compile_db;
using namespace ezmk::util;

// ===================================================================
// cache::build_compile_args() — single-source command construction
// ===================================================================

TEST_CASE("build_compile_args: GCC command structure", "[cache][compile_db]") {
    TempDir tmp;
    fs::path proj = tmp.path;
    fs::create_directories(proj / "src");
    fs::create_directories(proj / "include");
    fs::path src = proj / "src" / "main.cpp";
    { std::ofstream(src) << "int main() {}\n"; }

    CompileInput in;
    in.proj_root = proj;
    in.compile.flags = {"-Wall", "-O2"};
    in.compile.include_dirs = {"include"};
    in.lang = parse_language("C++17");
    in.lang.detected_compiler = "g++";
    in.tc.family = CompilerFamily::Gcc;
    in.stdlib = "libstdc++";
    in.dep_dir = proj / ".ezmk" / "temp";
    in.use_pic = true;

    fs::path obj = proj / "build" / "src" / "main.o";
    auto args = build_compile_args(in, src, obj);

    // Compiler + standard flag come first.
    REQUIRE(args.size() >= 8);
    REQUIRE(args[0] == "g++");
    REQUIRE(args[1] == "-std=c++17");
    // Compile mode + base flags + PIC.
    REQUIRE(std::find(args.begin(), args.end(), "-c") != args.end());
    REQUIRE(std::find(args.begin(), args.end(), "-Wall") != args.end());
    REQUIRE(std::find(args.begin(), args.end(), "-O2") != args.end());
    REQUIRE(std::find(args.begin(), args.end(), "-fPIC") != args.end());
    // Include paths (-I<abs proj>/include from include_dirs; default include/ exists too).
    REQUIRE(std::find_if(args.begin(), args.end(),
        [&](const std::string& a){ return a.rfind("-I" + (proj / "include").string(), 0) == 0; })
        != args.end());
    // Dependency output flags (GCC -MMD -MF <dep>).
    REQUIRE(std::find(args.begin(), args.end(), "-MMD") != args.end());
    REQUIRE(std::find(args.begin(), args.end(), "-MF") != args.end());
    // Source path and object output (-o <obj>).
    REQUIRE(std::find(args.begin(), args.end(), src.string()) != args.end());
    auto it_o = std::find(args.begin(), args.end(), "-o");
    REQUIRE(it_o != args.end());
    REQUIRE(*(it_o + 1) == obj.string());
}

TEST_CASE("build_compile_args: deterministic adds reproducibility flags", "[cache][compile_db]") {
    TempDir tmp;
    fs::path proj = tmp.path;
    fs::create_directories(proj / "src");
    fs::path src = proj / "src" / "main.cpp";
    { std::ofstream(src) << "int main() {}\n"; }

    CompileInput in;
    in.proj_root = proj;
    in.compile.deterministic = true;
    in.lang = parse_language("C++17");
    in.lang.detected_compiler = "g++";
    in.tc.family = CompilerFamily::Gcc;
    in.dep_dir = proj / ".ezmk" / "temp";

    auto args = build_compile_args(in, src, proj / "build" / "main.o");

    bool has_prefix_map = false, has_random_seed = false;
    for (auto& a : args) {
        if (a.rfind("-ffile-prefix-map=", 0) == 0) has_prefix_map = true;
        if (a.rfind("-frandom-seed=", 0) == 0) has_random_seed = true;
    }
    REQUIRE(has_prefix_map);
    REQUIRE(has_random_seed);
}

// ===================================================================
// cache::join_shell_args() — arg vector → shell command string
// ===================================================================

TEST_CASE("join_shell_args: quoting behavior", "[cache][compile_db]") {
    // Args without special chars are joined bare.
    REQUIRE(join_shell_args({"g++", "-c", "src/main.cpp"}) == "g++ -c src/main.cpp");
    // Args with whitespace are double-quoted (paths with spaces survive shells).
    REQUIRE(join_shell_args({"a b", "c"}) == "\"a b\" c");
    // Quotes inside an arg are backslash-escaped inside the double quotes.
    REQUIRE(join_shell_args({"a\"b"}) == "\"a\\\"b\"");
}

// 1.1.3 S4: expanded shell metacharacter blacklist — these must be quoted so a
// malicious flag (e.g. "-lfoo; echo pwned") cannot be executed by `sh -c`.
TEST_CASE("join_shell_args: quotes shell metacharacters", "[cache][compile_db][1.1.3]") {
    // Command-injection separators / operators
    REQUIRE(join_shell_args({"-lfoo; echo pwned"}) == "\"-lfoo; echo pwned\"");
    REQUIRE(join_shell_args({"a|b"}) == "\"a|b\"");
    REQUIRE(join_shell_args({"a&b"}) == "\"a&b\"");
    REQUIRE(join_shell_args({"a#b"}) == "\"a#b\"");
    REQUIRE(join_shell_args({"a(b)c"}) == "\"a(b)c\"");
    REQUIRE(join_shell_args({"a<b>c"}) == "\"a<b>c\"");
    // Globbing / expansion characters
    REQUIRE(join_shell_args({"a*b"}) == "\"a*b\"");
    REQUIRE(join_shell_args({"a?b"}) == "\"a?b\"");
    REQUIRE(join_shell_args({"a[b]"}) == "\"a[b]\"");
    REQUIRE(join_shell_args({"a{b}"}) == "\"a{b}\"");
    REQUIRE(join_shell_args({"a~b"}) == "\"a~b\"");
    REQUIRE(join_shell_args({"a!b"}) == "\"a!b\"");
    REQUIRE(join_shell_args({"a'b"}) == "\"a'b\"");
    // $ and backtick must be backslash-escaped inside the double quotes
    REQUIRE(join_shell_args({"a$b"}) == "\"a\\$b\"");
    REQUIRE(join_shell_args({"a`b"}) == "\"a\\`b\"");
    // Regression: whitespace / plain-arg quoting unchanged
    REQUIRE(join_shell_args({"a b", "c"}) == "\"a b\" c");
    REQUIRE(join_shell_args({"g++", "-c", "src/main.cpp"}) == "g++ -c src/main.cpp");
}

// ===================================================================
// compile_db::generate_compile_db() — compile_commands.json output
// ===================================================================

TEST_CASE("generate_compile_db: produces valid JSON entry", "[compile_db]") {
    TempDir tmp;
    fs::path proj = tmp.path;
    fs::create_directories(proj / "src");
    fs::create_directories(proj / "include");
    fs::path src = proj / "src" / "main.cpp";
    { std::ofstream(src) << "int main() {}\n"; }

    CompileInput cin;
    cin.sources = {src};
    cin.proj_root = proj;
    cin.compile.flags = {"-Wall", "-DEZMK=1"};
    cin.compile.include_dirs = {"include"};
    cin.lang = parse_language("C++17");
    cin.lang.detected_compiler = "g++";
    cin.tc.family = CompilerFamily::Gcc;
    cin.stdlib = "libstdc++";
    cin.dep_dir = proj / ".ezmk" / "temp";

    fs::path out = proj / "compile_commands.json";
    generate_compile_db(cin, proj, out);
    REQUIRE(fs::exists(out));

    auto j = nlohmann::json::parse(file_read(out));
    REQUIRE(j.is_array());
    REQUIRE(j.size() == 1);
    auto& e = j[0];
    REQUIRE(e["file"] == "src/main.cpp");
    REQUIRE(e["directory"] == proj.string());

    std::vector<std::string> args = e["arguments"].get<std::vector<std::string>>();
    // Build-only flags are stripped.
    REQUIRE(std::find(args.begin(), args.end(), "-MMD") == args.end());
    REQUIRE(std::find(args.begin(), args.end(), "-MF") == args.end());
    // Compile flags are preserved.
    REQUIRE(std::find(args.begin(), args.end(), "-DEZMK=1") != args.end());
    REQUIRE(std::find(args.begin(), args.end(), "-c") != args.end());
    // The source path matches `file` (relativized), for clangd substitution.
    REQUIRE(std::find(args.begin(), args.end(), "src/main.cpp") != args.end());
    // -o is relativized against the project root.
    auto it_o = std::find(args.begin(), args.end(), "-o");
    REQUIRE(it_o != args.end());
    REQUIRE(*(it_o + 1) == (fs::path("build") / "src" / "main.o").generic_string());
}

TEST_CASE("generate_compile_db: deterministic random-seed is stripped", "[compile_db]") {
    TempDir tmp;
    fs::path proj = tmp.path;
    fs::create_directories(proj / "src");
    fs::path src = proj / "src" / "main.cpp";
    { std::ofstream(src) << "int main() {}\n"; }

    CompileInput cin;
    cin.sources = {src};
    cin.proj_root = proj;
    cin.compile.deterministic = true;
    cin.lang = parse_language("C++17");
    cin.lang.detected_compiler = "g++";
    cin.tc.family = CompilerFamily::Gcc;
    cin.dep_dir = proj / ".ezmk" / "temp";

    fs::path out = proj / "compile_commands.json";
    generate_compile_db(cin, proj, out);

    auto j = nlohmann::json::parse(file_read(out));
    std::vector<std::string> args = j[0]["arguments"].get<std::vector<std::string>>();
    for (auto& a : args) {
        REQUIRE(a.rfind("-frandom-seed=", 0) != 0);
    }
}

TEST_CASE("generate_compile_db: no sources → warning, no output file", "[compile_db]") {
    TempDir tmp;
    CompileInput cin;
    cin.proj_root = tmp.path;
    cin.tc.family = CompilerFamily::Gcc;
    cin.lang = parse_language("C++17");

    fs::path out = tmp.path / "compile_commands.json";
    generate_compile_db(cin, tmp.path, out);
    REQUIRE(!fs::exists(out));
}
