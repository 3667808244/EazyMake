// src/import.cpp — CMake 项目导入（1.2.0，实验性）
// 轻量 CMake 解析 + 有限变量展开（§3.2「变量展开策略」）。
// 阶段一交付：命令骨架 + 解析器（tokenizer + set() 变量表 + 单层 ${VAR} 展开）
// 与最小生成物（[project] 基础）。§3.2 核心映射 / best-effort 在阶段二补充。
#include "ezmk/import.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace ezmk::import {

namespace {

// ===================================================================
// 轻量 CMake 解析
// ===================================================================

struct CmakeCall {
    std::string name;
    std::vector<std::string> args;
};

std::string read_file(const fs::path& p) {
    std::ifstream f(p);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void skip_space(const std::string& s, size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

void skip_comment(const std::string& s, size_t& i) {
    while (i < s.size() && s[i] != '\n') ++i;
}

// 解析 "..."（i 指向开引号），返回去转义后的内容。
std::string parse_quoted(const std::string& s, size_t& i) {
    ++i;  // 跳过开引号
    std::string out;
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[i + 1];
            out += (c == 'n') ? '\n' : (c == 't') ? '\t' : (c == 'r') ? '\r' : c;
            i += 2;
        } else {
            out += s[i];
            ++i;
        }
    }
    if (i < s.size()) ++i;  // 跳过闭引号
    return out;
}

// 解析 [[...]] / [=[...]=] 长字符串（i 指向第一个 '['）。
std::string parse_bracket(const std::string& s, size_t& i) {
    size_t eq = 0, j = i + 1;
    while (j < s.size() && s[j] == '=') { ++eq; ++j; }
    ++j;  // 跳过第二个 '['
    std::string closer = "]" + std::string(eq, '=') + "]";
    size_t pos = s.find(closer, j);
    std::string out;
    if (pos == std::string::npos) { out = s.substr(j); i = s.size(); }
    else { out = s.substr(j, pos - j); i = pos + closer.size(); }
    return out;
}

// 解析裸参数（到空白 / '(' / ')' / '#'）。
std::string parse_unquoted(const std::string& s, size_t& i) {
    std::string out;
    while (i < s.size()) {
        char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c)) || c == '(' || c == ')' || c == '#')
            break;
        out += c;
        ++i;
    }
    return out;
}

// 解析命令调用参数列表（i 指向 '('）。
std::vector<std::string> parse_args(const std::string& s, size_t& i) {
    ++i;  // 跳过 '('
    std::vector<std::string> args;
    while (i < s.size()) {
        skip_space(s, i);
        if (i >= s.size()) break;
        char c = s[i];
        if (c == '#') { skip_comment(s, i); continue; }
        if (c == ')') { ++i; break; }
        if (c == '"') { args.push_back(parse_quoted(s, i)); continue; }
        if (c == '[' && i + 1 < s.size() && (s[i + 1] == '[' || s[i + 1] == '=')) {
            args.push_back(parse_bracket(s, i)); continue;
        }
        if (c == '(') { ++i; continue; }  // 嵌套 '(' —— 跳过（mapped 命令不用）
        std::string arg = parse_unquoted(s, i);
        if (arg.empty()) { if (i < s.size()) ++i; continue; }  // 保证前进，防死循环
        args.push_back(arg);
    }
    return args;
}

// 解析整个 CMakeLists.txt → 命令调用列表。
std::vector<CmakeCall> parse_cmake(const std::string& src) {
    std::vector<CmakeCall> calls;
    size_t i = 0;
    while (i < src.size()) {
        skip_space(src, i);
        if (i >= src.size()) break;
        char c = src[i];
        if (c == '#') { skip_comment(src, i); continue; }
        if (!std::isalpha(static_cast<unsigned char>(c)) && c != '_') { ++i; continue; }
        size_t start = i;
        while (i < src.size()) {
            char d = src[i];
            if (!std::isalnum(static_cast<unsigned char>(d)) && d != '_') break;
            ++i;
        }
        std::string name = src.substr(start, i - start);
        skip_space(src, i);
        if (i < src.size() && src[i] == '(') {
            calls.push_back({name, parse_args(src, i)});
        }
    }
    return calls;
}

// ===================================================================
// 有限变量表（§3.2 变量展开策略）
// ===================================================================

// 值是否为常量（不含 ${} / $<...> / $ENV{}）。
bool is_constant(const std::string& v) {
    return v.find("${") == std::string::npos
        && v.find("$<") == std::string::npos
        && v.find("$ENV{") == std::string::npos;
}

