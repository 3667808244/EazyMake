# 2. Your first project

## Scaffold

```bash
$ ezmk project new hello
$ cd hello
```

This creates:

```
hello/
  ezmk.toml        # project configuration
  src/
    main.cpp       # a "Hello world!" entry point
  include/         # your headers go here
  .gitignore
  .git/            # initialized git repo
```

> Skip git or the `.gitignore` with `--disable-git-init` / `--disable-gitignore`.
> Choose a different kind of project with `--type static|shared|utils` (default `executable`).

> **Library templates (1.2.1+):** `--type static` / `--type shared` do **not** create
> `main.cpp` — instead they scaffold a library skeleton `include/<name>.hpp` (sample
> public API) + `src/<name>.cpp` (implementation), ready for `ezmk build` out of the
> box; `--type utils` generates no C++ code at all, only a `utils/` directory for Lua
> scripts.

The generated `src/main.cpp`:

```cpp
#include <iostream>

int main(int argc, char **argv){
    std::cout << "Hello world!" << std::endl;
    return 0;
}
```

## Build

```bash
$ ezmk build
[ezmk] Building hello (executable, C++17)...
[ezmk]   0 cached, 1 compiled
[ezmk]   Linking hello...
[ezmk] Build successful: build/hello
```

> `ezmk build` is equivalent to `ezmk project build` — use whichever you prefer.

`ezmk` compiles every source under `src/`, links them, and writes the executable to
`build/`. (On Windows/MSYS2 the binary is `hello.exe`.)

## Run

```bash
$ ezmk run
[ezmk] Building hello (executable, C++17)...
[ezmk]   1 cached, 0 compiled
[ezmk]   Linking hello...
[ezmk] Build successful: build/hello
[ezmk] Running hello...
Hello world!
```

`run` builds first (if needed), then executes. Pass arguments to *your* program after `--`:

```bash
$ ezmk run -- --name world
```

Anything after `--` goes straight to your binary, not to `ezmk`.

## Shorthands

Every command has a two-letter alias:

```bash
$ ezmk pn hello     # project new
$ ezmk pb           # project build
$ ezmk pr           # project run
$ ezmk pc           # project clean
```

Most `project` actions are also available as top-level commands (`ezmk build`,
`ezmk run`, `ezmk clean`, …) — same flags, shorter typing. See the
[CLI reference](../../docs/en/cli.md#top-level-aliases-110) for the full list.

Next: [Understanding `ezmk.toml` →](03-config.md)
