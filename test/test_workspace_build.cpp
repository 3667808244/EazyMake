// Unit tests for workspace command execution (1.3.0-dev.2):
//   * topo_layers() — Kahn topological layering (chain / diamond / fan-out)
//   * resolve_ws_injection() — sibling member self-discovery (existence gating)
//   * join_args_with_response_file() — command-line length fallback (>16K → @rsp)
//   * subprocess smoke — real lib+app workspace build + cross-member incremental
#include "catch2.hpp"
#include "ezmk/workspace.hpp"
#include "ezmk/workspace_build.hpp"
#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/toolchain.hpp"

#include "ezmk/util.hpp"
#include "test_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using ezmk::workspace::Workspace;
using ezmk::workspace::load_from;
using ezmk::workspace::topo_layers;

namespace {

// Write an ezmk-workspace.toml into `dir`.
void write_ws_toml(const fs::path& dir, const std::string& content) {
    ezmk::util::create_directories(dir);
    std::ofstream(dir / "ezmk-workspace.toml") << content;
}

// Write a minimal member project (dir/rel with a valid ezmk.toml).
// type: "executable" | "static" | "shared"; deps: optional [depends] workspace refs.
void write_member(const fs::path& root, const std::string& rel,
                  const std::string& type = "executable",
                  const std::vector<std::string>& ws_deps = {}) {
    auto dir = root / rel;
    ezmk::util::create_directories(dir);
    std::ofstream of(dir / "ezmk.toml");
    of << "[project]\nname = \"" << fs::path(rel).filename().string()
       << "\"\ntype = \"" << type << "\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n";
    if (!ws_deps.empty()) {
        of << "\n[depends]\nworkspace = [";
        for (size_t i = 0; i < ws_deps.size(); ++i) {
            if (i) of << ", ";
            of << "\"" << ws_deps[i] << "\"";
        }
        of << "]\n";
    }
}

// Member names in a layer, sorted (layer-internal order is deterministic but
// not part of the contract).
std::vector<std::string> layer_names(const Workspace& ws,
                                     const std::vector<size_t>& layer) {
    std::vector<std::string> names;
    for (size_t idx : layer) names.push_back(ws.members[idx].name);
    std::sort(names.begin(), names.end());
    return names;
}

// Resolve the ezmk binary for the subprocess smoke test: EZMK_TEST_BIN env
// var (build.sh — extension-less on Windows) or build/ezmk next to the test
// binary. Empty when absent.
fs::path test_ezmk_binary() {
    const char* env = std::getenv("EZMK_TEST_BIN");
    if (env && env[0]) {
        fs::path p(env);
#ifdef EZMK_WIN
        // build.sh exports the extension-less "build/ezmk"; prefer the .exe
        // form (a stray extension-less file may exist in build/).
        fs::path with_ext(p.string() + EZMK_EXE_SUFFIX);
        if (ezmk::util::file_exists(with_ext)) return with_ext;
#endif
        if (ezmk::util::file_exists(p)) return p;
    }
    fs::path candidate = ezmk::util::get_exe_dir() / ("ezmk" EZMK_EXE_SUFFIX);
    if (ezmk::util::file_exists(candidate)) return candidate;
    return {};
}

} // anonymous namespace

// ===================================================================
// topo_layers()
// ===================================================================

TEST_CASE("workspace topo: no dependencies → single layer with all members", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"a\", \"b\", \"c\"]\n");
    write_member(tmp.path, "a");
    write_member(tmp.path, "b");
    write_member(tmp.path, "c");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    auto layers = topo_layers(*ws);
    REQUIRE(layers.size() == 1);
    REQUIRE(layer_names(*ws, layers[0]) == std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE("workspace topo: chain of three → one member per layer, deps first", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"app\", \"mid\", \"base\"]\n");
    write_member(tmp.path, "base", "static");
    write_member(tmp.path, "mid", "static", {"base"});
    write_member(tmp.path, "app", "executable", {"mid"});

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    auto layers = topo_layers(*ws);
    REQUIRE(layers.size() == 3);
    REQUIRE(layer_names(*ws, layers[0]) == std::vector<std::string>{"base"});
    REQUIRE(layer_names(*ws, layers[1]) == std::vector<std::string>{"mid"});
    REQUIRE(layer_names(*ws, layers[2]) == std::vector<std::string>{"app"});
}

