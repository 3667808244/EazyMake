# Non-Goals (Features We Won't Design)

> EazyMake's design philosophy: **ease of use over feature richness — for complex builds, use CMake.** ([README](../../README.md))

This document lists features EazyMake deliberately does **not** design. For each one it states what the feature is, why it is out of scope, and what to do instead — so you don't wait on a feature request that will never come.

## How a feature becomes a non-goal

A feature is rejected when it would require any of:

1. **Structural complexity** — a target dependency graph, a platform matrix, or a programmable build graph.
2. **What another tool already does well** — especially CMake, which the design philosophy names as the escape hatch for complex builds.
3. **Costs that don't serve small projects** — the config stays one flat `ezmk.toml`; every feature must keep "small and straightforward" the default path.

When a requirement hits one of these, the recommended move is to grow into CMake. EazyMake provides a one-way export so the migration is a command, not a rewrite (see [Migration path](#the-migration-path)).

## Non-goals

### Multiple build targets

- **What**: producing several artifacts (executables / libraries with an inter-target dependency graph) from a single `ezmk.toml`.
- **Why not**: every project has exactly one artifact — `[project].type` is `executable` / `static` / `shared` / `utils`. Real multi-target support would force target selection (`--target`), per-target config, and a dependency graph: precisely the structural complexity this tool exists to avoid. It would also ripple through every consumer of the build model (`build`, `watch`, `project cc`, `export cmake`).
- **Instead**:
  - Split into separate projects (one `ezmk.toml` per artifact).
  - A `utils` package ships several executables via `[utils].tools`.
  - Build variants of one artifact with `[compile.profile.*]` (Debug/Release, …).

### Cross-compilation

- **What**: building for a target platform/architecture different from the host.
- **Why not**: toolchain detection is host-only — `detect_toolchain()` probes the host compiler (`$CXX`/`$CC` only override the host compiler). There is no `--target`, no target triple, no sysroot. Platform keys (`win-x64`, `windows_x86_64_msvc`) are derived at compile time from the host's own architecture, and packages install for the host platform only. Nothing in the pipeline produces or fetches foreign-platform artifacts.
- **Instead**:
  - Build on each target platform directly (e.g. a per-OS/per-arch CI matrix).
  - Use CMake with a toolchain file for full cross-compilation from a single host.

> Note: `[link].system_target` links **system libraries** (`-lpthread`, `-lm`) — despite the name, it has nothing to do with cross-compilation targets.

### Fully programmatic builds

- **What**: expressing the whole build as a script or program (custom rules, build-graph logic in code).
- **Why not**: the build is **declarative** (`ezmk.toml`). Lua via `[hooks]` runs `pre_build` / `post_build` / `on_failure` steps in a sandbox — it is not a build-graph language and never replaces the declarative build. A programmable build is exactly "a complex build".
- **Instead**:
  - Declarative `ezmk.toml` + `[hooks]` covers the overwhelming majority of small projects.
  - For genuine programmability, use CMake.

### Multi-project workspaces / inter-project references

- **What**: one "workspace" where projects reference and build each other.
- **Supported since 1.3.0 (minimal form)**: `ezmk-workspace.toml` defines the member set (`[workspace] members`), with `ezmk workspace build / test / clean` for batch management. Members may declare **one-directional acyclic dependencies** (`[depends] workspace = [...]` in the member's own `ezmk.toml`); builds are topologically ordered with static-library artifact reuse (`-I <ws>/<m>/include` + `-L <ws>/<m>/build -l<m>` injected automatically) — covering the most common monorepo shape: a shared base library plus several executables.
- **Still not designed (non-goal boundary)**:
  - Dependency **cycles** (A→B→A, self-cycles) — rejected at configuration time; member dependency graphs must be **one-directional and acyclic**.
  - Dependencies on members whose type is not `static` (`executable` / `shared` cannot be depended on; shared runtime dependencies are deferred).
  - Dependency **version constraints / platform matrix / programmable build graphs** — versioning and snapshot semantics go through packages (`[depends]` + `ezmk pkg install`); the workspace is for live source-level development only.
  - Multiple build targets inside a single project (see the "Multiple build targets" entry) — the member dependency graph does **not** open the door to per-project multi-target; each member still has exactly one artifact.
- **Instead**:
  - Members with no dependencies → flat batch (plain workspace, all members build in parallel on layer 0).
  - Versioned / distributable reuse → independent projects + shared packages.
  - An interlocked monorepo needing a full build graph (cycles / versions / platform matrix / programmable) → use CMake's multi-target model.

### Precompiled headers (PCH)

- **What**: precompiling a stable set of heavy headers (STL / system headers / large third-party headers such as Boost or nlohmann_json) into a `.gch` / `.pch` artifact that every translation unit reuses instead of re-parsing.
- **Why not**: PCH trips all three non-goal criteria at once:
  - **Structural complexity** — the current compile pipeline is flat + parallel + finely-grained caching (per-TU content hashing + `-MMD` depfiles; invalidation granularity is a single source file). PCH would require a compile **barrier** (the PCH must exist before any TU, conflicting with `-j` parallelism and needing a pre-phase), a new cache-invalidation dimension (the PCH hash enters every TU's cache entry; any header change inside the PCH rebuilds everything), three inconsistent compiler-family semantics (GCC/Clang produce a standalone `.gch` with `-include`; MSVC's `/Yc` creates the PCH as a side effect of the first consuming TU, with `/Yu`/`/Fp`), PCH's own dependency tracking plus watch-mode triggers, and a new `[compile]` config surface.
  - **What another tool already does well** — CMake's `target_precompile_headers()` is a mature solution (CMake itself treats PCH as best-effort, per-configuration).
  - **Costs that don't serve small projects** — PCH only pays off on cold builds (first / full); small projects have too few TUs to amortize it. Incremental builds actually get worse: touching any PCH'd header rebuilds the PCH and **recompiles every TU**, whereas the existing cache model already skips parsing headers for cache-hit TUs — the cost PCH removes does not even occur on the incremental path. Needing PCH is a signal the project has outgrown the "small and straightforward" model (the same source as multi-target and the platform matrix).
- **Instead**:
  - Stay small and direct: flat `src/` + `include/` with `-j` parallelism and the incremental cache is already enough for small projects.
  - Once the project is large enough for PCH to pay off (dozens of TUs + heavy stable headers, minute-scale cold builds) → use CMake's `target_precompile_headers()`; `ezmk project export cmake` carries the configuration over in one command.

### Native unit test dashboards

- **What**: a built-in test-result dashboard — a live terminal UI (TUI, per-test status/progress) or a web/HTML report page (history, charts, flakiness analysis).
- **Why not**:
  - **Structural complexity** — `ezmk test` runs tests in a subprocess and parses console text (`run_tests()` in `src/build.cpp`: it parses Catch2's summary line and falls back to the exit code when the format changes). A dashboard needs a **structured event stream** (per-test start/end, assertion-level events) plus a whole **rendering layer** (terminal redraw loop / HTTP + embedded HTML/JS assets + history persistence) — two new subsystems, not an incremental change to existing code.
  - **What another tool already does well** — CI platforms (GitHub Actions / GitLab / Jenkins) render machine-readable test reports natively; the vendored Catch2 v3 already ships `-r junit/xml/json` reporters. Building our own dashboard duplicates what CI already does; and 1.2.0-dev.11 removed the XML-report-ingestion dead code ("removed dead code — parse_catch2_xml") — the "consume machine-readable reports ourselves" route was tried and dropped.
  - **Costs that don't serve small projects** — a dashboard pays off only with hundreds of test cases and cross-run comparison (history/flakiness); for a small project, the summary line plus exit code is the right tool. A UI knob on the "small and straightforward" default path violates the third criterion.
- **Instead**:
  - **Machine-readable report output** (`ezmk test --report junit`, 1.3.2): EazyMake only emits data (JUnit XML to a file) and lets existing dashboards (CI) render it — no UI of our own, aligned with "what another tool already does well".
  - Run the test binary directly with Catch2 reporter arguments (`-r junit::out=<file>`).

### Compile-time flame graphs

- **What**: visualizing per-source-file compile times as a flame graph / stacked timeline (interactive HTML/SVG, hover, zoom).
- **Why not**:
  - **Structural complexity** — EazyMake already has the **data**: per-file compile timing (1.2.0-dev.6: full sorted detail with `-v`, automatic top-N on slow builds; `src/build.cpp:991,1103-1120`). But a flame graph needs a **structured trace export + an interactive rendering layer** (HTML/JS/SVG assets plus hover/zoom interactions) — a whole new subsystem, not an increment to the existing stats.
  - **What another tool already does well** — flame graphs are a standard artifact of the profiling ecosystem: `perf` / `ninja -t trace` (Chrome trace format) rendered by speedscope / `flamegraph.pl` out of the box; building our own duplicates what the ecosystem already does.
  - **Costs that don't serve small projects** — the automatic top-N text on slow builds already covers small projects; a flame graph is a need that appears once builds grow to minute scale.
- **Instead**:
  - Use the existing per-file top-N text output (`-v` for the full list, automatic on slow builds).
  - For deeper profiling → `perf record` + `flamegraph.pl` / speedscope, or `ninja -t trace` + `chrome://tracing`.

### Final binary size breakdown charts

- **What**: visualizing the final artifact's (executable / library) size composition — pie/bar charts per symbol / object file / section (interactive).
- **Why not**:
  - **Structural complexity** — it requires parsing the linked artifact's symbols / section sizes (`nm` / `size` / `objdump` output) → a new binary-parsing submodule plus a rendering layer; the existing link pipeline (`src/build.cpp` link stage) produces no symbol-level data.
  - **What another tool already does well** — `size` / `nm -S` (text); Google `bloaty` (symbol-level size profiler emitting JSON / HTML reports directly); `-Wl,-Map` (link map file) — all ready-made; building our own reinvents them.
  - **Costs that don't serve small projects** — small-project binary sizes are readable as-is; per-symbol/section size optimization is an embedded/large-project need.
- **Instead**:
  - `size <binary>` / `nm -S <binary>` for a quick text lookup.
  - For detailed analysis → `bloaty -d symbols <binary>` (direct JSON/HTML report).
  - Pass `-Wl,-Map=<file>` at link time for the full map.

## The migration path

"Use CMake for complex builds" is not a dead end:

- `ezmk project export cmake` (1.2.0) generates a `CMakeLists.txt` from your `ezmk.toml` in one command — when you outgrow EazyMake, you leave with your configuration, not from scratch.
- `ezmk project import --from cmake` (1.2.0-dev.4) is the reverse: bring a standard CMake project into EazyMake.

## Related

- Design philosophy — [README](../../README.md)
- What counts as "complex" (user-facing) — [complex-builds.md](complex-builds.md)
- Configuration reference — [config_file.md](config_file.md)
- `utils` multi-tool packages — [utils.md](utils.md)
- Lua hooks — [config_file.md](config_file.md)
