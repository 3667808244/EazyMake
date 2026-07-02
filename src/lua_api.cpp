#include "ezmk/lua_api.hpp"
#include "ezmk/config.hpp"
#include "ezmk/util.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/build.hpp"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "nlohmann_json.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

namespace ezmk::lua {

// ===================================================================
// Global state
// ===================================================================

static lua_State* g_L = nullptr;
static fs::path   g_project_root; // set by register_api
static fs::path   g_current_script_pkg_root; // set by run_script: root of the pkg owning the current script

void init() {
    if (g_L) return;

    g_L = luaL_newstate();
    if (!g_L) {
        util::fatal(ezmk::i18n::I18nKey::lua_init_failed);
    }

    // Open the safe subset of standard libraries.
    // linit.c has been modified to exclude io and os for security.
    luaL_openlibs(g_L);

    // Verify Lua is working
    if (luaL_dostring(g_L, "return 50407")) {
        std::string err = lua_tostring(g_L, -1);
        lua_pop(g_L, 1);
        util::fatal("lua init failed: " + err);
    }
    lua_pop(g_L, 1);

    // Register the ezmk API with current directory as default project root
    register_api(g_L, fs::current_path());
}

void shutdown() {
    if (g_L) {
        lua_close(g_L);
        g_L = nullptr;
    }
}

lua_State* state() {
    return g_L;
}

// ===================================================================
// Helper: type-check a Lua argument and produce a descriptive error
// ===================================================================

static void check_arg_count(lua_State* L, int expected) {
    int n = lua_gettop(L);
    if (n != expected) {
        luaL_error(L, "expected %d argument(s), got %d", expected, n);
    }
}

// ===================================================================
// Helper: read ezmk.toml config (cached after first read)
// ===================================================================

static config::EzConfig* g_cached_config = nullptr;
static bool               g_config_loaded  = false;

static config::EzConfig* get_config() {
    if (g_config_loaded) return g_cached_config;
    g_config_loaded = true;

    auto cfg_path = g_project_root / "ezmk.toml";
    if (!util::file_exists(cfg_path)) return nullptr;

    try {
        g_cached_config = new config::EzConfig(config::parse_config(cfg_path));
        return g_cached_config;
    } catch (...) {
        return nullptr;
    }
}

// ===================================================================
// Helper: push a std::vector<std::string> as a Lua array (1-indexed)
// ===================================================================

static void push_string_array(lua_State* L, const std::vector<std::string>& v) {
    lua_createtable(L, (int)v.size(), 0);
    for (size_t i = 0; i < v.size(); ++i) {
        lua_pushstring(L, v[i].c_str());
        lua_rawseti(L, -2, (int)i + 1);
    }
}

static void push_path_array(lua_State* L, const std::vector<fs::path>& v) {
    lua_createtable(L, (int)v.size(), 0);
    for (size_t i = 0; i < v.size(); ++i) {
        lua_pushstring(L, v[i].generic_string().c_str());
        lua_rawseti(L, -2, (int)i + 1);
    }
}

// ===================================================================
// 3.1 Project info (read-only)
// ===================================================================

static int ezmk_project_root(lua_State* L) {
    check_arg_count(L, 0);
    lua_pushstring(L, g_project_root.generic_string().c_str());
    return 1;
}

static int ezmk_project_name(lua_State* L) {
    check_arg_count(L, 0);
    auto* cfg = get_config();
    if (!cfg) lua_pushnil(L);
    else      lua_pushstring(L, cfg->project.name.c_str());
    return 1;
}

static int ezmk_project_type(lua_State* L) {
    check_arg_count(L, 0);
    auto* cfg = get_config();
    if (!cfg) lua_pushnil(L);
    else      lua_pushstring(L, cfg->project.type.c_str());
    return 1;
}

static int ezmk_project_config(lua_State* L) {
    check_arg_count(L, 0);
    lua_pushstring(L, (g_project_root / "ezmk.toml").generic_string().c_str());
    return 1;
}

static int ezmk_build_dir(lua_State* L) {
    check_arg_count(L, 0);
    lua_pushstring(L, (g_project_root / "build").generic_string().c_str());
    return 1;
}

// ===================================================================
// 3.2 Compile options (read-only)
// ===================================================================

static int ezmk_compile_flags(lua_State* L) {
    check_arg_count(L, 0);
    auto* cfg = get_config();
    if (!cfg) { lua_newtable(L); return 1; }
    push_string_array(L, cfg->compile.flags);
    return 1;
}

static int ezmk_include_dirs(lua_State* L) {
    check_arg_count(L, 0);
    auto* cfg = get_config();
    if (!cfg) { lua_newtable(L); return 1; }
    std::vector<std::string> abs_dirs;
    for (auto& d : cfg->compile.include_dirs) {
        fs::path p = d;
        if (p.is_absolute()) abs_dirs.push_back(p.generic_string());
        else                 abs_dirs.push_back((g_project_root / p).generic_string());
    }
    push_string_array(L, abs_dirs);
    return 1;
}

static int ezmk_link_flags(lua_State* L) {
    check_arg_count(L, 0);
    auto* cfg = get_config();
    if (!cfg) { lua_newtable(L); return 1; }
    push_string_array(L, cfg->link.flags);
    return 1;
}

static int ezmk_link_dirs(lua_State* L) {
    check_arg_count(L, 0);
    auto* cfg = get_config();
    if (!cfg) { lua_newtable(L); return 1; }
    std::vector<std::string> abs_dirs;
    for (auto& d : cfg->link.link_dirs) {
        fs::path p = d;
        if (p.is_absolute()) abs_dirs.push_back(p.generic_string());
        else                 abs_dirs.push_back((g_project_root / p).generic_string());
    }
    push_string_array(L, abs_dirs);
    return 1;
}

// ===================================================================
// 3.3 Filesystem
// ===================================================================

static int ezmk_list_sources(lua_State* L) {
    check_arg_count(L, 0);
    fs::path src_dir = g_project_root / "src";
    if (!util::file_exists(src_dir)) {
        lua_newtable(L);
        return 1;
    }
    auto files = util::list_files(src_dir, {".c", ".cpp", ".cxx", ".cc"});
    std::vector<fs::path> abs_files;
    abs_files.reserve(files.size());
    for (auto& f : files) abs_files.push_back(fs::absolute(f));
    push_path_array(L, abs_files);
    return 1;
}

// Resolve a possibly-relative path to an absolute one rooted at project_root
static fs::path resolve_path(const fs::path& p) {
    if (p.is_absolute()) return p;
    return g_project_root / p;
}

static int ezmk_file_exists(lua_State* L) {
    check_arg_count(L, 1);
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, util::file_exists(resolve_path(path)) ? 1 : 0);
    return 1;
}

