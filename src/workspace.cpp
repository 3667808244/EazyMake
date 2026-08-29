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
#include <fstream>
#include <set>
#include <sstream>
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
    auto resolved = resolve_member_ref(ws, ref);
    if (resolved) return *resolved;

    // Distinguish ambiguous (multiple basename matches) from unknown so the
    // error message can tell the user to use the full relative path.
    size_t count = 0;
    for (size_t i = 0; i < ws.members.size(); ++i) {
        if (ws.members[i].basename == ref) ++count;
    }
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

// ---- 1.3.0-dev.2: member resolution + topological layering ----

std::optional<size_t> resolve_member_ref(const Workspace& ws,
                                         const std::string& ref) {
    // 1) Exact full relative path match.
    for (size_t i = 0; i < ws.members.size(); ++i) {
        if (ws.members[i].name == ref) return i;
    }
    // 2) Basename match — must be unambiguous. Self-basename matches resolve
    //    too (they surface as self-loops in cycle detection upstream).
    size_t match = ws.members.size();
    size_t count = 0;
    for (size_t i = 0; i < ws.members.size(); ++i) {
        if (ws.members[i].basename == ref) {
            match = i;
            ++count;
        }
    }
    if (count == 1) return match;
    // Unknown ref or ambiguous basename collision — nullopt.
    return std::nullopt;
}

std::vector<std::vector<size_t>> topo_layers(const Workspace& ws) {
    const size_t n = ws.members.size();

    // Re-resolve the dependency graph over VALID members. validate_ws_deps
    // already guarantees: a valid member's ws_deps all resolve and point to
    // valid members (referencers of invalid members were marked invalid).
    // Everything below is defensive against internal drift.
    // Edge direction: dep → i (prerequisite edge); indeg[i] counts i's
    // dependencies, so zero-in-degree members have no dependencies and build
    // first.
    std::vector<std::vector<size_t>> edges(n);
    std::vector<size_t> indeg(n, 0);
    for (size_t i = 0; i < n; ++i) {
        const auto& m = ws.members[i];
        if (!m.valid) continue;
        edges[i].reserve(m.ws_deps.size());
        for (const auto& ref : m.ws_deps) {
            auto dep = resolve_member_ref(ws, ref);
            if (!dep || !ws.members[*dep].valid) {
                throw std::runtime_error(
                    "workspace: internal error: unresolved dependency '" + ref +
                    "' for member '" + m.name + "'");
            }
            edges[*dep].push_back(i);
            ++indeg[i];
        }
    }

    // Kahn: peel zero-in-degree members into successive layers. Members in
    // the same layer have no dependency between them → intra-layer parallelism.
    std::vector<std::vector<size_t>> layers;
    std::vector<bool> done(n, false);
    size_t remaining = 0;
    for (size_t i = 0; i < n; ++i) {
        if (ws.members[i].valid) ++remaining;
    }

    while (remaining > 0) {
        std::vector<size_t> layer;
        for (size_t i = 0; i < n; ++i) {
            if (!ws.members[i].valid || done[i]) continue;
            if (indeg[i] == 0) layer.push_back(i);
        }
        if (layer.empty()) {
            // Cycle — validate_ws_deps rejects these at config time; reaching
            // here means an internal invariant broke.
            throw std::runtime_error(
                "workspace: internal error: dependency cycle detected during "
                "topological layering");
        }
        for (size_t i : layer) {
            done[i] = true;
            --remaining;
            for (size_t d : edges[i]) --indeg[d];
        }
        layers.push_back(std::move(layer));
    }
    return layers;
}

// ---- 1.4.0-dev.7: workspace scan — adopt an existing directory tree ----

