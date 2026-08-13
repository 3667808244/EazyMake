#pragma once
// 1.2.0: 常见包别名表（best-effort），dev.2 的 `export cmake`（ezmk 包 →
// CMake find_package/target）与 dev.4 的 `import cmake`（find_package →
// ezmk 包）共用同一张表，避免两处漂移。单一事实源。
#include <string>

namespace ezmk {

struct PkgAlias {
    const char* pkg;    // [depends] 条目名（ezmk 小写包名）
    const char* find;   // find_package(<find> QUIET) 名
    const char* target; // 关联的 CMake imported target
};

inline const PkgAlias kPkgAliases[] = {
    {"catch2",          "Catch2",             "Catch2::Catch2"},
    {"openssl",         "OpenSSL",            "OpenSSL::SSL"},
    {"libcurl",         "CURL",               "CURL::libcurl"},
    {"protobuf",        "protobuf",           "protobuf::libprotobuf"},
    {"eigen",           "Eigen3",             "Eigen3::Eigen"},
    {"googletest",      "GTest",              "GTest::gtest"},
    {"fmt",             "fmt",                "fmt::fmt"},
    {"spdlog",          "spdlog",             "spdlog::spdlog"},
    {"nlohmann_json",   "nlohmann_json",      "nlohmann_json::nlohmann_json"},
    {"sqlite3",         "SQLite3",            "SQLite3::SQLite3"},
    {"zlib",            "ZLIB",               "ZLIB::ZLIB"},
    {"lua",             "Lua",                "Lua::Lua"},
    {"tomlplusplus",    "tomlplusplus",       "tomlplusplus::tomlplusplus"},
};

// 反向查询：find_package(<find>) 名 → ezmk 包名（找不到返回空串）。
inline const char* pkg_alias_from_find(const std::string& find_name) {
    for (const auto& a : kPkgAliases)
        if (find_name == a.find) return a.pkg;
    return "";
}

} // namespace ezmk