static int ezmk_file_read(lua_State* L) {
    check_arg_count(L, 1);
    const char* path = luaL_checkstring(L, 1);
    auto resolved = resolve_path(path);
    if (!util::file_exists(resolved)) {
        lua_pushnil(L);
        lua_pushstring(L, "file not found");
        return 2;
    }
    std::string content = util::file_read(resolved);
    lua_pushstring(L, content.c_str());
    return 1;
}

static int ezmk_file_write(lua_State* L) {
    check_arg_count(L, 2);
    const char* path    = luaL_checkstring(L, 1);
    const char* content = luaL_checkstring(L, 2);

    fs::path resolved = resolve_path(path);

    // Security: reject writes outside the project root
    // We canonicalize both paths for reliable prefix check
    std::string abs_dest = fs::absolute(resolved).generic_string();
    std::string abs_root = fs::absolute(g_project_root).generic_string();

    // Also allow writes to the temp directory (needed for some tools)
    std::string abs_temp = fs::absolute(g_project_root / ".ezmk/temp").generic_string();
    std::string abs_cache = fs::absolute(g_project_root / ".ezmk/cache").generic_string();

    bool inside_project = (abs_dest.size() >= abs_root.size() &&
                           abs_dest.compare(0, abs_root.size(), abs_root) == 0);
    bool inside_temp    = (abs_dest.size() >= abs_temp.size() &&
                           abs_dest.compare(0, abs_temp.size(), abs_temp) == 0);
    bool inside_cache   = (abs_dest.size() >= abs_cache.size() &&
                           abs_dest.compare(0, abs_cache.size(), abs_cache) == 0);

    if (!inside_project && !inside_temp && !inside_cache) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "access denied: write outside project root");
        return 2;
    }

    // Ensure parent directory exists
    util::create_directories(resolved.parent_path());

    bool ok = util::file_write(resolved, content);
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) lua_pushstring(L, "write failed");
    else     lua_pushnil(L);
    return 2;
}

