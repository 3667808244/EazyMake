#pragma once

// 1.3.0-dev.2 — Workspace command execution: `ezmk workspace list/build/test/
// clean`. Each member is executed as its own `ezmk <action>` subprocess
// (cwd = member dir), so per-member caches / Lua state / outputs stay
// isolated. Members are dispatched in Kahn topological layers (dependency
// layers first) with intra-layer parallelism; `--stop-on-error` stops
// dispatching after the first failure without killing in-flight members.

#include <filesystem>
#include <string>
#include <vector>

#include "ezmk/cli.hpp"
#include "ezmk/workspace.hpp"

namespace ezmk::workspace_build {
namespace fs = std::filesystem;

// Path of the ezmk executable used for member subprocesses. Prefers the
// EZMK_TEST_BIN environment variable (set by build.sh for tests), then the
// directory of the running binary (build/ezmk[.exe]); falls back to "ezmk"
// on PATH when neither exists.
fs::path ezmk_exe_path();

// `ezmk workspace list` — print the workspace root, every member
// (name / type / workspace deps) and invalid members with their reason.
void list_workspace(const workspace::Workspace& ws);

// 1.3.0-dev.3: effective parallel-job count for build/test —
// explicit `-j/--jobs` > `[workspace.options].default_jobs` > hardware
// concurrency (1 when the platform reports none). Extracted so the
// precedence rule is unit-testable.
int resolve_jobs(int cli_jobs, const workspace::Workspace& ws);

// `ezmk workspace build` — topological subprocess build with intra-layer
// parallelism. Returns the process exit code (0 = all members succeeded).
int run_build(const workspace::Workspace& ws, const cli::WorkspaceOptions& opts);

// `ezmk workspace test` — same execution model as run_build, per-member
// `ezmk test`. Members without any test sources are skipped (not an error).
int run_test(const workspace::Workspace& ws, const cli::WorkspaceOptions& opts);

// `ezmk workspace clean` — reverse-topological, sequential `ezmk clean` per
// member. `--stop-on-error` is rejected at parse time (clean has no
// dependency semantics). Returns the process exit code.
int run_clean(const workspace::Workspace& ws, const cli::WorkspaceOptions& opts);

} // namespace ezmk::workspace_build
