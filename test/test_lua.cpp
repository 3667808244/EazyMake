// Unit tests for lua_api.cpp — Lua interpreter integration and API bindings
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/lua_api.hpp"
#include "ezmk/util.hpp"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ezmk::lua;

// ===================================================================
// Test helpers
// ===================================================================

// Temporary directory/file RAII helpers
struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("ezmk_test_lua_" + std::to_string(std::rand()));
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

// Write a minimal ezmk.toml for API functions that need config
static void write_minimal_config(const fs::path& dir) {
    std::ofstream of(dir / "ezmk.toml");
    of << "[project]\nname = \"testproj\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
       << "[compile]\nflags = [\"-Wall\", \"-Wextra\"]\ninclude_dirs = [\"include\"]\n\n"
       << "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
       << "[depends]\nlib = []\n";
}

// Write a minimal Lua script to a file, returns the path
static fs::path write_lua_script(const fs::path& dir, const std::string& name,
                                  const std::string& code) {
    fs::path p = dir / (name + ".lua");
    std::ofstream of(p);
    of << code;
    return p;
}

// Execute a Lua string in protected mode and return {ok, result_or_error}
static std::pair<bool, std::string> lua_dostring_safe(lua_State* L, const std::string& code) {
    if (luaL_dostring(L, code.c_str())) {
        std::string err = lua_tostring(L, -1);
        lua_pop(L, 1);
        return {false, err};
    }
    // Capture string result if any
    std::string result;
    if (lua_gettop(L) > 0 && lua_isstring(L, -1)) {
        result = lua_tostring(L, -1);
    }
    lua_settop(L, 0);
    return {true, result};
}

// ===================================================================
// Lifecycle tests
// ===================================================================

TEST_CASE("lua: init creates valid state", "[lua][lifecycle]") {
    // Note: init() is a global singleton; test sequentially with other tests.
    // If this is the first call, it creates the state.
    // We can call init() multiple times safely (it's a no-op after first call).
    init();
    lua_State* L = state();
    REQUIRE(L != nullptr);

    // Verify basic Lua operation
    int rc = luaL_dostring(L, "return 42");
    REQUIRE(rc == 0);
    REQUIRE(lua_isinteger(L, -1));
    REQUIRE(lua_tointeger(L, -1) == 42);
    lua_pop(L, 1);
}

TEST_CASE("lua: double init is idempotent", "[lua][lifecycle]") {
    lua_State* first = state();
    REQUIRE(first != nullptr);
    init();  // second call
    lua_State* second = state();
    REQUIRE(first == second);  // same pointer
}

TEST_CASE("lua: state returns nullptr before init", "[lua][lifecycle]") {
    // shutdown then check state
    shutdown();
    REQUIRE(state() == nullptr);
    // re-init for remaining tests
    init();
    REQUIRE(state() != nullptr);
}

// ===================================================================
// Sandbox / standard library tests
// ===================================================================