// ===================================================================
// 3.4 Process execution
// ===================================================================

static int ezmk_run(lua_State* L) {
    check_arg_count(L, 1);
    const char* cmd = luaL_checkstring(L, 1);

    auto res = util::run_command(cmd);

    lua_createtable(L, 0, 3);
    lua_pushinteger(L, res.exit_code);
    lua_setfield(L, -2, "exit_code");
    lua_pushstring(L, res.out.c_str());
    lua_setfield(L, -2, "stdout");
    lua_pushstring(L, res.err.c_str());
    lua_setfield(L, -2, "stderr");
    return 1;
}

static int ezmk_run_capture(lua_State* L) {
    check_arg_count(L, 1);
    const char* cmd = luaL_checkstring(L, 1);

    auto res = util::run_command(cmd);
    if (res.exit_code != 0) {
        luaL_error(L, "command failed (exit %d): %s", res.exit_code, res.err.c_str());
    }
    lua_pushstring(L, res.out.c_str());
    return 1;
}

// ===================================================================
// 3.5 Logging
// ===================================================================

static int ezmk_info(lua_State* L) {
    check_arg_count(L, 1);
    const char* msg = luaL_checkstring(L, 1);
    util::info(msg);
    return 0;
}

static int ezmk_warn(lua_State* L) {
    check_arg_count(L, 1);
    const char* msg = luaL_checkstring(L, 1);
    util::warn(msg);
    return 0;
}

static int ezmk_error(lua_State* L) {
    check_arg_count(L, 1);
    const char* msg = luaL_checkstring(L, 1);
    util::error(msg);
    return 0;
}

// ===================================================================
// 3.6 Path tools
// ===================================================================

static int ezmk_pkg_dir(lua_State* L) {
    check_arg_count(L, 0);
    // Return the root directory of the package that contains the currently
    // executing Lua script. Falls back to the project .ezmk/pkg root when
    // no script context is active (e.g. API used outside of utils script).
    if (!g_current_script_pkg_root.empty()) {
        lua_pushstring(L, g_current_script_pkg_root.generic_string().c_str());
    } else {
        lua_pushstring(L, (g_project_root / ".ezmk/pkg").generic_string().c_str());
    }
    return 1;
}

static int ezmk_temp_dir(lua_State* L) {
    check_arg_count(L, 0);
    lua_pushstring(L, (g_project_root / ".ezmk/temp").generic_string().c_str());
    return 1;
}

static int ezmk_cache_dir(lua_State* L) {
    check_arg_count(L, 0);
    lua_pushstring(L, (g_project_root / ".ezmk/cache").generic_string().c_str());
    return 1;
}

// ===================================================================
// 3.7 JSON (reuses nlohmann/json.hpp)
// ===================================================================

