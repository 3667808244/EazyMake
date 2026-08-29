#include "ezmk/cache.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/util.hpp"
#include "ezmk/toolchain.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>
#include <atomic>
#include <ctime>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ezmk::cache {

// 1.1.3 C5: 对象文件后缀常量（消除散落魔法串）
constexpr const char* kObjExt        = ".o";      // GCC/Clang
constexpr const char* kObjExtMsvc    = ".obj";    // MSVC
constexpr const char* kTempObjSuffix     = ".tmp.o";   // GCC/Clang 中间对象
constexpr const char* kTempObjSuffixMsvc = ".tmp.obj"; // MSVC 中间对象

// ===================================================================
// Helpers
// ===================================================================

bool same_dependency_paths(const std::vector<DepEntry>& old_deps,
                            const std::vector<DepEntry>& new_deps) {
    if (old_deps.size() != new_deps.size()) return false;
    std::set<std::string> old_paths, new_paths;
    for (auto& d : old_deps) old_paths.insert(d.path);
    for (auto& d : new_deps) new_paths.insert(d.path);
    return old_paths == new_paths;
}

std::string iso_time() {
    auto t = std::time(nullptr);
    auto* tm = std::localtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return buf;
}

// 1.4.0-pre.1: fs::relative 在跨盘符（Windows，如绝对 src_dir 在 D:\、项目根在
// C:\）时返回空路径，空串缓存键会让不同源文件在 record.files 上碰撞（对象覆盖、
// 增量构建结果错误）——空则回退为绝对路径（绝对路径天然唯一）。error_code 重载
// 同时消除抛异常风险（-jN 下在 worker 线程调用，抛异常会崩线程）。只用于生成
// 缓存键 / 记录相对路径的场合。
static fs::path safe_relative(const fs::path& p, const fs::path& root) {
    std::error_code ec;
    auto r = fs::relative(p, root, ec);
    if (ec || r.empty()) return p;
    return r;
}

static std::string record_to_json(const CacheRecord& rec) {
    nlohmann::json j;
    j["version"] = rec.version;
    j["compiler"] = rec.compiler;
    j["compiler_version"] = rec.compiler_version;              // 1.1.0
    j["compile_options_signature"] = rec.compile_options_signature;
    j["deterministic"] = rec.deterministic;                    // 1.1.0

    auto& files = j["files"] = nlohmann::json::object();
    for (auto& [key, fe] : rec.files) {
        auto& f = files[key] = nlohmann::json::object();
        f["source_hash"] = fe.source_hash;
        f["object_file"] = fe.object_file;
        f["compiler"] = fe.compiler;
        f["compile_opts"] = fe.compile_opts;
        auto& deps = f["dependencies"] = nlohmann::json::array();
        for (auto& d : fe.dependencies) {
            deps.push_back({{"path", d.path}, {"hash", d.hash}});
        }
        f["last_build_time"] = fe.last_build_time;
    }

    return j.dump(2); // pretty-print with 2-space indent
}

static CacheRecord json_to_record(std::string_view json) {
    auto j = nlohmann::json::parse(json);
    CacheRecord rec;
    rec.version = j.value("version", 1);
    rec.compiler = j.value("compiler", "");
    rec.compiler_version = j.value("compiler_version", "");    // 1.1.0
    rec.compile_options_signature = j.value("compile_options_signature", "");
    rec.deterministic = j.value("deterministic", false);       // 1.1.0

    if (j.contains("files")) {
        for (auto& [fname, fj] : j["files"].items()) {
            FileEntry fe;
            fe.source_hash = fj.value("source_hash", "");
            fe.object_file = fj.value("object_file", "");
            fe.compiler = fj.value("compiler", "");
            for (auto& opt : fj["compile_opts"]) {
                fe.compile_opts.push_back(opt.get<std::string>());
            }
            for (auto& dep : fj["dependencies"]) {
                fe.dependencies.push_back({
                    dep.value("path", ""),
                    dep.value("hash", "")
                });
            }
            fe.last_build_time = fj.value("last_build_time", "");
            rec.files[std::string(fname)] = std::move(fe);
        }
    }
    return rec;
}

// ===================================================================
// Cache operations
// ===================================================================

