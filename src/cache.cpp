#include "ezmk/cache.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/util.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <stdexcept>

namespace ezmk::cache {

// ===================================================================
// Helpers
// ===================================================================

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

    // Normalize CRLF → LF
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\r') content[i] = '\n';
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
        if (tok.back() == ':') continue;
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
    return compile_options_signature(compile, {});
}

std::string compile_options_signature(const config::CompileSection& compile,
                                      const std::vector<fs::path>& extra_includes) {
    std::string combined;
    for (auto& f : compile.flags) {
        combined += f;
        combined += ' ';
    }
    for (auto& d : compile.include_dirs) {
        combined += "-I";
        combined += d;
        combined += ' ';
    }
    for (auto& inc : extra_includes) {
        combined += "-I";
        combined += inc.string();
        combined += ' ';
    }
    return crypto::sha256(combined);
}

std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record) {
    return check_cache(src_file, compile, record, fs::current_path());
}

std::optional<fs::path> check_cache(const fs::path& src_file,
                                    const config::CompileSection& compile,
                                    const CacheRecord& record,
                                    const fs::path& proj_root) {
    auto rel_src = fs::relative(src_file, proj_root).generic_string();

    auto it = record.files.find(rel_src);
    if (it == record.files.end()) return std::nullopt;

    auto& entry = it->second;

    // 1. Source hash
    std::string cur_hash = crypto::sha256_file(src_file);
    if (cur_hash != entry.source_hash) return std::nullopt;

    // 2. Compile options signature
    auto cur_sig = compile_options_signature(compile);
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

} // namespace ezmk::cache
