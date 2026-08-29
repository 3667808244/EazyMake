#pragma once

// 1.3.0-dev.1 — Workspace configuration: ezmk-workspace.toml parsing, root
// location, member validation and member dependency validation.
//
// A workspace is a directory (the workspace root) that contains an
// `ezmk-workspace.toml` plus a set of independent member projects (each with
// its own ezmk.toml). Members may declare one-way, acyclic dependencies on
// other members via `[depends] workspace = [...]` in their own ezmk.toml.
//
// This module is fully independent from the single-project config parser
// (config.cpp) except for one small extension: config::DependsSection gains an
// optional `workspace` vector so member ezmk.toml files can declare sibling
// dependencies. dev.2 consumes the validated Workspace for the `ezmk workspace`
// command group (topological build, parallel execution).

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ezmk::workspace {
namespace fs = std::filesystem;

// [workspace.options] — 1.3.0-dev.1
struct Options {
    int default_jobs = 0;        // 0 = auto (hardware_concurrency)
    bool stop_on_error = false;
};

// One workspace member — a single project with its own ezmk.toml.
struct Member {
    std::string name;            // relative path from workspace root (e.g. "apps/tool-a")
    std::string basename;        // last segment (e.g. "tool-a"), used by `workspace = [...]` refs
    fs::path path;               // resolved absolute path (canonical)
    std::string type;            // member ezmk.toml [project].type ("" if not readable)
    std::vector<std::string> ws_deps;  // [depends] workspace refs (member basename/relative path)
    bool valid = false;          // passed validation (exists + ezmk.toml + no nesting)
    std::string error;           // reason when invalid
};

struct Workspace {
    std::string name;            // [workspace].name (may be empty)
    Options options;             // [workspace.options]
    std::vector<Member> members;
    fs::path root;               // canonical path of the directory containing ezmk-workspace.toml
};

// ---- Root location (symmetric to util::locate_project_root) ----

// Upward workspace-root search limit — mirrors util::kProjectRootMaxUp.
inline constexpr int kWorkspaceRootMaxUp = 5;

// Starting from `start_dir` (itself level 0), walk up at most `max_up` parent
// directories looking for one that contains an `ezmk-workspace.toml`. Returns
// that directory if found, otherwise nullopt. Independent from project-root
// location: a directory may be both a project root and a workspace root.
std::optional<fs::path> locate_workspace_root(const fs::path& start_dir,
                                              int max_up = kWorkspaceRootMaxUp);

// ---- Loading & validation ----

// Locate the workspace root from `start_dir` and load + fully validate the
// workspace: parses ezmk-workspace.toml, validates every member (path safety
// throws; existence/nesting mark the member invalid), then resolves and checks
// member dependencies (unknown ref / cycle / non-static dependency throw).
// Returns nullopt when no ezmk-workspace.toml is found upward.
// Throws std::runtime_error on any configuration error.
std::optional<Workspace> load_from(const fs::path& start_dir);

// Validate a single member: path safety (relative, no `..` escape, no
// absolute/drive/UNC, canonical path stays inside the root — violations throw),
// existence + ezmk.toml, and no nested ezmk-workspace.toml (existence/nesting
// violations mark the member invalid with `error` set, they do not throw).
void validate_member(const Workspace& ws, Member& m);

// Validate member dependencies: reads each valid member's ezmk.toml
// ([project].type + [depends].workspace), resolves references (full relative
// path or basename), marks members that reference invalid members invalid,
// detects dependency cycles (including self-loops) via DFS, and enforces the
// "dependency must be type = static" rule. Unknown/ambiguous refs, cycles and
// non-static dependencies throw std::runtime_error.
void validate_ws_deps(Workspace& ws);

// 1.3.0-dev.2: Resolve a `[depends] workspace` reference (member basename or
// full relative path) against the member list — exact full-relative-path match
// first, then a UNIQUE basename match. Returns the member index, or nullopt
// when the ref matches nothing or is ambiguous (basename collision).
// Used by the sibling-injection path (build.cpp) and --member selection.
std::optional<size_t> resolve_member_ref(const Workspace& ws,
                                         const std::string& ref);

// 1.3.0-dev.2: Kahn topological layering over the member dependency graph.
// Returns layers of member indices: every member in layer N depends only on
// members in layers < N, so layers can be executed in order with intra-layer
// parallelism. Invalid members are excluded (they are never built). Cycles
// were rejected at config time (validate_ws_deps) — this is a defensive
// re-check that throws std::runtime_error if one somehow exists.
std::vector<std::vector<size_t>> topo_layers(const Workspace& ws);

// 1.4.0-dev.7: `ezmk workspace scan` — adopt an existing directory tree.
//
// Result of a recursive scan of `root`: every subdirectory containing an
// `ezmk.toml` becomes a member candidate. Paths are relative to `root`,
// '/' -separated (generic_string) and sorted lexicographically for
// deterministic output. The root itself (even when it has its own ezmk.toml)
// is never a member — it is the container.
struct ScanResult {
    std::vector<std::string> members;   // relative member paths (sorted)
    std::vector<std::pair<std::string, std::string>> skipped;  // (path, reason)
};

// Recursively scan `root` for ezmk projects. Skip rules (1.4.0-dev.7 §3.2):
//   * hidden entries (first char '.') — whole subtree skipped silently
//   * directories containing their own ezmk-workspace.toml (nested workspace
//     root) — whole subtree skipped, recorded in `skipped`
//   * directories whose canonical path escapes the root (symlink escape) —
//     skipped, recorded in `skipped`
// Member directories are still descended into (nested projects are allowed).
ScanResult scan_projects(const fs::path& root);

// Merge an existing member list with newly discovered ones. Normalizes each
// path for comparison ('\' → '/', trailing '/' stripped, dedupe); keeps the
// EXISTING entries in their original order and spelling, then appends the
// discovered entries that are missing (already sorted). Never removes.
std::vector<std::string> merge_members(
    const std::vector<std::string>& existing,
    const std::vector<std::string>& discovered);

// Read just the [workspace].members array of an existing workspace file —
// a light parse that does NOT run member/dependency validation (unlike
// load_from, which would throw on stale deps or missing member dirs).
// Throws std::runtime_error on TOML syntax errors; returns an empty vector
// when the file has no [workspace].members section.
std::vector<std::string> read_workspace_members(const fs::path& root);

// Write a fresh ezmk-workspace.toml at `root` with exactly `members`
// (atomic temp + rename).
void write_workspace_file(const fs::path& root,
                          const std::vector<std::string>& members);

// Update an existing ezmk-workspace.toml at `root`: replace its members
// array with `members`, preserving everything else (name / [workspace.options]
// / comments / formatting) byte-for-byte. Implemented as a text-level splice
// (toml++ v3.4 drops comments in its AST, so a formatter round-trip would
// lose them). Multi-line member arrays collapse to a single line. Atomic
// write; throws when the file has no [workspace] section.
void update_workspace_file(const fs::path& root,
                           const std::vector<std::string>& members);

} // namespace ezmk::workspace
