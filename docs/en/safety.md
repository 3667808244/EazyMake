# Security

This document centralizes EazyMake's security model. It is the **single authority**; related sections in other documents (`repo.md`, `pkg.md`,
`utils.md`, `cache.md`) keep only a one-liner and link back here, avoiding drift from maintenance in multiple places.

---

## Package Installation (`pkg.cpp`)

- **Global install secondary confirmation**: `pkg install -g` requires secondary confirmation before writing to `<ezmk_install_dir>/pkg/`.
- **Overwrite secondary confirmation**: If installation would overwrite existing files, secondary confirmation is required.
- **SHA-256 verification**: When `--sha256 <hash>` is provided on the command line or `sha256` is provided in the repository `index.toml`,
  the archive digest must be verified before installation; abort on mismatch.
- **`-y`**: Skips the above interactive confirmations (for non-interactive scenarios), but does not skip verification.

See [`pkg.md`](pkg.md) for details.

> **Why a second confirmation?** Global and overwrite installs write to shared,
> system-wide locations (`<ezmk_install_dir>/pkg/`) or replace existing files, so
> the extra prompt guards against accidental or unwanted changes. `-y` is for
> scripts, but it never disables the integrity checks.

> **Why SHA-256 verification?** Archives arrive over the network from repos or
> URLs, so verifying the digest catches corrupted or tampered downloads before
> anything is written to disk.

## Repository Management (`repo.cpp`)

- `git clone` failure → clear the incomplete local cache directory and report an error.
- `git pull` failure → warn and continue using the existing cache (does not block the build).
- Global repository **registration** does not require secondary confirmation (clone is not equivalent to install); however, **installing packages** from a global repository still triggers the global install confirmation above.
- Local repository (`type = "local"`) validation: `index.toml` must be parseable, `file` paths must exist, `sha256` format must be valid (0.2.5+).

See [`repo.md`](repo.md) for details.

> **Why warn-and-continue on `git pull` failure?** A transient network problem
> shouldn't block your build — a stale cache is a better outcome than no build.
> This also lets `--auto-update` refresh repos without failing the build.

## Build Cache (`cache.cpp`)

- Before writing `record.json` and `.o`, write to `.tmp` temporary files first; on success, `rename` to overwrite (atomic write).
- A build failure midway does not corrupt the consistency of existing cache.

See [`cache.md`](cache.md) for details.

> **Why `.tmp` + `rename`?** A crash or failure mid-write would otherwise leave a
> half-written `record.json`/`.o` that the cache would treat as valid, causing
> spurious rebuilds or corruption. The atomic write keeps the cache consistent.

## Lua Sandbox (`lua_api.cpp` / `linit.c`, 0.2.0+)

- The `os` and `io` libraries are removed from Lua at **compile time** (`linit.c`); external commands can only be executed via `ezmk.run()`.
- `ezmk.file_write()` refuses writes to absolute paths outside the project root directory (hard limit, cannot be bypassed).
- Does not expose `require` for loading C extensions (pure Lua modules only).
- Each invocation receives an independent sandbox environment table; global variables do not leak between scripts.
- **Install hooks (0.9.9+)**: Install lifecycle hooks (`preinstall`/`postinstall`) share the same sandbox infrastructure as build hooks and utils. Lua install hooks do NOT open the editor for review — the sandbox boundary (removed `os`/`io`, `file_write` limits, `ezmk.run()` permission checks) already bounds what the script can do. Only a user confirmation prompt (`[y/N]`) is required before execution.

> **Why remove `os`/`io` at compile time?** Stripping them from the Lua binary
> means scripts cannot reach them at all — the only path to external commands is
> `ezmk.run()`, which ezmk can audit and gate with permissions.

> **Why no editor review for Lua install hooks?** Shell hooks can run arbitrary
> commands, so reviewing the script is the only safeguard. A Lua hook is already
> confined by the sandbox (no `os`/`io`, `file_write` limits, `ezmk.run()`
> checks), so the `[y/N]` confirmation is sufficient.

## Utils Permission Management (`[utils.permissions]`, 0.2.5+)

Packages may declare allowlists/denylists in their `ezmk.toml` for three categories of controlled access: `file_read` / `file_write` / `run`.
The evaluation order is fixed as **deny > allow > ask**:

1. Match against deny denylist → deny (highest priority);
2. Otherwise, match against allow allowlist → allow;
3. Neither matches → ask the user.

`file_write` first passes through the sandbox "no out-of-bounds write" hard limit, then enters deny/allow/ask.
**Backward compatibility**: old packages that omit the entire section retain unrestricted behavior, but a deprecation warning is printed once on the first call to a controlled API.

See [`utils.md` Permission Management](utils.md#permission-management-version--025) for complete fields and semantics.

> **Why deny > allow > ask?** Deny must always win, even when a too-broad
> allowlist also matches — it is the last line of defense. Unlisted access falls
> through to an interactive ask, and fails closed (denied) in non-interactive
> runs rather than being silently allowed.

> **Why do legacy packages stay unrestricted?** So existing utils packages keep
> working without an immediate migration; the one-time warning nudges maintainers
> to declare permissions at their own pace.

---

## Summary Table

| Scenario | Safeguard |
|---|---|
| Global package install | Secondary confirmation |
| Overwrite existing files | Secondary confirmation |
| Archive `sha256` (CLI/repository) | Verify before installation |
| Global repository registration | No confirmation needed (clone ≠ install) |
| `git clone` failure | Clear cache, then error |
| `git pull` failure | Warn and continue using cache |
| Cache writes | `.tmp` + `rename` atomic write |
| Lua `os`/`io` | Removed at compile time |
| Lua `file_write` out-of-bounds | Deny (hard limit) |
| Utils controlled access | deny > allow > ask |
| Lua install hook execution | Sandbox + confirmation prompt (no editor review, 0.9.9+) |
| Shell install hook execution | Open editor for review + confirmation prompt (legacy) |
