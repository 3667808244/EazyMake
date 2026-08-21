// 1.3.0-dev.2 — Workspace command execution.
//
// The orchestrator runs `ezmk <action>` in each member as a subprocess
// (cwd = member dir), so each member build/test/clean keeps its own cache,
// Lua state and output. Members self-discover sibling artifacts at build time
// (build.cpp) — the orchestrator itself only needs to order and dispatch.
//
// Execution model (dev.2 §3.3):
//   * layers = Kahn topological layering (dependency layers first)
//   * intra-layer parallelism via util::ThreadPool (workers = -j / default_jobs)
//   * --stop-on-error: after the first failure the scheduler stops dispatching
//     (not-yet-started members of the current layer + all later layers become
//     `skipped`); in-flight members run to completion, never killed
//   * summary with succeeded / failed / skipped + non-zero exit on failure

#include "ezmk/workspace_build.hpp"

#include "ezmk/config.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/thread_pool.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace ezmk::workspace_build {
namespace fs = std::filesystem;
using ezmk::i18n::I18nKey;

namespace {

// 1.3.0-dev.2: `--member <name>` selection = named members + their dependency
// closure (dev.2 §3.1): the target member and every transitive dependency are
// built in topological order so sibling artifacts are fresh. Unknown member
// → fatal. Returns layers filtered to the selection (empty names = all).
std::vector<std::vector<size_t>> select_layers(
    const workspace::Workspace& ws,
    const std::vector<std::vector<size_t>>& layers,
    const std::vector<std::string>& names) {
    if (names.empty()) return layers;

    // Resolve the named roots.
    std::vector<bool> selected(ws.members.size(), false);
    for (const auto& name : names) {
        auto idx = workspace::resolve_member_ref(ws, name);
        if (!idx) {
            std::string list;
            for (size_t i = 0; i < ws.members.size(); ++i) {
                if (i > 0) list += ", ";
                list += ws.members[i].name;
            }
            util::fatal(ezmk::i18n::fmt(I18nKey::workspace_err_unknown_member,
                                        {{"name", name}}));
        }
        selected[*idx] = true;
    }

    // Dependency closure over FORWARD edges (i → its deps): BFS from the
    // selected roots collects every transitive dependency, so the closure is
    // built in topological order before the target member.
    std::vector<std::vector<size_t>> fwd(ws.members.size());
    for (size_t i = 0; i < ws.members.size(); ++i) {
        if (!ws.members[i].valid) continue;
        for (const auto& ref : ws.members[i].ws_deps) {
            if (auto dep = workspace::resolve_member_ref(ws, ref)) {
                if (*dep < ws.members.size() && ws.members[*dep].valid) {
                    fwd[i].push_back(*dep);
                }
            }
        }
    }
    std::vector<bool> in = selected;
    std::vector<size_t> stack;
    for (size_t i = 0; i < ws.members.size(); ++i) {
        if (in[i]) stack.push_back(i);
    }
    while (!stack.empty()) {
        size_t cur = stack.back();
        stack.pop_back();
        for (size_t next : fwd[cur]) {
            if (!in[next]) {
                in[next] = true;
                stack.push_back(next);
            }
        }
    }

    // Filter the topo layers to the selection, preserving layer order.
    std::vector<std::vector<size_t>> out;
    for (const auto& layer : layers) {
        std::vector<size_t> filtered;
        for (size_t idx : layer) {
            if (in[idx]) filtered.push_back(idx);
        }
        if (!filtered.empty()) out.push_back(std::move(filtered));
    }
    return out;
}

// Print the captured subprocess output with a per-member prefix.
// Child stdout → parent stdout; child stderr → parent stderr.
void print_prefixed(const std::string& member, const std::string& text,
                    bool to_stderr) {
    if (text.empty()) return;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        // Trim a trailing \r (child \r progress remnants) for clean output.
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string prefixed = "[" + member + "] " + line;
        if (to_stderr) {
            std::cerr << prefixed << "\n";
        } else {
            std::cout << prefixed << "\n";
        }
    }
}

// True when the member actually has test sources (any configured test dir
// exists and contains a .c/.cc/.cpp/.cxx). Members without tests are skipped
// by `workspace test` — running `ezmk test` on them would fatal on "no test
// source files".
bool member_has_tests(const fs::path& member_dir, const config::EzConfig& cfg) {
    for (const auto& d : cfg.test.dirs) {
        fs::path dir = d;
        if (dir.is_relative()) dir = member_dir / dir;
        if (util::file_exists(dir)) {
            auto files = util::list_files(dir, {".c", ".cc", ".cpp", ".cxx"});
            if (!files.empty()) return true;
        }
    }
    return false;
}

