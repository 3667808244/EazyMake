// End-to-end integration tests for EazyMake — workspace (split from
// test_integration.cpp in 1.3.6; shared helpers in test_integration_helpers.hpp).
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "test_helpers.hpp"
#include "test_integration_helpers.hpp"
#include "ezmk/util.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/toolchain.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace ezi;
// ==============================================================
// 1.3.0-dev.3: workspace integration tests
// (fixture: 3-member workspace — libs/strutil static lib + apps/tool-a
//  and apps/tool-b executables; tool-a also has [test] ezmk framework)
// ==============================================================
namespace {

// Write the 3-member workspace fixture at `root` (root may already exist).
//   root/apps/tool-a   executable; [depends] workspace=["strutil"]; [test] ezmk
//   root/apps/tool-b   executable; [depends] workspace=["strutil"]
//   root/libs/strutil  static; include/strutil.hpp + src/strutil.cpp
// The app output is "sum = add(2,3) + OFFSET": 5 (baseline) → 6 (source
// change) → 106 (header OFFSET change) — used to prove relink/recompile.
void write_ws_fixture(const fs::path& root) {
    fs::create_directories(root / "libs/strutil" / "include");
    fs::create_directories(root / "libs/strutil" / "src");
    fs::create_directories(root / "apps/tool-a" / "src");
    fs::create_directories(root / "apps/tool-a" / "test");
    fs::create_directories(root / "apps/tool-b" / "src");

    file_write(root / "ezmk-workspace.toml",
        "[workspace]\n"
        "members = [\"apps/tool-a\", \"apps/tool-b\", \"libs/strutil\"]\n\n"
        "[workspace.options]\ndefault_jobs = 2\n");

    file_write(root / "libs/strutil" / "ezmk.toml",
        "[project]\nname = \"strutil\"\ntype = \"static\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n");
    file_write(root / "libs/strutil" / "include" / "strutil.hpp",
        "#pragma once\nnamespace strutil {\ninline constexpr int OFFSET = 0;\n"
        "int add(int a, int b);\n}\n");
    file_write(root / "libs/strutil" / "src" / "strutil.cpp",
        "#include \"strutil.hpp\"\nnamespace strutil {\n"
        "int add(int a, int b) { return a + b; }\n}\n");

    file_write(root / "apps/tool-a" / "ezmk.toml",
        "[project]\nname = \"tool-a\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[depends]\nworkspace = [\"strutil\"]\n\n"
        "[test]\nframework = \"ezmk\"\ndirs = [\"test\"]\n");
    file_write(root / "apps/tool-a" / "src" / "main.cpp",
        "#include \"strutil.hpp\"\n#include <cstdio>\n"
        "int main() { std::printf(\"sum=%d\\n\", strutil::add(2, 3) + strutil::OFFSET); return 0; }\n");
    file_write(root / "apps/tool-a" / "test" / "test_smoke.cpp",
        "#include <cstdio>\nint main() { std::printf(\"tool-a tests ok\\n\"); return 0; }\n");

    file_write(root / "apps/tool-b" / "ezmk.toml",
        "[project]\nname = \"tool-b\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[depends]\nworkspace = [\"strutil\"]\n");
    file_write(root / "apps/tool-b" / "src" / "main.cpp",
        "#include \"strutil.hpp\"\n#include <cstdio>\n"
        "int main() { std::printf(\"b=%d\\n\", strutil::add(10, 20) + strutil::OFFSET); return 0; }\n");
}

// Built executable of an app member (root/apps/<name>/build/<name>[.exe]).
fs::path ws_app_exe(const fs::path& root, const std::string& name) {
    return root / "apps" / name / "build" / (name + EZMK_EXE_SUFFIX);
}

// Combined stdout+stderr of a run (workspace output goes to stderr; member
// prefixed child output is echoed by the orchestrator).
std::string ws_output(const ProcResult& r) {
    return r.out + "\n" + r.err;
}

} // anonymous namespace

