#include "ezmk/lockfile.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/pkg.hpp"
#include "ezmk/util.hpp"
#include "toml.hpp"
#include "nlohmann_json.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ezmk::lockfile {

// ===================================================================
// Lockfile paths (1.4.5: JSON is the written format; TOML is read-only legacy)
// ===================================================================

fs::path lockfile_path(const fs::path& proj_root) {
    return proj_root / "ezmk.lock.json";
}

fs::path legacy_lockfile_path(const fs::path& proj_root) {
    return proj_root / "ezmk.lock";   // pre-1.4.5 TOML — read-only
}

fs::path active_path(const fs::path& proj_root) {
    auto json_path = lockfile_path(proj_root);
    if (util::file_exists(json_path)) return json_path;
    auto legacy = legacy_lockfile_path(proj_root);
    if (util::file_exists(legacy)) return legacy;
    return json_path;   // nothing on disk yet → the write target
}

// ===================================================================
// Load / Save
// ===================================================================

// 1.4.5: pre-1.4.5 TOML reader — kept as the migration path for projects whose
// lockfile was written by ezmk <= 1.4.4. Removed in 2.0.0 (REMOVALS R-03).
static std::optional<config::Lockfile> parse_toml(const fs::path& path) {
    try {
        auto root = toml::parse_file(path.string());
        config::Lockfile lf;

        // [metadata]
        auto meta = root["metadata"].as_table();
        if (meta) {
            lf.version = (*meta)["version"].value_or(1);
            lf.generated_by = (*meta)["generated_by"].value_or("");
            lf.generated_at = (*meta)["generated_at"].value_or("");
            lf.toolchain = (*meta)["toolchain"].value_or("");
            lf.toolchain_version = (*meta)["toolchain_version"].value_or("");
            // 1.1.2 C3: direct deps (absent in pre-1.1.2 lockfiles → empty)
            auto dd = (*meta)["direct_deps"].as_array();
            if (dd) {
                for (size_t i = 0; i < dd->size(); ++i) {
                    if (auto v = (*dd)[i].value<std::string>()) {
                        lf.direct_deps.push_back(*v);
                    }
                }
            }
        }

        // [[packages]]
        auto pkgs = root["packages"].as_array();
        if (pkgs) {
            for (size_t i = 0; i < pkgs->size(); ++i) {
                auto tbl = (*pkgs)[i].as_table();
                if (!tbl) continue;

                config::LockedPackage pkg;
                pkg.name = (*tbl)["name"].value_or("");
                pkg.version = (*tbl)["version"].value_or("");
                pkg.source = (*tbl)["source"].value_or("");
                pkg.source_url = (*tbl)["source_url"].value_or("");
                pkg.sha256 = (*tbl)["sha256"].value_or("");
                // 1.4.2 F-04: explicit hash fields. A pre-1.4.2 lockfile has only
                // `sha256` (the artifact hash) → it is the legacy alias of
                // lib_sha256; archive_sha256 stays empty (nothing to verify the
                // install archive against).
                pkg.archive_sha256 = (*tbl)["archive_sha256"].value_or("");
                pkg.lib_sha256 = (*tbl)["lib_sha256"].value_or("");
                // 1.4.1: optional commit pin for git sources — absent in old
                // lockfiles → empty string, normalizes fine.
                pkg.commit = (*tbl)["commit"].value_or("");
                pkg.type = (*tbl)["type"].value_or("static");
                pkg.scope = (*tbl)["scope"].value_or("user");
                pkg.platform = (*tbl)["platform"].value_or("");

                auto deps = (*tbl)["dependencies"].as_array();
                if (deps) {
                    for (size_t j = 0; j < deps->size(); ++j) {
                        if (auto v = (*deps)[j].value<std::string>()) {
                            pkg.dependencies.push_back(*v);
                        }
                    }
                }

                if (!pkg.name.empty()) {
                    lf.packages.push_back(std::move(pkg));
                }
            }
        }

        return lf;
    } catch (const std::exception& e) {
        util::warn(std::string("failed to parse ezmk.lock: ") + e.what());
        return std::nullopt;
    }
}

