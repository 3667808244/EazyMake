# Contributing to EazyMake

Thank you for your interest in contributing! This document outlines the workflow and
conventions for contributing to EazyMake.

## Code of Conduct

Be respectful, constructive, and inclusive. Assume good faith.

## Development setup

1. Clone the repository:
   ```bash
   git clone https://github.com/3667808244/EazyMake.git
   cd EazyMake
   ```

2. Build (requires MSYS2 on Windows, or GCC/Clang on Linux/macOS):
   ```bash
   bash build.sh
   ```

3. Run tests:
   ```bash
   bash build.sh test
   ```
   (If you build by hand instead, keep the source list identical to `TEST_SRC` in `build.sh` —
   an incomplete list fails at link time with undefined references.)

See [`README.md`](README.md) for more details.

## Pull request workflow

1. Fork the repository and create a feature branch from `main`.
2. Make your changes, including tests if applicable.
3. Ensure all existing tests pass.
4. Submit a PR against the `main` branch.

## Documentation conventions

EazyMake maintains bilingual documentation: **Chinese (zh)** and **English (en)**.

### Document locations

| Path | Language | Role |
|------|----------|------|
| `docs/en/` | English | Authoritative reference for English-speaking users |
| `docs/zh/` | Chinese | Authoritative language (source of truth) for maintainers |
| `tutorial/en/` | English | Hands-on getting-started guide |
| `tutorial/zh/` | Chinese | Hands-on getting-started guide |
| `plans/` | Chinese | Internal version plans (not translated) |

### Writing and updating documentation

1. **New or updated content → write in Chinese first** (`docs/zh/` or `tutorial/zh/`).
   Chinese is the source of truth because the core maintainers work in Chinese.

2. **Then translate to English** (`docs/en/` or `tutorial/en/`) — either in the same PR
   or a follow-up PR shortly after.

3. **Keep file lists identical** — `docs/en/` and `docs/zh/` must contain exactly the
   same set of `.md` files. Same for `tutorial/en/` and `tutorial/zh/`. Run the sync
   check script to verify:
   ```bash
   bash scripts/check_docs_sync.sh     # Linux/macOS/MSYS2
   # or
   powershell scripts/check_docs_sync.ps1   # Windows
   ```

4. **Terminology** — consult [`docs/en/glossary.md`](docs/en/glossary.md) (English) or
   [`docs/zh/glossary.md`](docs/zh/glossary.md) (Chinese) for standardized translations
   of technical terms.

5. **Code examples** — when command output is shown, use the appropriate locale:
   - English docs: assume `EZMK_LANG=en` (the default)
   - Chinese docs: specify `EZMK_LANG=zh` if showing localized output

### Man pages

`man/` holds the hand-written roff manual pages (`ezmk.1`, `ezmk-lua.1`,
`ezmk.toml.5`, `ezmk-workspace.toml.5`); they are a concise offline reference,
while `docs/` stays the full specification. Two rules:

- **Never let them drift**: after touching the CLI option specs (`src/cli.cpp`), the
  configuration parser (`src/config.cpp`), or the man pages themselves, run
  ```bash
  python scripts/check_man_sync.py
  ```
  It compares options, shorthands, commands, environment variables and configuration
  keys in both directions and fails on any difference (including a stale entry the CLI
  no longer has). A built-in extraction baseline also fails when a refactor makes the
  extractor stop matching, so the gate cannot silently pass.
- **Render before committing**: the script above cannot see layout, so also run
  ```bash
  groff -man -Tutf8 -z -ww man/*.1 man/*.5   # zero warnings
  MANPAGER=cat man -l man/ezmk.1             # eyeball the layout
  ```
  On MSYS2 install the tooling once with `pacman -S --needed groff man-db`. When
  editing, remember the roff traps documented in
  [`plans/1.4.x/1.4.3.md`](plans/1.4.x/1.4.3.md) §3.10: escape `\-`, write `\(ha` and
  `\(ti` for literal `^`/`~`, use `\e` for a backslash inside macro arguments, `\(dq`
  for a literal double quote, and keep the files LF-only.
- **The release commit dates the pages**: `.TH` carries the release date (e.g.
  `"2026-09-19"`); update it in the release commit together with the version bump.
  Nothing is stamped at build time, so rendering a page stays reproducible.

### Translation guidelines

- **Technical identifiers stay in English**: command names, flags, TOML keys, field names,
  file paths, Lua function names, and URLs are **never** translated.
- **Code blocks are never translated**: TOML examples, shell commands, JSON, Lua code
  remain exactly as-is.
- **Accuracy over style**: a technically correct translation with slightly awkward
  phrasing is better than a fluent translation that misrepresents a technical detail.
- **Term consistency**: use the same Chinese term for the same English concept throughout.
  See the glossary for the canonical mapping.

### What is NOT translated

| Content | Reason |
|---------|--------|
| `plans/*.md` | Internal version plans — maintainer communication in Chinese |
| Source code comments | Already primarily in English; occasional Chinese is fine |
| `locale/en.json` / `locale/zh.json` | Part of the i18n system, managed separately |
| `CHANGES.md` | Version changelog (Chinese) |

## Code style

- C++17 with GCC/Clang/MSVC compatibility.
- Match the surrounding code's style (indentation, naming, comment density).
- Use the existing `I18nKey` enum for any new user-visible strings — add keys to
  `include/ezmk/i18n_keys.def` and strings to both `locale/en.json` and `locale/zh.json`.

### .clang-format

A `.clang-format` configuration is provided in the project root (based on LLVM style,
4-space indent, 120-column limit, Attach (K&R) braces). To auto-format your changes:

```bash
# Check formatting without modifying files:
clang-format --dry-run -Werror <file>

# Apply formatting:
clang-format -i <file>
```

**IDE integration**:
- **VS Code**: Install the "C/C++" extension (Microsoft) — it picks up `.clang-format`
  automatically. Set `"editor.formatOnSave": true` for convenience.
- **CLion**: Enable "ClangFormat" in Settings → Editor → Code Style → C/C++.
- **vim/neovim**: Use `clang-format.py` from the clang tools, or a plugin like
  `rhysd/vim-clang-format`.

Formatting existing code is **not** required for contributions — only new or modified
code is expected to follow the style.

## Pre-commit checklist

Before submitting a pull request, please verify:

- [ ] Code compiles with `bash build.sh` (MSYS2 or Linux)
- [ ] All tests pass with `bash build.sh test`
- [ ] New user-visible strings are added to `include/ezmk/i18n_keys.def` and both `locale/en.json` and `locale/zh.json`
- [ ] Documentation is updated in both `docs/en/` and `docs/zh/` (if applicable)
- [ ] Consider running `clang-format --dry-run` on modified files to verify style consistency
- [ ] If the CLI, the configuration parser or `man/` changed: `python scripts/check_man_sync.py` passes and `groff -man -Tutf8 -z -ww man/*` reports no warnings
- [ ] New features include test coverage in `test/`

## Reporting issues

- Use the GitHub Issues tracker.
- Include the output of `ezmk version` and your OS/compiler information.
- For build issues, attach the full error output with `--verbose`.