// Case 1: workspace list
TEST_CASE("integration: workspace list shows members + types + deps (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";   // space in the root exercises quoting
    write_ws_fixture(root);

    ProcResult r = run_ezmk("workspace list", root);
    INFO("list output: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);
    REQUIRE(out.find("apps/tool-a") != std::string::npos);
    REQUIRE(out.find("apps/tool-b") != std::string::npos);
    REQUIRE(out.find("libs/strutil") != std::string::npos);
    // Member type column and dependency listing.
    REQUIRE(out.find("[executable]") != std::string::npos);
    REQUIRE(out.find("[static]") != std::string::npos);
    REQUIRE(out.find("strutil") != std::string::npos);
}

// Cases 2+3: build all + dependency build order (lib layer before apps)
TEST_CASE("integration: workspace build succeeds and respects dependency order (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);

    // All three members succeeded.
    REQUIRE(out.find("3 succeeded") != std::string::npos);
    // Artifacts landed (executables + static archive).
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
    REQUIRE(fs::exists(ws_app_exe(root, "tool-b")));
    REQUIRE(fs::exists(root / "libs/strutil/build/libstrutil.a"));
    // The app runs and links the sibling library via self-discovery injection.
    auto app = run_command("\"" + ws_app_exe(root, "tool-a").string() + "\"");
    REQUIRE(app.exit_code == 0);
    REQUIRE(app.out.find("sum=5") != std::string::npos);

    // Dependency order: the strutil layer's output precedes BOTH app layers
    // (layers execute sequentially; intra-layer order is not asserted).
    auto lib_pos = out.find("[libs/strutil]");
    auto a_pos = out.find("[apps/tool-a]");
    auto b_pos = out.find("[apps/tool-b]");
    REQUIRE(lib_pos != std::string::npos);
    REQUIRE(a_pos != std::string::npos);
    REQUIRE(b_pos != std::string::npos);
    REQUIRE(lib_pos < a_pos);
    REQUIRE(lib_pos < b_pos);
}

// Case 4: cross-member incremental — lib .cpp change → dependents RELINK only
TEST_CASE("integration: workspace incremental — lib .cpp change relinks dependents (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);

    // Change the lib implementation (not the header).
    file_write(root / "libs/strutil/src/strutil.cpp",
        "#include \"strutil.hpp\"\nnamespace strutil {\n"
        "int add(int a, int b) { return a + b + 1; }\n}\n");
    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("rebuild stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);

    // strutil recompiled ("0 cached, 1 compiled"); the apps were NOT
    // recompiled ("1 cached, 0 compiled" — source + headers unchanged).
    REQUIRE(out.find("0 cached, 1 compiled") != std::string::npos);
    REQUIRE(out.find("1 cached, 0 compiled") != std::string::npos);
    // The apps relinked against the new archive → new runtime output.
    auto app = run_command("\"" + ws_app_exe(root, "tool-a").string() + "\"");
    REQUIRE(app.exit_code == 0);
    REQUIRE(app.out.find("sum=6") != std::string::npos);
}

// Case 5: cross-member incremental — lib .h change → dependents RECOMPILE
TEST_CASE("integration: workspace incremental — lib .h change recompiles dependents (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);

    // Change the header (OFFSET constant) — the apps include it via the
    // injected -I, so their depfile tracks it and they must recompile.
    file_write(root / "libs/strutil/include/strutil.hpp",
        "#pragma once\nnamespace strutil {\ninline constexpr int OFFSET = 100;\n"
        "int add(int a, int b);\n}\n");
    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("rebuild stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);

    // All three members recompile (lib source + both app mains).
    size_t recompiles = 0;
    for (size_t pos = out.find("0 cached, 1 compiled"); pos != std::string::npos;
         pos = out.find("0 cached, 1 compiled", pos + 1)) {
        ++recompiles;
    }
    REQUIRE(recompiles >= 3);
    // Header change reached the consumers → new runtime output (5 + 100).
    auto app = run_command("\"" + ws_app_exe(root, "tool-a").string() + "\"");
    REQUIRE(app.exit_code == 0);
    REQUIRE(app.out.find("sum=105") != std::string::npos);
}

// Case 6: workspace test — members with tests run; others are skipped
TEST_CASE("integration: workspace test runs tested members and skips the rest (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    // Build first so tool-a's test runner links against the sibling lib.
    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);

    ProcResult r = run_ezmk("workspace test -j 2", root);
    INFO("test stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);
    // tool-a ran its [test] (ezmk framework, zero dependencies) → PASS.
    REQUIRE(out.find("[PASS]") != std::string::npos);
    // tool-b and strutil have no tests → skipped (not an error).
    REQUIRE(out.find("no tests configured") != std::string::npos);
    REQUIRE(out.find("1 succeeded") != std::string::npos);
}

// Case 7: workspace clean clears member caches (same semantics as single
// project `ezmk clean` — .ezmk/cache and .ezmk/temp, build/ artifacts kept).
TEST_CASE("integration: workspace clean clears member caches (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);
    REQUIRE(fs::exists(root / "apps/tool-a/.ezmk/cache"));

    ProcResult r = run_ezmk("workspace clean", root);
    INFO("clean stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    REQUIRE_FALSE(fs::exists(root / "apps/tool-a/.ezmk/cache"));
    REQUIRE_FALSE(fs::exists(root / "apps/tool-b/.ezmk/cache"));
    REQUIRE_FALSE(fs::exists(root / "libs/strutil/.ezmk/cache"));
}

// Case 8: failure summary — one member fails, the rest complete (no flag)
TEST_CASE("integration: workspace build failure summary without --stop-on-error (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);
    // Break tool-b's source (deterministic compile failure).
    file_write(root / "apps/tool-b/src/main.cpp",
        "int broken( { this does not compile\n");
    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code != 0);
    std::string out = ws_output(r);
    // strutil + tool-a completed; tool-b failed; nothing skipped.
    REQUIRE(out.find("2 succeeded, 1 failed") != std::string::npos);
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
}

// Case 9: --stop-on-error — dependency-layer failure skips all dependents
TEST_CASE("integration: workspace build --stop-on-error skips dependents of the failing member (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    // Break the LIB (layer 0) — both apps depend on it, so the whole layer 1
    // must be skipped deterministically (nothing dispatched after the failure).
    file_write(root / "libs/strutil/src/strutil.cpp",
        "int broken( { this does not compile\n");
    ProcResult r = run_ezmk("workspace build --stop-on-error -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code != 0);
    std::string out = ws_output(r);
    REQUIRE(out.find("0 succeeded, 1 failed, 2 skipped") != std::string::npos);
    // Dependents never started → no artifacts.
    REQUIRE_FALSE(fs::exists(root / "apps/tool-a/build"));
    REQUIRE_FALSE(fs::exists(root / "apps/tool-b/build"));
}

// Case 10: --member subset + dependency closure; unknown member → fatal
TEST_CASE("integration: workspace build --member selects member + dependency closure (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    // --member tool-a → tool-a + its dependency strutil; tool-b untouched.
    ProcResult r = run_ezmk("workspace build --member tool-a -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    REQUIRE(fs::exists(root / "libs/strutil/build/libstrutil.a"));
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
    REQUIRE_FALSE(fs::exists(root / "apps/tool-b/build"));

    // Full relative path is equivalent.
    TempDir tmp2;
    fs::path root2 = tmp2.path / "ws dir";
    write_ws_fixture(root2);
    ProcResult r2 = run_ezmk("workspace build --member apps/tool-a -j 2", root2);
    REQUIRE(r2.exit_code == 0);
    REQUIRE(fs::exists(ws_app_exe(root2, "tool-a")));
    REQUIRE_FALSE(fs::exists(root2 / "apps/tool-b/build"));

    // Unknown member → fatal with a clear message.
    ProcResult r3 = run_ezmk("workspace build --member nope", root2);
    REQUIRE(r3.exit_code != 0);
    REQUIRE(ws_output(r3).find("unknown workspace member") != std::string::npos);
}

// Case 11: member-internal standalone build — injects EXISTING siblings, does
// not trigger the dependency closure.
TEST_CASE("integration: member-internal build injects existing siblings without closure (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    REQUIRE(run_ezmk("workspace build -j 2", root).exit_code == 0);

    // cd apps/tool-a && ezmk build — only tool-a builds; strutil is NOT
    // rebuilt (its artifact already exists and is injected via self-discovery).
    ProcResult r = run_ezmk("build", root / "apps/tool-a");
    INFO("member build stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    std::string out = ws_output(r);
    REQUIRE(out.find("Archiving libstrutil.a") == std::string::npos);
    REQUIRE(out.find("Linking tool-a") != std::string::npos);
    REQUIRE(out.find("Build successful") != std::string::npos);
}

// Case 12: injection is self-discovery — EZK_WS_* environment variables are
// never read (pit 1 locked at the integration level).
TEST_CASE("integration: workspace injection ignores EZK_WS_* environment variables (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    // Garbage values that would break the build IF the env were consulted.
    EnvGuard env_deps("EZK_WS_DEPS", "/nonexistent/deps");
    EnvGuard env_root("EZK_WS_ROOT", "/nonexistent/root");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);

    // The build still links both apps against the sibling lib → the injection
    // came from the workspace file (member self-discovery), not the env.
    ProcResult r = run_ezmk("workspace build -j 2", root);
    INFO("build stderr: " << r.err);
    REQUIRE(r.exit_code == 0);
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
    auto app = run_command("\"" + ws_app_exe(root, "tool-a").string() + "\"");
    REQUIRE(app.exit_code == 0);
    REQUIRE(app.out.find("sum=5") != std::string::npos);
}

// Cases 13–15: configuration rejection — path escape / cycle / non-static dep
TEST_CASE("integration: workspace validation rejects escape/cycle/non-static (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");

    SECTION("path escape (../outside)") {
        TempDir tmp;
        fs::path root = tmp.path / "ws";
        write_ws_fixture(root);
        file_write(root / "ezmk-workspace.toml",
            "[workspace]\nmembers = [\"../outside\"]\n");
        ProcResult r = run_ezmk("workspace list", root);
        REQUIRE(r.exit_code != 0);
        REQUIRE(ws_output(r).find("outside") != std::string::npos);
    }
    SECTION("dependency cycle (tool-a <-> strutil)") {
        TempDir tmp;
        fs::path root = tmp.path / "ws";
        write_ws_fixture(root);
        file_write(root / "libs/strutil/ezmk.toml",
            "[project]\nname = \"strutil\"\ntype = \"static\"\nversion = \"0.1.0\"\n\n"
            "[depends]\nworkspace = [\"tool-a\"]\n");
        ProcResult r = run_ezmk("workspace list", root);
        REQUIRE(r.exit_code != 0);
        REQUIRE(ws_output(r).find("cycle") != std::string::npos);
    }
    SECTION("non-static dependency (tool-a depends on executable tool-b)") {
        TempDir tmp;
        fs::path root = tmp.path / "ws";
        write_ws_fixture(root);
        file_write(root / "apps/tool-a/ezmk.toml",
            "[project]\nname = \"tool-a\"\ntype = \"executable\"\nversion = \"0.1.0\"\n\n"
            "[depends]\nworkspace = [\"tool-b\"]\n");
        ProcResult r = run_ezmk("workspace list", root);
        REQUIRE(r.exit_code != 0);
        REQUIRE(ws_output(r).find("static") != std::string::npos);
    }
}

// Case 16: nested workspace file marks the member invalid; execution skips it
TEST_CASE("integration: nested ezmk-workspace.toml invalidates the member (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);
    // strutil gains its own workspace file → invalid; tool-a depends on it →
    // tool-a invalid too (referencer of an invalid member).
    file_write(root / "libs/strutil/ezmk-workspace.toml",
        "[workspace]\nmembers = [\"x\"]\n");
    // Drop tool-b's dependency so one member stays valid (tool-b and tool-a
    // both depend on strutil in the fixture — keep only tool-a invalid).
    file_write(root / "apps/tool-b/ezmk.toml",
        "[project]\nname = \"tool-b\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n");
    file_write(root / "apps/tool-b/src/main.cpp",
        "int main() { return 0; }\n");

    ProcResult l = run_ezmk("workspace list", root);
    REQUIRE(l.exit_code == 0);
    REQUIRE(ws_output(l).find("invalid") != std::string::npos);

    // Build: only tool-b remains valid → 1 succeeded, invalid members skipped.
    ProcResult b = run_ezmk("workspace build -j 2", root);
    INFO("build stderr: " << b.err);
    REQUIRE(b.exit_code == 0);
    REQUIRE(ws_output(b).find("1 succeeded") != std::string::npos);
    REQUIRE(fs::exists(ws_app_exe(root, "tool-b")));
    REQUIRE_FALSE(fs::exists(root / "apps/tool-a/build"));
}

// Case 17: missing member directory marks it invalid, does not block the rest
TEST_CASE("integration: missing member directory is skipped, others build (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws dir";
    write_ws_fixture(root);
    file_write(root / "ezmk-workspace.toml",
        "[workspace]\nmembers = [\"apps/tool-a\", \"apps/tool-b\", \"libs/strutil\", \"ghost\"]\n");

    ProcResult l = run_ezmk("workspace list", root);
    REQUIRE(l.exit_code == 0);
    REQUIRE(ws_output(l).find("ghost") != std::string::npos);
    REQUIRE(ws_output(l).find("invalid") != std::string::npos);

    ProcResult b = run_ezmk("workspace build -j 2", root);
    REQUIRE(b.exit_code == 0);
    REQUIRE(ws_output(b).find("3 succeeded") != std::string::npos);
    REQUIRE(fs::exists(ws_app_exe(root, "tool-a")));
    REQUIRE(fs::exists(ws_app_exe(root, "tool-b")));
}

// Case 18: pure container root hint; project root with workspace file unchanged
TEST_CASE("integration: pure container root hints at workspace build (1.3.0)", "[integration][workspace][1.3.0]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;

    SECTION("workspace file only (no ezmk.toml) → hint") {
        // Fixture root has ezmk-workspace.toml but no ezmk.toml → pure
        // container root; `ezmk build` must hint at the workspace command.
        fs::path root = tmp.path / "ws";
        write_ws_fixture(root);
        ProcResult r = run_ezmk("build", root);
        REQUIRE(r.exit_code != 0);
        REQUIRE(ws_output(r).find("workspace build") != std::string::npos);
    }
    SECTION("root is BOTH a project and a workspace → normal build") {
        fs::path root = tmp.path / "both";
        write_ws_fixture(root);
        // A real project at the container root: ezmk.toml + src/main.cpp.
        file_write(root / "ezmk.toml",
            "[project]\nname = \"root-proj\"\ntype = \"executable\"\nversion = \"0.1.0\"\n");
        fs::create_directories(root / "src");
        file_write(root / "src/main.cpp", "int main() { return 0; }\n");
        ProcResult r = run_ezmk("build", root);
        INFO("build stderr: " << r.err);
        REQUIRE(r.exit_code == 0);
        REQUIRE(fs::exists(root / "build/root-proj" EZMK_EXE_SUFFIX));
    }
}

// 1.3.2: `workspace test --report` forwards the flag to every member — each
// member writes its OWN report file (default path under its project root).
TEST_CASE("integration: workspace test --report writes per-member reports (1.3.2)", "[integration][workspace][1.3.2]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws_rpt";
    write_ws_fixture(root);  // tool-a has [test] ezmk + test dir

    // Give tool-b a test dir + [test] section so TWO members write reports.
    fs::create_directories(root / "apps/tool-b" / "test");
    file_write(root / "apps/tool-b" / "test" / "b_test.cpp",
        "#include <cstdio>\nint main() { std::printf(\"b-ok\\n\"); return 0; }\n");
    file_write(root / "apps/tool-b" / "ezmk.toml",
        "[project]\nname = \"tool-b\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
        "[depends]\nworkspace = [\"strutil\"]\n\n"
        "[test]\ndirs = [\"test\"]\nframework = \"ezmk\"\n");

    // Members link against sibling artifacts — build the workspace first.
    ProcResult b = run_ezmk("workspace build -j 2", root);
    REQUIRE(b.exit_code == 0);

    ProcResult r = run_ezmk("workspace test --report junit -j 2", root);
    INFO("stderr: " << r.err);
    REQUIRE(r.exit_code == 0);

    // Each tested member wrote its own report (default per-member path).
    fs::path rpt_a = root / "apps/tool-a" / ".ezmk" / "test-results" / "junit.xml";
    fs::path rpt_b = root / "apps/tool-b" / ".ezmk" / "test-results" / "junit.xml";
    REQUIRE(fs::exists(rpt_a));
    REQUIRE(fs::exists(rpt_b));
    REQUIRE(file_read(rpt_a).find("<testcase name=\"test_smoke.cpp\"") != std::string::npos);
    REQUIRE(file_read(rpt_b).find("<testcase name=\"b_test.cpp\"") != std::string::npos);
    // The workspace root itself has no report (members are the units).
    REQUIRE_FALSE(fs::exists(root / ".ezmk" / "test-results" / "junit.xml"));
}

// ==============================================================
// 1.4.0-dev.5: workspace watch — member-level watch
// ==============================================================
namespace {

// Start `ezmk workspace watch [flags]` in root (background), redirecting output
// to log_file. Caller must kill_ws_watch_processes() at the end.
void start_ws_watch(const fs::path& root, const fs::path& log_file,
                    const std::string& flags) {
    std::string ezmk_bin = find_ezmk_binary().string();
    std::string watch_cmd;
#ifdef EZMK_WIN
    watch_cmd = "cmd /c start \"\" /D \"" + root.string() + "\" /B " +
                escape_shell_arg(ezmk_bin) +
                " workspace watch " + flags + " > \"" +
                escape_shell_arg(log_file.string()) + "\" 2>&1";
#else
    watch_cmd = "cd " + escape_shell_arg(root.string()) + " && " +
                escape_shell_arg(ezmk_bin) +
                " workspace watch " + flags + " > " +
                escape_shell_arg(log_file.string()) + " 2>&1 &";
#endif
    run_command(watch_cmd);
}

// Kill the workspace-watch process tree (parent orchestrator + member watchers).
void kill_ws_watch_processes() {
#ifdef EZMK_WIN
    run_command("cmd /c taskkill /F /IM ezmk.exe 2>nul");
#else
    run_command("pkill -f \"ezmk workspace watch\" 2>/dev/null || true");
    run_command("pkill -f \"ezmk watch\" 2>/dev/null || true");
#endif
}

} // anonymous namespace

// workspace watch starts a watcher in every member (initial build artifacts
// appear) and rebuilds a member when its sources change.
TEST_CASE("integration: workspace watch starts member watchers and rebuilds (1.4.0-dev.5)", "[integration][workspace][1.4.0-dev.5]") {
    if (!ezmk_available()) {
        SKIP("ezmk binary not found — build it first with: bash build.sh");
    }
    EnvGuard lang_guard("EZMK_LANG", "en");
    TempDir tmp;
    fs::path root = tmp.path / "ws_ww";
    write_ws_fixture(root);

    // Pre-build so every member watcher's initial build is a cache hit — the
    // alternative (watching with missing sibling artifacts) races the strutil
    // watcher's rebuild against tool-a's initial build (design pit 1: a
    // dependent may briefly see a half-written sibling artifact, and watch
    // does not retry a failed INITIAL build).
    ProcResult b = run_ezmk("workspace build -j 2", root);
    REQUIRE(b.exit_code == 0);

    // RAII: kill the watch process tree on EVERY exit path (a REQUIRE failure
    // used to leak the orchestrator + member watchers).
    struct WsWatchKiller {
        bool armed = true;
        ~WsWatchKiller() { if (armed) kill_ws_watch_processes(); }
    } killer;

    fs::path log_file = tmp.path / "ws_watch.log";
    start_ws_watch(root, log_file, "");

    // The orchestrator prints its start line immediately (stderr, unbuffered).
    bool started = poll_log(log_file, "workspace watch:", std::chrono::seconds(15));
    INFO("watch log:\n" << (fs::exists(log_file) ? file_read(log_file) : ""));
    REQUIRE(started);

    // Every member watcher's initial build ran (cache hit, so artifacts exist
    // from the pre-build). The rebuild below proves tool-a's watcher is live.
    fs::path exe_a = ws_app_exe(root, "tool-a");
    REQUIRE(fs::exists(exe_a));

    // Record tool-a's exe mtime, touch its source → member watcher rebuilds.
    std::this_thread::sleep_for(std::chrono::milliseconds(800));  // let watchers settle
    auto t0 = fs::last_write_time(exe_a);
    {
        std::ofstream f(root / "apps/tool-a/src/main.cpp", std::ios::app);
        f << "// workspace-watch-touch\n";
    }

    bool rebuilt = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code ec;
        auto t = fs::last_write_time(exe_a, ec);
        if (!ec && t > t0) { rebuilt = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    INFO("watch log after touch:\n"
         << (fs::exists(log_file) ? file_read(log_file) : ""));
    killer.armed = false;  // explicit kill below (already-asserted path)
    kill_ws_watch_processes();
    REQUIRE(rebuilt);
}