// 1.4.5: JSON reader — the format ezmk writes from 1.4.5 on. Field-by-field
// identical to parse_toml (including the default values), so a lockfile is
// format-independent once loaded; test_lockfile.cpp pins that equivalence.
static std::optional<config::Lockfile> parse_json(const fs::path& path) {
    try {
        auto j = nlohmann::json::parse(util::file_read(path));
        if (!j.is_object()) {
            util::warn(std::string("failed to parse ") + path.filename().string() +
                       ": expected a JSON object");
            return std::nullopt;
        }

        config::Lockfile lf;

        // metadata
        if (auto meta = j.find("metadata"); meta != j.end() && meta->is_object()) {
            lf.version = meta->value("version", 1);
            lf.generated_by = meta->value("generated_by", "");
            lf.generated_at = meta->value("generated_at", "");
            lf.toolchain = meta->value("toolchain", "");
            lf.toolchain_version = meta->value("toolchain_version", "");
            if (auto dd = meta->find("direct_deps"); dd != meta->end() && dd->is_array()) {
                for (auto& v : *dd) {
                    if (v.is_string()) lf.direct_deps.push_back(v.get<std::string>());
                }
            }
        }

        // packages
        if (auto pkgs = j.find("packages"); pkgs != j.end() && pkgs->is_array()) {
            for (auto& tbl : *pkgs) {
                if (!tbl.is_object()) continue;

                config::LockedPackage pkg;
                pkg.name = tbl.value("name", "");
                pkg.version = tbl.value("version", "");
                pkg.source = tbl.value("source", "");
                pkg.source_url = tbl.value("source_url", "");
                // 1.4.2 F-04: a pre-1.4.2 lockfile has only `sha256` (the artifact
                // hash) → it is the legacy alias of lib_sha256; archive_sha256
                // stays empty (nothing to verify the install archive against).
                pkg.sha256 = tbl.value("sha256", "");
                pkg.archive_sha256 = tbl.value("archive_sha256", "");
                pkg.lib_sha256 = tbl.value("lib_sha256", "");
                // 1.4.1: optional commit pin for git sources — absent in old
                // lockfiles → empty string, normalizes fine.
                pkg.commit = tbl.value("commit", "");
                pkg.type = tbl.value("type", "static");
                pkg.scope = tbl.value("scope", "user");
                pkg.platform = tbl.value("platform", "");

                if (auto deps = tbl.find("dependencies");
                    deps != tbl.end() && deps->is_array()) {
                    for (auto& v : *deps) {
                        if (v.is_string()) pkg.dependencies.push_back(v.get<std::string>());
                    }
                }

                if (!pkg.name.empty()) {
                    lf.packages.push_back(std::move(pkg));
                }
            }
        }

        return lf;
    } catch (const std::exception& e) {
        util::warn(std::string("failed to parse ") + path.filename().string() +
                   ": " + e.what());
        return std::nullopt;
    }
}

std::optional<config::Lockfile> load(const fs::path& proj_root) {
    auto json_path = lockfile_path(proj_root);
    auto legacy = legacy_lockfile_path(proj_root);

    if (util::file_exists(json_path)) {
        // Both files on disk: the JSON one wins, the TOML one is stale (it is
        // cleaned up by the next save()). Never merge, never pick silently.
        if (util::file_exists(legacy)) {
            util::warn(ezmk::i18n::I18nKey::lock_legacy_stale);
        }
        return parse_json(json_path);
    }

    if (util::file_exists(legacy)) {
        // Pre-1.4.5 lockfile: read it (so upgrades are transparent) and tell the
        // user it will be rewritten as ezmk.lock.json by the next write.
        util::info(ezmk::i18n::I18nKey::lock_legacy_detected);
        return parse_toml(legacy);
    }

    return std::nullopt;
}

