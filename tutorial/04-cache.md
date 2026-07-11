# 4. Incremental builds & caching

`ezmk` only recompiles what changed. It hashes each source file's content (and every
header it includes) and compares against a recorded fingerprint. Unchanged files reuse
their cached `.o`.

## See it in action

Add a second source file so there's something to *not* recompile:

```bash
$ cat > src/greet.cpp <<'EOF'
#include <iostream>
void greet(){ std::cout << "hi\n"; }
EOF
$ ezmk project build --verbose
```

Build once. Then build again without changing anything:

```bash
$ ezmk project build --verbose
[ezmk] Building hello (executable, C++17)...
[ezmk]   [cached] src/main.cpp  (source hash matches, 1 headers unchanged)
[ezmk]   [cached] src/greet.cpp  (source hash matches, 1 headers unchanged)
[ezmk]   2 cached, 0 compiled
[ezmk] Build successful: build/hello
```

Both files are cached — nothing recompiles. Now touch just one file:

```bash
$ echo '// tweak' >> src/greet.cpp
$ ezmk project build --verbose
[ezmk]   [cached] src/main.cpp  (source hash matches, 1 headers unchanged)
[ezmk]   1 cached, 1 compiled
[ezmk] Build successful: build/hello
```

Only `greet.cpp` recompiles (`1 cached, 1 compiled`). Editing a **header** recompiles
every source that `#include`s it — transitively.

## Where the cache lives

```
.ezmk/cache/
  record.json     # per-file source + header hashes and compile flags
  obj/            # cached .o files
```

Cache writes are atomic (`.tmp` then rename), so a build interrupted midway never
corrupts the cache.

## Forcing a rebuild

```bash
$ ezmk project build --disable-cache   # recompile everything (cache still updated after)
$ ezmk project clean                   # delete .ezmk/cache and temp files
```

Changing compile flags (e.g. editing `[compile].flags`) invalidates the cache
automatically — the flags are part of the fingerprint.

For the full algorithm, see [`docs/@cache.md`](../docs/@cache.md).

Next: [Build profiles & parallelism →](05-profiles-parallel.md)
