# 5. Build profiles & parallelism

## Parallel compilation

By default `ezmk` compiles sources in parallel using all available cores:

```bash
$ ezmk project build          # -j 0 (auto) is the default
$ ezmk project build -j4      # exactly 4 jobs
$ ezmk project build -j1      # serial (useful for readable error output)
```

`-j 0` auto-detects via the hardware thread count.

## Build profiles

A profile is a named set of extra compile/link options you opt into with `--profile`.
Add them to `ezmk.toml`:

```toml
[compile.profile.debug]
flags = ["-g", "-O0"]

[compile.profile.debug.macros]
DEBUG = true

[compile.profile.release]
flags = ["-O3", "-DNDEBUG"]

[link.profile.release]
flags = ["-s"]           # strip symbols
```

Apply one:

```bash
$ ezmk project build --profile debug
$ ezmk project run   --profile release
```

Rules:

- Profiles **do not** apply automatically — you must pass `--profile`.
- Profile `flags` are **appended after** base flags, so they override on conflict
  (matching GCC/Clang "last flag wins").
- Profile `macros` **merge into** base macros; the profile wins on key conflicts.

Different profiles produce different compile fingerprints, so switching profiles
rebuilds as needed without clobbering the other profile's cache.

Next: [Using packages →](06-packages.md)
