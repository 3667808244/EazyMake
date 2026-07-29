# CLAUDE.md

EazyMake is a simple C/C++ build tool (CLI named `ezmk`), based on GCC/g++ (MSYS2 on Windows). Design philosophy: ease of use over feature richness. **See `README.md` for user-facing documentation.**

## Skills

This project provides the following skills for AI coding agents. Load the relevant skill for your task:

| Skill | File | When to load |
|-------|------|--------------|
| Build | `.claude/skills/ezmk-build.md` | Building, modifying `src/` or `build.sh` |
| Test | `.claude/skills/ezmk-test.md` | Running/writing tests |
| Codebase | `.claude/skills/ezmk-codebase.md` | Understanding architecture, config, CLI flags, subsystems |
| i18n | `.claude/skills/ezmk-i18n.md` | Modifying translations or `i18n_keys.def` |
| Planning | `.claude/skills/ezmk-planning.md` | Working on version plans (`plans/`, `plan.md`) |
| Repo | `.claude/skills/ezmk-repo.md` | Managing packages in the official EazyMake repository |

## Quick reference

- Build: `bash build.sh`
- Test: `bash build.sh test`
- Source code: `src/` (core) + `include/ezmk/` (public headers) + `test/` (Catch2 tests)
- Third-party code in `src/vendor/` and `include/vendor/` — do not modify.
