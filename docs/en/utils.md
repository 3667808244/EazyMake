# Utils Tool System [version >= 0.2.0]

`ezmk utils <name> [args...]` provides an extensible plugin-style tool entry point. Tools are written as **Lua scripts**, distributed and installed as **packages**, with no specific tools hardcoded into the C++ codebase.

---

## Design motivation

| Comparison dimension | Shell (.sh/.ps1/.bat) | Embedded Lua |
|---|---|---|
| Cross-platform | Requires writing different scripts per platform | One `.lua` runs everywhere |
| Dependencies | Depends on bash / powershell / cmd | No external dependencies (Lua compiled into ezmk) |
| Feature boundaries | Can only invoke external commands | Can call C++ APIs exposed by ezmk + external commands |
| Error handling | Platform-specific syntax, error-prone | Unified `pcall` / `error()` |
| Distribution | Text scripts, but must watch line endings | Plain text, naturally cross-platform |

---

## Package structure

Utils tools follow the same package structure as regular packages (see `pkg.md`), with an additional `utils/` directory:

```
<utils_pkg>/
    ezmk.toml         # [project] section declares name, version, type = "utils"
    utils/            # Lua scripts (required)
        <name>.lua    # Entry script (filename = tool name)
        lib/          # Optional: Lua helper modules
    include/          # Optional: C/C++ headers
    src/              # Optional: C/C++ source (compiled to .a, callable from Lua via FFI)
```

### `ezmk.toml` format

```toml
[project]
name = "ezmk-cc"
version = "0.1.0"
type = "utils"              # New type: marks this package as a utils package

[utils]
tools = ["cc", "compile-commands"]   # List of tool names provided by this package
```

| Field | Type | Required | Description |
|---|---|---|---|
| `[project].type` | string | Yes | Value `"utils"`, marks as a utils package |
| `[utils].tools` | string[] | Yes | List of provided tool names, each corresponding to `utils/<name>.lua` |

- If the package contains both `src/` and `utils/`: compile `src/` → `build/*.a` first during installation, then register tools
- If the package contains only `utils/` (no `src/`): skip compilation during installation, only extract to the target path

---

## Lua entry script conventions

Each tool entry script (e.g. `utils/cc.lua`) must implement:

```lua
-- Optional: return help text. ezmk calls this when the user passes -h / --help
function help()
    return [[
usage: ezmk utils cc [options]

Generate compile_commands.json for the current project.

Options:
  -o, --output  <path>   Output path (default: <project_root>/compile_commands.json)
  -h, --help             Show this help
]]
end

-- Required: entry function. args is a string array (everything after ezmk utils <name>)
-- Returns an integer exit code (0 = success), or calls error("msg") for ezmk to handle
function run(args)
    -- ...
    return 0
end
```

### Invocation flow

1. ezmk locates `utils/<name>.lua` among installed packages
2. Loads and executes the script, registering `help()` and `run()`
3. If args contain `-h` / `--help` → calls `help()` and prints the return value
4. Otherwise calls `run(args)`, forwarding the remaining arguments
5. `run()` returns an integer → used as the process exit code; `error()` → ezmk prints the error and returns 1

---

## Search order

`ezmk utils <name>` searches for `<name>.lua` in the following priority order:

| Priority | Path | Description |
|---|---|---|
| 1 (project-level) | `<project>/.ezmk/pkg/*/utils/<name>.lua` | Scans all installed packages in project scope |
| 2 (user-level) | `~/.local/ezmk/pkg/*/utils/<name>.lua` | Scans all installed packages in user scope |
| 3 (global-level) | `<ezmk_install_dir>/pkg/*/utils/<name>.lua` | Scans all installed packages in global scope |

- Within each scope, packages are scanned in alphabetical order by package name; stops at the first match within the same scope
- The lookup is filesystem-based: directly checks whether `utils/<name>.lua` exists (does not rely on `[utils].tools` in `ezmk.toml`; the toml declaration is only used for `pkg info` display)

---

## ezmk Lua API

ezmk exposes a global `ezmk` module to Lua scripts:

### Project info (read-only)

```lua
ezmk.project_root()       -- → string   Absolute path of the project root
ezmk.project_name()       -- → string   Project name (ezmk.toml [project].name)
ezmk.project_type()       -- → string   "executable" | "static" | "shared" | "utils"
ezmk.project_config()     -- → string   Absolute path of ezmk.toml
ezmk.build_dir()          -- → string   Absolute path of the build/ directory
```

### Compile options (read-only)

```lua
ezmk.compile_flags()      -- → {string...}  [compile].flags array
ezmk.include_dirs()       -- → {string...}  [compile].include_dirs absolute path array
ezmk.link_flags()         -- → {string...}  [link].flags array
ezmk.link_dirs()          -- → {string...}  [link].link_dirs absolute path array
```

### Filesystem

```lua
ezmk.list_sources()       -- → {string...}  Absolute paths of all source files under src/
ezmk.file_exists(path)    -- → bool
ezmk.file_read(path)      -- → string | nil, err
ezmk.file_write(path, content)  -- → bool, err
```

- Relative paths are resolved against the project root
- `file_write` denies writes to absolute paths outside the project root

### Process execution

```lua
ezmk.run(cmd)             -- → {exit_code=int, stdout=string, stderr=string}
ezmk.run_capture(cmd)     -- → string   stdout content, calls error() on failure
```

