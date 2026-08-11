#pragma once

#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"

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

} // namespace ezmk::export_gen
