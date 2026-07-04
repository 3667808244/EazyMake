#include "ezmk/cache.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/util.hpp"
#include "ezmk/toolchain.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>
#include <ctime>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ezmk::cache {

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

static std::string record_to_json(const CacheRecord& rec) {
    nlohmann::json j;
    j["version"] = rec.version;
    j["compile_options_signature"] = rec.compile_options_signature;

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
    rec.compile_options_signature = j.value("compile_options_signature", "");

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
    return fs::current_path() / ".ezmk/cache";
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

    // Split on whitespace
    std::vector<std::string> tokens;
    std::string curr;
    for (char c : joined) {
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
    return compile_options_signature(compile, {}, "");
}

std::string compile_options_signature(const config::CompileSection& compile,
                                      const std::vector<fs::path>& extra_includes,
                                      std::string_view std_flag) {
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
    return crypto::sha256(combined);
}

std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record) {
    return check_cache(src_file, compile, record, fs::current_path(), {}, "");
}

std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record,
                                    const fs::path& proj_root) {
    return check_cache(src_file, compile, record, proj_root, {}, "");
}

std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record,
                                    const fs::path& proj_root,
                                    const std::vector<fs::path>& extra_includes,
                                    std::string_view std_flag) {
    auto rel_src = fs::relative(src_file, proj_root).generic_string();

    auto it = record.files.find(rel_src);
    if (it == record.files.end()) return std::nullopt;

    auto& entry = it->second;

    // 1. Source hash
    std::string cur_hash = crypto::sha256_file(src_file);
    if (cur_hash != entry.source_hash) return std::nullopt;

    // 2. Compile options signature (includes extra_includes and std_flag)
    auto cur_sig = compile_options_signature(compile, extra_includes, std_flag);
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
    util::file_write(tmp, json);
    std::error_code ec;
    fs::rename(tmp, json_path, ec);
    if (ec) {
        util::file_write(json_path, json);
    }
}

void clear_cache() {
    util::remove_all(cache_dir());
}

// ===================================================================
// 0.2.3+: Single-source compile (thread-safe — read-only on record)
// ===================================================================

