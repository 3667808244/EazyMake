# 12. Semantic version constraints & deterministic builds

`ezmk pkg install <name>` installs the highest available version by default. For long-lived projects, team collaboration, and reproducible CI, you need two tools: **version constraints** (which versions you accept) and **`ezmk.lock`** (which exact versions got installed).

## Constraining dependency versions

Constraints live in the `[depends]` (hard) or `want` (optional) section of `ezmk.toml`:

```toml
[depends]
lib = [
    "fmt",
    "spdlog@1.14.1",     # exact version
    "catch2^3.6.0",      # compatible: >=3.6.0, <4.0.0
    "nlohmann_json~3.11" # approximate: >=3.11, <3.12
]
want = [
    "yaml-cpp>=0.8.0"
]
```

| Syntax | Meaning | Example |
|--------|---------|---------|
| `pkg@1.2.3` | Exact version | `fmt@10.2.1` |
| `pkg^1.2.3` | Compatible (same major) | `spdlog^1.14.0` → `>=1.14.0, <2.0.0` |
| `pkg~1.2.3` | Approximate (same minor) | `nlohmann_json~3.11.0` → `>=3.11.0, <3.12.0` |
| `pkg>=1.2.3` | Greater than or equal | `zlib>=1.2.0` |
| `pkg>1.2.3` | Strictly greater | `boost>1.80.0` |
| `pkg` | No constraint (latest) | `fmt` — highest available |

Then install:

```bash
$ ezmk pkg install <name>
```

> If no version satisfies the constraints, installation fails and lists all available versions.

## ezmk.lock: pinning what was actually installed

`ezmk pkg install <name>` writes `ezmk.lock` (TOML) into the project root, recording each installed package's **exact version**, `sha256`, platform, and dependency graph:

```toml
[metadata]
version = 1
generated_by = "ezmk 1.4.2"
toolchain = "gcc"
direct_deps = ["fmt", "spdlog@^1.14.0"]

[[packages]]
name = "spdlog"
version = "1.14.1"
sha256 = "..."
lib_sha256 = "..."
archive_sha256 = "..."
type = "static"
scope = "project"
platform = "windows_x86_64_gcc"
dependencies = []
```

- **Generated** automatically on every `ezmk pkg install <name>`.
- **`--locked`**: install only according to the existing `ezmk.lock`; anything inconsistent is an **error** — CI uses it to guarantee "never install what isn't locked".
- **`--no-lock`**: skip lockfile generation.
- **Don't hand-edit**: `ezmk.lock` is auto-generated; to change dependencies, edit `ezmk.toml` and reinstall.
- **`platform`** is the **detected toolchain triple** (`windows_x86_64_gcc` under MSYS2/g++, `windows_x86_64_msvc` under MSVC, `linux_x86_64_gcc`, `darwin_arm64_clang`, …) — it describes the machine the lockfile was generated on.
- **`lib_sha256` / `archive_sha256`** (1.4.2+): `lib_sha256` hashes the **installed artifact** (for header-only packages, a manifest hash of `include/`) and is what `verify` checks; `archive_sha256` hashes the **archive the install came from** and is what `--locked` re-verifies. `sha256` is kept as the legacy alias of `lib_sha256`.

```bash
$ ezmk pkg install --locked <name>
```

## deterministic: making the check a hard requirement

An **inconsistent** lockfile (hash or dependency changes) is only a **warning**, and a **missing** lockfile is silently ignored — unless `[compile] deterministic = true`, which turns both into a hard build-time check:

```toml
[compile]
deterministic = true
```

- Missing or failed lockfile validation → **build error** (not a warning)
- The lockfile's content hash becomes part of the compile cache signature — dependency changes invalidate the cache automatically

```bash
$ ezmk build
```

## Pitfalls

- **Commit `ezmk.lock`** (don't add it to `.gitignore`) — it's part of reproducible builds; your team and CI rely on it.
- **Constraints are validated at build time, not resolved at install time**: only `--locked` reads `ezmk.lock` and pins versions; a plain `ezmk pkg install <name>` (and the repo lookup behind it) takes the **highest** available version, and `pkg update` also moves to the latest. The `[depends]` version constraints are checked during `ezmk build` (`lib` mismatch is **fatal**, `want` mismatch only warns), and `ezmk build` never writes a lockfile.
- **Bare entries keep "latest" semantics**: `"fmt"` and `"fmt@10.2.1"` differ — the former may jump to a new version on `pkg update`.

> 💡 Want a complete runnable example? Run `ezmk example with-packages` to scaffold a
> project with a `fmt^10.0` constraint + lockfile (see
> [`examples/README.md`](../../../examples/README.md) for the list).
