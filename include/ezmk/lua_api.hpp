#pragma once

#include <string>
#include <vector>
#include <filesystem>

// Forward-declare Lua state type (lua.h typedefs this to an opaque struct)
struct lua_State;

namespace ezmk::lua {
namespace fs = std::filesystem;

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
