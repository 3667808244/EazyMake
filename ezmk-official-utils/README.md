# ezmk-official-utils

Official utility tools for EazyMake — distributed as a `type = "utils"` package.

## Tools

| Tool | Command | Description |
|------|---------|-------------|
| **link** | `ezmk utils link` | Manage `.ezmk/links.json` for cross-directory source sharing |
| **cc** | `ezmk utils cc` | Generate `compile_commands.json` for clangd/LSP integration — **deprecated since 1.2.0**, use `ezmk project cc` |
| **gen-build-package** | `ezmk utils gen-build-package` | Package project source + build script into `.tar.gz` |

## Installation

```bash
# Global scope (recommended — available to all projects)
ezmk pkg install -g ezmk-official-utils -y

# User scope
ezmk pkg install -u ezmk-official-utils -y

# Project scope
ezmk pkg install ezmk-official-utils -y
```

This package is pre-installed by the EazyMake installer (`install.sh` / `install.ps1`).

## Link Tool (`ezmk utils link`)

Manage symbolic directory references in `.ezmk/links.json`:

```bash
ezmk utils link add shared ../common-lib/src    # Add a link
ezmk utils link list                             # List all links
ezmk utils link show shared                      # Show link details
ezmk utils link remove shared                    # Remove a link
```

Links can be referenced in `ezmk.toml` using the `@link:` syntax:

```toml
[compile]
src_dirs = ["src", "@link:shared/src"]
include_dirs = ["include", "@link:shared/include"]
```

## CC Tool (`ezmk utils cc`)

> **Deprecated since 1.2.0** — use `ezmk project cc` instead. This tool stays
> functional until 2.0.0; running `ezmk utils cc` prints a deprecation notice
> and redirects to the built-in `ezmk project cc` implementation.

Generates a clangd-compatible JSON Compilation Database:

```bash
ezmk utils cc                     # Output: compile_commands.json
ezmk utils cc -o custom.json      # Custom output path
```

## Gen-Build-Package Tool (`ezmk utils gen-build-package`)

Package project source files + build script into a self-contained `.tar.gz`:

```bash
ezmk utils gen-build-package                    # Default output
ezmk utils gen-build-package -o ../dist         # Custom output directory
ezmk utils gen-build-package -n myapp-v1.0      # Custom package name
```

## License

This package is part of the EazyMake project. See the main repository for license information.