SingleCompileResult compile_one_source(const fs::path& src,
                                       const CompileInput& in,
                                       const CacheRecord& record) {
    SingleCompileResult result;
    result.source = src;
    result.success = false;

    bool is_msvc = (in.tc.family == toolchain::CompilerFamily::Msvc);
    const char* obj_suffix = is_msvc ? ".obj" : ".o";
    const char* tmp_suffix = is_msvc ? ".tmp.obj" : ".tmp.o";

    auto rel = fs::relative(src, in.proj_root);
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
                                  in.extra_includes, in.lang.std_flag);
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
                                    "  [cached] " + result.rel_src +
                                    "  (source hash matches, " +
                                    std::to_string(it->second.dependencies.size()) + " headers unchanged)"));
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
                                                            in.lang.std_flag);
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

    // Build compile command
    std::ostringstream cmd;

    if (is_msvc) {
        cmd << "cl.exe /c ";
        auto translated = toolchain::translate_compile_flags(
            std::vector<std::string>{in.lang.std_flag}, toolchain::CompilerFamily::Msvc);
        if (!translated.translated.empty()) {
            cmd << translated.translated[0] << " ";
        }
        auto flag_trans = toolchain::translate_compile_flags(
            in.compile.flags, toolchain::CompilerFamily::Msvc);
        for (auto& f : flag_trans.translated) cmd << f << " ";
        for (auto& f : flag_trans.unrecognized) {
            if (in.verbose) {
                util::warn(std::string("unrecognized GCC flag in MSVC mode: ") + f);
            }
        }
        for (auto& f : in.compile.msvc_flags) cmd << f << " ";
        cmd << "/utf-8 /MD ";
        auto def_inc = in.proj_root / "include";
        if (util::file_exists(def_inc)) cmd << "/I\"" << def_inc.string() << "\" ";
        for (auto& d : in.compile.include_dirs) {
            fs::path resolved = d;
            if (resolved.is_relative()) resolved = in.proj_root / resolved;
            cmd << "/I\"" << resolved.string() << "\" ";
        }
        for (auto& inc : in.extra_includes) cmd << "/I\"" << inc.string() << "\" ";
        cmd << "/Fo\"" << obj_tmp.string() << "\" ";
        cmd << "/showIncludes ";
        cmd << "\"" << src.string() << "\"";
    } else {
        std::string compiler = in.lang.detected_compiler.empty()
            ? in.lang.compiler : in.lang.detected_compiler;
        cmd << compiler << " " << in.lang.std_flag << " -c ";
        for (auto& f : in.compile.flags) cmd << f << " ";
        if (in.use_pic) cmd << "-fPIC ";
        auto def_inc = in.proj_root / "include";
        if (util::file_exists(def_inc)) cmd << "-I\"" << def_inc.string() << "\" ";
        for (auto& d : in.compile.include_dirs) {
            fs::path resolved = d;
            if (resolved.is_relative()) resolved = in.proj_root / resolved;
            cmd << "-I\"" << resolved.string() << "\" ";
        }
        for (auto& inc : in.extra_includes) cmd << "-I\"" << inc.string() << "\" ";
        fs::path dep = in.dep_dir / rel;
        dep.replace_extension(".d");
        cmd << "-MMD -MF \"" << dep.string() << "\" ";
        cmd << "\"" << src.string() << "\" -o \"" << obj_tmp.string() << "\"";
    }

    if (in.verbose) {
        util::info(util::color_msg(util::color::dim, "    cmd: " + cmd.str()));
    }

    auto res = util::run_command(cmd.str());
    if (res.exit_code != 0) {
        std::ostringstream err;
        err << ezmk::i18n::fmt(ezmk::i18n::I18nKey::compilation_failed,
                                {{"file", src.string()},
                                 {"code", std::to_string(res.exit_code)}}) << "\n";
        if (!res.err.empty()) err << res.err << "\n";
        if (!res.out.empty()) err << res.out << "\n";
        err << "  cmd: " << cmd.str();
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

    // Build record entry
    auto& entry = result.record_entry;
    entry.source_hash = crypto::sha256_file(src);
    entry.object_file = fs::relative(cache_obj, in.proj_root).generic_string();
    entry.compiler = is_msvc ? "cl.exe" : in.lang.compiler;
    if (is_msvc) {
        auto flag_trans = toolchain::translate_compile_flags(
            in.compile.flags, toolchain::CompilerFamily::Msvc);
        entry.compile_opts = flag_trans.translated;
        for (auto& f : in.compile.msvc_flags) entry.compile_opts.push_back(f);
    } else {
        entry.compile_opts = in.compile.flags;
    }

    // Parse dependencies
    if (is_msvc) {
        auto includes = toolchain::parse_show_includes(res.err);
        for (auto& inc_path : includes) {
            DepEntry dep;
            dep.path = inc_path.string();
            if (util::file_exists(inc_path)) {
                dep.hash = crypto::sha256_file(inc_path);
            }
            result.new_deps.push_back(std::move(dep));
        }
    } else {
        fs::path dep = in.dep_dir / rel;
        dep.replace_extension(".d");
        result.new_deps = parse_depfile_and_hash(dep);
    }

    // Normalize dep paths
    for (auto& d : result.new_deps) {
        fs::path dp(d.path);
        if (dp.is_absolute()) {
            auto r = fs::relative(dp, in.proj_root);
            if (!r.empty() && r.string().find("..") == std::string::npos) {
                d.path = r.generic_string();
            }
        }
    }

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

    // Determine object file suffix based on toolchain
    bool is_msvc = (in.tc.family == toolchain::CompilerFamily::Msvc);
    const char* obj_suffix = is_msvc ? ".obj" : ".o";
    const char* tmp_suffix = is_msvc ? ".tmp.obj" : ".tmp.o";

    for (auto& src : in.sources) {
        // Compute paths
        auto rel = fs::relative(src, in.proj_root);
        fs::path obj = in.obj_dir / rel;
        obj.replace_extension(obj_suffix);
        fs::path obj_tmp = in.obj_dir / rel;
        obj_tmp.replace_extension(tmp_suffix);

        fs::path cache_obj = in.cache_obj_dir / rel;
        cache_obj.replace_extension(obj_suffix);

        fs::create_directories(obj.parent_path());
        fs::create_directories(cache_obj.parent_path());

        // Check cache (unless disabled)
        bool cache_hit = false;
        if (!in.disable_cache) {
            auto cached = check_cache(src, in.compile, record, in.proj_root,
                                      in.extra_includes, in.lang.std_flag);
            if (cached) {
                auto cache_src = *cached;
                bool same_dir = (fs::absolute(in.cache_obj_dir) == fs::absolute(in.obj_dir));
                if (same_dir && util::file_exists(cache_src)) {
                    result.objects.push_back(cache_src);
                    ++result.cache_hits;
                    cache_hit = true;
                    if (in.verbose) {
                        auto& entry = record.files[fs::relative(src, in.proj_root).generic_string()];
                        util::info(util::color_msg(util::color::cyan,
                            ezmk::i18n::fmt(ezmk::i18n::I18nKey::cache_hit,
                                {{"file", fs::relative(src, in.proj_root).string()},
                                 {"count", std::to_string(entry.dependencies.size())}})));
                    }
                } else if (!same_dir && util::file_exists(cache_src)) {
                    std::error_code ec;
                    fs::copy_file(cache_src, obj_tmp, fs::copy_options::overwrite_existing, ec);
                    if (!ec) {
                        fs::rename(obj_tmp, obj, ec);
                        if (!ec) {
                            result.objects.push_back(obj);
                            ++result.cache_hits;
                            cache_hit = true;
                            if (in.verbose) {
                                auto& entry = record.files[fs::relative(src, in.proj_root).generic_string()];
                                util::info(util::color_msg(util::color::cyan,
                                    "  [cached] " + fs::relative(src, in.proj_root).string() +
                                    "  (source hash matches, " +
                                    std::to_string(entry.dependencies.size()) + " headers unchanged)"));
                            }
                        }
                    }
                    if (!cache_hit) {
                        fs::remove(obj_tmp, ec);
                    }
                }
            }
            // Verbose: explain cache miss
            if (!cache_hit && in.verbose) {
                auto rel_src = fs::relative(src, in.proj_root).generic_string();
                auto it = record.files.find(rel_src);
                if (it == record.files.end()) {
                    util::info(ezmk::i18n::I18nKey::cache_miss_record,
                               {{"file", rel_src}});
                } else {
                    std::string cur_hash = crypto::sha256_file(src);
                    if (cur_hash != it->second.source_hash) {
                        util::info(ezmk::i18n::I18nKey::cache_miss_source,
                                   {{"file", rel_src}});
                    } else {
                        auto cur_sig = compile_options_signature(in.compile, in.extra_includes,
                                                                in.lang.std_flag);
                        if (cur_sig != record.compile_options_signature) {
                            util::info(ezmk::i18n::I18nKey::cache_miss_options);
                        } else {
                            // Check which header changed
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

        if (cache_hit) continue;

        // Cache miss: compile to temp file
        ++result.cache_misses;

        // Build compile command
        std::ostringstream cmd;

        if (is_msvc) {
            // ---- MSVC compile command ----
            cmd << "cl.exe /c ";

            // Standard flag (GCC→MSVC translated)
            auto translated = toolchain::translate_compile_flags(
                std::vector<std::string>{in.lang.std_flag}, toolchain::CompilerFamily::Msvc);
            if (!translated.translated.empty()) {
                cmd << translated.translated[0] << " ";
            }

            // Compile flags: translate GCC→MSVC, then add MSVC-specific flags
            auto flag_trans = toolchain::translate_compile_flags(
                in.compile.flags, toolchain::CompilerFamily::Msvc);
            for (auto& f : flag_trans.translated) {
                cmd << f << " ";
            }
            for (auto& f : flag_trans.unrecognized) {
                if (in.verbose) {
                    util::warn(std::string("unrecognized GCC flag in MSVC mode: ") + f);
                }
            }

            // MSVC-specific flags (no translation needed)
            for (auto& f : in.compile.msvc_flags) {
                cmd << f << " ";
            }

            // Default MSVC flags
            cmd << "/utf-8 /MD ";

            // Default include: proj_root/include
            auto def_inc = in.proj_root / "include";
            if (util::file_exists(def_inc)) {
                cmd << "/I\"" << def_inc.string() << "\" ";
            }

            // User include dirs
            for (auto& d : in.compile.include_dirs) {
                fs::path resolved = d;
                if (resolved.is_relative()) resolved = in.proj_root / resolved;
                cmd << "/I\"" << resolved.string() << "\" ";
            }

            // Extra includes (dependency packages)
            for (auto& inc : in.extra_includes) {
                cmd << "/I\"" << inc.string() << "\" ";
            }

            // Output + source
            cmd << "/Fo\"" << obj_tmp.string() << "\" ";
            cmd << "/showIncludes ";
            cmd << "\"" << src.string() << "\"";

        } else {
            // ---- GCC/Clang compile command ----
            // Use detected compiler if available (e.g. clang++), fall back to default (g++)
            std::string compiler = in.lang.detected_compiler.empty()
                ? in.lang.compiler : in.lang.detected_compiler;
            cmd << compiler << " " << in.lang.std_flag << " -c ";
            for (auto& f : in.compile.flags) {
                cmd << f << " ";
            }
            if (in.use_pic) {
                cmd << "-fPIC ";
            }

            // Default include: proj_root/include
            auto def_inc = in.proj_root / "include";
            if (util::file_exists(def_inc)) {
                cmd << "-I\"" << def_inc.string() << "\" ";
            }

            for (auto& d : in.compile.include_dirs) {
                fs::path resolved = d;
                if (resolved.is_relative()) resolved = in.proj_root / resolved;
                cmd << "-I\"" << resolved.string() << "\" ";
            }
            for (auto& inc : in.extra_includes) {
                cmd << "-I\"" << inc.string() << "\" ";
            }

            // GCC: depfile for dependency tracking
            fs::path dep = in.dep_dir / rel;
            dep.replace_extension(".d");
            cmd << "-MMD -MF \"" << dep.string() << "\" ";
            cmd << "\"" << src.string() << "\" -o \"" << obj_tmp.string() << "\"";
        }

        if (in.verbose) {
            util::info(ezmk::i18n::I18nKey::compiling,
                       {{"file", fs::relative(src, in.proj_root).string()}});
            util::info(util::color_msg(util::color::dim, "    cmd: " + cmd.str()));
        }

        auto res = util::run_command(cmd.str());
        if (res.exit_code != 0) {
            util::error(ezmk::i18n::I18nKey::compilation_failed,
                        {{"file", src.string()},
                         {"code", std::to_string(res.exit_code)}});
            if (!res.err.empty()) util::error(res.err);
            if (!res.out.empty()) util::error(res.out);
            // Show the full command so user can reproduce
            util::error("  cmd: " + cmd.str());
            // Remove partial temp file
            std::error_code ec;
            fs::remove(obj_tmp, ec);
            throw ezmk::fatal_error(ezmk::i18n::fmt(ezmk::i18n::I18nKey::build_failed));
        }

        // Atomically rename temp to final
        {
            std::error_code ec;
            fs::rename(obj_tmp, obj, ec);
            if (ec) {
                // Fallback: copy + remove
                fs::copy_file(obj_tmp, obj, fs::copy_options::overwrite_existing, ec);
                fs::remove(obj_tmp, ec);
            }
        }
        result.objects.push_back(obj);

        // Copy compiled object to cache (atomic: copy to tmp then rename)
        {
            std::error_code ec;
            fs::path cache_tmp = cache_obj;
            cache_tmp += ".tmp";
            fs::copy_file(obj, cache_tmp, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                fs::rename(cache_tmp, cache_obj, ec);
                if (ec) {
                    // Fallback
                    fs::copy_file(obj, cache_obj, fs::copy_options::overwrite_existing, ec);
                }
            }
        }

        // Update cache record
        auto rel_src = rel.generic_string();
        auto& entry = record.files[rel_src];

        // Parse dependencies: GCC uses .d file, MSVC uses /showIncludes output
        std::vector<DepEntry> new_deps;

        if (is_msvc) {
            // MSVC: parse /showIncludes output from stderr
            // cl.exe writes include notes to stderr
            auto includes = toolchain::parse_show_includes(res.err);
            for (auto& inc_path : includes) {
                DepEntry dep;
                dep.path = inc_path.string();
                // Hash the header file for cache validation
                if (util::file_exists(inc_path)) {
                    dep.hash = crypto::sha256_file(inc_path);
                }
                new_deps.push_back(std::move(dep));
            }
        } else {
            // GCC: parse .d depfile
            fs::path dep = in.dep_dir / rel;
            dep.replace_extension(".d");
            new_deps = parse_depfile_and_hash(dep);
        }

        // Normalize dep paths: absolute paths under proj_root → relative
        // (so package caches survive relocation; system headers stay absolute)
        for (auto& d : new_deps) {
            fs::path dp(d.path);
            if (dp.is_absolute()) {
                auto r = fs::relative(dp, in.proj_root);
                if (!r.empty() && r.string().find("..") == std::string::npos) {
                    d.path = r.generic_string();
                }
            }
        }

        // Check if dependency path set changed (include structure change)
        if (!record.files.empty()) {
            auto old_it = record.files.find(rel_src);
            if (old_it != record.files.end() &&
                !same_dependency_paths(old_it->second.dependencies, new_deps)) {
                util::info(ezmk::i18n::I18nKey::include_structure_changed,
                           {{"file", rel_src}});
            }
        }

        entry.source_hash = crypto::sha256_file(src);
        entry.object_file = fs::relative(cache_obj, in.proj_root).generic_string();
        entry.compiler = is_msvc ? "cl.exe" : in.lang.compiler;
        // Store the effective compile opts used for this file
        if (is_msvc) {
            // Store MSVC-translated flags
            auto flag_trans = toolchain::translate_compile_flags(
                in.compile.flags, toolchain::CompilerFamily::Msvc);
            entry.compile_opts = flag_trans.translated;
            for (auto& f : in.compile.msvc_flags) {
                entry.compile_opts.push_back(f);
            }
        } else {
            entry.compile_opts = in.compile.flags;
        }
        entry.dependencies = std::move(new_deps);
        entry.last_build_time = iso_time();
    }

    return result;
}

} // namespace ezmk::cache
