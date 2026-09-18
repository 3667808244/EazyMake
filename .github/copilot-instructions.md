# EazyMake — Copilot Instructions

This is the EazyMake build tool project. See `CLAUDE.md` for project overview and `.claude/skills/` for domain-specific development guides.

## Build

Run `bash build.sh` to compile. Use `bash build.sh -v` for verbose output.

## Test

Run `bash build.sh test` to run the Catch2 test suite (~987 cases; skips tests tagged `[integration]`). Use `bash build.sh test-all` to include integration tests (1099 cases / 6342 assertions — 1094 passed / 5 skipped).

## Key directories

| Directory | Purpose |
|-----------|---------|
| `src/` | Core implementation |
| `include/ezmk/` | Public headers |
| `test/` | Unit tests (Catch2, `test_<module>.cpp`) |
| `docs/en/` + `docs/zh/` | User documentation (bilingual) |
| `plans/` | Version planning |
| `locale/` | Translation files (en.json + zh.json) |
| `scripts/` | Build helper scripts |
| `.claude/skills/` | AI agent skills (domain-specific guides) |