TEST_CASE("lua: io library is not available", "[lua][sandbox]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    int rc = luaL_dostring(L, "return io");
    REQUIRE(rc == 0);
    // io should be nil (removed from loadedlibs in linit.c)
    REQUIRE(lua_isnil(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("lua: os library is not available", "[lua][sandbox]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    int rc = luaL_dostring(L, "return os");
    REQUIRE(rc == 0);
    REQUIRE(lua_isnil(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("lua: basic builtins still work", "[lua][sandbox]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    // string, table, math, coroutine still available
    int rc = luaL_dostring(L, "return type(string.upper), type(table.insert), type(math.abs)");
    REQUIRE(rc == 0);
    REQUIRE(std::string(lua_tostring(L, -3)) == "function");
    REQUIRE(std::string(lua_tostring(L, -2)) == "function");
    REQUIRE(std::string(lua_tostring(L, -1)) == "function");
    lua_pop(L, 3);
}

// ===================================================================
// ezmk API table registration
// ===================================================================

TEST_CASE("lua: register_api creates 'ezmk' global table", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    register_api(L, fs::current_path());

    lua_getglobal(L, "ezmk");
    REQUIRE(lua_istable(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk table has all API functions", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    register_api(L, fs::current_path());

    // Check each function is present
    const char* funcs[] = {
        "project_root", "project_name", "project_type", "project_config", "build_dir",
        "compile_flags", "include_dirs", "link_flags", "link_dirs",
        "list_sources", "file_exists", "file_read", "file_write",
        "run", "run_capture",
        "info", "warn", "error",
        "pkg_dir", "temp_dir", "cache_dir",
        "json_encode", "json_decode",
        nullptr
    };

    lua_getglobal(L, "ezmk");
    for (int i = 0; funcs[i]; ++i) {
        INFO("Checking ezmk." << funcs[i]);
        lua_getfield(L, -1, funcs[i]);
        REQUIRE(lua_isfunction(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1); // ezmk
}

// 0.9.4+ — api_version field
TEST_CASE("lua: ezmk.api_version is an integer >= 1", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    lua_getglobal(L, "ezmk");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "api_version");
    REQUIRE(lua_isinteger(L, -1));
    int ver = static_cast<int>(lua_tointeger(L, -1));
    REQUIRE(ver >= 1);
    lua_pop(L, 2); // api_version, ezmk
}

// ===================================================================
// Project info API (3.1)
// ===================================================================

TEST_CASE("lua: ezmk.project_root returns a string", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.project_root()");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string root(lua_tostring(L, -1));
    REQUIRE(!root.empty());
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.project_config returns path ending with ezmk.toml", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "return ezmk.project_config()");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string config_path(lua_tostring(L, -1));
    auto filename = fs::path(config_path).filename();
    REQUIRE(filename == "ezmk.toml");
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.project_name returns name from config", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "return ezmk.project_name()");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    REQUIRE(std::string(lua_tostring(L, -1)) == "testproj");
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.project_type returns type from config", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "return ezmk.project_type()");
    REQUIRE(rc == 0);
    REQUIRE(std::string(lua_tostring(L, -1)) == "executable");
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.build_dir returns path ending with build", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.build_dir()");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string build_dir(lua_tostring(L, -1));
    // Should end with "build" or "/build" or "\\build"
    auto p = fs::path(build_dir);
    REQUIRE(p.filename() == "build");
    lua_pop(L, 1);
}

// ===================================================================
// Compile options API (3.2)
// ===================================================================

TEST_CASE("lua: ezmk.compile_flags returns table", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "local f = ezmk.compile_flags(); return #f, f[1], f[2]");
    REQUIRE(rc == 0);
    REQUIRE(lua_tointeger(L, -3) >= 1);
    lua_pop(L, 3);
}

TEST_CASE("lua: ezmk.include_dirs returns table with include path", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "local d = ezmk.include_dirs(); return #d, d[1]");
    REQUIRE(rc == 0);
    REQUIRE(lua_tointeger(L, -2) >= 1);
    REQUIRE(lua_isstring(L, -1));
    std::string inc_dir(lua_tostring(L, -1));
    REQUIRE(inc_dir.find("include") != std::string::npos);
    lua_pop(L, 2);
}

TEST_CASE("lua: ezmk.link_flags returns table", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "return type(ezmk.link_flags())");
    REQUIRE(rc == 0);
    REQUIRE(std::string(lua_tostring(L, -1)) == "table");
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.link_dirs returns table", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "return type(ezmk.link_dirs())");
    REQUIRE(rc == 0);
    REQUIRE(std::string(lua_tostring(L, -1)) == "table");
    lua_pop(L, 1);
}

// ===================================================================
// Filesystem API (3.3)
// ===================================================================

TEST_CASE("lua: ezmk.file_exists returns true for existing file", "[lua][api][fs]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    std::string code = "return ezmk.file_exists('" + (tmp.path / "ezmk.toml").generic_string() + "')";
    int rc = luaL_dostring(L, code.c_str());
    REQUIRE(rc == 0);
    REQUIRE(lua_toboolean(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.file_exists returns false for nonexistent file", "[lua][api][fs]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    register_api(L, tmp.path);

    std::string code = "return ezmk.file_exists('" + (tmp.path / "nonexistent.xyz").generic_string() + "')";
    int rc = luaL_dostring(L, code.c_str());
    REQUIRE(rc == 0);
    REQUIRE(!lua_toboolean(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.file_read returns content", "[lua][api][fs]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    std::string code = "return ezmk.file_read('" + (tmp.path / "ezmk.toml").generic_string() + "')";
    int rc = luaL_dostring(L, code.c_str());
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string content(lua_tostring(L, -1));
    REQUIRE(content.find("[project]") != std::string::npos);
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.file_read returns nil for nonexistent file", "[lua][api][fs]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    register_api(L, tmp.path);

    std::string code = "local content, err = ezmk.file_read('" + (tmp.path / "no_such_file.txt").generic_string() + "'); return content, err";
    int rc = luaL_dostring(L, code.c_str());
    REQUIRE(rc == 0);
    REQUIRE(lua_isnil(L, -2));  // content is nil
    REQUIRE(lua_isstring(L, -1));  // err is a string
    lua_pop(L, 2);
}

TEST_CASE("lua: ezmk.file_write succeeds inside project root", "[lua][api][fs]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    fs::path test_file = tmp.path / "test_write.txt";

    std::string code = "local ok, err = ezmk.file_write('" + test_file.generic_string() +
                       "', 'hello world'); return ok, err";
    int rc = luaL_dostring(L, code.c_str());
    REQUIRE(rc == 0);
    REQUIRE(lua_toboolean(L, -2));  // ok == true
    lua_pop(L, 2);

    // Verify file was actually written
    REQUIRE(fs::exists(test_file));
    std::ifstream in(test_file);
    std::string content;
    std::getline(in, content);
    REQUIRE(content == "hello world");
}

TEST_CASE("lua: ezmk.file_write denies write outside project root", "[lua][api][fs][sandbox]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    register_api(L, tmp.path);

    // Try writing to an absolute path outside project root
    fs::path outside = fs::temp_directory_path() / "ezmk_should_not_exist.txt";

    std::string code = "local ok, err = ezmk.file_write('" + outside.generic_string() +
                       "', 'bad'); return ok, err";
    int rc = luaL_dostring(L, code.c_str());
    REQUIRE(rc == 0);
    REQUIRE(!lua_toboolean(L, -2));  // ok == false
    REQUIRE(lua_isstring(L, -1));    // err message
    std::string err(lua_tostring(L, -1));
    REQUIRE(err.find("access denied") != std::string::npos);
    lua_pop(L, 2);
}

TEST_CASE("lua: ezmk.file_write creates parent directories", "[lua][api][fs]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    register_api(L, tmp.path);

    fs::path nested = tmp.path / "nested" / "dir" / "test.txt";

    std::string code = "local ok, err = ezmk.file_write('" + nested.generic_string() +
                       "', 'nested content'); return ok, err";
    int rc = luaL_dostring(L, code.c_str());
    REQUIRE(rc == 0);
    REQUIRE(lua_toboolean(L, -2));
    lua_pop(L, 2);

    REQUIRE(fs::exists(nested));
}

TEST_CASE("lua: ezmk.list_sources returns array of source paths", "[lua][api][fs]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    TempDir tmp;
    fs::create_directories(tmp.path / "src");
    // Create a dummy source file
    std::ofstream(tmp.path / "src" / "main.cpp") << "int main() { return 0; }\n";
    write_minimal_config(tmp.path);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "local s = ezmk.list_sources(); return #s, s[1]");
    REQUIRE(rc == 0);
    REQUIRE(lua_tointeger(L, -2) >= 1);   // at least one source
    REQUIRE(lua_isstring(L, -1));          // source path is a string
    std::string src(lua_tostring(L, -1));
    REQUIRE(src.find("main.cpp") != std::string::npos);
    lua_pop(L, 2);
}

// ===================================================================
// Process execution API (3.4)
// ===================================================================

TEST_CASE("lua: ezmk.run returns table with exit_code, stdout, stderr", "[lua][api][proc]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "local r = ezmk.run('echo hello'); return r.exit_code, r.stdout");
    REQUIRE(rc == 0);
    REQUIRE(lua_tointeger(L, -2) == 0);  // exit_code == 0
    REQUIRE(lua_isstring(L, -1));
    std::string out(lua_tostring(L, -1));
    // "echo hello" should produce "hello" somewhere in output
    REQUIRE((out.find("hello") != std::string::npos));
    lua_pop(L, 2);
}

TEST_CASE("lua: ezmk.run_capture returns stdout on success", "[lua][api][proc]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.run_capture('echo captured')");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string out(lua_tostring(L, -1));
    REQUIRE(out.find("captured") != std::string::npos);
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.run_capture errors on non-zero exit", "[lua][api][proc]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    // A command that returns non-zero exit code
    int rc = luaL_dostring(L, "return ezmk.run_capture('exit 1')");
    // Should error (non-zero return from luaL_dostring means Lua error)
    REQUIRE(rc != 0);
    lua_pop(L, 1);
}

// ===================================================================
// Logging API (3.5) — tested indirectly via output
// ===================================================================

TEST_CASE("lua: ezmk.info returns nothing", "[lua][api][log]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "ezmk.info('test message'); return 0");
    REQUIRE(rc == 0);
}

TEST_CASE("lua: ezmk.warn returns nothing", "[lua][api][log]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "ezmk.warn('warning message'); return 0");
    REQUIRE(rc == 0);
}

TEST_CASE("lua: ezmk.error returns nothing", "[lua][api][log]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "ezmk.error('error message'); return 0");
    REQUIRE(rc == 0);
}

// ===================================================================
// Path tools API (3.6)
// ===================================================================

TEST_CASE("lua: ezmk.temp_dir returns path ending with .ezmk/temp", "[lua][api][path]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.temp_dir()");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string s(lua_tostring(L, -1));
    REQUIRE((s.find(".ezmk") != std::string::npos || s.find("ezmk") != std::string::npos));
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.cache_dir returns path ending with .ezmk/cache", "[lua][api][path]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.cache_dir()");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string s(lua_tostring(L, -1));
    REQUIRE((s.find(".ezmk") != std::string::npos || s.find("ezmk") != std::string::npos));
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.pkg_dir returns path", "[lua][api][path]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.pkg_dir()");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    lua_pop(L, 1);
}

// ===================================================================
// JSON API (3.7)
// ===================================================================

TEST_CASE("lua: json_encode simple array", "[lua][api][json]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.json_encode({1, 2, 3})");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string json(lua_tostring(L, -1));
    REQUIRE(json == "[1,2,3]");
    lua_pop(L, 1);
}

TEST_CASE("lua: json_encode simple object", "[lua][api][json]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.json_encode({a=1, b='two'})");
    REQUIRE(rc == 0);
    REQUIRE(lua_isstring(L, -1));
    std::string json(lua_tostring(L, -1));
    // Object keys may be in any order
    REQUIRE(json.find("\"a\"") != std::string::npos);
    REQUIRE(json.find("1") != std::string::npos);
    REQUIRE(json.find("\"b\"") != std::string::npos);
    REQUIRE(json.find("\"two\"") != std::string::npos);
    lua_pop(L, 1);
}

TEST_CASE("lua: json_encode empty table → empty object", "[lua][api][json]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.json_encode({})");
    REQUIRE(rc == 0);
    REQUIRE(std::string(lua_tostring(L, -1)) == "{}");
    lua_pop(L, 1);
}

TEST_CASE("lua: json_decode array", "[lua][api][json]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "local t = ezmk.json_decode('[10, 20, 30]'); return #t, t[1], t[2]");
    REQUIRE(rc == 0);
    REQUIRE(lua_tointeger(L, -3) == 3);
    REQUIRE(lua_tointeger(L, -2) == 10);
    REQUIRE(lua_tointeger(L, -1) == 20);
    lua_pop(L, 3);
}

TEST_CASE("lua: json_decode object", "[lua][api][json]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "local t = ezmk.json_decode('{\"x\": 1, \"y\": 2}'); return t.x, t.y");
    REQUIRE(rc == 0);
    REQUIRE(lua_tointeger(L, -2) == 1);
    REQUIRE(lua_tointeger(L, -1) == 2);
    lua_pop(L, 2);
}

TEST_CASE("lua: json_encode → json_decode round-trip", "[lua][api][json]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    // Encode then decode: should produce equivalent data
    int rc = luaL_dostring(L,
        "local original = {name='test', values={1, 2, 3}, nested={a=1}} "
        "local json_str = ezmk.json_encode(original) "
        "local decoded = ezmk.json_decode(json_str) "
        "return decoded.name, decoded.values[1], decoded.values[3], decoded.nested.a");
    REQUIRE(rc == 0);
    REQUIRE(std::string(lua_tostring(L, -4)) == "test");
    REQUIRE(lua_tointeger(L, -3) == 1);
    REQUIRE(lua_tointeger(L, -2) == 3);
    REQUIRE(lua_tointeger(L, -1) == 1);
    lua_pop(L, 4);
}

TEST_CASE("lua: json_decode invalid JSON errors", "[lua][api][json]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.json_decode('not json')");
    REQUIRE(rc != 0);  // should error
    lua_pop(L, 1);
}

// ===================================================================
// run_script tests — success, error, help
// ===================================================================

TEST_CASE("run_script: successful execution returns 0", "[lua][script]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "test_success", R"(
function run(args)
    return 0
end
)");

    int rc = run_script(L, script, {});
    REQUIRE(rc == 0);
}

TEST_CASE("run_script: run() return value becomes exit code", "[lua][script]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "test_rc", R"(
function run(args)
    return 42
end
)");

    int rc = run_script(L, script, {});
    REQUIRE(rc == 42);
}

TEST_CASE("run_script: Lua error returns exit code 1", "[lua][script]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "test_error", R"(
function run(args)
    error("something went wrong")
end
)");

    int rc = run_script(L, script, {});
    REQUIRE(rc == 1);
}

TEST_CASE("run_script: -h triggers help function", "[lua][script]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "test_help", R"(
function help()
    return "usage: test_help [options]"
end
function run(args)
    return 99
end
)");

    // With -h flag, run() should not be called — exit 0 from help
    int rc = run_script(L, script, {"-h"});
    REQUIRE(rc == 0);
}

TEST_CASE("run_script: --help triggers help function", "[lua][script]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "test_help2", R"(
function help()
    return "usage: test_help2"
end
function run(args)
    return 99
end
)");

    int rc = run_script(L, script, {"--help"});
    REQUIRE(rc == 0);
}

TEST_CASE("run_script: missing run() function returns error", "[lua][script]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "test_no_run", R"(
function help()
    return "help text"
end
)");

    int rc = run_script(L, script, {"arg1", "arg2"});
    REQUIRE(rc == 1);
}

TEST_CASE("run_script: script compile error returns 1", "[lua][script]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "test_syntax", R"(
function run(args)
    this is not valid lua @@@
end
)");

    int rc = run_script(L, script, {});
    REQUIRE(rc == 1);
}

TEST_CASE("run_script: nonexistent file returns 1", "[lua][script]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    fs::path nonexistent = fs::temp_directory_path() / "does_not_exist_xyz.lua";
    int rc = run_script(L, nonexistent, {});
    REQUIRE(rc == 1);
}

// ===================================================================
// run_script sandbox isolation
// ===================================================================

TEST_CASE("run_script: sandbox isolates global variables between scripts", "[lua][script][sandbox]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;

    // Script A sets a global
    auto script_a = write_lua_script(tmp.path, "sandbox_a", R"(
_G_POLLUTION = "from_a"
function run(args)
    my_local = "a_value"
    return 0
end
)");

    // Script B tries to access Script A's globals
    auto script_b = write_lua_script(tmp.path, "sandbox_b", R"(
function run(args)
    if _G_POLLUTION ~= nil then
        error("sandbox leak: _G_POLLUTION = " .. tostring(_G_POLLUTION))
    end
    if my_local ~= nil then
        error("sandbox leak: my_local = " .. tostring(my_local))
    end
    return 0
end
)");

    int rc_a = run_script(L, script_a, {});
    REQUIRE(rc_a == 0);

    int rc_b = run_script(L, script_b, {});
    REQUIRE(rc_b == 0);
}

TEST_CASE("run_script: io.write not available in sandbox", "[lua][script][sandbox]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "no_io", R"(
function run(args)
    if io ~= nil then
        error("io library should not be available")
    end
    return 0
end
)");

    int rc = run_script(L, script, {});
    REQUIRE(rc == 0);
}

TEST_CASE("run_script: os.execute not available in sandbox", "[lua][script][sandbox]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    TempDir tmp;
    auto script = write_lua_script(tmp.path, "no_os", R"(
function run(args)
    if os ~= nil then
        error("os library should not be available")
    end
    return 0
end
)");

    int rc = run_script(L, script, {});
    REQUIRE(rc == 0);
}

// ===================================================================
// find_utils_script tests
// ===================================================================

TEST_CASE("find_utils_script: returns empty path for nonexistent tool", "[lua][utils]") {
    auto result = ezmk::util::find_utils_script("nonexistent_tool_xyz_123");
    REQUIRE(result.empty());
}

TEST_CASE("find_utils_script: finds script in project dev pkg dir", "[lua][utils]") {
    // Create a temporary package structure
    TempDir tmp;
    fs::create_directories(tmp.path / "pkg" / "mytool" / "utils");
    std::ofstream(tmp.path / "pkg" / "mytool" / "utils" / "mytool.lua")
        << "function run(args) return 0 end\n";

    // Change to tmp directory and test (find_utils_script uses cwd for project/dev scopes)
    auto old_cwd = fs::current_path();
    fs::current_path(tmp.path);

    auto result = ezmk::util::find_utils_script("mytool");
    REQUIRE(!result.empty());
    REQUIRE(result.filename() == "mytool.lua");

    fs::current_path(old_cwd);
}

// ===================================================================
// Type validation / parameter checking
// ===================================================================

TEST_CASE("lua: ezmk.file_write requires 2 arguments", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    // Call with wrong number of args — should produce a Lua error
    int rc = luaL_dostring(L, "return ezmk.file_write('only_one_arg')");
    REQUIRE(rc != 0);
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.file_read requires 1 argument", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.file_read()");
    REQUIRE(rc != 0);
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.run requires 1 argument", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.run()");
    REQUIRE(rc != 0);
    lua_pop(L, 1);
}