// 构建有限变量表：仅捕获顶层、条件块外的常量 set()；被修改的变量剔除。
std::map<std::string, std::string> build_var_table(const std::vector<CmakeCall>& calls) {
    std::map<std::string, std::string> table;
    int depth = 0;  // 条件/函数块嵌套深度
    for (const auto& call : calls) {
        if (call.name == "if" || call.name == "function" || call.name == "macro"
            || call.name == "foreach" || call.name == "while")
            ++depth;
        else if (call.name == "endif" || call.name == "endfunction"
                 || call.name == "endmacro" || call.name == "endforeach"
                 || call.name == "endwhile")
            depth = depth > 0 ? depth - 1 : 0;

        if (depth == 0 && call.name == "set" && call.args.size() >= 2) {
            const std::string& vname = call.args[0];
            if (vname.find("${") == std::string::npos && vname.find("$<") == std::string::npos) {
                bool all_const = true;
                for (size_t k = 1; k < call.args.size(); ++k)
                    if (!is_constant(call.args[k])) { all_const = false; break; }
                if (all_const) {
                    std::string val;
                    for (size_t k = 1; k < call.args.size(); ++k) {
                        if (k > 1) val += ";";
                        val += call.args[k];
                    }
                    table[vname] = val;
                }
            }
        } else {
            // 变量被任何命令修改 → 剔除（再次 set / list(APPEND ...)）
            if (call.name == "set" && !call.args.empty())
                table.erase(call.args[0]);
            if (call.name == "list" && call.args.size() >= 2 && call.args[0] == "APPEND")
                table.erase(call.args[1]);
        }
    }
    return table;
}

// 单层（非递归）${VAR} 展开。
std::string expand_var(std::string arg, const std::map<std::string, std::string>& table) {
    std::string out;
    size_t i = 0;
    while (i < arg.size()) {
        if (arg[i] == '$' && i + 1 < arg.size() && arg[i + 1] == '{') {
            size_t close = arg.find('}', i + 2);
            if (close != std::string::npos) {
                std::string vname = arg.substr(i + 2, close - (i + 2));
                auto it = table.find(vname);
                if (it != table.end()) { out += it->second; i = close + 1; continue; }
            }
        }
        out += arg[i];
        ++i;
    }
    return out;
}

} // namespace

// ===================================================================
// 入口：import_project
// ===================================================================

int import_project(const cli::ProjectImportOptions& opts,
                   const fs::path& project_root) {
    auto cmake_file = project_root / "CMakeLists.txt";
    if (!fs::is_regular_file(cmake_file))
        util::fatal(i18n::I18nKey::import_missing_cmakelists);

    auto toml_file = project_root / "ezmk.toml";
    if (fs::exists(toml_file) && !opts.overwrite)
        util::fatal(i18n::I18nKey::export_exists_refuse, {{"path", toml_file.string()}});

    std::string src = read_file(cmake_file);
    auto calls = parse_cmake(src);
    auto table = build_var_table(calls);

    // ---- 阶段一最小映射：从 project() / add_executable() / add_library() 提取 ----
    std::string proj_name = project_root.filename().string();
    std::string proj_type = "executable";
    std::string proj_version = "0.1.0";
    std::string language = "C++17";
    for (const auto& call : calls) {
        if (call.name == "project" && !call.args.empty()) {
            proj_name = call.args[0];
            for (size_t k = 1; k + 1 < call.args.size(); ++k) {
                std::string key = call.args[k];
                for (auto& c : key) c = static_cast<char>(std::toupper((unsigned char)c));
                if (key == "VERSION") proj_version = call.args[k + 1];
                else if (key == "LANGUAGES")
                    language = (call.args[k + 1].find("CXX") != std::string::npos)
                                   ? "C++17" : "C17";
            }
        } else if (call.name == "add_executable") {
            proj_type = "executable";
        } else if (call.name == "add_library" && call.args.size() >= 2) {
            std::string t = call.args[1];
            for (auto& c : t) c = static_cast<char>(std::toupper((unsigned char)c));
            proj_type = (t == "SHARED") ? "shared" : "static";
        }
    }

    std::string text;
    text += "# ============================================\n";
    text += "# 此文件由 `ezmk project import --from cmake` 自动生成 (v1.2.0)\n";
    text += "# 基于: CMakeLists.txt\n";
    text += "# 该命令为实验性功能，请手动校对库链接和平台宏定义\n";
    text += "# ============================================\n\n";
    text += "[project]\n";
    text += "name = " + util::toml_quote(proj_name) + "\n";
    text += "type = " + util::toml_quote(proj_type) + "\n";
    text += "version = " + util::toml_quote(proj_version) + "\n";
    text += "language = " + util::toml_quote(language) + "\n\n";
    text += "[compile]\n";
    text += "flags = [\"-Wall\", \"-Wextra\"]\n";
    text += "default_profile = \"debug\"\n";
    text += "include_dirs = [\"include\"]\n";

    util::file_write(toml_file, text);
    util::info("wrote " + toml_file.string());
    return 0;
}

} // namespace ezmk::import
