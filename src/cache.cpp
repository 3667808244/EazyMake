#include "ezmk/cache.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ezmk::cache {

// ===================================================================
// Helpers: minimal JSON read/write for record.json
// ===================================================================

static std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::string iso_time() {
    auto t = std::time(nullptr);
    auto* tm = std::localtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return buf;
}

// Very minimal JSON writer — only writes the exact structure we need.
static std::string record_to_json(const CacheRecord& rec) {
    std::ostringstream j;
    j << "{\n";
    j << "  \"version\": " << rec.version << ",\n";
    j << "  \"compile_options_signature\": \"" << json_escape(rec.compile_options_signature) << "\",\n";
    j << "  \"files\": {\n";

    bool first_file = true;
    for (auto& [key, fe] : rec.files) {
        if (!first_file) j << ",\n";
        first_file = false;

        j << "    \"" << json_escape(key) << "\": {\n";
        j << "      \"source_hash\": \"" << fe.source_hash << "\",\n";
        j << "      \"object_file\": \"" << json_escape(fe.object_file) << "\",\n";
        j << "      \"compiler\": \"" << json_escape(fe.compiler) << "\",\n";
        j << "      \"compile_opts\": [";
        for (size_t i = 0; i < fe.compile_opts.size(); ++i) {
            if (i) j << ", ";
            j << "\"" << json_escape(fe.compile_opts[i]) << "\"";
        }
        j << "],\n";
        j << "      \"dependencies\": [";
        for (size_t i = 0; i < fe.dependencies.size(); ++i) {
            if (i) j << ", ";
            j << "{\"path\": \"" << json_escape(fe.dependencies[i].path)
              << "\", \"hash\": \"" << fe.dependencies[i].hash << "\"}";
        }
        j << "],\n";
        j << "      \"last_build_time\": \"" << fe.last_build_time << "\"\n";
        j << "    }";
    }

    j << "\n  }\n";
    j << "}\n";
    return j.str();
}

// Minimal JSON parser — only handles the exact structure of record.json.
static void json_skip_ws(std::string_view& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'
                          || s.front() == '\n' || s.front() == '\r'))
        s.remove_prefix(1);
}

static std::string_view json_read_string(std::string_view& s) {
    json_skip_ws(s);
    if (s.empty() || s.front() != '"') throw std::runtime_error("expected '\"' in JSON");
    s.remove_prefix(1);
    size_t end = 0;
    bool escape = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (escape) { escape = false; continue; }
        if (s[i] == '\\') { escape = true; continue; }
        if (s[i] == '"') { end = i; break; }
    }
    auto val = s.substr(0, end);
    s.remove_prefix(end + 1);
    return val;
}

static void json_expect(std::string_view& s, char c) {
    json_skip_ws(s);
    if (s.empty() || s.front() != c)
        throw std::runtime_error(std::string("expected '") + c + "' in JSON");
    s.remove_prefix(1);
}

static void json_skip_value(std::string_view& s) {
    json_skip_ws(s);
    if (s.empty()) return;
    char c = s.front();
    if (c == '"') {
        json_read_string(s);
    } else if (c == '{') {
        s.remove_prefix(1);
        int depth = 1;
        while (depth > 0 && !s.empty()) {
            if (s.front() == '"') { json_read_string(s); continue; }
            if (s.front() == '{') ++depth;
            if (s.front() == '}') --depth;
            s.remove_prefix(1);
        }
    } else if (c == '[') {
        s.remove_prefix(1);
        int depth = 1;
        while (depth > 0 && !s.empty()) {
            if (s.front() == '"') { json_read_string(s); continue; }
            if (s.front() == '[') ++depth;
            if (s.front() == ']') --depth;
            s.remove_prefix(1);
        }
    } else {
        // number, bool, null — skip to next structural char
        while (!s.empty() && s.front() != ',' && s.front() != '}' && s.front() != ']')
            s.remove_prefix(1);
    }
}