static fs::path cache_dir() {
    // 1.2.0-dev.7: cache lives under the located project root (upward search);
    // falls back to CWD when no ezmk.toml is found (unchanged behavior).
    auto root = util::locate_project_root(fs::current_path());
    return (root.value_or(fs::current_path())) / ".ezmk/cache";
}

static fs::path record_path() {
    return cache_dir() / "record.json";
}

std::vector<DepEntry> parse_depfile_and_hash(const fs::path& depfile) {
    std::vector<DepEntry> deps;

    if (!util::file_exists(depfile)) return deps;

    std::string content = util::file_read(depfile);
    if (content.empty()) return deps;

    // Normalize CRLF → LF: only skip \r when followed by \n
    // This preserves bare \r characters that are not line endings.
    {
        std::string normalized;
        normalized.reserve(content.size());
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\r' && i + 1 < content.size() && content[i + 1] == '\n') {
                normalized += '\n';
                ++i; // skip the \n
            } else {
                normalized += content[i];
            }
        }
        content = std::move(normalized);
    }

    // Join continuation lines: remove "\\\n"
    std::string joined;
    joined.reserve(content.size());
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\\' && i + 1 < content.size() && content[i + 1] == '\n') {
            ++i; // skip backslash+newline
            continue;
        }
        joined += content[i];
    }

    // 1.2.0-dev.11: split on whitespace honoring GCC's depfile escaping — a
    // path with spaces is written "C:\My\ Project\..." where "\ " is an escaped
    // space. The previous plain split cut the token at the escaped space, so
    // the header's real path was never hashed and changes went undetected.
    std::vector<std::string> tokens;
    std::string curr;
    for (size_t i = 0; i < joined.size(); ++i) {
        char c = joined[i];
        if (c == '\\' && i + 1 < joined.size()) {
            char next = joined[i + 1];
            if (next == ' ' || next == '\t' || next == '\n') {
                // Backslash-escaped whitespace → decode to the literal char.
                curr += next;
                ++i;
                continue;
            }
            // Windows path separator (or any other backslash) stays literal.
            curr += c;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n') {
            if (!curr.empty()) {
                tokens.push_back(curr);
                curr.clear();
            }
        } else {
            curr += c;
        }
    }
    if (!curr.empty()) tokens.push_back(curr);

    // First token is the target (ends with ':'), rest are dependency paths
    for (size_t i = 0; i < tokens.size(); ++i) {
        auto& tok = tokens[i];
        // Target: ends with ':' (could be the first token or have colon at end)
        if (!tok.empty() && tok.back() == ':') continue;
        // Skip empty or lone backslash artifacts
        if (tok.empty() || tok == "\\") continue;

        DepEntry dep;
        dep.path = tok;
        dep.hash = crypto::sha256_file(fs::path(tok));
        deps.push_back(std::move(dep));
    }

    return deps;
}

std::string compile_options_signature(const config::CompileSection& compile) {
    return compile_options_signature(compile, {}, "", "", false);
}

std::string compile_options_signature(const config::CompileSection& compile,
                                      const std::vector<fs::path>& extra_includes,
                                      std::string_view std_flag,
                                      std::string_view stdlib,
                                      bool use_pic) {
    std::string combined;
    // Language standard flag (e.g. "-std=c++17")
    if (!std_flag.empty()) {
        combined += std_flag;
        combined += ' ';
    }
    // Compile flags (merged macros, -D flags, etc.)
    for (auto& f : compile.flags) {
        combined += f;
        combined += ' ';
    }
    // Include dirs
    for (auto& d : compile.include_dirs) {
        combined += "-I";
        combined += d;
        combined += ' ';
    }
    // Extra includes (dependency packages)
    for (auto& inc : extra_includes) {
        combined += "-I";
        combined += inc.string();
        combined += ' ';
    }
    // MSVC-specific flags (0.2.1+) — changing these should invalidate cache
    for (auto& f : compile.msvc_flags) {
        combined += f;
        combined += ' ';
    }
    // 1.1.0: deterministic flag — toggling triggers full rebuild
    if (compile.deterministic) {
        combined += "deterministic=1 ";
    }
    // 1.2.0-dev.11: the RESOLVED SOURCE_DATE_EPOCH is injected into the child
    // environment and embedded in the object's timestamps/build-id — a change
    // (config, env, git HEAD time) must invalidate cached objects even though
    // the command line itself does not change.
    if (compile.source_date_epoch > 0) {
        combined += "sde=";
        combined += std::to_string(compile.source_date_epoch);
        combined += ' ';
    }
    // 1.1.2 C2: stdlib and use_pic both change the emitted command
    // (build_compile_args injects -stdlib=... and -fPIC) — without them here,
    // switching [project].stdlib or type static↔shared reused stale objects.
    if (!stdlib.empty()) {
        combined += "stdlib=";
        combined += stdlib;
        combined += ' ';
    }
    if (use_pic) {
        combined += "pic=1 ";
    }
    return crypto::sha256(combined);
}

