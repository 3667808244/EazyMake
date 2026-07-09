#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

#include "ezmk/config.hpp"

// Forward-declare Lua state type (lua.h typedefs this to an opaque struct)
struct lua_State;

namespace ezmk::lua {
namespace fs = std::filesystem;

// ---- 0.2.5+ Utils permission model ----
//
// Three-state result of a permission check. Priority: Deny > Allow > Ask.
// The check_* functions are pure (no I/O, no prompting) so they are trivially
// unit-testable; the lua_api layer turns Ask into Allow/Deny via resolve_ask().
enum class PermResult { Allow, Deny, Ask };

// Category of a controlled access, used for prompt text and the ask cache key.
enum class PermCategory { Read, Write, Run };

// Decide read access to `path` (already resolved to an absolute path).
// nullopt perms → Allow (backward compat with packages that declare none).
// The project's own ezmk.toml and the tool's package dir are implicitly
// allowed unless a read_deny entry matches them.
PermResult check_read_permission(const fs::path& path,
                                 const std::optional<config::UtilsPermissions>& perms,
                                 const fs::path& project_root,
                                 const fs::path& pkg_root);

// Decide write access to `path`. The "outside project root" hard limit is
// enforced by the caller before allow is ever considered (see lua_api.cpp).
PermResult check_write_permission(const fs::path& path,
                                  const std::optional<config::UtilsPermissions>& perms,
                                  const fs::path& project_root);

// Decide run access for a command line. Only the first token (executable) is
// matched. Supports exact match, trailing "*" prefix wildcard, and full path.
PermResult check_run_permission(std::string_view cmd,
                                const std::optional<config::UtilsPermissions>& perms);

// Turn an Ask result into a concrete allow/deny decision: prompt the user
// interactively and cache the decision for the session (keyed by category +
// target). Non-interactive environments (no TTY, or assume_yes) fail safe and
// return false (deny), recording the denied target.
bool resolve_ask(PermCategory cat, std::string_view target);

// Test/CLI hook: set non-interactive mode (mirrors -y / no TTY). When true,
// resolve_ask() denies without prompting.
void set_noninteractive(bool value);

// Test hook: clear the session ask-decision cache.
void clear_ask_cache();

// ---- Lifecycle ----

// Initialize the global Lua state. Must be called once at startup.
// Internally calls luaL_newstate() + opens safe subset of standard libs.
void init();

// Destroy the global Lua state. Must be called once before exit.
void shutdown();

// Return the global Lua state (valid after init(), nullptr before).
lua_State* state();

// ---- API registration ----

// Register all ezmk.* functions into the given Lua state.
// Must be called after init(), with the current project root.
// Safe to call multiple times (re-registers with the new project_root).
void register_api(lua_State* L, const fs::path& project_root);

// ---- Script execution ----

// Run a utils Lua script.
// script_path: absolute path to the .lua file
// args:        CLI arguments to pass to the run() function
// Returns the exit code from run(), or 1 on Lua error.
int run_script(lua_State* L, const fs::path& script_path,
               const std::vector<std::string>& args);

// 0.2.3+: Run a build hook script.
// script_path: absolute path to the .lua file
// output: path to the built artifact (exe/.a/.dll/.so)
// project_root: project root directory
// profile: profile name (empty string if none)
// Returns 0 on success, non-zero on error or if run() returns non-zero.
int run_hook_script(lua_State* L, const fs::path& script_path,
                    const fs::path& output,
                    const fs::path& project_root,
                    const std::string& profile);

} // namespace ezmk::lua