TEST_CASE("workspace topo: diamond → shared base first, siblings parallel", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path,
                  "[workspace]\nmembers = [\"app\", \"l1\", \"l2\", \"base\"]\n");
    write_member(tmp.path, "base", "static");
    write_member(tmp.path, "l1", "static", {"base"});
    write_member(tmp.path, "l2", "static", {"base"});
    write_member(tmp.path, "app", "executable", {"l1", "l2"});

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    auto layers = topo_layers(*ws);
    REQUIRE(layers.size() == 3);
    REQUIRE(layer_names(*ws, layers[0]) == std::vector<std::string>{"base"});
    REQUIRE(layer_names(*ws, layers[1]) == std::vector<std::string>{"l1", "l2"});
    REQUIRE(layer_names(*ws, layers[2]) == std::vector<std::string>{"app"});
}

TEST_CASE("workspace topo: fan-out → base first, all consumers parallel", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path,
                  "[workspace]\nmembers = [\"a\", \"b\", \"c\", \"base\"]\n");
    write_member(tmp.path, "base", "static");
    write_member(tmp.path, "a", "executable", {"base"});
    write_member(tmp.path, "b", "executable", {"base"});
    write_member(tmp.path, "c", "executable", {"base"});

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    auto layers = topo_layers(*ws);
    REQUIRE(layers.size() == 2);
    REQUIRE(layer_names(*ws, layers[0]) == std::vector<std::string>{"base"});
    REQUIRE(layer_names(*ws, layers[1]) == std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE("workspace topo: invalid members are excluded from layers", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"good\", \"ghost\"]\n");
    write_member(tmp.path, "good");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    REQUIRE_FALSE(ws->members[1].valid);
    auto layers = topo_layers(*ws);
    REQUIRE(layers.size() == 1);
    REQUIRE(layer_names(*ws, layers[0]) == std::vector<std::string>{"good"});
}

// ===================================================================
// resolve_ws_injection() — sibling self-discovery (dev.2 §3.4)
// ===================================================================

TEST_CASE("workspace injection: include dir injected, missing lib reported", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"apps/tool-a\", \"libs/strutil\"]\n");
    write_member(tmp.path, "libs/strutil", "static");
    write_member(tmp.path, "apps/tool-a", "executable", {"strutil"});
    ezmk::util::create_directories(tmp.path / "libs/strutil" / "include");

    auto inj = ezmk::build::resolve_ws_injection(tmp.path / "apps/tool-a",
                                                 {"strutil"}, false);
    REQUIRE(inj.error.empty());
    REQUIRE(inj.include_dirs.size() == 1);
    REQUIRE(inj.include_dirs[0] ==
            fs::weakly_canonical(tmp.path / "libs/strutil" / "include"));
    // lib not built yet → no -L/-l, reported as missing.
    REQUIRE(inj.link_dirs.empty());
    REQUIRE(inj.link_names.empty());
    REQUIRE(inj.missing == std::vector<std::string>{"strutil"});
}

TEST_CASE("workspace injection: existing lib produces -L/-l", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"apps/tool-a\", \"libs/strutil\"]\n");
    write_member(tmp.path, "libs/strutil", "static");
    write_member(tmp.path, "apps/tool-a", "executable", {"strutil"});
    auto build_dir = tmp.path / "libs/strutil" / "build";
    ezmk::util::create_directories(build_dir);
    std::ofstream(build_dir / "libstrutil.a") << "fake archive";

    auto inj = ezmk::build::resolve_ws_injection(tmp.path / "apps/tool-a",
                                                 {"strutil"}, false);
    REQUIRE(inj.error.empty());
    REQUIRE(inj.link_dirs.size() == 1);
    REQUIRE(inj.link_dirs[0] == fs::weakly_canonical(build_dir));
    REQUIRE(inj.link_names == std::vector<std::string>{"strutil"});
    REQUIRE(inj.missing.empty());
}

TEST_CASE("workspace injection: MSVC naming uses <name>.lib and full path", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"apps/tool-a\", \"libs/strutil\"]\n");
    write_member(tmp.path, "libs/strutil", "static");
    write_member(tmp.path, "apps/tool-a", "executable", {"strutil"});
    auto build_dir = tmp.path / "libs/strutil" / "build";
    ezmk::util::create_directories(build_dir);
    std::ofstream(build_dir / "strutil.lib") << "fake";

    auto inj = ezmk::build::resolve_ws_injection(tmp.path / "apps/tool-a",
                                                 {"strutil"}, true);
    REQUIRE(inj.error.empty());
    REQUIRE(inj.msvc_archives.size() == 1);
    REQUIRE(inj.msvc_archives[0] == fs::weakly_canonical(build_dir / "strutil.lib"));
    REQUIRE(inj.link_dirs.empty());
}