// Recursive helper: push a Lua value from a nlohmann::json object
static void json_to_lua(lua_State* L, const nlohmann::json& j) {
    switch (j.type()) {
    case nlohmann::json::value_t::null:
        lua_pushnil(L);
        break;
    case nlohmann::json::value_t::boolean:
        lua_pushboolean(L, j.get<bool>() ? 1 : 0);
        break;
    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
        lua_pushinteger(L, j.get<lua_Integer>());
        break;
    case nlohmann::json::value_t::number_float:
        lua_pushnumber(L, j.get<lua_Number>());
        break;
    case nlohmann::json::value_t::string:
        lua_pushstring(L, j.get<std::string>().c_str());
        break;
    case nlohmann::json::value_t::array: {
        lua_createtable(L, (int)j.size(), 0);
        int idx = 1;
        for (auto& el : j) {
            json_to_lua(L, el);
            lua_rawseti(L, -2, idx++);
        }
        break;
    }
    case nlohmann::json::value_t::object: {
        lua_createtable(L, 0, (int)j.size());
        for (auto& [k, v] : j.items()) {
            lua_pushstring(L, k.c_str());
            json_to_lua(L, v);
            lua_settable(L, -3);
        }
        break;
    }
    default:
        lua_pushnil(L);
        break;
    }
}

// Recursive helper: convert a Lua value at the given stack index to nlohmann::json
// NOTE: uses lua_absindex so the index stays valid across stack pushes/pops.
static nlohmann::json lua_to_json(lua_State* L, int idx) {
    idx = lua_absindex(L, idx);  // stabilize against stack changes
    int t = lua_type(L, idx);
    switch (t) {
    case LUA_TNIL:
        return nullptr;
    case LUA_TBOOLEAN:
        return (bool)lua_toboolean(L, idx);
    case LUA_TNUMBER:
        if (lua_isinteger(L, idx))
            return lua_tointeger(L, idx);
        else
            return lua_tonumber(L, idx);
    case LUA_TSTRING:
        return lua_tostring(L, idx);
    case LUA_TTABLE: {
        // Determine if this is an array or object.
        // Array = all keys are consecutive integers 1..N with no gaps.
        // Object = everything else (including empty table and mixed keys).
        bool is_array_candidate = true;
        lua_Integer max_int_key = 0;
        lua_Integer int_key_count = 0;
        bool has_any = false;

        lua_pushnil(L);
        while (lua_next(L, idx) != 0) {
            has_any = true;
            if (lua_type(L, -2) == LUA_TNUMBER && lua_isinteger(L, -2)) {
                lua_Integer k = lua_tointeger(L, -2);
                if (k >= 1) {
                    if (k > max_int_key) max_int_key = k;
                    ++int_key_count;
                } else {
                    is_array_candidate = false;
                }
            } else {
                is_array_candidate = false;
            }
            lua_pop(L, 1);
        }

        if (!has_any) {
            return nlohmann::json::object(); // empty → {}
        }

        if (is_array_candidate && int_key_count == max_int_key) {
            // Consecutive 1..N integer keys → JSON array
            nlohmann::json arr = nlohmann::json::array();
            for (lua_Integer i = 1; i <= max_int_key; ++i) {
                lua_rawgeti(L, idx, (int)i);
                arr.push_back(lua_to_json(L, -1));
                lua_pop(L, 1);
            }
            return arr;
        } else {
            // Everything else → JSON object
            nlohmann::json obj = nlohmann::json::object();
            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                std::string key;
                if (lua_type(L, -2) == LUA_TSTRING) {
                    key = lua_tostring(L, -2);
                } else if (lua_type(L, -2) == LUA_TNUMBER && lua_isinteger(L, -2)) {
                    key = std::to_string(lua_tointeger(L, -2));
                } else {
                    lua_pop(L, 1);
                    continue;
                }
                obj[key] = lua_to_json(L, -1);
                lua_pop(L, 1);
            }
            return obj;
        }
    }
    default:
        return nullptr;
    }
}

