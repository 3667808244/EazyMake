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

// 1.4.0-dev.5: `ezmk workspace watch` — start `ezmk watch` in every selected
// member (long-running subprocesses; the orchestrator waits until all member
// watchers exit, normally via SIGINT which each member handles itself).
// Topological layers still gate startup order (dependencies start first, so
// their initial builds finish before dependents watch). Returns the process
// exit code (0 = all members exited cleanly).
int run_watch(const workspace::Workspace& ws, const cli::WorkspaceOptions& opts);

// `ezmk workspace clean` — reverse-topological, sequential `ezmk clean` per
// member. `--stop-on-error` is rejected at parse time (clean has no
// dependency semantics). Returns the process exit code.
int run_clean(const workspace::Workspace& ws, const cli::WorkspaceOptions& opts);

// 1.4.0-dev.7: `ezmk workspace scan` — adopt an existing directory tree as a
// workspace. Scans `opts.dir` (default cwd) recursively for ezmk.toml
// members; when an existing workspace root is found upward it is reused (and
// its file merged/updated), otherwise a new ezmk-workspace.toml is created at
// `opts.dir`. `--dry-run` previews without writing; `-y` skips the merge
// confirmation. Returns 0 on success/cancel, 1 on error.
int run_scan(const cli::WorkspaceScanOptions& opts);

} // namespace ezmk::workspace_build