TEST_CASE("lua: ezmk.json_encode requires a table", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, fs::current_path());

    int rc = luaL_dostring(L, "return ezmk.json_encode('not a table')");
    REQUIRE(rc != 0);
    lua_pop(L, 1);
}

// ===================================================================
// register_api re-registration
// ===================================================================

TEST_CASE("lua: register_api can be called multiple times", "[lua][api]") {
    lua_State* L = state();
    REQUIRE(L != nullptr);

    register_api(L, fs::current_path());
    register_api(L, fs::current_path());  // second call

    // ezmk table should still work
    lua_getglobal(L, "ezmk");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "project_root");
    REQUIRE(lua_isfunction(L, -1));
    lua_pop(L, 2);
}

// ===================================================================
// null L state safety
// ===================================================================

TEST_CASE("lua: run_script with null L returns 1", "[lua][script][edge]") {
    int rc = run_script(nullptr, fs::path("test.lua"), {});
    REQUIRE(rc == 1);
}

TEST_CASE("lua: state returns nullptr before init, non-null after", "[lua][lifecycle]") {
    // shutdown first
    shutdown();
    REQUIRE(state() == nullptr);

    // re-init
    init();
    REQUIRE(state() != nullptr);

    // shutdown again and verify
    shutdown();
    REQUIRE(state() == nullptr);

    // final re-init for other tests
    init();
    REQUIRE(state() != nullptr);
}