static int ezmk_json_encode(lua_State* L) {
    check_arg_count(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);

    try {
        nlohmann::json j = lua_to_json(L, 1);
        std::string s = j.dump();
        lua_pushstring(L, s.c_str());
        return 1;
    } catch (const std::exception& e) {
        luaL_error(L, "json encode error: %s", e.what());
        return 0;
    }
}

static int ezmk_json_decode(lua_State* L) {
    check_arg_count(L, 1);
    const char* s = luaL_checkstring(L, 1);

    try {
        nlohmann::json j = nlohmann::json::parse(s);
        json_to_lua(L, j);
        return 1;
    } catch (const std::exception& e) {
        luaL_error(L, "json decode error: %s", e.what());
        return 0;
    }
}

// ===================================================================
// API registration table
// ===================================================================

static const luaL_Reg ezmk_api[] = {
    // 3.1 Project info
    {"project_root",   ezmk_project_root},
    {"project_name",   ezmk_project_name},
    {"project_type",   ezmk_project_type},
    {"project_config", ezmk_project_config},
    {"build_dir",      ezmk_build_dir},

    // 3.2 Compile options
    {"compile_flags",  ezmk_compile_flags},
    {"include_dirs",   ezmk_include_dirs},
    {"link_flags",     ezmk_link_flags},
    {"link_dirs",      ezmk_link_dirs},

    // 3.3 Filesystem
    {"list_sources",   ezmk_list_sources},
    {"file_exists",    ezmk_file_exists},
    {"file_read",      ezmk_file_read},
    {"file_write",     ezmk_file_write},

    // 3.4 Process execution
    {"run",            ezmk_run},
    {"run_capture",    ezmk_run_capture},

    // 3.5 Logging
    {"info",           ezmk_info},
    {"warn",           ezmk_warn},
    {"error",          ezmk_error},

    // 3.6 Path tools
    {"pkg_dir",        ezmk_pkg_dir},
    {"temp_dir",       ezmk_temp_dir},
    {"cache_dir",      ezmk_cache_dir},

    // 3.7 JSON
    {"json_encode",    ezmk_json_encode},
    {"json_decode",    ezmk_json_decode},

    {nullptr, nullptr}
};

// ===================================================================
// register_api — called once after luaL_newstate()
// ===================================================================

void register_api(lua_State* L, const fs::path& project_root) {
    g_project_root = fs::absolute(project_root);

    // Create the global "ezmk" table
    lua_newtable(L);
    luaL_setfuncs(L, ezmk_api, 0);
    lua_setglobal(L, "ezmk");

    // Invalidate cached config so next access re-reads
    delete g_cached_config;
    g_cached_config = nullptr;
    g_config_loaded = false;
}

// ===================================================================
// Script execution (with sandbox)
// ===================================================================