// 1.3.0-dev.2: run one member's `ezmk <action>` subprocess (cwd = member dir)
// and stream its output prefixed with the member name. Returns true on success.
// The stop-check lives in the caller (task body) so a not-yet-started task
// counts as `skipped`, not `failed`.
bool run_member(const workspace::Member& m, const std::string& action,
                std::mutex& print_mutex) {
    {
        std::lock_guard<std::mutex> lk(print_mutex);
        util::info(I18nKey::workspace_member_start,
                   {{"member", m.name}, {"action", action}});
    }

    std::string cmd = "\"" + ezmk_exe_path().string() + "\" " + action;
    util::RunOptions ro;
    ro.cwd = m.path;
    auto res = util::run_command(cmd, ro);

    {
        std::lock_guard<std::mutex> lk(print_mutex);
        print_prefixed(m.name, res.out, false);
        print_prefixed(m.name, res.err, true);
    }
    return res.exit_code == 0;
}

// 1.3.0-dev.2: shared build/test dispatcher. `action` is the member
// subcommand ("build" / "test"). Returns the process exit code.
int run_workspace_action(const workspace::Workspace& ws,
                         const std::string& action,
                         const cli::WorkspaceOptions& opts) {
    // Jobs: -j/--jobs > [workspace.options].default_jobs > hardware_concurrency.
    int jobs = opts.jobs;
    if (jobs <= 0) jobs = ws.options.default_jobs;
    if (jobs <= 0) {
        jobs = static_cast<int>(std::thread::hardware_concurrency());
        if (jobs <= 0) jobs = 1;
    }

    auto layers = workspace::topo_layers(ws);
    auto sel = select_layers(ws, layers, opts.members);

    size_t total = 0;
    for (const auto& l : sel) total += l.size();
    util::info(action == "build" ? I18nKey::workspace_build_start
                                 : I18nKey::workspace_test_start,
               {{"count", std::to_string(total)}, {"jobs", std::to_string(jobs)}});

    // Members that failed validation are skipped with a warning (dev.1:
    // "执行时跳过不阻断").
    std::vector<const workspace::Member*> invalid;
    for (const auto& m : ws.members) {
        if (!m.valid) invalid.push_back(&m);
    }

    std::atomic<bool> stop{false};
    std::atomic<int> succeeded{0}, failed{0}, skipped{0};
    std::mutex print_mutex;

    for (const auto& layer : sel) {
        if (stop.load()) {
            // --stop-on-error fired in an earlier layer: whole layer skipped.
            skipped.fetch_add(static_cast<int>(layer.size()));
            continue;
        }
        util::ThreadPool pool(static_cast<size_t>(jobs));
        std::vector<std::future<void>> futures;
        futures.reserve(layer.size());
        for (size_t idx : layer) {
            const auto& m = ws.members[idx];
            // Per-member test-availability check (test action only).
            if (action == "test") {
                bool has_tests = true;
                try {
                    auto cfg = config::parse_config((m.path / "ezmk.toml").string());
                    has_tests = member_has_tests(m.path, cfg);
                } catch (...) {
                    has_tests = false;
                }
                if (!has_tests) {
                    {
                        std::lock_guard<std::mutex> lk(print_mutex);
                        util::info(I18nKey::workspace_test_no_tests,
                                   {{"member", m.name}});
                    }
                    skipped.fetch_add(1);
                    continue;
                }
            }
            futures.push_back(pool.submit([&ws, idx, &action, &stop, &print_mutex,
                                           &succeeded, &failed, &skipped,
                                           &opts]() {
                // --stop-on-error fired while this task was queued → skipped
                // (never started), not failed.
                if (stop.load()) {
                    skipped.fetch_add(1);
                    return;
                }
                bool ok = run_member(ws.members[idx], action, print_mutex);
                if (ok) {
                    succeeded.fetch_add(1);
                } else {
                    failed.fetch_add(1);
                    if (opts.stop_on_error) stop.store(true);
                }
            }));
        }
        // Await every in-flight member of this layer (never kill a running
        // subprocess — the design's "不 kill 在跑成员").
        for (auto& f : futures) {
            f.get();
        }
    }

    // Warn about invalid members (they are not in any layer).
    for (const auto* m : invalid) {
        util::warn(I18nKey::workspace_list_invalid,
                   {{"name", m->name}, {"reason", m->error}});
    }

    util::info(I18nKey::workspace_summary,
               {{"action", action},
                {"succeeded", std::to_string(succeeded.load())},
                {"failed", std::to_string(failed.load())},
                {"skipped", std::to_string(skipped.load())}});
    return failed.load() == 0 ? 0 : 1;
}

} // anonymous namespace