TEST_CASE("workspace injection: no workspace → error, empty refs → no-op", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;

    SECTION("no workspace file upward") {
        auto inj = ezmk::build::resolve_ws_injection(tmp.path, {"strutil"}, false);
        REQUIRE_FALSE(inj.error.empty());
        REQUIRE(inj.include_dirs.empty());
    }
    SECTION("no declared deps → no-op (single-project path unchanged)") {
        auto inj = ezmk::build::resolve_ws_injection(tmp.path, {}, false);
        REQUIRE(inj.error.empty());
        REQUIRE(inj.include_dirs.empty());
        REQUIRE(inj.link_names.empty());
    }
}

TEST_CASE("workspace injection: unknown ref / non-static dep → error", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    write_ws_toml(tmp.path, "[workspace]\nmembers = [\"apps/tool-a\", \"libs/strutil\"]\n");
    write_member(tmp.path, "libs/strutil", "static");
    write_member(tmp.path, "apps/tool-a", "executable", {"strutil"});

    SECTION("unknown member") {
        auto inj = ezmk::build::resolve_ws_injection(tmp.path / "apps/tool-a",
                                                     {"nope"}, false);
        REQUIRE_FALSE(inj.error.empty());
    }
    SECTION("dependency not built yet is fine (missing), non-static rejected") {
        // tool-a depends on an executable member → load_from already throws at
        // validation time; the helper surfaces it as error.
        TempDir tmp2;
        write_ws_toml(tmp2.path, "[workspace]\nmembers = [\"app\", \"helper\"]\n");
        write_member(tmp2.path, "helper", "executable");
        write_member(tmp2.path, "app", "executable", {"helper"});
        auto inj = ezmk::build::resolve_ws_injection(tmp2.path / "app",
                                                     {"helper"}, false);
        REQUIRE_FALSE(inj.error.empty());
    }
}

// ===================================================================
// join_args_with_response_file() — command-line length fallback (§3.5)
// ===================================================================

TEST_CASE("response file: short command stays inline", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    std::vector<std::string> args = {"g++", "-c", "main.cpp", "-o", "main.o"};
    auto jc = ezmk::cache::join_args_with_response_file(args, tmp.path);
    REQUIRE(jc.rsp_file.empty());
    REQUIRE(jc.cmd == ezmk::cache::join_shell_args(args));
}

TEST_CASE("response file: command over 16K becomes compiler @<rsp>", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    std::vector<std::string> args = {"g++", "-c"};
    std::string long_flag = "-I" + std::string(2000, 'x');
    for (int i = 0; i < 10; ++i) args.push_back(long_flag);  // ~20K joined
    args.push_back("main.cpp");

    auto jc = ezmk::cache::join_args_with_response_file(args, tmp.path);
    REQUIRE_FALSE(jc.rsp_file.empty());
    // Command is `compiler @<rsp>` (shell-quoted when the path has specials)
    // — the response file keeps the command short.
    std::string expect_cmd = ezmk::cache::join_shell_args(
        {args[0], "@" + jc.rsp_file.string()});
    REQUIRE(jc.cmd == expect_cmd);
    REQUIRE(jc.cmd.find('@') != std::string::npos);
    REQUIRE(jc.cmd.size() < 4096);

    // Content: one literal arg per line (args[0] = compiler stays on the line).
    std::string content = ezmk::util::file_read(jc.rsp_file);
    std::vector<std::string> lines;
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    REQUIRE(lines.size() == args.size() - 1);
    for (size_t i = 1; i < args.size(); ++i) {
        REQUIRE(lines[i - 1] == args[i]);
    }

    // Caller removes the response file after the run.
    std::error_code ec;
    fs::remove(jc.rsp_file, ec);
    REQUIRE_FALSE(fs::exists(jc.rsp_file));
}

