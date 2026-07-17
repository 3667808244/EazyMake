#pragma once
// Shared test fixtures for EazyMake unit/integration tests.
// Include this header to get TempDir, CwdGuard, and EnvGuard.

#include "catch2.hpp"
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <string>
#include <fstream>

#ifdef _WIN32
#include <io.h>
#endif

namespace fs = std::filesystem;

// RAII temporary directory — created on construction, removed on destruction.
struct TempDir {
    fs::path path;

    TempDir() {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("ezmk_test_" + std::to_string(ts));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

// RAII working directory guard — changes CWD on construction, restores on destruction.
struct CwdGuard {
    fs::path original;
    fs::path temp_dir;

    CwdGuard() : original(fs::current_path()) {
        temp_dir = fs::temp_directory_path() / ("ezmk_cwd_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(temp_dir);
        fs::current_path(temp_dir);
    }

    ~CwdGuard() {
        fs::current_path(original);
        std::error_code ec;
        fs::remove_all(temp_dir, ec);
    }
};

// RAII environment variable guard — sets on construction, restores on destruction.
struct EnvGuard {
    std::string name;
    bool had_old;
    std::string old_val;

    EnvGuard(const char* n, const char* v) : name(n) {
        const char* old = std::getenv(n);
        had_old = (old != nullptr);
        if (had_old) old_val = old;
#ifdef _WIN32
        _putenv_s(n, v ? v : "");
#else
        if (v) setenv(n, v, 1);
        else unsetenv(n);
#endif
    }

    ~EnvGuard() {
#ifdef _WIN32
        _putenv_s(name.c_str(), had_old ? old_val.c_str() : "");
#else
        if (had_old) setenv(name.c_str(), old_val.c_str(), 1);
        else unsetenv(name.c_str());
#endif
    }
};

// Write a minimal ezmk.toml for tests that need project config.
inline void write_minimal_config(const fs::path& dir,
                                  const std::string& name = "testproj",
                                  const std::string& version = "0.1.0") {
    std::ofstream of(dir / "ezmk.toml");
    of << "[project]\nname = \"" << name << "\"\ntype = \"executable\"\nversion = \""
       << version << "\"\nlanguage = \"C++17\"\n\n"
       << "[compile]\nflags = [\"-Wall\", \"-Wextra\"]\ninclude_dirs = [\"include\"]\n\n"
       << "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
       << "[depends]\nlib = []\n";
}
