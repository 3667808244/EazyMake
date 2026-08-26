#pragma once

#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/toolchain.hpp"

#include <filesystem>
#include <string>

namespace ezmk::export_gen {
namespace fs = std::filesystem;

// 1.2.0: `ezmk project export cmake` — one-shot CMakeLists.txt generation from
// ezmk.toml. Single-direction snapshot: ezmk.toml is the source of truth; the
// generated file is a snapshot (regenerate, don't hand-edit).
//
// `project export <target>` reserves `<target>` for future formats
// (make/meson); `cmake` is the first target.

struct ExportOptions {
    std::string output;    // -o/--output (empty → <project_root>/CMakeLists.txt)
    bool overwrite = false; // --overwrite: replace an existing target file
    std::string profile;   // --profile: apply a build profile's flags/macros
    bool resolve = false;  // --resolve: emit concrete installed dep paths (non-portable)
    bool use_glob = true;  // --glob (default) / --no-glob: explicit source list
};

// Build the CMakeLists.txt content (pure, no I/O) — unit-testable.
std::string build_cmake_text(const config::EzConfig& cfg,
                             const fs::path& project_root,
                             const ExportOptions& opts);

// Resolve output path, refuse overwrite, atomically write the generated text.
// Returns 0 on success; a refusal is a fatal (exit 1).
int export_cmake(const config::EzConfig& cfg,
                 const fs::path& project_root,
                 const ExportOptions& opts);

// ===================================================================
// 1.4.0-dev.1: `ezmk project export vscode` — generate .vscode/ debug config
// trio (launch.json + tasks.json + settings.json) from ezmk.toml. Same
// single-direction snapshot contract as `export cmake`: regenerate, don't
// hand-edit. Overwrite safety mirrors export_cmake (refuse unless --overwrite).
// ===================================================================

// Host platform for the per-platform debugger table (compile-time detected;
// parameterized so tests can exercise every row of the table).
enum class HostPlatform { Windows, MacOs, Linux };

HostPlatform current_host_platform();

// Per-platform debugger selection (design doc §3.2). Pure — no I/O.
struct VscodeDebugger {
    std::string type;             // "cppvsdbg" | "cppdbg" | "lldb"
    std::string mi_debugger_path; // "gdb"/"lldb" (cppdbg only; empty otherwise)
    std::string program;          // "build/<name>" or "build/<name>.exe"
};
VscodeDebugger select_vscode_debugger(HostPlatform plat,
                                      toolchain::CompilerFamily family,
                                      const std::string& project_name);

// The three generated files as JSON text (pure, no I/O) — unit-testable.
struct VscodeFiles {
    std::string launch;
    std::string tasks;
    std::string settings;
};
VscodeFiles build_vscode_files(const config::EzConfig& cfg,
                               const fs::path& project_root,
                               const ExportOptions& opts);

// Write the trio under <project_root>/.vscode/ with overwrite protection and
// atomic writes. Returns 0 on success; a refusal is a fatal (exit 1).
int export_vscode(const config::EzConfig& cfg,
                  const fs::path& project_root,
                  const ExportOptions& opts);

} // namespace ezmk::export_gen
