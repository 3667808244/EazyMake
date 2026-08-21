// 1.3.0-dev.1 — Workspace configuration: ezmk-workspace.toml parsing, root
// location, member validation and member dependency validation.
//
// Independent from config.cpp's project parsing (no coupling), except for the
// small `config::DependsSection::workspace` extension used to read member
// `[depends] workspace` declarations. See include/ezmk/workspace.hpp.

#include "ezmk/workspace.hpp"

#include "ezmk/config.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

// toml++ header-only (exceptions enabled — same usage as config.cpp)
#include "toml.hpp"

namespace ezmk::workspace {
namespace fs = std::filesystem;
using ezmk::i18n::I18nKey;

namespace {

// [workspace] / [workspace.options] allowlists for unknown-key rejection.
// Unknown sections/keys in a workspace file are a config mistake — fail fast
// with the file path and field name (1.3.0-dev.1, §3.2).
const char* const kWorkspaceKeys[] = {"name", "members", "options"};
const char* const kOptionsKeys[]   = {"default_jobs", "stop_on_error"};

bool contains_key(const char* const* keys, size_t n, std::string_view key) {
    for (size_t i = 0; i < n; ++i) {
        if (key == keys[i]) return true;
    }
    return false;
}

// Canonical form of a path for containment checks. Non-existing paths are
// resolved lexically for their non-existing tail (weakly_canonical) so member
// existence failures still report the intended path. Named canonicalize to
// avoid ADL ambiguity with std::filesystem::canonical.
fs::path canonicalize(const fs::path& p) {
    std::error_code ec;
    auto c = fs::weakly_canonical(p, ec);
    return ec ? fs::absolute(p).lexically_normal() : c;
}

// True when `child` is `parent` itself or located inside it (canonicalized).
bool is_within(const fs::path& child, const fs::path& parent) {
    auto rel = child.lexically_relative(parent);
    if (rel.empty()) return false;
    for (const auto& comp : rel) {
        if (comp == "..") return false;
    }
    return true;
}

// Lexical path-safety checks on the raw member path string (before canonical
// resolution). Returns the error message on failure, empty when safe.
std::string path_safety_error(const Workspace& ws, const std::string& name) {
    if (name.empty()) {
        return i18n::fmt(I18nKey::workspace_err_member_path,
                         {{"member", name}});
    }
    fs::path p(name);
    // Reject absolute paths, drive-relative paths (Windows "C:foo"), UNC
    // roots and root directories — members must be relative to the root.
    if (p.is_absolute() || p.has_root_name() || p.has_root_directory()) {
        return i18n::fmt(I18nKey::workspace_err_member_path,
                         {{"member", name}});
    }
    // Reject any ".." component — no escaping upward from the root.
    for (const auto& comp : p) {
        if (comp == "..") {
            return i18n::fmt(I18nKey::workspace_err_member_path,
                             {{"member", name}});
        }
    }
    // Canonical resolution must stay inside the root — catches symlinks that
    // point outside the workspace (and any remaining escape trick).
    auto member_canon = canonicalize(ws.root / name);
    auto root_canon = canonicalize(ws.root);
    if (!is_within(member_canon, root_canon)) {
        return i18n::fmt(I18nKey::workspace_err_member_escape,
                         {{"member", name}, {"root", root_canon.string()}});
    }
    return {};
}

// Read a member's own ezmk.toml for [project].type and [depends].workspace.
// Only called for members that passed existence validation; a malformed
// member config propagates as a runtime_error (config-time fail-fast).
void read_member_config(Member& m) {
    auto cfg = config::parse_config(m.path / "ezmk.toml");
    m.type = cfg.project.type;
    m.ws_deps = cfg.depends.workspace;
}

// Resolve one `[depends] workspace` reference against the member list.
// Matching: exact full relative path first, then unique basename. Returns the
// member index; throws on unknown ref or ambiguous basename (§3.5 / §6).
size_t resolve_dep_ref(const Workspace& ws, size_t member_idx,
                       const std::string& ref) {
    const auto& m = ws.members[member_idx];

    // 1) Exact full relative path match.
    for (size_t i = 0; i < ws.members.size(); ++i) {
        if (ws.members[i].name == ref) return i;
    }

    // 2) Basename match — must be unambiguous. Self-basename matches are
    //    resolved too (they surface as self-loops in cycle detection).
    size_t match = ws.members.size();
    size_t count = 0;
    for (size_t i = 0; i < ws.members.size(); ++i) {
        if (ws.members[i].basename == ref) {
            match = i;
            ++count;
        }
    }
    if (count == 1) return match;

    if (count > 1) {
        throw std::runtime_error(
            i18n::fmt(I18nKey::workspace_err_dep_ambiguous,
                      {{"member", m.name}, {"dep", ref}}));
    }
    throw std::runtime_error(
        i18n::fmt(I18nKey::workspace_err_dep_unknown,
                  {{"member", m.name}, {"dep", ref}}));
}

// DFS cycle detection over the resolved dependency graph (three colors:
// 0 = unvisited, 1 = in progress, 2 = done). Returns a cycle path string
// ("a -> b -> a") when a cycle (including self-loops) is found.
std::string find_cycle(const Workspace& ws,
                       std::vector<std::vector<size_t>>& edges,
                       std::vector<int>& color,
                       std::vector<size_t>& stack,
                       size_t node) {
    color[node] = 1;  // in progress
    stack.push_back(node);

    for (size_t next : edges[node]) {
        if (color[next] == 1) {
            // Cycle: walk the stack from the first occurrence of `next`.
            auto it = std::find(stack.begin(), stack.end(), next);
            std::string cycle;
            for (auto s = it; s != stack.end(); ++s) {
                if (!cycle.empty()) cycle += " -> ";
                cycle += ws.members[*s].name;
            }
            cycle += " -> " + ws.members[next].name;
            return cycle;
        }
        if (color[next] == 0) {
            auto sub = find_cycle(ws, edges, color, stack, next);
            if (!sub.empty()) return sub;
        }
    }

    stack.pop_back();
    color[node] = 2;  // done
    return {};
}

} // anonymous namespace

// ---- Root location ----

std::optional<fs::path> locate_workspace_root(const fs::path& start_dir,
                                              int max_up) {
    fs::path cur = start_dir;
    for (int i = 0; i <= max_up; ++i) {
        if (util::file_exists(cur / "ezmk-workspace.toml")) return cur;
        if (cur == cur.parent_path()) break;  // reached filesystem root
        cur = cur.parent_path();
    }
    return std::nullopt;
}

// ---- Loading & validation ----

std::optional<Workspace> load_from(const fs::path& start_dir) {
    auto root = locate_workspace_root(start_dir);
    if (!root) return std::nullopt;

    Workspace ws;
    ws.root = canonicalize(*root);
    auto file = ws.root / "ezmk-workspace.toml";

    toml::table table;
    try {
        table = toml::parse_file(file.string());
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(
            "failed to parse " + file.string() + ":\n  " + e.what());
    }

    // Unknown top-level sections → error (§3.2).
    for (const auto& [key, val] : table) {
        std::string key_name(key.str());
        if (key_name != "workspace") {
            throw std::runtime_error(
                i18n::fmt(I18nKey::workspace_err_unknown_key,
                          {{"key", key_name},
                           {"section", "(root)"},
                           {"file", file.string()}}));
        }
        (void)val;
    }

    // [workspace] — required.
    auto ws_table = table["workspace"].as_table();
    if (!ws_table) {
        throw std::runtime_error(
            i18n::fmt(I18nKey::workspace_err_no_section,
                      {{"file", file.string()}}));
    }
    for (const auto& [key, val] : *ws_table) {
        std::string key_name(key.str());
        if (!contains_key(kWorkspaceKeys, 3, key_name)) {
            throw std::runtime_error(
                i18n::fmt(I18nKey::workspace_err_unknown_key,
                          {{"key", key_name},
                           {"section", "[workspace]"},
                           {"file", file.string()}}));
        }
        (void)val;
    }

    // [workspace].name — optional string.
    if (auto name = (*ws_table)["name"].value<std::string>()) {
        ws.name = *name;
    }

    // [workspace].members — required, non-empty, string array.
    auto members_node = (*ws_table)["members"];
    if (!members_node.is_array() || members_node.as_array()->empty()) {
        throw std::runtime_error(
            i18n::fmt(I18nKey::workspace_err_members_required,
                      {{"file", file.string()}}));
    }
    auto& arr = *members_node.as_array();
    for (size_t i = 0; i < arr.size(); ++i) {
        auto val = arr[i].value<std::string>();
        if (!val) {
            throw std::runtime_error(
                i18n::fmt(I18nKey::workspace_err_members_type,
                          {{"index", std::to_string(i)},
                           {"file", file.string()}}));
        }
        Member m;
        m.name = *val;
        // basename = last path segment ("apps/tool-a" → "tool-a")
        fs::path p(*val);
        m.basename = p.filename().string();
        if (m.basename.empty()) m.basename = p.string();  // e.g. "." or "/"
        ws.members.push_back(std::move(m));
    }

    // [workspace.options] — optional table with defaults (§3.2).
    if (auto opts = (*ws_table)["options"].as_table()) {
        for (const auto& [key, val] : *opts) {
            std::string key_name(key.str());
            if (!contains_key(kOptionsKeys, 2, key_name)) {
                throw std::runtime_error(
                    i18n::fmt(I18nKey::workspace_err_unknown_key,
                              {{"key", key_name},
                               {"section", "[workspace.options]"},
                               {"file", file.string()}}));
            }
            (void)val;
        }
        if (auto jobs = (*opts)["default_jobs"]) {
            if (!jobs.is_integer() || jobs.as_integer()->get() < 0) {
                throw std::runtime_error(
                    i18n::fmt(I18nKey::workspace_err_default_jobs,
                              {{"file", file.string()}}));
            }
            ws.options.default_jobs =
                static_cast<int>(jobs.as_integer()->get());
        }
        if (auto stop = (*opts)["stop_on_error"]) {
            if (!stop.is_boolean()) {
                throw std::runtime_error(
                    i18n::fmt(I18nKey::workspace_err_stop_on_error,
                              {{"file", file.string()}}));
            }
            ws.options.stop_on_error = stop.as_boolean()->get();
        }
    }

    // Member validation (path safety throws; existence/nesting mark invalid).
    for (auto& m : ws.members) {
        validate_member(ws, m);
    }

    // Member dependency validation (unknown ref / cycle / type throw).
    validate_ws_deps(ws);

    return ws;
}

void validate_member(const Workspace& ws, Member& m) {
    m.valid = false;

    // Path safety — violations throw (config-time fail-fast).
    auto safety_err = path_safety_error(ws, m.name);
    if (!safety_err.empty()) {
        throw std::runtime_error(safety_err);
    }

    m.path = canonicalize(ws.root / m.name);

    // Existence — marks invalid (execution skips, does not block).
    std::error_code ec;
    if (!fs::is_directory(m.path, ec)) {
        m.error = i18n::fmt(I18nKey::workspace_err_member_missing,
                            {{"member", m.name}});
        return;
    }
    if (!util::file_exists(m.path / "ezmk.toml")) {
        m.error = i18n::fmt(I18nKey::workspace_err_member_no_config,
                            {{"member", m.name}});
        return;
    }

    // Nesting — a member may not contain its own workspace file. The root
    // itself as a member is exempt (its ezmk-workspace.toml is this file).
    if (m.path != ws.root && util::file_exists(m.path / "ezmk-workspace.toml")) {
        m.error = i18n::fmt(I18nKey::workspace_err_member_nested,
                            {{"member", m.name}});
        return;
    }

    m.valid = true;
}

void validate_ws_deps(Workspace& ws) {
    // 1) Read each valid member's ezmk.toml for type + [depends].workspace.
    for (auto& m : ws.members) {
        if (m.valid) read_member_config(m);
    }

    // 2) Resolve references into an index graph; unknown/ambiguous refs throw.
    std::vector<std::vector<size_t>> edges(ws.members.size());
    for (size_t i = 0; i < ws.members.size(); ++i) {
        const auto& m = ws.members[i];
        if (!m.valid) continue;  // no readable config — no deps to resolve
        edges[i].reserve(m.ws_deps.size());
        for (const auto& ref : m.ws_deps) {
            size_t dep = resolve_dep_ref(ws, i, ref);
            // 3) Referencing an invalid member marks the referencer invalid
            //    (e.g. the dependency has no ezmk.toml).
            if (!ws.members[dep].valid) {
                ws.members[i].valid = false;
                ws.members[i].error =
                    i18n::fmt(I18nKey::workspace_err_dep_invalid,
                              {{"member", m.name},
                               {"dep", ws.members[dep].name},
                               {"reason", ws.members[dep].error}});
                continue;
            }
            edges[i].push_back(dep);
        }
    }

    // 4) Cycle detection (incl. self-loops) — DFS with three colors.
    std::vector<int> color(ws.members.size(), 0);
    std::vector<size_t> stack;
    for (size_t i = 0; i < ws.members.size(); ++i) {
        if (color[i] == 0) {
            auto cycle = find_cycle(ws, edges, color, stack, i);
            if (!cycle.empty()) {
                throw std::runtime_error(
                    i18n::fmt(I18nKey::workspace_err_dep_cycle,
                              {{"cycle", cycle}}));
            }
        }
    }

    // 5) Type constraint — dependencies must be type = "static".
    for (size_t i = 0; i < ws.members.size(); ++i) {
        const auto& m = ws.members[i];
        if (!m.valid) continue;
        for (size_t dep : edges[i]) {
            const auto& d = ws.members[dep];
            if (d.type != "static") {
                throw std::runtime_error(
                    i18n::fmt(I18nKey::workspace_err_dep_not_static,
                              {{"member", m.name},
                               {"dep", d.name},
                               {"type", d.type.empty() ? "(none)" : d.type}}));
            }
        }
    }
}

} // namespace ezmk::workspace