std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record) {
    return check_cache(src_file, compile, record, fs::current_path(), {}, "", "", false);
}

std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record,
                                    const fs::path& proj_root) {
    return check_cache(src_file, compile, record, proj_root, {}, "", "", false);
}

std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record,
                                    const fs::path& proj_root,
                                    const std::vector<fs::path>& extra_includes,
                                    std::string_view std_flag,
                                    std::string_view stdlib,
                                    bool use_pic) {
    // 1.4.0-pre.1: rel_src 是 record.files 的缓存键——跨盘符空路径会让不同源
    // 文件键碰撞；safe_relative 空则回退绝对路径（唯一）。
    auto rel_src = safe_relative(src_file, proj_root).generic_string();

    auto it = record.files.find(rel_src);
    if (it == record.files.end()) return std::nullopt;

    auto& entry = it->second;

    // 1. Source hash
    std::string cur_hash = crypto::sha256_file(src_file);
    if (cur_hash != entry.source_hash) return std::nullopt;

    // 2. Compile options signature (extra_includes + std_flag + stdlib + use_pic)
    auto cur_sig = compile_options_signature(compile, extra_includes, std_flag,
                                             stdlib, use_pic);
    // 1.1.0: deterministic build — lockfile hash is part of the signature.
    // Must mirror build.cpp's save side exactly, or every build is a cache
    // miss (the saved signature carries the lock hash, the check doesn't).
    if (record.deterministic) {
        auto lock_path = proj_root / "ezmk.lock";
        if (util::file_exists(lock_path)) {
            cur_sig += ":" + crypto::sha256_file(lock_path);
        }
    }
    if (cur_sig != record.compile_options_signature) return std::nullopt;

    // 3. Headers: re-hash each stored header path with current content
    for (auto& dep : entry.dependencies) {
        fs::path dep_path(dep.path);
        if (dep_path.is_relative()) dep_path = proj_root / dep_path;
        std::string cur_hdr_hash = crypto::sha256_file(dep_path);
        if (cur_hdr_hash != dep.hash) return std::nullopt;
    }

    // All checks passed — cache hit
    return proj_root / entry.object_file;
}

CacheRecord load_record() {
    return load_record(record_path());
}

CacheRecord load_record(const fs::path& json_path) {
    if (!util::file_exists(json_path)) return CacheRecord{};

    std::string json = util::file_read(json_path);
    if (json.empty()) return CacheRecord{};

    try {
        return json_to_record(json);
    } catch (const std::exception& e) {
        util::warn(std::string("cache corrupted, rebuilding: ") + e.what());
        return CacheRecord{};
    }
}

void save_record(const CacheRecord& record) {
    fs::create_directories(cache_dir());
    save_record(record, record_path());
}

void save_record(const CacheRecord& record, const fs::path& json_path) {
    fs::create_directories(json_path.parent_path());

    std::string json = record_to_json(record);

    auto tmp = json_path;
    tmp += ".tmp";
    if (!util::file_write(tmp, json)) {
        // 1.4.0-dev.5: a failed cache write must be surfaced — silently
        // dropping it made the next build recompile everything AND left
        // record.json stale (build still reported success).
        util::warn("failed to write cache record to " + tmp.string() +
                   " — incremental cache will not persist");
        return;
    }
    std::error_code ec;
    fs::rename(tmp, json_path, ec);
    if (ec) {
        if (!util::file_write(json_path, json)) {
            util::warn("failed to write cache record to " + json_path.string() +
                       " — incremental cache will not persist");
        }
    }
}

