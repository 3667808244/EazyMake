#pragma once
// Shared helpers for EazyMake integration tests (1.3.6: extracted from
// test_integration.cpp's anonymous namespace so the per-theme integration test
// files can share them without duplication). All helpers are `inline` in
// `namespace ezi` — include this header and `using namespace ezi;`.

#include "catch2.hpp"
#include "test_helpers.hpp"
#include "ezmk/util.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/toolchain.hpp"
#include "nlohmann_json.hpp"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::util;

namespace ezi {

// Find the repo root by walking up from the current directory.
// Looks for a directory containing both "build.sh" and "src/main.cpp".
inline fs::path find_repo_root() {
    fs::path cwd = fs::current_path();
    while (!cwd.empty() && cwd != cwd.root_path()) {
        if (fs::exists(cwd / "build.sh") && fs::exists(cwd / "src" / "main.cpp")) {
            return cwd;
        }
        cwd = cwd.parent_path();
    }
    return fs::current_path(); // fallback
}

// Resolve the ezmk binary path.
// 1. EZMK_TEST_BIN env var (highest priority)
// 2. build/ezmk[.exe] relative to repo root
inline fs::path find_ezmk_binary() {
    const char* env = std::getenv("EZMK_TEST_BIN");
    if (env && fs::exists(env)) {
        return fs::path(env);
    }

    fs::path repo_root = find_repo_root();
    fs::path candidate = repo_root / "build" / ("ezmk" EZMK_EXE_SUFFIX);
    if (fs::exists(candidate)) return fs::canonical(candidate);

    // Last resort: relative to cwd
    fs::path fallback = fs::current_path() / "build" / ("ezmk" EZMK_EXE_SUFFIX);
    return fallback;
}

// 1.2.0-dev.8: resolve the standalone ezmk-lua runtime binary (built alongside
// ezmk by build.sh: build/ezmk-lua[.exe]). Skipped gracefully if absent.
inline fs::path find_ezmk_lua_binary() {
    fs::path repo_root = find_repo_root();
    fs::path candidate = repo_root / "build" / ("ezmk-lua" EZMK_EXE_SUFFIX);
    if (fs::exists(candidate)) return fs::canonical(candidate);
    fs::path fallback = fs::current_path() / "build" / ("ezmk-lua" EZMK_EXE_SUFFIX);
    if (fs::exists(fallback)) return fs::canonical(fallback);
    return {};
}

// Build the shell command to run ezmk in a specific working directory.
// Uses "cd <dir> && ezmk <args>" to avoid changing the process CWD.
inline std::string build_ezmk_cmd(const std::string& args, const fs::path& cwd) {
    std::string ezmk_path = find_ezmk_binary().string();

#ifdef EZMK_WIN
    // cmd.exe: use cd /d to switch drive + directory. Double-quote paths
    // (no bash escaping needed — cmd.exe doesn't interpret backslashes).
    return "cd /d \"" + cwd.string() + "\" && \"" + ezmk_path + "\" " + args;
#else
    return "cd " + escape_shell_arg(cwd.string()) + " && " +
           escape_shell_arg(ezmk_path) + " " + args;
#endif
}

// Run ezmk with given arguments in the specified working directory.
// Uses "cd <dir> && ezmk ..." so the process CWD is never changed.
inline ProcResult run_ezmk(const std::string& args, const fs::path& cwd = fs::current_path()) {
    std::string cmd = build_ezmk_cmd(args, cwd);
#ifdef EZMK_WIN
    cmd = "cmd /c " + cmd;
#endif
    return run_command(cmd);
}

// Detect if EazyMake binary is available (skip tests gracefully if not).
inline bool ezmk_available() {
    return fs::exists(find_ezmk_binary());
}

// 1.2.0-dev.11: independent oracle for the precompiled-tag tests. Derives the
// expected compiler tag from the RAW version string instead of calling the
// production toolchain::compiler_tag() — a regression in tag derivation would
// otherwise pass silently (the test's expectation was derived from the very
// code under test). Mirrors the documented rule:
//   "g++ (GCC) 13.2.0"        → gcc13
//   "clang version 18.1.8"    → clang18
// Returns "" when unparseable (MSVC is skipped by the precompiled tests).
inline std::string independent_compiler_tag(const ezmk::toolchain::Toolchain& tc) {
    std::string prefix;
    switch (tc.family) {
    case ezmk::toolchain::CompilerFamily::Gcc:   prefix = "gcc"; break;
    case ezmk::toolchain::CompilerFamily::Clang: prefix = "clang"; break;
    default: return "";  // MSVC — not covered by these tests
    }
    // First "<digits>.<digit" sequence (a real major.minor) → leading number.
    const std::string& v = tc.version;
    for (size_t i = 0; i + 2 < v.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(v[i])) &&
            v[i + 1] == '.' &&
            std::isdigit(static_cast<unsigned char>(v[i + 2]))) {
            size_t start = i;
            while (start > 0 &&
                   std::isdigit(static_cast<unsigned char>(v[start - 1]))) --start;
            return prefix + v.substr(start, i - start + 1);
        }
    }
    return "";
}

// Poll `file` until it contains `needle` (or timeout). ezmk logs to stderr
// unbuffered, so redirected output appears promptly — polling is reliable.
inline bool poll_log(const fs::path& file, const std::string& needle,
                     std::chrono::seconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (fs::exists(file)) {
            std::string content = file_read(file);
            if (content.find(needle) != std::string::npos) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return false;
}

// Kill all ezmk watch processes (same approach as the existing watch test).
inline void kill_watch_processes() {
#ifdef EZMK_WIN
    run_command("cmd /c taskkill /F /IM ezmk.exe 2>nul");
#else
    run_command("pkill -f \"ezmk project watch\" 2>/dev/null || true");
#endif
}

// Count occurrences of a needle in the log (for "ran N times" assertions).
inline size_t count_in_log(const fs::path& log_file, const std::string& needle) {
    if (!fs::exists(log_file)) return 0;
    std::string content = file_read(log_file);
    size_t count = 0, pos = 0;
    while ((pos = content.find(needle, pos)) != std::string::npos) {
        count++;
        pos += needle.size();
    }
    return count;
}

// Recursive (relative path → sha256) map of an extracted directory.
inline std::map<std::string, std::string> tree_hashes(const fs::path& dir) {
    std::map<std::string, std::string> out;
    for (auto& e : fs::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        auto rel = fs::relative(e.path(), dir).generic_string();
        out[rel] = ezmk::crypto::sha256_file(e.path());
    }
    return out;
}

} // namespace ezi