int run_script(lua_State* L, const fs::path& script_path,
               const std::vector<std::string>& args) {
    if (!L) return 1;

    // Derive package root from script path.
    // Script path: <pkg_root>/utils/<name>.lua  →  pkg root = parent of utils/
    g_current_script_pkg_root.clear();
    {
        auto parent = script_path.parent_path();
        if (parent.filename() == "utils") {
            g_current_script_pkg_root = parent.parent_path();
        }
    }

    // Ensure ezmk API is registered (in case register_api wasn't called explicitly)
    lua_getglobal(L, "ezmk");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        register_api(L, g_project_root.empty() ? fs::current_path() : g_project_root);
    } else {
        lua_pop(L, 1);
    }

    // ---- Build sandbox environment ----
    // Create a fresh sandbox table with _G as read-only fallback (via __index)

    // [1] sandbox env table
    lua_newtable(L);

    // [2] sandbox metatable
    lua_newtable(L);

    // metatable.__index = _G  (read-only access to global functions)
    lua_getglobal(L, "_G");
    lua_setfield(L, -2, "__index");

    // metatable.__newindex = function() error("read-only global") end
    // Actually, we want to allow writing to sandbox globals but not pollute _G.
    // So __newindex writes to sandbox itself (which is the default behavior).
    // No __newindex needed — writes go to sandbox, reads fall through to _G.

    lua_setmetatable(L, -2);
    // Stack: sandbox

    // Inject ezmk table into sandbox
    lua_getglobal(L, "ezmk");
    lua_setfield(L, -2, "ezmk");

    int sandbox_idx = lua_gettop(L);

    // ---- Load the script file ----
    if (luaL_loadfile(L, script_path.string().c_str())) {
        // Compile error
        std::string err = lua_tostring(L, -1);
        util::error(ezmk::i18n::I18nKey::lua_error, {{"msg", err}});
        lua_pop(L, 2); // pop error + sandbox
        return 1;
    }

    // Set the chunk's _ENV upvalue to the sandbox
    // Lua 5.4: the first upvalue of a loaded chunk is _ENV
    lua_pushvalue(L, sandbox_idx);
    if (lua_setupvalue(L, -2, 1) == nullptr) {
        // Lua 5.1 or no _ENV upvalue — set globals instead
        // This shouldn't happen with Lua 5.4, but handle gracefully
        lua_pop(L, 1); // pop the sandbox copy that failed
    }

    // ---- Execute the script chunk (define help/run functions) ----
    if (lua_pcall(L, 0, 0, 0)) {
        // Runtime error in script body
        std::string err = lua_tostring(L, -1);
        util::error(ezmk::i18n::I18nKey::lua_error, {{"msg", err}});
        lua_pop(L, 2); // pop error + sandbox
        return 1;
    }

    // ---- Check for --help / -h in args ----
    bool show_help = false;
    for (auto& a : args) {
        if (a == "-h" || a == "--help") {
            show_help = true;
            break;
        }
    }

    if (show_help) {
        // Get help function from sandbox
        lua_getfield(L, sandbox_idx, "help");
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 1, 0) == 0) {
                // help() returns a string
                if (lua_isstring(L, -1)) {
                    std::cout << lua_tostring(L, -1) << std::endl;
                }
                lua_pop(L, 1);
            } else {
                std::string err = lua_tostring(L, -1);
                util::error(ezmk::i18n::I18nKey::lua_error, {{"msg", err}});
                lua_pop(L, 2); // error + sandbox
                return 1;
            }
        } else {
            lua_pop(L, 1); // nil or non-function
        }
        lua_pop(L, 1); // sandbox
        return 0;
    }

    // ---- Call run(args) ----
    lua_getfield(L, sandbox_idx, "run");
    if (!lua_isfunction(L, -1)) {
        util::error(ezmk::i18n::I18nKey::lua_error,
                    {{"msg", "script does not define a run() function"}});
        lua_pop(L, 2); // nil + sandbox
        return 1;
    }

    // Build args table (Lua array, 1-indexed)
    lua_createtable(L, (int)args.size(), 0);
    for (size_t i = 0; i < args.size(); ++i) {
        lua_pushstring(L, args[i].c_str());
        lua_rawseti(L, -2, (int)i + 1);
    }

    // Call run(args_table)
    if (lua_pcall(L, 1, 1, 0)) {
        // Lua error in run()
        // After pcall failure: function + args are popped, error msg is pushed
        // Stack: [sandbox, error_msg]
        std::string err = lua_tostring(L, -1);
        util::error(ezmk::i18n::I18nKey::lua_error, {{"msg", err}});
        lua_pop(L, 2); // error_msg + sandbox
        return 1;
    }

    // Get return value (exit code)
    int exit_code = 0;
    if (lua_isinteger(L, -1)) {
        exit_code = (int)lua_tointeger(L, -1);
    } else if (lua_isnumber(L, -1)) {
        exit_code = (int)lua_tonumber(L, -1);
    }
    // non-number return → exit code 0

    lua_pop(L, 2); // return value + sandbox
    return exit_code;
}

} // namespace ezmk::lua