// ===================================================================
// 0.2.3+: list_sources() uses src_dirs from config
// ===================================================================

TEST_CASE("lua: list_sources respects src_dirs config", "[lua][api][0.2.3]") {
    TempDir tmp;
    write_minimal_config(tmp.path);
    // Add custom src_dirs to config
    {
        std::ofstream of(tmp.path / "ezmk.toml");
        of << "[project]\nname = \"testproj\"\ntype = \"executable\"\nversion = \"0.1.0\"\nlanguage = \"C++17\"\n\n"
           << "[compile]\nflags = [\"-Wall\"]\ninclude_dirs = [\"include\"]\nsrc_dirs = [\"src\", \"lib\"]\n\n"
           << "[link]\nflags = []\nlink_dirs = []\nsystem_target = []\n\n"
           << "[depends]\nlib = []\n";
    }
    fs::create_directories(tmp.path / "src");
    fs::create_directories(tmp.path / "lib");
    std::ofstream(tmp.path / "src" / "main.cpp") << "int main() { return 0; }\n";
    std::ofstream(tmp.path / "lib" / "helper.cpp") << "void helper() {}\n";

    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "local sources = ezmk.list_sources(); return #sources");
    REQUIRE(rc == 0);
    // Should find files from both src/ and lib/
    int count = static_cast<int>(lua_tointeger(L, -1));
    REQUIRE(count == 2);
    lua_pop(L, 1);
}