static CacheRecord json_to_record(std::string_view json) {
    CacheRecord rec;
    json_skip_ws(json);
    json_expect(json, '{');

    while (true) {
        json_skip_ws(json);
        if (json.empty()) break;
        if (json.front() == '}') { json.remove_prefix(1); break; }
        if (json.front() == ',') { json.remove_prefix(1); continue; }

        auto key = std::string(json_read_string(json));
        json_expect(json, ':');

        if (key == "version") {
            json_skip_ws(json);
            size_t n = 0;
            while (!json.empty() && json.front() >= '0' && json.front() <= '9') {
                n = n * 10 + (json.front() - '0');
                json.remove_prefix(1);
            }
            rec.version = static_cast<int>(n);
        } else if (key == "compile_options_signature") {
            rec.compile_options_signature = std::string(json_read_string(json));
        } else if (key == "files") {
            json_skip_ws(json);
            json_expect(json, '{');
            while (true) {
                json_skip_ws(json);
                if (json.empty()) break;
                if (json.front() == '}') { json.remove_prefix(1); break; }
                if (json.front() == ',') { json.remove_prefix(1); continue; }

                auto fname = std::string(json_read_string(json));
                json_expect(json, ':');
                json_skip_ws(json);
                json_expect(json, '{');

                FileEntry fe;
                while (true) {
                    json_skip_ws(json);
                    if (json.empty()) break;
                    if (json.front() == '}') { json.remove_prefix(1); break; }
                    if (json.front() == ',') { json.remove_prefix(1); continue; }

                    auto fkey = std::string(json_read_string(json));
                    json_expect(json, ':');

                    if (fkey == "source_hash") {
                        fe.source_hash = std::string(json_read_string(json));
                    } else if (fkey == "object_file") {
                        fe.object_file = std::string(json_read_string(json));
                    } else if (fkey == "compiler") {
                        fe.compiler = std::string(json_read_string(json));
                    } else if (fkey == "compile_opts") {
                        json_expect(json, '[');
                        while (true) {
                            json_skip_ws(json);
                            if (json.empty()) break;
                            if (json.front() == ']') { json.remove_prefix(1); break; }
                            if (json.front() == ',') { json.remove_prefix(1); continue; }
                            fe.compile_opts.push_back(std::string(json_read_string(json)));
                        }
                    } else if (fkey == "dependencies") {
                        json_expect(json, '[');
                        while (true) {
                            json_skip_ws(json);
                            if (json.empty()) break;
                            if (json.front() == ']') { json.remove_prefix(1); break; }
                            if (json.front() == ',') { json.remove_prefix(1); continue; }
                            json_expect(json, '{');
                            DepEntry dep;
                            while (true) {
                                json_skip_ws(json);
                                if (json.empty()) break;
                                if (json.front() == '}') { json.remove_prefix(1); break; }
                                if (json.front() == ',') { json.remove_prefix(1); continue; }
                                auto dkey = std::string(json_read_string(json));
                                json_expect(json, ':');
                                if (dkey == "path") dep.path = std::string(json_read_string(json));
                                else if (dkey == "hash") dep.hash = std::string(json_read_string(json));
                                else json_skip_value(json);
                            }
                            fe.dependencies.push_back(std::move(dep));
                        }
                    } else if (fkey == "last_build_time") {
                        fe.last_build_time = std::string(json_read_string(json));
                    } else {
                        json_skip_value(json);
                    }
                }
                rec.files[fname] = std::move(fe);
            }
        } else {
            json_skip_value(json);
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
        dep.hash = util::sha256_file(fs::path(tok));
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
    return util::sha256(combined);
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
    std::string cur_hash = util::sha256_file(src_file);
    if (cur_hash != entry.source_hash) return std::nullopt;

    // 2. Compile options signature
    auto cur_sig = compile_options_signature(compile);
    if (cur_sig != record.compile_options_signature) return std::nullopt;

    // 3. Headers: re-hash each stored header path with current content
    for (auto& dep : entry.dependencies) {
        fs::path dep_path(dep.path);
        if (dep_path.is_relative()) dep_path = proj_root / dep_path;
        std::string cur_hdr_hash = util::sha256_file(dep_path);
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