void clear_cache() {
    util::remove_all(cache_dir());
}

// ===================================================================
// 1.1.1: Single-source compile command construction (single source of truth)
//
// build_compile_args() returns the UNESCAPED compile args for one source
// file. The build path (compile_one_source) joins them via join_shell_args()
// and runs the compiler; compile_db reuses the same args for
// compile_commands.json — so the index can never drift from the real build.
// ===================================================================

// 1.1.3 C1: 解析 SOURCE_DATE_EPOCH。优先级：config 值 > 环境变量（非数字警告并按 0
// 处理，不抛异常）。在 -jN 下于 worker 线程调用，旧的裸 stoull 遇非数字 SDE 抛异常
// 直接崩线程。
uint64_t resolve_source_date_epoch(const config::CompileSection& compile) {
    if (compile.deterministic && compile.source_date_epoch > 0) {
        return compile.source_date_epoch;
    }
    if (!compile.deterministic) return 0;
    const char* env_sde = std::getenv("SOURCE_DATE_EPOCH");
    if (env_sde && env_sde[0]) {
        try {
            return std::stoull(env_sde);
        } catch (...) {
            util::warn("invalid SOURCE_DATE_EPOCH: " + std::string(env_sde));
            return 0;
        }
    }
    return 0;
}

// 1.2.0-dev.9: 自编译 -I 构造 — [def_inc (proj_root/include)] + [compile].include_dirs
// 解析结果的保序去重（首次出现顺序保留，lexically_normal 判重）。当 include_dirs 含
// 默认 "include" 时与 def_inc 重复，去重后 compile_commands.json 输出更干净，
// 编译器语义不变。extra_includes（依赖包注入）不参与去重。
static std::vector<fs::path> resolve_compile_include_paths(const CompileInput& in) {
    std::vector<fs::path> result;
    std::set<std::string> seen;
    auto add = [&](const fs::path& p) {
        std::string key = p.lexically_normal().string();
        if (seen.insert(key).second) result.push_back(p);
    };
    auto def_inc = in.proj_root / "include";
    if (util::file_exists(def_inc)) add(def_inc);
    for (auto& d : in.compile.include_dirs) {
        fs::path resolved = d;
        if (resolved.is_relative()) resolved = in.proj_root / resolved;
        add(resolved);
    }
    return result;
}

