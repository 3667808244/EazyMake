# 12. Semantic version constraints & deterministic builds

`ezmk pkg install` installs the highest available version by default. For long-lived projects, team collaboration, and reproducible CI, you need two tools: **version constraints** (which versions you accept) and **`ezmk.lock`** (which exact versions got installed).

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
$ ezmk pkg install
```

> If no version satisfies the constraints, installation fails and lists all available versions.

## ezmk.lock: pinning what was actually installed

`ezmk pkg install` writes `ezmk.lock` (TOML) into the project root, recording each installed package's **exact version**, `sha256`, platform, and dependency graph:

```toml
[metadata]
version = 1
generated_by = "ezmk 1.1.0"
toolchain = "gcc"
direct_deps = ["fmt", "spdlog@^1.14.0"]

[[packages]]
name = "spdlog"
version = "1.14.1"
sha256 = "..."
type = "static"
scope = "user"
platform = "windows_x86_64_msvc"
dependencies = []
```

- **Generated** automatically on every `ezmk pkg install`.
- **`--locked`**: install only according to the existing `ezmk.lock`; anything inconsistent is an **error** — CI uses it to guarantee "never install what isn't locked".
- **`--no-lock`**: skip lockfile generation.
- **Don't hand-edit**: `ezmk.lock` is auto-generated; to change dependencies, edit `ezmk.toml` and reinstall.

```bash
$ ezmk pkg install --locked
```

## deterministic: making the check a hard requirement

By default a missing or inconsistent lockfile is only a **warning**. `[compile] deterministic = true` turns it into a hard build-time check:

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
- **Constraints are resolved at install time**: after the lockfile exists, daily `install`/`build` follow the locked versions; re-resolution happens on `pkg update`.
- **Bare entries keep "latest" semantics**: `"fmt"` and `"fmt@10.2.1"` differ — the former may jump to a new version on `pkg update`.
