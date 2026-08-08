#include "ezmk/compile_db.hpp"
#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/toolchain.hpp"
#include "ezmk/util.hpp"
#include "ezmk/i18n.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>

namespace ezmk::compile_db {
namespace fs = std::filesystem;

namespace {

// clangd doesn't need build-only flags: GCC dependency (.d) output, MSVC
// /showIncludes listing, and the deterministic random-seed. Path-bearing args
// are relativized against the project root so the index is portable.
std::vector<std::string> normalize_for_index(std::vector<std::string> args,
                                             const fs::path& proj_root,
                                             const fs::path& rel_src) {
    std::vector<std::string> out;
    out.reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "-MMD") continue;                        // gcc: dep output flag
        if (a == "/showIncludes") continue;               // msvc: include listing
        if (a == "-MF") { ++i; continue; }                // gcc: dep output path
        if (a.rfind("-frandom-seed=", 0) == 0) continue;  // determinism
        if (a == "-c" && i + 1 < args.size()) {           // source (relativized)
            out.push_back(a);
            out.push_back(rel_src.generic_string());
            ++i;
            continue;
        }
        if (a == "-o" && i + 1 < args.size()) {           // object (relativized)
            out.push_back(a);
            fs::path o(args[++i]);
            if (o.is_absolute()) {
                auto r = fs::relative(o, proj_root);
                if (!r.empty() && r.string().find("..") == std::string::npos) {
                    out.push_back(r.generic_string());
                } else {
                    out.push_back(o.generic_string());
                }
            } else {
                out.push_back(o.generic_string());
            }
            continue;
        }
        if (a.rfind("/Fo", 0) == 0 && a.size() > 3) {     // msvc: /Fo<path>
            fs::path o(a.substr(3));
            if (o.is_absolute()) {
                auto r = fs::relative(o, proj_root);
                if (!r.empty() && r.string().find("..") == std::string::npos) {
                    out.push_back("/Fo" + r.generic_string());
                } else {
                    out.push_back("/Fo" + o.generic_string());
                }
            } else {
                out.push_back(a);
            }
            continue;
        }
        out.push_back(a);
    }
    return out;
}

} // namespace

void generate_compile_db(const config::EzConfig& cfg,
                         const cli::BuildOptions& opts,
                         const fs::path& project_root,
                         const fs::path& output_path) {
    auto cin = build::prepare_compile_input(cfg, opts);
    generate_compile_db(cin, project_root, output_path);
}

void generate_compile_db(const cache::CompileInput& cin,
                         const fs::path& project_root,
                         const fs::path& output_path) {
    fs::path root = project_root.empty() ? cin.proj_root : project_root;
    fs::path out = output_path.empty() ? root / "compile_commands.json" : output_path;

    if (cin.sources.empty()) {
        util::warn(ezmk::i18n::I18nKey::compile_db_no_sources);
        return;
    }

    // One entry per source, sorted by relative path for stable output.
    struct Entry {
        std::string file;
        std::string directory;
        std::vector<std::string> arguments;
    };
    std::vector<Entry> entries;
    entries.reserve(cin.sources.size());

    bool is_msvc = (cin.tc.family == toolchain::CompilerFamily::Msvc);
    for (auto& src : cin.sources) {
        auto rel = fs::relative(src, root);
        // Logical object path (build/<rel>.<ext>) — the index is read-only, so
        // a stable relative -o keeps the JSON clean without touching the build.
        fs::path obj = root / "build" / rel;
        obj.replace_extension(is_msvc ? ".obj" : ".o");
        auto args = normalize_for_index(cache::build_compile_args(cin, src, obj),
                                        root, rel);
        entries.push_back({rel.generic_string(), root.string(), std::move(args)});
    }

    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.file < b.file; });

    nlohmann::json arr = nlohmann::json::array();
    for (auto& e : entries) {
        arr.push_back({
            {"directory", e.directory},
            {"arguments", e.arguments},
            {"file", e.file},
        });
    }
    std::string json = arr.dump(2) + "\n";

    // Atomic write (temp → rename), mirroring cache::save_record().
    auto tmp = out;
    tmp += ".tmp";
    util::file_write(tmp, json);
    std::error_code ec;
    fs::rename(tmp, out, ec);
    if (ec) {
        util::file_write(out, json);
    }
}

} // namespace ezmk::compile_db