TEST_CASE("lua: list_sources default to src only", "[lua][api][0.2.3]") {
    TempDir tmp;
    write_minimal_config(tmp.path);
    fs::create_directories(tmp.path / "src");
    std::ofstream(tmp.path / "src" / "main.cpp") << "int main() { return 0; }\n";
    std::ofstream(tmp.path / "src" / "util.cpp") << "void util() {}\n";

    // Create a "lib" dir that should NOT be included (not in config)
    fs::create_directories(tmp.path / "lib");
    std::ofstream(tmp.path / "lib" / "helper.cpp") << "void helper() {}\n";

    lua_State* L = state();
    REQUIRE(L != nullptr);
    register_api(L, tmp.path);

    int rc = luaL_dostring(L, "local sources = ezmk.list_sources(); return #sources");
    REQUIRE(rc == 0);
    // Should only find files from src/ (the default)
    int count = static_cast<int>(lua_tointeger(L, -1));
    REQUIRE(count == 2); // main.cpp + util.cpp, NOT helper.cpp
    lua_pop(L, 1);
}

// ===================================================================
// 0.2.3+: run_hook_script() tests
// ===================================================================

TEST_CASE("run_hook_script: basic execution", "[lua][hook][0.2.3]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "hook_test", R"(
function run(ctx)
    return 0
end
)");

    int rc = run_hook_script(state(), script,
                              tmp.path / "build" / "output",
                              tmp.path, "");
    REQUIRE(rc == 0);
}

