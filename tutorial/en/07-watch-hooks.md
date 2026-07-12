# 7. Watch mode & hooks

## Watch mode

`ezmk project watch` rebuilds automatically whenever you edit a source, header, or
`ezmk.toml`:

```bash
$ ezmk project watch
=== Build successful: build/hello ===
watching for changes... (Ctrl-C to stop)
```

- Watches `src_dirs`, `include_dirs`, and `ezmk.toml`.
- Rapid edits are coalesced (300 ms debounce).
- Editing `ezmk.toml` clears the cache and does a full rebuild.
- A failing build **does not** stop the watch loop — fix and save to retry.

Useful flags (same build options as `build`):

```bash
$ ezmk project watch --profile debug -j4
$ ezmk project watch --no-build-on-start   # wait for the first change instead of building now
```

## Build hooks

Hooks are Lua scripts run at points in the build lifecycle. Declare them in `ezmk.toml`
(paths are relative to the project root):

```toml
[hooks]
pre_build  = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
```

| Hook | Runs |
|---|---|
| `pre_build` | before compilation |
| `post_build` | after a successful link |
| `on_failure` | when the build errors |

Each hook receives a `ctx` table:

```lua
-- scripts/post.lua
ezmk.info("built: " .. ctx.output)
ezmk.info("root:  " .. ctx.project_root)
if ctx.profile ~= "" then
    ezmk.info("profile: " .. ctx.profile)
end
```

- `ctx.output` — path to the built artifact
- `ctx.project_root` — project root directory
- `ctx.profile` — active profile name (empty if none)

Hooks run in the same sandbox as utils tools (no `os`/`io`; use the `ezmk.*` API). A
missing hook script warns and is skipped — it is not fatal. Hooks apply only to your
project, not to package compilation.

Next: [Utils tools →](08-utils.md)