void save(const fs::path& proj_root, const config::Lockfile& lf) {
    auto path = lockfile_path(proj_root);

    // 1.4.5: JSON writer (was hand-rolled TOML string concatenation). The shape
    // mirrors the pre-1.4.5 TOML one-for-one (metadata object + packages array,
    // identical key names) so the migration can be verified field-by-field, and
    // escaping is the library's job instead of a per-interpolation discipline.
    // ordered_json (not json): the emitted field order is the insertion order, so
    // a lockfile committed to git diffs in a readable, stable order and matches
    // the documented example (nlohmann::json would sort keys alphabetically and
    // bury `name` in the middle of every package).
    nlohmann::ordered_json j;
    auto& meta = j["metadata"] = nlohmann::ordered_json::object();
    meta["version"] = lf.version;
    meta["generated_by"] = lf.generated_by;
    meta["generated_at"] = lf.generated_at;
    meta["toolchain"] = lf.toolchain;
    meta["toolchain_version"] = lf.toolchain_version;
    // 1.1.2 C3: root project's direct deps (name or name@spec), sorted
    auto& direct = meta["direct_deps"] = nlohmann::ordered_json::array();
    for (auto& d : lf.direct_deps) direct.push_back(d);

    auto& pkgs = j["packages"] = nlohmann::ordered_json::array();
    for (auto& pkg : lf.packages) {
        nlohmann::ordered_json p = nlohmann::ordered_json::object();
        p["name"] = pkg.name;
        p["version"] = pkg.version;
        p["source"] = pkg.source;
        p["source_url"] = pkg.source_url;
        p["sha256"] = pkg.sha256;
        // 1.4.2 F-04: the artifact hash (verify) and the install-source archive
        // hash (--locked reinstall) are distinct values. `sha256` is kept as the
        // legacy alias of lib_sha256 so pre-1.4.2 readers keep working; both new
        // fields are written only when known — mirroring the pre-1.4.5 TOML
        // writer, whose "absent" and "unknown" were the same thing (readers
        // default to "").
        if (!pkg.lib_sha256.empty()) p["lib_sha256"] = pkg.lib_sha256;
        if (!pkg.archive_sha256.empty()) p["archive_sha256"] = pkg.archive_sha256;
        // 1.4.1: git-source commit pin — only written when non-empty.
        if (!pkg.commit.empty()) p["commit"] = pkg.commit;
        p["type"] = pkg.type;
        p["scope"] = pkg.scope;
        p["platform"] = pkg.platform;
        auto& deps = p["dependencies"] = nlohmann::ordered_json::array();
        for (auto& d : pkg.dependencies) deps.push_back(d);
        pkgs.push_back(std::move(p));
    }

    if (!util::atomic_write_text(path, j.dump(2) + "\n")) return;

    // Migration: a legacy ezmk.lock was only ever read to keep existing projects
    // working. Now that ezmk.lock.json is on disk it must not linger — two copies
    // of the truth is exactly the drift this change removes. Best-effort: if the
    // removal fails the next load() warns about the stale file instead.
    auto legacy = legacy_lockfile_path(proj_root);
    if (util::file_exists(legacy)) {
        std::error_code ec;
        fs::remove(legacy, ec);
        if (ec) {
            util::warn(std::string("could not remove the legacy lockfile: ") +
                       legacy.filename().string() + " (" + ec.message() + ")");
        } else {
            util::info(ezmk::i18n::I18nKey::lock_migrated);
        }
    }
}