namespace {

// '/' -separated, trailing '/' stripped — canonical form used for member
// comparisons in merge_members. Raw input is preserved for output; only the
// comparison key is normalized.
std::string member_compare_key(const std::string& p) {
    std::string s = p;
    std::replace(s.begin(), s.end(), '\\', '/');
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

// Atomic text write: temp file + util::atomic_rename (crash-safe). Binary
// mode — text mode would translate '\n' → '\r\n' on Windows and corrupt a
// CRLF-preserving splice into '\r\r\n'.
void atomic_write_text(const fs::path& target, const std::string& content) {
    auto tmp = target;
    tmp += ".tmp";
    {
        std::ofstream of(tmp, std::ios::binary);
        of << content;
    }
    util::atomic_rename(tmp, target);
}

// Leading whitespace of a line.
std::string_view ltrim(std::string_view s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// Render the merged members list as a single-line TOML assignment
// (`members = ["a", "b"]`). util::toml_quote already wraps in quotes.
std::string render_members_line(const std::vector<std::string>& members) {
    std::string line = "members = [";
    for (size_t i = 0; i < members.size(); ++i) {
        if (i > 0) line += ", ";
        line += util::toml_quote(members[i]);
    }
    line += "]";
    return line;
}

// True when the trimmed line starts the `members` KEY (not `membership`).
bool is_members_key(std::string_view t) {
    if (t.rfind("members", 0) != 0) return false;
    if (t.size() == 7) return true;           // bare "members" (no '=')
    char c = t[7];
    return c == ' ' || c == '\t' || c == '='; // "members =", "members=..."
}

// Replace the `members` array of the [workspace] table in the source text of
// an existing workspace file. Everything else (name, [workspace.options],
// comments, formatting) is preserved byte-for-byte — toml++ v3.4 does NOT
// store comments in its AST, so a formatter round-trip would silently drop
// them; this text-level splice is the only way to keep them.
// Multi-line member arrays collapse to a single line. Throws when the file
// has no [workspace] section (degenerate — never silently rewritten).
std::string replace_members_in_text(const std::string& text,
                                    const std::string& file_str,
                                    const std::vector<std::string>& members) {
    // Split into lines, keeping their terminators.
    std::vector<std::string> lines;
    {
        size_t pos = 0;
        while (pos <= text.size()) {
            size_t nl = text.find('\n', pos);
            if (nl == std::string::npos) {
                lines.push_back(text.substr(pos));
                break;
            }
            lines.push_back(text.substr(pos, nl - pos + 1));
            pos = nl + 1;
        }
    }
    std::string eol = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::string new_line = render_members_line(members) + eol;

    std::string section;
    int ws_header = -1;  // line index of the [workspace] header
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string_view t = ltrim(std::string_view(lines[i]));
        if (!t.empty() && t.back() == '\r') t.remove_suffix(1);  // CRLF
        if (t.empty()) continue;
        if (t.front() == '[') {
            size_t end = t.find(']');
            if (end != std::string_view::npos) {
                std::string hdr(t.substr(1, end - 1));
                size_t a = hdr.find_first_not_of(" \t");
                size_t b = hdr.find_last_not_of(" \t");
                hdr = (a == std::string::npos) ? std::string()
                                               : hdr.substr(a, b - a + 1);
                section = hdr;
                if (hdr == "workspace" && ws_header < 0) {
                    ws_header = static_cast<int>(i);
                }
            }
            continue;
        }
        if (section == "workspace" && is_members_key(t)) {
            size_t last = i;
            if (t.find(']') == std::string_view::npos) {
                // Multi-line array: scan forward to the line containing ']'.
                for (size_t j = i + 1; j < lines.size(); ++j) {
                    if (lines[j].find(']') != std::string::npos) {
                        last = j;
                        break;
                    }
                }
            }
            lines[i] = new_line;
            lines.erase(lines.begin() + static_cast<long>(i) + 1,
                        lines.begin() + static_cast<long>(last) + 1);
            std::string out;
            for (const auto& l : lines) out += l;
            if (out.empty() || out.back() != '\n') out += eol;
            return out;
        }
    }
    if (ws_header < 0) {
        throw std::runtime_error(
            i18n::fmt(I18nKey::workspace_err_no_section,
                      {{"file", file_str}}));
    }
    // No members key yet — insert right after the [workspace] header.
    lines.insert(lines.begin() + ws_header + 1, new_line);
    std::string out;
    for (const auto& l : lines) out += l;
    if (out.empty() || out.back() != '\n') out += eol;
    return out;
}

// Recursive scan of `dir` (rel = its path relative to `root`). Appends member
// candidates and skipped entries into `result`. See scan_projects() doc.
void scan_dir(const fs::path& root, const fs::path& rel, const fs::path& dir,
              ScanResult& result) {
    std::error_code ec;
    auto it = fs::directory_iterator(
        dir, fs::directory_options::skip_permission_denied, ec);
    for (; !ec && it != fs::directory_iterator(); it.increment(ec)) {
        const auto& entry = *it;
        std::string name = entry.path().filename().string();
        if (!name.empty() && name[0] == '.') continue;  // hidden — skip subtree

        std::error_code sec;
        if (!entry.is_directory(sec)) continue;  // follows symlinks to dirs

        fs::path sub = dir / name;
        fs::path sub_rel = rel / name;
        std::string rel_str = sub_rel.generic_string();

        // Nested workspace root — the whole subtree belongs to that workspace
        // (validate_member would reject the dir as a member anyway).
        if (util::file_exists(sub / "ezmk-workspace.toml")) {
            result.skipped.emplace_back(
                rel_str, i18n::get(I18nKey::workspace_scan_skip_nested));
            continue;
        }
        // Symlink escape — canonical path leaves the root.
        if (!is_within(canonicalize(sub), canonicalize(root))) {
            result.skipped.emplace_back(
                rel_str, i18n::get(I18nKey::workspace_scan_skip_escape));
            continue;
        }
        // Candidate member.
        if (util::file_exists(sub / "ezmk.toml")) {
            result.members.push_back(rel_str);
        }
        // Keep descending — nested projects are allowed (e.g. a project that
        // itself contains another project directory).
        scan_dir(root, sub_rel, sub, result);
    }
}

} // anonymous namespace

ScanResult scan_projects(const fs::path& root) {
    ScanResult result;
    scan_dir(root, fs::path(), root, result);
    std::sort(result.members.begin(), result.members.end());
    return result;
}

std::vector<std::string> merge_members(
    const std::vector<std::string>& existing,
    const std::vector<std::string>& discovered) {
    std::vector<std::string> out;
    std::set<std::string> seen;
    for (const auto& m : existing) {
        if (seen.insert(member_compare_key(m)).second) out.push_back(m);
    }
    for (const auto& m : discovered) {
        if (seen.insert(member_compare_key(m)).second) out.push_back(m);
    }
    return out;
}

std::vector<std::string> read_workspace_members(const fs::path& root) {
    auto file = root / "ezmk-workspace.toml";
    toml::table table;
    try {
        table = toml::parse_file(file.string());
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(
            "failed to parse " + file.string() + ":\n  " + e.what());
    }
    std::vector<std::string> out;
    auto ws_table = table["workspace"].as_table();
    if (!ws_table) return out;
    auto members_node = (*ws_table)["members"];
    if (!members_node.is_array()) return out;
    for (const auto& v : *members_node.as_array()) {
        if (auto s = v.value<std::string>()) out.push_back(*s);
    }
    return out;
}

void write_workspace_file(const fs::path& root,
                          const std::vector<std::string>& members) {
    std::string content = "[workspace]\nmembers = [";
    for (size_t i = 0; i < members.size(); ++i) {
        if (i > 0) content += ", ";
        content += util::toml_quote(members[i]);
    }
    content += "]\n";
    atomic_write_text(root / "ezmk-workspace.toml", content);
}

void update_workspace_file(const fs::path& root,
                           const std::vector<std::string>& members) {
    auto file = root / "ezmk-workspace.toml";
    // Text-level splice, NOT a toml++ formatter round-trip: toml++ v3.4 does
    // not store comments in its AST, so a round-trip would silently drop
    // user comments. Only the members array is replaced; everything else is
    // preserved byte-for-byte. The caller has already parsed the file (via
    // read_workspace_members), so a syntax error here is impossible.
    std::string updated =
        replace_members_in_text(util::file_read(file), file.string(), members);
    atomic_write_text(file, updated);
}

} // namespace ezmk::workspace
