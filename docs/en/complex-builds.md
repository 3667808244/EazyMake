# Complex Builds (When to Use CMake)

> The README states the design philosophy: **ease of use over feature richness — for complex builds, use CMake.** ([README](../../README.md))

This document spells out what "complex" actually means, so you can judge whether your project belongs in EazyMake or CMake. It is the *user-facing* companion to [non-goals.md](non-goals.md), which covers the same boundary from the "features we won't design" angle.

## The one-line rule

EazyMake is built for projects that are **single-artifact, declarative, and small-to-medium**. Anything that pushes past one of those three walls is a "complex build."

## What EazyMake is for (not complex)

A project is a good EazyMake fit when all of these hold:

- **One artifact** — one `ezmk.toml` produces exactly one thing: an executable, a static lib, a shared lib, or a `utils` tool package (`[project].type`).
- **Declarative build** — sources, include dirs, macros, flags, and dependencies are *listed* in `ezmk.toml`, not *computed* by code.
- **Standard toolchain** — GCC, Clang, or MSVC on the host machine.
- **Package dependencies** — third-party libraries come from `[depends]` + `ezmk pkg install`.
- **A little customization** — `[hooks]` Lua scripts cover `pre_build` / `post_build` / `on_failure` side steps.

If your project is one library, one CLI tool, or one small service, EazyMake is designed for exactly that.

## What counts as "complex" — the four signatures

These are the four shapes that push a build out of EazyMake's scope. Hitting any one of them is the signal to use CMake instead.

### 1. Multiple build targets

A single project that must produce several artifacts — executables and/or libraries with an **inter-target dependency graph**.

- **Example**: one codebase builds `server`, `client`, and a shared `libcore`, where both `server` and `client` link `libcore`.
- **Why it's out of scope**: EazyMake's model is one artifact per `ezmk.toml`. Real multi-target support needs `--target` selection, per-target config, and a dependency graph — the structural complexity the tool exists to avoid.
- **What to do instead**: split into one project per artifact, or use CMake's `add_executable` / `add_library` graph. (`utils` packages *can* ship several tools via `[utils].tools`, and build variants of one artifact use `[compile.profile.*]` — those are not multi-target.)

### 2. Cross-compilation

Building for a target platform or architecture **different from the host** you run the build on.

- **Example**: compiling ARM firmware from an x86 workstation, or targeting Windows from a Linux CI box.
- **Why it's out of scope**: toolchain detection is host-only (`detect_toolchain()` probes the host's compiler). There is no `--target`, no target triple, no sysroot, and packages install only for the host platform.
- **What to do instead**: build natively on each target platform (a per-OS/per-arch CI matrix), or use CMake with a toolchain file for true cross-compilation.

> `[link].system_target` links **system libraries** (`-lpthread`, `-lm`). Despite the name, it has nothing to do with cross-compilation targets.

### 3. Programmable / custom build logic

A build that needs custom rules, code generation, or graph logic expressed in code rather than declared.

- **Example**: generating sources with `protoc` before compiling, embedding assets with a custom step, or defining build rules that depend on runtime state.
- **Why it's out of scope**: the build is declarative (`ezmk.toml`). `[hooks]` Lua scripts run side steps in a sandbox — they are not a build-graph language and don't replace the declarative model.
- **What to do instead**: declarative config + `[hooks]` for the common cases; for genuine programmability, use CMake.

### 4. Multi-project workspaces / inter-project references

One "workspace" where several projects reference and build each other directly.

- **Example**: a monorepo where `app/` includes headers straight from `lib/` and expects `lib/` to build first.
- **Why it's out of scope**: the config model has no workspace or sub-project concept. Cross-project reuse goes through packages (`[depends]` + `ezmk pkg install`), not project references.
- **What to do instead**: independent projects that share packages, or CMake's multi-target model for an interlocked monorepo.

## Deciding: a quick checklist

Start from your project and answer in order:

| Question | Answer | Direction |
|---|---|---|
| Do I produce exactly **one** artifact? | Yes | EazyMake ✓ |
| | No (several, or interlinked) | → CMake |
| Is the build **declarative** (list files/flags/deps)? | Yes | EazyMake ✓ |
| | No (needs rules/generation) | → CMake |
| Am I building **for the host** platform? | Yes | EazyMake ✓ |
| | No (cross-compiling) | → CMake |
| Are my projects **independent** (shared via packages)? | Yes | EazyMake ✓ |
| | No (direct references) | → CMake |

If you pass all four "Yes" rows, EazyMake is the right tool. If you hit a single "No", you're in complex-build territory — CMake is the smoother path.

## Warning signs you've outgrown EazyMake

These are the symptoms that usually appear *just before* a project tips over:

- You find yourself wishing for `--target` to build one artifact out of several.
- You want to write conditionals or loops in `ezmk.toml` to generate config.
- Your `[hooks]` scripts are increasingly re-implementing `add_custom_command`.
- You want one project to include sources or link artifacts from another project directly.
- You need a toolchain file or a sysroot.

None of these are bugs — they're the boundary showing through. When they pile up, it's time to grow into CMake.

## Growing into CMake is not a dead end

"Use CMake for complex builds" is not an abandonment — it's a supported path:

- `ezmk project export cmake` generates a `CMakeLists.txt` from your `ezmk.toml` in one command, so you leave with your configuration, not from scratch.
- `ezmk project import --from cmake` goes the other way: bring a standard CMake project into EazyMake.

See [migrate-from-cmake.md](migrate-from-cmake.md) for the supported/rejected constructs of the importer, and [non-goals.md](non-goals.md) for the deeper reasoning behind each boundary.

## Related

- Design philosophy — [README](../../README.md)
- Why each boundary exists — [non-goals.md](non-goals.md)
- Moving between EazyMake and CMake — [migrate-from-cmake.md](migrate-from-cmake.md)
- Configuration reference — [config_file.md](config_file.md)
- `utils` multi-tool packages — [utils.md](utils.md)