// 1.4.2 F-28: see header. Sorted (relative path, content hash) pairs → one hash.
std::string payload_manifest_hash(const fs::path& pkg_dir) {
    fs::path include_dir = pkg_dir / "include";
    std::error_code ec;
    if (!fs::exists(include_dir, ec)) return {};

    std::vector<std::pair<std::string, fs::path>> entries;
    for (auto it = fs::recursive_directory_iterator(include_dir, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::error_code rec;
        fs::path rel = fs::relative(it->path(), pkg_dir, rec);
        std::string name = rec ? it->path().filename().string()
                               : rel.generic_string();
        entries.emplace_back(std::move(name), it->path());
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::string manifest;
    for (auto& [name, path] : entries) {
        manifest += name;
        manifest += '\n';
        manifest += crypto::sha256_file(path);
        manifest += '\n';
    }
    return crypto::sha256(manifest);
}

// ===================================================================
// Verify
// ===================================================================

// 1.4.2 F-04: the artifact hash of a lockfile entry — the explicit lib_sha256
// when present, otherwise the pre-1.4.2 `sha256` field (which always held the
// artifact hash).
std::string artifact_hash(const config::LockedPackage& pkg) {
    return !pkg.lib_sha256.empty() ? pkg.lib_sha256 : pkg.sha256;
}

// 1.4.2 F-04: the installation-source archive hash. Empty for git, directory
// and pre-1.4.2 entries — there is no archive to verify against.
std::string archive_hash(const config::LockedPackage& pkg) {
    return pkg.archive_sha256;
}

std::vector<std::string> verify(const fs::path& proj_root,
                                const config::Lockfile& lf) {
    std::vector<std::string> mismatches;

    for (auto& pkg : lf.packages) {
        // 1.1.3 S2: lockfile 不可信，包名先校验再拼路径（防路径穿越）
        util::validate_pkg_name(pkg.name);
        // Determine package install path based on scope
        fs::path pkg_path;
        if (pkg.scope == "project") {
            pkg_path = proj_root / ".ezmk/pkg" / pkg.name;
        } else if (pkg.scope == "user") {
#ifdef EZMK_WIN
            const char* appdata = std::getenv("LOCALAPPDATA");
            if (appdata) pkg_path = fs::path(appdata) / "ezmk/pkg" / pkg.name;
            else pkg_path = util::get_home_dir() / "AppData/Local/ezmk/pkg" / pkg.name;
#else
            pkg_path = util::get_home_dir() / ".local/ezmk/pkg" / pkg.name;
#endif
        } else { // global
            pkg_path = util::get_exe_dir() / "pkg" / pkg.name;
        }

        if (!util::file_exists(pkg_path)) {
            mismatches.push_back(pkg.name);
            continue;
        }

        // 1.4.2 F-28: header-only packages have no built archive — verify the
        // include/ payload against the pinned manifest hash. Legacy lockfiles
        // recorded no hash for them → keep the old "verified at install time"
        // behavior instead of failing every entry.
        if (pkg.type == "header-only") {
            const std::string recorded = artifact_hash(pkg);
            if (!recorded.empty()) {
                std::string actual = payload_manifest_hash(pkg_path);
                if (actual != recorded) mismatches.push_back(pkg.name);
            }
            continue;
        }

        // 1.4.2 F-28: git sources are pinned by commit, not by content hash —
        // verify the provenance marker written at install time.
        if (pkg.source == "git") {
            fs::path marker = pkg_path / ".ezmk-git-source";
            if (pkg.commit.empty() || !util::file_exists(marker)) {
                mismatches.push_back(pkg.name);
                continue;
            }
            std::string content = util::file_read(marker);
            auto nl = content.find('\n');
            std::string marker_commit =
                nl == std::string::npos ? std::string() : content.substr(nl + 1);
            while (!marker_commit.empty() &&
                   (marker_commit.back() == '\n' || marker_commit.back() == '\r')) {
                marker_commit.pop_back();
            }
            if (marker_commit != pkg.commit) mismatches.push_back(pkg.name);
            continue;
        }

        fs::path lib_file;
        auto build_dir = pkg_path / "build";
        // 1.2.0-dev.11: deterministic pick (shared with pkg.cpp record side) —
        // the previous "first directory entry" was non-deterministic.
        lib_file = util::find_package_archive(build_dir, pkg.name);

        const std::string recorded = artifact_hash(pkg);
        if (recorded.empty()) {
            // 1.4.2 F-28: nothing was pinned for this entry (pre-1.4.2 lockfile
            // or a source with no artifact) — warn instead of failing, so old
            // lockfiles keep working while new ones are actually verified.
            util::warn(std::string("lockfile entry '") + pkg.name +
                       "' has no content hash to verify — reinstall it to pin one");
            continue;
        }
        if (!lib_file.empty()) {
            std::string actual = crypto::sha256_file(lib_file);
            if (actual != recorded) {
                mismatches.push_back(pkg.name);
            }
        }
    }

    return mismatches;
}

// ===================================================================
// Depends changed detection
// ===================================================================

std::vector<std::string> direct_dep_specs(const config::EzConfig& cfg) {
    std::vector<std::string> specs;
    auto push = [&](const config::DependsEntry& d) {
        // user syntax: "pkg" | "pkg@1.2" | "pkg@^1.2" | "pkg@~1.2" | "pkg@>=1.2" ...
        std::string s = d.name;
        if (d.constraint.op != config::VersionConstraint::None) {
            s += "@";
            switch (d.constraint.op) {
                case config::VersionConstraint::Exact:     break;   // "name@1.2"
                case config::VersionConstraint::Compatible: s += "^"; break;
                case config::VersionConstraint::Approx:    s += "~";  break;
                case config::VersionConstraint::Gte:       s += ">="; break;
                case config::VersionConstraint::Gt:        s += ">";  break;
                default: break;
            }
            s += d.constraint.version;
        }
        specs.push_back(std::move(s));
    };
    for (auto& d : cfg.depends.libs) push(d);
    for (auto& d : cfg.depends.want) push(d);
    std::sort(specs.begin(), specs.end());
    return specs;
}

bool depends_changed(const config::EzConfig& cfg,
                     const config::Lockfile& lf) {
    // 1.1.2 C3: compare DIRECT deps (with constraints), not cfg deps vs ALL
    // lockfile packages. packages[] includes transitive/auto-installed deps, so
    // the old name-set comparison was always "changed" whenever there was any
    // transitive dep — making --locked always fatal even when nothing changed.
    // NOTE: bind the vector — the set ctor must use one object's begin/end pair
    // (two separate temporaries would be undefined behavior).
    auto cfg_vec = direct_dep_specs(cfg);
    std::set<std::string> cfg_deps(cfg_vec.begin(), cfg_vec.end());
    std::set<std::string> lock_deps(lf.direct_deps.begin(), lf.direct_deps.end());

    if (cfg_deps != lock_deps) return true;

    // Backward compat: a pre-1.1.2 lockfile has no direct_deps field. If the
    // project declares deps, treat as changed so the lockfile regenerates.
    if (lf.direct_deps.empty() && !lf.packages.empty() && !cfg_deps.empty()) return true;

    return false;
}

} // namespace ezmk::lockfile