fs::path ezmk_exe_path() {
    // Tests (build.sh test / test-all) set EZMK_TEST_BIN to build/ezmk — the
    // value has NO ".exe". CreateProcessA appends .exe only for extension-less
    // module names WITHOUT a path, so a path-qualified bare name fails to
    // spawn — and a stray extension-less build/ezmk file would make it worse.
    // On Windows always prefer the .exe form.
    const char* env = std::getenv("EZMK_TEST_BIN");
    if (env && env[0]) {
        fs::path p(env);
#ifdef EZMK_WIN
        fs::path with_ext(p.string() + EZMK_EXE_SUFFIX);
        if (util::file_exists(with_ext)) return with_ext;
#endif
        if (util::file_exists(p)) return p;
    }
    // Production: the directory of the running binary.
    fs::path candidate = util::get_exe_dir() / ("ezmk" EZMK_EXE_SUFFIX);
    if (util::file_exists(candidate)) return candidate;
    return fs::path("ezmk");  // last resort: resolve via PATH
}

void list_workspace(const workspace::Workspace& ws) {
    util::info_line(ezmk::i18n::fmt(
        I18nKey::workspace_list_header,
        {{"count", std::to_string(ws.members.size())},
         {"root", ws.root.string()}}));
    for (const auto& m : ws.members) {
        if (m.valid) {
            util::info_line(ezmk::i18n::fmt(
                I18nKey::workspace_list_member,
                {{"name", m.name},
                 {"type", m.type.empty() ? "?" : m.type}}));
            if (!m.ws_deps.empty()) {
                std::string deps;
                for (size_t i = 0; i < m.ws_deps.size(); ++i) {
                    if (i > 0) deps += ", ";
                    deps += m.ws_deps[i];
                }
                util::info_line(ezmk::i18n::fmt(I18nKey::workspace_list_deps,
                                                {{"deps", deps}}));
            }
        } else {
            util::info_line(ezmk::i18n::fmt(
                I18nKey::workspace_list_invalid,
                {{"name", m.name}, {"reason", m.error}}));
        }
    }
}

int run_build(const workspace::Workspace& ws, const cli::WorkspaceOptions& opts) {
    return run_workspace_action(ws, "build", opts);
}

int run_test(const workspace::Workspace& ws, const cli::WorkspaceOptions& opts) {
    return run_workspace_action(ws, "test", opts);
}

int run_clean(const workspace::Workspace& ws, const cli::WorkspaceOptions& opts) {
    auto layers = workspace::topo_layers(ws);
    auto sel = select_layers(ws, layers, opts.members);

    size_t total = 0;
    for (const auto& l : sel) total += l.size();
    util::info(I18nKey::workspace_clean_start,
               {{"count", std::to_string(total)}});

    int cleaned = 0;
    int failed = 0;
    // Reverse topological order — dependents before their dependencies.
    for (auto it = sel.rbegin(); it != sel.rend(); ++it) {
        for (auto rit = it->rbegin(); rit != it->rend(); ++rit) {
            const auto& m = ws.members[*rit];
            util::info(I18nKey::workspace_member_start,
                       {{"member", m.name}, {"action", "clean"}});
            std::string cmd = "\"" + ezmk_exe_path().string() + "\" clean";
            util::RunOptions ro;
            ro.cwd = m.path;
            auto res = util::run_command(cmd, ro);
            print_prefixed(m.name, res.out, false);
            print_prefixed(m.name, res.err, true);
            if (res.exit_code == 0) {
                ++cleaned;
            } else {
                ++failed;
                util::info(I18nKey::workspace_member_fail,
                           {{"member", m.name},
                            {"code", std::to_string(res.exit_code)}});
            }
        }
    }

    util::info(I18nKey::workspace_clean_done,
               {{"count", std::to_string(cleaned)}});
    return failed == 0 ? 0 : 1;
}

} // namespace ezmk::workspace_build