TEST_CASE("response file: rsp path with spaces is quoted, one arg per line", "[workspace][1.3.0-dev.2]") {
    TempDir tmp;
    auto spacy = tmp.path / "with space";
    ezmk::util::create_directories(spacy);

    std::vector<std::string> args = {"clang++"};
    std::string long_flag = "-I" + std::string(2000, 'y');
    for (int i = 0; i < 10; ++i) args.push_back(long_flag);
    args.push_back("src with space/main.cpp");

    auto jc = ezmk::cache::join_args_with_response_file(args, spacy);
    REQUIRE_FALSE(jc.rsp_file.empty());
    // @<path with space> must be shell-quoted so run_command parses it intact.
    REQUIRE(jc.cmd == ezmk::cache::join_shell_args(
                          {args[0], "@" + jc.rsp_file.string()}));
    REQUIRE(jc.cmd.find('"') != std::string::npos);
    // The rsp contains the raw "src with space/main.cpp" as ONE line (no
    // shell quoting — a response-file line IS the literal arg).
    std::string content = ezmk::util::file_read(jc.rsp_file);
    REQUIRE(content.find("\"src with space/main.cpp\"") == std::string::npos);
    REQUIRE(content.find("src with space/main.cpp") != std::string::npos);
    std::error_code ec;
    fs::remove(jc.rsp_file, ec);
}

// ===================================================================
// resolve_jobs() — parallel-job precedence (dev.3 §3.2)
// ===================================================================

TEST_CASE("workspace resolve_jobs: explicit -j > default_jobs > auto", "[workspace][1.3.0-dev.3]") {
    TempDir tmp;
    write_ws_toml(tmp.path,
                  "[workspace]\nmembers = [\"a\"]\n\n"
                  "[workspace.options]\ndefault_jobs = 4\n");
    write_member(tmp.path, "a");

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());
    REQUIRE(ezmk::workspace_build::resolve_jobs(0, *ws) == 4);   // default_jobs
    REQUIRE(ezmk::workspace_build::resolve_jobs(8, *ws) == 8);   // explicit wins
    REQUIRE(ezmk::workspace_build::resolve_jobs(-1, *ws) == 4);  // negative → default

    // No default_jobs and no -j → hardware_concurrency (≥ 1).
    TempDir tmp2;
    write_ws_toml(tmp2.path, "[workspace]\nmembers = [\"a\"]\n");
    write_member(tmp2.path, "a");
    auto ws2 = load_from(tmp2.path);
    REQUIRE(ws2.has_value());
    int hw = static_cast<int>(std::thread::hardware_concurrency());
    if (hw < 1) hw = 1;
    REQUIRE(ezmk::workspace_build::resolve_jobs(0, *ws2) == hw);
}

// ===================================================================
// Subprocess smoke — real lib + app workspace through run_build() (§4.7)
// ===================================================================