std::vector<std::string> build_compile_args(const CompileInput& in,
                                            const fs::path& src,
                                            const fs::path& obj) {
    std::vector<std::string> args;
    bool is_msvc = (in.tc.family == toolchain::CompilerFamily::Msvc);
    // 1.4.0-pre.1: rel 用于 dep 文件路径（in.dep_dir / rel）——跨盘符空路径会让
    // 所有跨盘符源文件写到同一 .d 文件（依赖数据串扰）；safe_relative 回退绝对路径。
    auto rel = safe_relative(src, in.proj_root);

    if (is_msvc) {
        args.push_back("cl.exe");
        args.push_back("/c");
        // 1.1.0: deterministic build flags
        if (in.compile.deterministic) {
            args.push_back("/Brepro");
        }
        auto translated = toolchain::translate_compile_flags(
            std::vector<std::string>{in.lang.std_flag}, toolchain::CompilerFamily::Msvc);
        if (!translated.translated.empty()) {
            args.push_back(translated.translated[0]);
        }
        auto flag_trans = toolchain::translate_compile_flags(
            in.compile.flags, toolchain::CompilerFamily::Msvc);
        for (auto& f : flag_trans.translated) args.push_back(f);
        for (auto& f : flag_trans.unrecognized) {
            if (in.verbose) {
                util::warn(std::string("unrecognized GCC flag in MSVC mode: ") + f);
            }
        }
        for (auto& f : in.compile.msvc_flags) args.push_back(f);
        args.push_back("/utf-8");
        args.push_back("/MD");
        // 1.2.0-dev.9: def_inc + include_dirs 保序去重
        for (auto& inc : resolve_compile_include_paths(in)) args.push_back("/I" + inc.string());
        for (auto& inc : in.extra_includes) args.push_back("/I" + inc.string());
        args.push_back("/Fo" + obj.string());
        args.push_back("/showIncludes");
        args.push_back(src.string());
    } else {
        std::string compiler = in.lang.detected_compiler.empty()
            ? in.lang.compiler : in.lang.detected_compiler;
        args.push_back(compiler);
        args.push_back(in.lang.std_flag);
        // 1.1.0-dev.4: inject stdlib flags (e.g., -stdlib=libc++)
        if (!in.stdlib.empty()) {
            auto stdlib_flags = toolchain::get_stdlib_flags(in.stdlib, in.tc.family);
            for (auto& sf : stdlib_flags) args.push_back(sf);
        }
        args.push_back("-c");
        // 1.1.0: deterministic build flags
        if (in.compile.deterministic) {
            args.push_back("-ffile-prefix-map=" + in.proj_root.string() + "=.");
            args.push_back("-frandom-seed=" + src.filename().string());
        }
        for (auto& f : in.compile.flags) args.push_back(f);
        if (in.use_pic) args.push_back("-fPIC");
        // 1.2.0-dev.9: def_inc + include_dirs 保序去重
        for (auto& inc : resolve_compile_include_paths(in)) args.push_back("-I" + inc.string());
        for (auto& inc : in.extra_includes) args.push_back("-I" + inc.string());
        fs::path dep = in.dep_dir / rel;
        dep.replace_extension(".d");
        args.push_back("-MMD");
        args.push_back("-MF");
        args.push_back(dep.string());
        args.push_back(src.string());
        args.push_back("-o");
        args.push_back(obj.string());
    }

    return args;
}

std::string join_shell_args(const std::vector<std::string>& args) {
    std::string r;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) r += ' ';
        const auto& a = args[i];
        // Double-quote args containing whitespace or a shell metacharacter so
        // paths with spaces survive run_command() intact (escape_shell_arg does
        // not escape spaces). Args without special chars are emitted bare —
        // behaviorally equivalent to the historical always-quoted form.
        // 1.1.3 S4: 黑名单补全为 POSIX `/bin/sh` 解析涉及的全部字符（含 `;|&#()<>`、
        // glob 通配 `*?[]{}`、波浪号展开 `~`、历史展开 `!`、换行等）。漏掉任何一个，
        // 恶意 flags（如 `-lfoo; echo pwned`）都会在 `sh -c` 下被执行。
        bool need_quote = false;
        for (char c : a) {
            if (c==' '||c=='\t'||c=='"'||c=='\''||c=='\\'||c=='`'||c=='$'||
                c==';'||c=='|'||c=='&'||c=='#'||c=='('||c==')'||c=='<'||c=='>'||
                c=='~'||c=='*'||c=='?'||c=='['||c==']'||c=='{'||c=='}'||c=='!'||
                c=='\n'||c=='\r') { need_quote = true; break; }
        }
        if (need_quote) {
            r += '"';
            r += util::escape_shell_arg(a);
            r += '"';
        } else {
            r += util::escape_shell_arg(a);
        }
    }
    return r;
}

JoinedCommand join_args_with_response_file(const std::vector<std::string>& args,
                                           const fs::path& rsp_dir) {
    std::string joined = join_shell_args(args);
    if (joined.size() <= kResponseFileThreshold) return {std::move(joined), {}};
    if (args.size() < 2) return {std::move(joined), {}};  // nothing to offload

    // GCC/Clang response file: one literal argument per line (no shell
    // quoting/escaping — a line IS an arg, so paths with spaces are safe).
    // args[0] (the compiler executable) stays on the command line: `g++ @file`.
    static std::atomic<unsigned> counter{0};
    fs::path rsp = rsp_dir / ("ezmk-rsp-" + std::to_string(counter.fetch_add(1)) +
                              ".rsp.tmp");
    std::error_code ec;
    fs::create_directories(rsp_dir, ec);
    std::string content;
    for (size_t i = 1; i < args.size(); ++i) {
        content += args[i];
        content += '\n';
    }
    if (!util::file_write(rsp, content)) {
        // Response file could not be written — fall back to the plain command
        // (it may still work; otherwise the compiler's own error is clearer).
        return {std::move(joined), {}};
    }
    std::vector<std::string> cmd_args = {args[0], "@" + rsp.string()};
    return {join_shell_args(cmd_args), rsp};
}

