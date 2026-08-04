# 10. Top-level aliases (quick reference)

Since 1.1.0, the most-used `project` actions are also available as top-level
commands. Skip the `project` keyword entirely:

| Top-level alias | Full form |
|---|---|
| `ezmk build` | `ezmk project build` |
| `ezmk run` | `ezmk project run` |
| `ezmk clean` | `ezmk project clean` |
| `ezmk watch` | `ezmk project watch` |
| `ezmk install` | `ezmk project install` |
| `ezmk test` | `ezmk project test` |
| `ezmk pack` | `ezmk project pack` |

Both forms are **exactly equivalent** — every flag and argument works the same way:

```bash
$ ezmk build --profile release -j8          # alias form
$ ezmk project build --profile release -j8  # full form — identical
```

## When to use which

- **Daily work** — use the short top-level forms: `ezmk build`, `ezmk run`,
  `ezmk test`, `ezmk watch`.
- **Scripts & muscle memory** — the full `ezmk project <action>` forms are kept
  stable and are unambiguous when read by others.

## Other alias families

Two more layers exist for typing speed:

- **Two-letter shorthands** (`0.2.6+`): `ezmk pb` → `project build`, `ezmk pt` →
  `project test`, `ezmk ki` → `pkg install`, … These apply only at the command
  position — `ezmk project pb` is still an unknown subcommand.
- **Scope flags**: `-p` (project), `-u` (user), `-g` (global) for `pkg`/`repo`.

The full alias tables are in [`docs/en/cli.md`](../../docs/en/cli.md#command-shorthands-026).

That wraps up the tutorial. You now know how to install, scaffold, configure, build
incrementally, use profiles and parallelism, pull in packages, watch, hook, integrate
with clangd, run tests, and use every command form ezmk offers.