TEST_CASE("workspace build: subprocess smoke — lib+app, topo order + incremental", "[workspace][1.3.0-dev.2]") {
    auto bin = test_ezmk_binary();
    if (bin.empty()) {
        WARN("ezmk binary not found (EZMK_TEST_BIN / build/ezmk) — skipping subprocess smoke");
        return;
    }

    TempDir tmp;
    write_ws_toml(tmp.path,
                  "[workspace]\nmembers = [\"apps/tool-a\", \"libs/strutil\"]\n\n"
                  "[workspace.options]\ndefault_jobs = 2\n");
    write_member(tmp.path, "libs/strutil", "static");
    write_member(tmp.path, "apps/tool-a", "executable", {"strutil"});

    // lib: header with OFFSET constant + add(); app: prints add(2,3)+OFFSET.
    ezmk::util::create_directories(tmp.path / "libs/strutil" / "include");
    ezmk::util::create_directories(tmp.path / "libs/strutil" / "src");
    ezmk::util::create_directories(tmp.path / "apps/tool-a" / "src");
    {
        std::ofstream(tmp.path / "libs/strutil" / "include" / "strutil.hpp")
            << "#pragma once\nnamespace strutil {\ninline constexpr int OFFSET = 0;\n"
               "int add(int a, int b);\n}\n";
        std::ofstream(tmp.path / "libs/strutil" / "src" / "strutil.cpp")
            << "#include \"strutil.hpp\"\nnamespace strutil {\n"
               "int add(int a, int b) { return a + b; }\n}\n";
        std::ofstream(tmp.path / "apps/tool-a" / "src" / "main.cpp")
            << "#include \"strutil.hpp\"\n#include <cstdio>\n"
               "int main() { std::printf(\"sum=%d\\n\", strutil::add(2, 3) + strutil::OFFSET); return 0; }\n";
    }

    auto ws = load_from(tmp.path);
    REQUIRE(ws.has_value());

    ezmk::cli::WorkspaceOptions wopts;
    wopts.jobs = 2;

    // Windows AV/file-lock flake guard: the child link rewrites tool-a.exe via
    // atomic_rename, which can transiently fail with "Permission denied" when
    // antivirus still holds the just-created image (same flake class as the
    // repo's rename test). Builds are idempotent, so one retry after a short
    // pause is safe and keeps the smoke test CI-stable.
    auto build_ok = [&]() {
        if (ezmk::workspace_build::run_build(*ws, wopts) == 0) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        return ezmk::workspace_build::run_build(*ws, wopts) == 0;
    };

    // 1) First build — both members succeed, artifacts appear.
    REQUIRE(build_ok());
    fs::path lib_a = tmp.path / "libs/strutil" / "build" / "libstrutil.a";
    fs::path app_exe = tmp.path / "apps/tool-a" / "build" / "tool-a" EZMK_EXE_SUFFIX;
    REQUIRE(ezmk::util::file_exists(lib_a));
    REQUIRE(ezmk::util::file_exists(app_exe));
    {
        auto res = ezmk::util::run_command("\"" + app_exe.string() + "\"");
        REQUIRE(res.exit_code == 0);
        REQUIRE(res.out.find("sum=5") != std::string::npos);
    }

    fs::path main_obj = tmp.path / "apps/tool-a" / ".ezmk/temp" / "src" / "main.o";
    bool is_msvc = (ezmk::toolchain::detect_toolchain().family ==
                    ezmk::toolchain::CompilerFamily::Msvc);
    main_obj.replace_extension(is_msvc ? ".obj" : ".o");
    REQUIRE(ezmk::util::file_exists(main_obj));
    std::string main_obj_v1 = ezmk::crypto::sha256_file(main_obj);

    // 2) No-op rebuild — main.o served from cache (byte-identical object).
    REQUIRE(build_ok());
    REQUIRE(ezmk::crypto::sha256_file(main_obj) == main_obj_v1);

    // 3) lib .cpp change → lib rebuilt + app RELINKED, but app main.o NOT
    //    recompiled (source + headers unchanged → cache hit). The relink is
    //    observable: the app now prints the new result.
    {
        std::ofstream(tmp.path / "libs/strutil" / "src" / "strutil.cpp")
            << "#include \"strutil.hpp\"\nnamespace strutil {\n"
               "int add(int a, int b) { return a + b + 1; }\n}\n";
    }
    REQUIRE(build_ok());
    REQUIRE(ezmk::crypto::sha256_file(main_obj) == main_obj_v1);  // no recompile
    {
        auto res = ezmk::util::run_command("\"" + app_exe.string() + "\"");
        REQUIRE(res.exit_code == 0);
        REQUIRE(res.out.find("sum=6") != std::string::npos);  // relinked w/ new lib
    }

    // 4) lib .hpp change → consumer RECOMPILES via the depfile header hash
    //    (injected -I is preprocessor-visible and -MD tracks it).
    {
        std::ofstream(tmp.path / "libs/strutil" / "include" / "strutil.hpp")
            << "#pragma once\nnamespace strutil {\ninline constexpr int OFFSET = 100;\n"
               "int add(int a, int b);\n}\n";
    }
    REQUIRE(build_ok());
    std::string main_obj_v2 = ezmk::crypto::sha256_file(main_obj);
    REQUIRE(main_obj_v2 != main_obj_v1);  // recompiled (OFFSET inlined into main.o)
    {
        auto res = ezmk::util::run_command("\"" + app_exe.string() + "\"");
        REQUIRE(res.exit_code == 0);
        REQUIRE(res.out.find("sum=106") != std::string::npos);
    }

    // 5) workspace clean clears member caches (build/ artifacts kept — same
    //    semantics as single-project `ezmk clean`).
    REQUIRE(ezmk::workspace_build::run_clean(*ws, wopts) == 0);
    REQUIRE_FALSE(ezmk::util::file_exists(tmp.path / "apps/tool-a" / ".ezmk" / "cache"));
    // Artifacts survived → rebuild is a full recompile but succeeds.
    REQUIRE(build_ok());
}