// ===================================================================
// 0.2.3+: Single-source compile (thread-safe — read-only on record)
//
// INVARIANT: This function only reads from `record` (const ref).
// In parallel compilation, multiple threads call this concurrently
// with the same record. The caller (build.cpp) is responsible for
// updating record.files after all threads complete.
// ===================================================================

// 1.1.3 Q2: 填充缓存记录条目（source hash / 编译选项）。
static void fill_record_entry(FileEntry& entry, const CompileInput& in,
                              const fs::path& src, const fs::path& cache_obj,
                              bool is_msvc) {
    entry.source_hash = crypto::sha256_file(src);
    // 1.4.0-pre.1: object_file 是记录字段——跨盘符空路径会让命中时解析到
    // proj_root 本身（对象错乱）；safe_relative 回退绝对路径。
    entry.object_file = safe_relative(cache_obj, in.proj_root).generic_string();
    entry.compiler = is_msvc ? "cl.exe" : in.lang.compiler;
    if (is_msvc) {
        auto flag_trans = toolchain::translate_compile_flags(
            in.compile.flags, toolchain::CompilerFamily::Msvc);
        entry.compile_opts = flag_trans.translated;
        for (auto& f : in.compile.msvc_flags) entry.compile_opts.push_back(f);
    } else {
        entry.compile_opts = in.compile.flags;
    }
}

// 1.1.3 Q2: 解析编译依赖（MSVC /showIncludes 或 GCC .d 文件）并归一化相对路径。
static std::vector<DepEntry> parse_compile_dependencies(
        const CompileInput& in, const fs::path& rel,
        const util::ProcResult& res, bool is_msvc) {
    std::vector<DepEntry> deps;
    if (is_msvc) {
        auto includes = toolchain::parse_show_includes(res.err);
        for (auto& inc_path : includes) {
            DepEntry dep;
            dep.path = inc_path.string();
            if (util::file_exists(inc_path)) {
                dep.hash = crypto::sha256_file(inc_path);
            }
            deps.push_back(std::move(dep));
        }
    } else {
        fs::path dep = in.dep_dir / rel;
        dep.replace_extension(".d");
        deps = parse_depfile_and_hash(dep);
    }
    // Normalize dep paths: absolute under proj_root become relative
    for (auto& d : deps) {
        fs::path dp(d.path);
        if (dp.is_absolute()) {
            // 1.4.0-pre.1: safe_relative——跨盘符返回空路径时回退绝对路径
            // （保持原样存储），同时消除无 ec 重载的抛异常风险。
            auto r = safe_relative(dp, in.proj_root);
            if (!r.empty() && r.string().find("..") == std::string::npos) {
                d.path = r.generic_string();
            }
        }
    }
    return deps;
}

