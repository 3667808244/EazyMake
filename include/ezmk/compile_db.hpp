#pragma once

#include <filesystem>
#include <string>
#include "ezmk/config.hpp"
#include "ezmk/cli.hpp"
#include "ezmk/cache.hpp"

namespace ezmk::compile_db {
namespace fs = std::filesystem;

// 1.1.1: Generate compile_commands.json (clangd format — `arguments` array,
// `file` relative to project root, `directory` project root absolute, sorted
// by relative path, atomic write via temp → rename).
//
// First overload mirrors a real build: include collection + profile merge via
// build::prepare_compile_input(), then cache::build_compile_args() for every
// source. Used by the standalone `ezmk utils cc` interception (Phase 3).
//
// Second overload reuses an already-prepared cache::CompileInput — the
// build-time state from `ezmk build` auto-generation (Phase 4) — guaranteeing
// the index matches that build entry-for-entry.
//
// project_root: project root (defaults to cin.proj_root when empty).
// output_path: output file (defaults to <project_root>/compile_commands.json).
// No sources → warning + success (no output file).
void generate_compile_db(const config::EzConfig& cfg,
                         const cli::BuildOptions& opts,
                         const fs::path& project_root,
                         const fs::path& output_path = {});

void generate_compile_db(const cache::CompileInput& cin,
                         const fs::path& project_root,
                         const fs::path& output_path = {});

} // namespace ezmk::compile_db
