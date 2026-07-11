# 8. Utils tools (clangd integration)

`ezmk utils <name>` runs Lua-based tools. Some are built in; others come from installed
`type = "utils"` packages.

## Built-in: `ezmk utils cc`

Generates a `compile_commands.json` so editors and language servers (clangd) understand
your build:

```bash
$ ezmk utils cc
generated compile_commands.json
$ ezmk utils cc -o build/compile_commands.json   # custom output path
```

Point clangd at it (usually automatic if the file is in the project root) and you get
accurate completion, go-to-definition, and diagnostics matching your `ezmk.toml` flags
and include dirs.

## Installing more tools

Utils tools ship as packages:

```bash
$ ezmk pkg install some-utils-pkg
$ ezmk utils <tool-name> [args...]
```

Everything after the tool name is passed through to the tool.

## Writing your own (quick taste)

A utils package is a project with `type = "utils"` and a `[utils]` table listing tools;
each tool is `utils/<name>.lua`. Scripts use the sandboxed `ezmk.*` API (23 functions:
project info, compile options, filesystem, process, logging, JSON, paths):

```lua
-- utils/hello.lua
ezmk.info("project: " .. ezmk.project_name())
for _, src in ipairs(ezmk.list_sources()) do
    ezmk.info("  " .. src)
end
```

```bash
$ ezmk project new mytools --type utils
$ ezmk utils hello
```

The sandbox removes `os`/`io`; run external commands via `ezmk.run()`, and
`ezmk.file_write()` refuses paths outside the project root. Packages can further
restrict access with `[utils.permissions]`.

See [`docs/utils.md`](../docs/utils.md) for the full plugin API and permission model.

---

That's the tour. You can now scaffold, configure, build incrementally, use profiles and
parallelism, pull in packages, watch, hook into the build, and integrate with clangd.
For exact semantics of any command or option, [`docs/cli.md`](../docs/cli.md) is the
authoritative reference.