SingleCompileResult compile_one_source(const fs::path& src,
                                       const CompileInput& in,
                                       const CacheRecord& record) {
    SingleCompileResult result;
    result.source = src;
    result.success = false;

    bool is_msvc = (in.tc.family == toolchain::CompilerFamily::Msvc);
    const char* obj_suffix = is_msvc ? kObjExtMsvc : kObjExt;
    const char* tmp_suffix = is_msvc ? kTempObjSuffixMsvc : kTempObjSuffix;

    // 1.4.0-pre.1: rel_src 是 record.files 的缓存键（compile_sources 用它写入/
    // 查找条目）——跨盘符空路径会键碰撞（不同源文件互相覆盖记录）；safe_relative
    // 空则回退绝对路径（唯一）。obj/cache_obj 路径随之回退到源文件所在目录，
    // 虽不在 obj_dir 内但每源唯一，不会互相覆盖。
    auto rel = safe_relative(src, in.proj_root);
    result.rel_src = rel.generic_string();

    fs::path obj = in.obj_dir / rel;
    obj.replace_extension(obj_suffix);
    fs::path obj_tmp = in.obj_dir / rel;
    obj_tmp.replace_extension(tmp_suffix);

    fs::path cache_obj = in.cache_obj_dir / rel;
    cache_obj.replace_extension(obj_suffix);

    fs::create_directories(obj.parent_path());
    fs::create_directories(cache_obj.parent_path());

    // Check cache (unless disabled)
    if (!in.disable_cache) {
        auto cached = check_cache(src, in.compile, record, in.proj_root,
                                  in.extra_includes, in.lang.std_flag,
                                  in.stdlib, in.use_pic);
        if (cached) {
            auto cache_src = *cached;
            bool same_dir = (fs::absolute(in.cache_obj_dir) == fs::absolute(in.obj_dir));
            if (same_dir && util::file_exists(cache_src)) {
                result.object = cache_src;
                result.cache_hit = true;
                result.success = true;
                if (in.verbose) {
                    auto it = record.files.find(result.rel_src);
                    if (it != record.files.end()) {
                        util::info(util::color_msg(util::color::cyan,
                            ezmk::i18n::fmt(ezmk::i18n::I18nKey::cache_hit,
                                {{"file", result.rel_src},
                                 {"count", std::to_string(it->second.dependencies.size())}})));
                    }
                }
                return result;
            } else if (!same_dir && util::file_exists(cache_src)) {
                std::error_code ec;
                fs::copy_file(cache_src, obj_tmp, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    fs::rename(obj_tmp, obj, ec);
                    if (!ec) {
                        result.object = obj;
                        result.cache_hit = true;
                        result.success = true;
                        if (in.verbose) {
                            auto it = record.files.find(result.rel_src);
                            if (it != record.files.end()) {
                                util::info(util::color_msg(util::color::cyan,
                                    ezmk::i18n::fmt(ezmk::i18n::I18nKey::cache_hit,
                                        {{"file", result.rel_src},
                                         {"count", std::to_string(it->second.dependencies.size())}})));
                            }
                        }
                        return result;
                    }
                }
                fs::remove(obj_tmp, ec);
            }
        }
        // Verbose: explain cache miss
        if (in.verbose) {
            auto it = record.files.find(result.rel_src);
            if (it == record.files.end()) {
                util::info(ezmk::i18n::I18nKey::cache_miss_record,
                           {{"file", result.rel_src}});
            } else {
                std::string cur_hash = crypto::sha256_file(src);
                if (cur_hash != it->second.source_hash) {
                    util::info(ezmk::i18n::I18nKey::cache_miss_source,
                               {{"file", result.rel_src}});
                } else {
                    auto cur_sig = compile_options_signature(in.compile, in.extra_includes,
                                                            in.lang.std_flag,
                                                            in.stdlib, in.use_pic);
                    if (cur_sig != record.compile_options_signature) {
                        util::info(ezmk::i18n::I18nKey::cache_miss_options);
                    } else {
                        for (auto& dep : it->second.dependencies) {
                            fs::path dp(dep.path);
                            if (dp.is_relative()) dp = in.proj_root / dp;
                            std::string hdr_hash = crypto::sha256_file(dp);
                            if (hdr_hash != dep.hash) {
                                util::info(ezmk::i18n::I18nKey::cache_miss_header,
                                           {{"header", dep.path}});
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Cache miss: compile to temp file
    if (in.verbose) {
        util::info(ezmk::i18n::I18nKey::compiling,
                   {{"file", result.rel_src}});
    }

    // 1.1.0: deterministic build — resolve SOURCE_DATE_EPOCH and inject flags
    // 1.1.3 C1: 解析逻辑集中在 resolve_source_date_epoch()（可单测、worker 线程安全）
    uint64_t sde = resolve_source_date_epoch(in.compile);
    std::string sde_str = sde > 0 ? std::to_string(sde) : "";

    // 1.1.1: single-source command construction — shared with compile_db
    auto args = build_compile_args(in, src, obj_tmp);
    // 1.3.0-dev.2: command-line length fallback — >16K joined command becomes
    // `compiler @<rsp>` (GCC/Clang only; MSVC has its own rsp syntax and the
    // threshold simply never triggers there — see dev.2 §3.5).
    JoinedCommand jc = is_msvc ? JoinedCommand{join_shell_args(args), {}}
                               : join_args_with_response_file(args, in.obj_dir);
    std::string cmd = jc.cmd;

    if (in.verbose) {
        util::info(util::color_msg(util::color::dim, "    cmd: " + cmd));
    }

    // 1.1.0/1.1.2 C7: SOURCE_DATE_EPOCH is injected via the child's environment,
    // not the process-global one. The old _putenv_s/setenv + restore mutated the
    // global environment from worker threads — a data race that made deterministic
    // -jN builds non-deterministic (thread A's value could leak into thread B's
    // compiler, or A could unset it while B's compiler was still reading it).
    // RunOptions.env reaches only the child (POSIX: setenv after fork).
    util::RunOptions opts;
    if (!sde_str.empty()) opts.env["SOURCE_DATE_EPOCH"] = sde_str;
    auto res = util::run_command(cmd, opts);

    // 1.3.0-dev.2: the response file is transient — remove it right after the
    // run (a crashed run leaks a .rsp.tmp that the next build's stale-temp
    // cleanup reaps).
    if (!jc.rsp_file.empty()) {
        std::error_code ec;
        fs::remove(jc.rsp_file, ec);
    }

    if (res.exit_code != 0) {
        std::ostringstream err;
        err << ezmk::i18n::fmt(ezmk::i18n::I18nKey::compilation_failed,
                                {{"file", src.string()},
                                 {"code", std::to_string(res.exit_code)}}) << "\n";
        if (!res.err.empty()) err << res.err << "\n";
        if (!res.out.empty()) err << res.out << "\n";
        err << "  cmd: " << cmd;
        result.error_msg = err.str();
        std::error_code ec;
        fs::remove(obj_tmp, ec);
        return result;
    }

    // Atomically rename temp to final
    {
        std::error_code ec;
        fs::rename(obj_tmp, obj, ec);
        if (ec) {
            fs::copy_file(obj_tmp, obj, fs::copy_options::overwrite_existing, ec);
            fs::remove(obj_tmp, ec);
        }
    }
    result.object = obj;

    // Copy compiled object to cache (atomic)
    {
        std::error_code ec;
        fs::path cache_tmp = cache_obj;
        cache_tmp += ".tmp";
        fs::copy_file(obj, cache_tmp, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            fs::rename(cache_tmp, cache_obj, ec);
            if (ec) {
                fs::copy_file(obj, cache_obj, fs::copy_options::overwrite_existing, ec);
            }
        }
    }

    // Build record entry（1.1.3 Q2: 提取到 fill_record_entry）
    auto& entry = result.record_entry;
    fill_record_entry(entry, in, src, cache_obj, is_msvc);

    // Parse dependencies（1.1.3 Q2: 提取到 parse_compile_dependencies）
    result.new_deps = parse_compile_dependencies(in, rel, res, is_msvc);

    entry.dependencies = result.new_deps;
    entry.last_build_time = iso_time();
    result.success = true;
    return result;
}

// ===================================================================
// Unified compile loop (0.1.5 DRY refactoring)
// ===================================================================

CompileResult compile_sources(const CompileInput& in, CacheRecord& record) {
    CompileResult result;

    for (auto& src : in.sources) {
        auto sr = compile_one_source(src, in, record);

        if (sr.cache_hit) {
            result.objects.push_back(sr.object);
            ++result.cache_hits;
        } else if (sr.success) {
            result.objects.push_back(sr.object);
            ++result.cache_misses;
            // Check if dependency path set changed (include structure change)
            auto old_it = record.files.find(sr.rel_src);
            if (old_it != record.files.end() &&
                !same_dependency_paths(old_it->second.dependencies, sr.new_deps)) {
                util::info(ezmk::i18n::I18nKey::include_structure_changed,
                           {{"file", sr.rel_src}});
            }
            // Update cache record with new entry
            record.files[sr.rel_src] = std::move(sr.record_entry);
        } else {
            // Compilation failed
            util::error(sr.error_msg);
            throw ezmk::fatal_error(ezmk::i18n::fmt(ezmk::i18n::I18nKey::build_failed));
        }
    }

    return result;
}

} // namespace ezmk::cache