- `run_capture` throws a Lua error when exit code ≠ 0, including stderr information

### Logging

```lua
ezmk.info(msg)            -- Green [ezmk] prefix
ezmk.warn(msg)            -- Yellow [ezmk warn] prefix
ezmk.error(msg)           -- Red [ezmk error] prefix
```

### Path utilities

```lua
ezmk.pkg_dir()            -- → string   Root directory of the package containing the current tool
ezmk.temp_dir()           -- → string   .ezmk/temp/ directory
ezmk.cache_dir()          -- → string   .ezmk/cache/ directory
```

### JSON

```lua
ezmk.json_encode(table)   -- → string   Encode a Lua table as JSON
ezmk.json_decode(string)  -- → table    Decode a JSON string into a Lua table
```

---

## Installation and removal

Utils packages use the exact same commands as regular library packages (see `pkg.md`):

```bash
# Install from a local file
ezmk pkg install -u ./ezmk-cc-0.1.0.zip

# Install from a URL
ezmk pkg install -g https://example.com/tools/ezmk-cc-0.1.0.tar.gz

# Install from a registered repo (by name)
ezmk pkg install -p ezmk-cc

# Remove
ezmk pkg remove -u ezmk-cc
```

- After installation, all tools declared in the package's `[utils].tools` are immediately available
- After removal, the corresponding tools are automatically unavailable (no manual cleanup needed)

---

## Security

> For a global security model overview, see [`@safety.md`](@safety.md); this section provides detailed specifications for the Lua sandbox and permissions.

Lua scripts run in a restricted environment:

- `os.execute` and `io.popen` are removed — external commands must be executed via `ezmk.run()`
- `ezmk.file_write()` denies writes to absolute paths outside the project root
- `require` for loading C extensions is not exposed (only pure Lua modules are allowed)

---

## Permission management [version >= 0.2.5]

Beyond the Lua sandbox, the package's `ezmk.toml` can declare **fine-grained** allowlists and denylists for the three controlled access categories — `file_read`, `file_write`, and `run` — via `[utils.permissions]`:

```toml
[utils]
tools = ["cc", "compile-commands"]

[utils.permissions]
read       = ["src/", "include/", "ezmk.toml"]   # Allowed to read (relative to project root)
read_deny  = ["**/.ssh/", "*.key", ".env"]       # Denied to read (takes priority over allowlist)
write      = ["build/", ".ezmk/cache/"]          # Allowed to write
write_deny = []                                  # Denied to write
run        = ["g++", "clang++", "git*"]          # Allowed commands to execute
run_deny   = ["rm", "curl", "wget"]              # Denied commands to execute
network    = false                               # Declarative, not yet enforced
```

### Judgment order: deny > allow > ask

Each access category is judged by a fixed priority (consistent across all three categories):

1. Matches the **deny denylist** → **Denied** (highest priority, even if the same target also appears in the allowlist)
2. Otherwise matches the **allow allowlist** → **Allowed**
3. Matches neither → **Ask the user** (interactive confirmation)

- **Read**: `ezmk.toml` itself and the package directory returned by `ezmk.pkg_dir()` are allowed by default, but `read_deny` can still override this.
- **Write**: First passes through the hard restriction of "no writing outside the project root" (non-bypassable), then enters deny/allow/ask.
- **Execute**: Only matches against the first token of the command (the executable filename). Exact match (`g++` does not match `g++-13`), `*`-suffix prefix wildcard (`git*` matches `git`, `git.exe`), full path match.

### Ask and non-interactive fallback

When the access target "matches neither the allowlist nor the denylist", the user is prompted interactively with options `y`/`n`, or `a` (always allow for this session) / `d` (always deny for this session). Decisions are cached per session by "category + target".

**Non-interactive environment** (no TTY, or CI scenarios): unable to prompt → uniformly **denied** (fail-safe), and the denied target is printed so the user can add it to the allowlist and retry. In CI, all required access should be explicitly written into the allowlist.

### Backward compatibility

Old packages that omit the entire `[utils.permissions]` section retain unrestricted behavior, but a deprecation warning is printed once on the first call to a controlled API. Once the section is declared, all three permission categories enter the deny/allow/ask model; an omitted field = empty list = access of that category defaults to ask.

Return values of controlled APIs when denied:

- `ezmk.file_read()` → `nil, "permission denied: read access to <path>"`
- `ezmk.file_write()` → `false, "permission denied: write access to <path>"`
- `ezmk.run()` → `{exit_code=-1, stderr="permission denied: '<cmd>'"}`; `run_capture()` throws an error

---

## Integration with `pkg info`

When running `ezmk pkg info` on a package with `type = "utils"`, the following extra fields are displayed in addition to the regular fields:

```
Package: ezmk-cc
Version: 0.1.0
Type: utils
Tools: cc, compile-commands
...
```

---

## Implementation notes

- Lua 5.4 is statically linked into the ezmk binary; a `lua_State*` is initialized at process startup and reused globally
- Each `run()` invocation is isolated with a sandbox environment table to prevent global variable pollution between scripts
- API bindings are implemented via the Lua C API (`lua_pushcfunction` / `lua_setglobal`)
- JSON encoding/decoding reuses `nlohmann/json.hpp` (already present in the project as a vendor library)