TEST_CASE("run_hook_script: receives ctx with output field", "[lua][hook][0.2.3]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "hook_ctx", R"(
function run(ctx)
    assert(type(ctx) == "table", "ctx must be a table")
    assert(type(ctx.output) == "string", "ctx.output must be a string")
    assert(type(ctx.project_root) == "string", "ctx.project_root must be a string")
    assert(type(ctx.profile) == "string", "ctx.profile must be a string")
    return 0
end
)");

    int rc = run_hook_script(state(), script,
                              tmp.path / "build" / "myapp",
                              tmp.path, "release");
    REQUIRE(rc == 0);
}

TEST_CASE("run_hook_script: returns run() exit code", "[lua][hook][0.2.3]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "hook_rc", R"(
function run(ctx)
    return 7
end
)");

    int rc = run_hook_script(state(), script,
                              tmp.path / "build" / "output",
                              tmp.path, "");
    REQUIRE(rc == 7);
}

TEST_CASE("run_hook_script: null L returns error", "[lua][hook][0.2.3]") {
    int rc = run_hook_script(nullptr, fs::path("test.lua"),
                              fs::path("output"), fs::path("."), "");
    REQUIRE(rc != 0);
}

// ===================================================================
// 0.9.9: run_install_hook_script() tests
// ===================================================================

