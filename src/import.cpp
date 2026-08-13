// src/import.cpp — CMake 项目导入（1.2.0，实验性）
// 轻量 CMake 解析 + 有限变量展开（§3.2「变量展开策略」）。
// 阶段一：命令骨架 + 解析器；阶段二：核心映射（§3.2 八命令）+ find_package /
// 条件编译 best-effort。拒绝逻辑（阶段三）与生成物打磨（阶段三）后续补充。
#include "ezmk/import.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"
#include "pkg_alias.hpp"      // 共享包别名表（dev.2 export / dev.4 import）

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <set>
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

// 展开 + 按 ';' 拆分（CMake 列表展开为多个参数）。
std::vector<std::string> expand_args(const std::string& raw,
                                     const std::map<std::string, std::string>& table) {
    std::string e = expand_var(raw, table);
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= e.size(); ++i) {
        if (i == e.size() || e[i] == ';') {
            out.push_back(e.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

// 是否仍含未解析变量/生成器表达式。
bool unparsed(const std::string& s) {
    return s.find("${") != std::string::npos || s.find("$<") != std::string::npos;
}

// ===================================================================
// 平台条件求值（best-effort，§3.3-2）
// ===================================================================

// 当前平台模型：导入运行平台即目标平台（Windows 开发机 → WIN32=true）。
std::optional<bool> eval_cond(const std::vector<std::string>& args,
                              const std::map<std::string, std::string>& table) {
    if (args.empty()) return std::nullopt;
    if (args.size() == 1) {
        const std::string& a = args[0];
        if (a == "WIN32")   return true;   // 当前平台为 Windows
        if (a == "UNIX")    return false;
        if (a == "APPLE")   return false;
        if (a == "MSVC")    return false;  // ezmk 用 g++
        if (a == "ANDROID" || a == "IOS") return false;
        if (a == "CMAKE_SYSTEM_NAME") return false;
        if (a.find("${") != std::string::npos || a.find("$ENV") != std::string::npos)
            return std::nullopt;
        // 常量变量查表：set(VAR ON/OFF) / 非空
        auto it = table.find(a);
        if (it != table.end()) {
            std::string v = it->second;
            for (auto& c : v) c = static_cast<char>(std::toupper((unsigned char)c));
            return v == "ON" || v == "TRUE" || v == "1" || v == "YES";
        }
        return std::nullopt;  // 自定义变量未解析 → 无法求值
    }
    if (args.size() >= 2 && args[0] == "NOT") {
        std::vector<std::string> inner(args.begin() + 1, args.end());
        auto r = eval_cond(inner, table);
        if (r) return !*r;
        return std::nullopt;
    }
    if (args.size() >= 3 && args[0] == "CMAKE_SYSTEM_NAME") {
        std::string target = args[2];
        for (auto& c : target) c = static_cast<char>(std::toupper((unsigned char)c));
#ifdef EZMK_WIN
        return target.find("WINDOW") != std::string::npos;
#else
        return target.find("LINUX") != std::string::npos;
#endif
    }
    return std::nullopt;  // AND/OR/比较 → 无法求值（跳过 + TODO）
}

// ===================================================================
// 核心映射（§3.2）
// ===================================================================

bool is_source_file(const std::string& p) {
    static const char* kSrcExt[] = {".c", ".cc", ".cpp", ".cxx", ".c++", ".m", ".mm"};
    for (auto ext : kSrcExt)
        if (p.size() >= std::strlen(ext) &&
            p.compare(p.size() - std::strlen(ext), std::strlen(ext), ext) == 0)
            return true;
    return false;
}

void ascii_upper(std::string& s) {
    for (auto& c : s) c = static_cast<char>(std::toupper((unsigned char)c));
}
void ascii_lower(std::string& s) {
    for (auto& c : s) c = static_cast<char>(std::tolower((unsigned char)c));
}

std::string join(const std::vector<std::string>& v, const char* sep = " ") {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

struct ImportedProject {
    std::string name;
    std::string type = "executable";
    std::string version = "0.1.0";
    std::string language = "C++17";
    std::string main_target;                    // 首个 add_executable/library 目标
    std::vector<std::string> src_files;
    std::vector<std::string> src_dirs;          // 源文件所在目录（相对项目根，去重）
    std::vector<std::string> include_dirs;
    std::map<std::string, std::string> macros;
    std::vector<std::string> compile_flags;
    std::vector<std::string> system_targets;    // 无法识别的链接库（去 -l）
    std::set<std::string> find_packages;        // find_package → ezmk 包名
    std::map<std::string, std::string> find_pkg_versions;
    std::vector<std::string> todos;             // 未解析参数/未求值条件块的 TODO 注释
};

ImportedProject build_project(const std::vector<CmakeCall>& calls,
                              const std::map<std::string, std::string>& table,
                              const fs::path& project_root) {
    ImportedProject p;
    p.name = project_root.filename().string();

    // 条件栈：optional<bool>，nullopt = 无法求值（跳过内容 + TODO）。
    std::vector<std::optional<bool>> cond_stack;
    std::set<std::string> todo_seen;

    auto add_todo = [&](const std::string& note) {
        if (todo_seen.insert(note).second) p.todos.push_back(note);
    };
    auto is_skipped = [&]() {
        for (auto& c : cond_stack)
            if (!c.value_or(false)) return true;
        return false;
    };

    for (const auto& call : calls) {
        // ---- 条件/函数块 ----
        if (call.name == "if") {
            auto r = eval_cond(call.args, table);
            if (r) cond_stack.push_back(*r);
            else {
                cond_stack.push_back(std::nullopt);
                add_todo("未求值的条件块: if(" + join(call.args) + ")");
            }
            continue;
        }
        if (call.name == "elseif") {
            if (!cond_stack.empty()) {
                if (!cond_stack.back().value_or(false)) {
                    auto r = eval_cond(call.args, table);
                    cond_stack.back() = r;
                    if (!r) add_todo("未求值的条件块: elseif(" + join(call.args) + ")");
                } else {
                    cond_stack.back() = false;
                }
            }
            continue;
        }
        if (call.name == "else") {
            if (!cond_stack.empty())
                cond_stack.back() = !cond_stack.back().value_or(false) ? true : false;
            continue;
        }
        if (call.name == "endif") {
            if (!cond_stack.empty()) cond_stack.pop_back();
            continue;
        }
        // 函数/宏/循环：内容跳过（阶段三再决定拒绝策略）。
        if (call.name == "function" || call.name == "macro" ||
            call.name == "foreach" || call.name == "while") {
            cond_stack.push_back(false);
            continue;
        }
        if (call.name == "endfunction" || call.name == "endmacro" ||
            call.name == "endforeach" || call.name == "endwhile") {
            if (!cond_stack.empty()) cond_stack.pop_back();
            continue;
        }

        if (is_skipped()) continue;

        // ---- 映射命令 ----
        if (call.name == "project") {
            if (!call.args.empty()) {
                p.name = call.args[0];
                for (size_t k = 1; k + 1 < call.args.size(); ++k) {
                    std::string key = call.args[k];
                    ascii_upper(key);
                    if (key == "VERSION") p.version = call.args[k + 1];
                    else if (key == "LANGUAGES") {
                        std::string lang = call.args[k + 1];
                        ascii_upper(lang);
                        if (lang.find("CXX") != std::string::npos) p.language = "C++17";
                        else if (lang.find("C") != std::string::npos) p.language = "C17";
                    }
                }
            }
        } else if (call.name == "add_executable") {
            if (!call.args.empty()) { p.main_target = call.args[0]; p.type = "executable"; }
            for (size_t k = 1; k < call.args.size(); ++k) {
                for (auto& a : expand_args(call.args[k], table)) {
                    if (unparsed(a)) add_todo("未解析的参数: " + call.args[k]);
                    else if (is_source_file(a)) p.src_files.push_back(a);
                }
            }
        } else if (call.name == "add_library") {
            if (!call.args.empty()) p.main_target = call.args[0];
            if (call.args.size() >= 2) {
                std::string t = call.args[1];
                ascii_upper(t);
                if (t == "SHARED") p.type = "shared";
                else if (t == "STATIC" || t == "INTERFACE" || t == "MODULE") p.type = "static";
            }
        } else if (call.name == "target_sources" && !p.main_target.empty() &&
                   call.args.size() >= 3 && call.args[0] == p.main_target) {
            for (size_t k = 2; k < call.args.size(); ++k) {
                for (auto& a : expand_args(call.args[k], table)) {
                    if (unparsed(a)) add_todo("未解析的参数: " + call.args[k]);
                    else if (is_source_file(a)) p.src_files.push_back(a);
                }
            }
        } else if (call.name == "target_include_directories" && !p.main_target.empty() &&
                   call.args.size() >= 3 && call.args[0] == p.main_target) {
            for (size_t k = 2; k < call.args.size(); ++k) {
                for (auto& a : expand_args(call.args[k], table)) {
                    if (unparsed(a)) add_todo("未解析的参数: " + call.args[k]);
                    else p.include_dirs.push_back(a);
                }
            }
        } else if (call.name == "target_compile_definitions" && !p.main_target.empty() &&
                   call.args.size() >= 3 && call.args[0] == p.main_target) {
            for (size_t k = 2; k < call.args.size(); ++k) {
                std::string a = expand_var(call.args[k], table);
                if (unparsed(a)) { add_todo("未解析的参数: " + call.args[k]); continue; }
                auto eq = a.find('=');
                if (eq != std::string::npos) p.macros[a.substr(0, eq)] = a.substr(eq + 1);
                else p.macros[a] = "";
            }
        } else if (call.name == "target_compile_options" && !p.main_target.empty() &&
                   call.args.size() >= 3 && call.args[0] == p.main_target) {
            for (size_t k = 2; k < call.args.size(); ++k) {
                std::string a = expand_var(call.args[k], table);
                if (unparsed(a)) { add_todo("未解析的参数: " + call.args[k]); continue; }
                p.compile_flags.push_back(a);
            }
        } else if (call.name == "target_link_libraries" && !p.main_target.empty() &&
                   call.args.size() >= 3 && call.args[0] == p.main_target) {
            for (size_t k = 2; k < call.args.size(); ++k) {
                for (auto& a : expand_args(call.args[k], table)) {
                    if (unparsed(a)) { add_todo("未解析的参数: " + call.args[k]); continue; }
                    std::string lib = a;
                    if (lib.rfind("-l", 0) == 0) lib = lib.substr(2);
                    std::string low = lib;
                    ascii_lower(low);
                    if (p.find_packages.count(low)) continue;  // 走 [depends]
                    p.system_targets.push_back(lib);
                }
            }
        } else if (call.name == "find_package") {
            if (!call.args.empty()) {
                std::string raw = call.args[0];
                const char* mapped = pkg_alias_from_find(raw);
                std::string pkg = (mapped && mapped[0]) ? std::string(mapped) : raw;
                ascii_lower(pkg);
                p.find_packages.insert(pkg);
                if (call.args.size() >= 2 && call.args[1] != "REQUIRED" &&
                    call.args[1].find("REQUIRED") == std::string::npos) {
                    p.find_pkg_versions[pkg] = call.args[1];
                }
            }
        }
    }

    // 源文件所在目录 → src_dirs（相对项目根，去重保序）。
    {
        std::vector<std::string> dirs;
        std::set<std::string> seen;
        for (auto& f : p.src_files) {
            fs::path fp(f);
            std::string dir = fp.parent_path().empty() ? "." : fp.parent_path().string();
            std::replace(dir.begin(), dir.end(), '\\', '/');
            if (seen.insert(dir).second) dirs.push_back(dir);
        }
        p.src_dirs = dirs;
    }
    return p;
}

std::string str_array(const std::vector<std::string>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ", ";
        out += util::toml_quote(v[i]);
    }
    out += "]";
    return out;
}

std::string build_toml(const ImportedProject& p) {
    std::string t;
    t += "# ============================================\n";
    t += "# 此文件由 `ezmk project import --from cmake` 自动生成 (v1.2.0)\n";
    t += "# 基于: CMakeLists.txt\n";
    t += "# 该命令为实验性功能，请手动校对库链接和平台宏定义\n";
    t += "# ============================================\n\n";

    t += "[project]\n";
    t += "name = " + util::toml_quote(p.name) + "\n";
    t += "type = " + util::toml_quote(p.type) + "\n";
    t += "version = " + util::toml_quote(p.version) + "\n";
    t += "language = " + util::toml_quote(p.language) + "\n\n";

    t += "[compile]\n";
    if (p.compile_flags.empty())
        t += "flags = [\"-Wall\", \"-Wextra\"]\n";
    else
        t += "flags = " + str_array(p.compile_flags) + "\n";
    t += "default_profile = \"debug\"\n";
    if (!p.include_dirs.empty())
        t += "include_dirs = " + str_array(p.include_dirs) + "\n";
    if (!p.src_dirs.empty())
        t += "src_dirs = " + str_array(p.src_dirs) + "\n";
    t += "\n";

    t += "[compile.profile.debug]\n";
    t += "flags = [\"-g\", \"-O0\"]\n";
    t += "msvc_flags = [\"/Zi\", \"/Od\"]\n\n";
    t += "[compile.profile.release]\n";
    t += "flags = [\"-O2\", \"-DNDEBUG\"]\n";
    t += "msvc_flags = [\"/O2\", \"/DNDEBUG\"]\n\n";

    if (!p.macros.empty()) {
        t += "[compile.macros]\n";
        for (auto& [k, v] : p.macros)
            t += util::toml_quote(k) + " = " + util::toml_quote(v) + "\n";
        t += "\n";
    }

    t += "[link]\n";
    t += "flags = []\n";
    t += "link_dirs = []\n";
    t += "system_target = " + str_array(p.system_targets) + "\n\n";

    t += "[depends]\n";
    for (auto& name : p.find_packages) {
        std::string spec = name;
        auto it = p.find_pkg_versions.find(name);
        if (it != p.find_pkg_versions.end()) spec += "@" + it->second;
        t += "# TODO: 原 CMake 引用了 " + name + "，请手动执行 `ezmk pkg install " + name +
             "` 后取消注释\n";
        t += "# lib = [" + util::toml_quote(spec) + "]\n";
    }
    t += "lib = []\n";

    for (auto& note : p.todos)
        t += "# TODO: " + note + "\n";

    return t;
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
    auto project = build_project(calls, table, project_root);

    std::string text = build_toml(project);

    util::file_write(toml_file, text);
    util::info("wrote " + toml_file.string());
    return 0;
}

} // namespace ezmk::import
