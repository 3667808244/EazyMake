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