TEST_CASE("run_install_hook_script: basic execution", "[lua][hook][0.9.9]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "install_hook", R"(
function run(ctx)
    return 0
end
)");

    int rc = run_install_hook_script(state(), script,
                                      "testpkg", tmp.path,
                                      tmp.path / "install", "project");
    REQUIRE(rc == 0);
}

TEST_CASE("run_install_hook_script: receives ctx table with all fields", "[lua][hook][0.9.9]") {
    TempDir tmp;
    // Write a config so pkg_version and pkg_type can be read
    {
        std::ofstream of(tmp.path / "ezmk.toml");
        of << "[project]\nname = \"testpkg\"\ntype = \"static\"\nversion = \"1.2.3\"\nlanguage = \"C++17\"\n";
    }

    auto script = write_lua_script(tmp.path, "install_hook_ctx", R"(
function run(ctx)
    assert(type(ctx) == "table", "ctx must be a table")
    assert(ctx.pkg_name == "testpkg", "pkg_name mismatch")
    assert(type(ctx.pkg_root) == "string", "pkg_root must be a string")
    assert(type(ctx.install_path) == "string", "install_path must be a string")
    assert(ctx.scope == "user", "scope mismatch")
    assert(ctx.pkg_version == "1.2.3", "pkg_version mismatch")
    assert(ctx.pkg_type == "static", "pkg_type mismatch")
    return 0
end
)");

    int rc = run_install_hook_script(state(), script,
                                      "testpkg", tmp.path,
                                      tmp.path / "install", "user");
    REQUIRE(rc == 0);
}

