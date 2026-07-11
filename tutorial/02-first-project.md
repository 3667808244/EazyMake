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
$ ezmk project build
[ezmk] Building hello (executable, C++17)...
[ezmk]   0 cached, 1 compiled
[ezmk]   Linking hello...
[ezmk] Build successful: build/hello
```

`ezmk` compiles every source under `src/`, links them, and writes the executable to
`build/`. (On Windows/MSYS2 the binary is `hello.exe`.)

## Run

```bash
$ ezmk project run
[ezmk] Building hello (executable, C++17)...
[ezmk]   1 cached, 0 compiled
[ezmk]   Linking hello...
[ezmk] Build successful: build/hello
[ezmk] Running hello...
Hello world!
```

`run` builds first (if needed), then executes. Pass arguments to *your* program after `--`:

```bash
$ ezmk project run -- --name world
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

Next: [Understanding `ezmk.toml` →](03-config.md)