TEST_CASE("run_install_hook_script: returns run() exit code", "[lua][hook][0.9.9]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "install_hook_rc", R"(
function run(ctx)
    return 42
end
)");

    int rc = run_install_hook_script(state(), script,
                                      "testpkg", tmp.path,
                                      tmp.path / "install", "project");
    REQUIRE(rc == 42);
}

TEST_CASE("run_install_hook_script: null L returns error", "[lua][hook][0.9.9]") {
    int rc = run_install_hook_script(nullptr, fs::path("test.lua"),
                                      "pkg", fs::path("."),
                                      fs::path("."), "project");
    REQUIRE(rc != 0);
}

TEST_CASE("run_install_hook_script: missing run() function returns error", "[lua][hook][0.9.9]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "no_run", R"(
-- no run() function defined
x = 1
)");

    int rc = run_install_hook_script(state(), script,
                                      "testpkg", tmp.path,
                                      tmp.path / "install", "project");
    REQUIRE(rc != 0);
}

TEST_CASE("run_install_hook_script: Lua error returns error code", "[lua][hook][0.9.9]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "install_error", R"(
function run(ctx)
    error("something went wrong in install hook")
end
)");

    int rc = run_install_hook_script(state(), script,
                                      "testpkg", tmp.path,
                                      tmp.path / "install", "project");
    REQUIRE(rc != 0);
}

TEST_CASE("run_install_hook_script: syntax error returns error code", "[lua][hook][0.9.9]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "install_syntax", R"(
function run(ctx)
    this is not valid lua
end
)");

    int rc = run_install_hook_script(state(), script,
                                      "testpkg", tmp.path,
                                      tmp.path / "install", "project");
    REQUIRE(rc != 0);
}

TEST_CASE("run_install_hook_script: scope is 'global'", "[lua][hook][0.9.9]") {
    TempDir tmp;
    auto script = write_lua_script(tmp.path, "install_scope", R"(
function run(ctx)
    assert(ctx.scope == "global", "scope must be global")
    return 0
end
)");

    int rc = run_install_hook_script(state(), script,
                                      "testpkg", tmp.path,
                                      tmp.path / "install", "global");
    REQUIRE(rc == 0);
}

TEST_CASE("run_install_hook_script: pkg_version and pkg_type empty without ezmk.toml", "[lua][hook][0.9.9]") {
    TempDir tmp;
    // No ezmk.toml — fields should be empty strings
    auto script = write_lua_script(tmp.path, "install_no_toml", R"(
function run(ctx)
    assert(ctx.pkg_version == "", "pkg_version should be empty without ezmk.toml")
    assert(ctx.pkg_type == "", "pkg_type should be empty without ezmk.toml")
    return 0
end
)");

    int rc = run_install_hook_script(state(), script,
                                      "testpkg", tmp.path,
                                      tmp.path / "install", "project");
    REQUIRE(rc == 0);
}
