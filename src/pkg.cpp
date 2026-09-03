#include "ezmk/pkg.hpp"
#include "ezmk/build.hpp"
#include "ezmk/cache.hpp"
#include "ezmk/config.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/lockfile.hpp"
#include "ezmk/lua_api.hpp"
#include "ezmk/repo.hpp"
#include "ezmk/util.hpp"
#include "ezmk/version.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ezmk::pkg {

// 1.2.0-dev.11: packages currently being auto-installed in the dependency
// walk (recursion guard). A⇄B mutual hard dependencies used to recurse
// unboundedly — each recursive install re-walks the other's deps before the
// first install lands, so neither is ever found installed.
static std::set<std::string> g_auto_installing;

// RAII: mark a package as being auto-installed; removes on scope exit.
struct AutoInstallGuard {
    const std::string& name;
    explicit AutoInstallGuard(const std::string& n) : name(n) {
        g_auto_installing.insert(name);
    }
    ~AutoInstallGuard() { g_auto_installing.erase(name); }
};

// 0.9.6+ — Check if a package version satisfies a version constraint.
// Returns true if `version` satisfies `constraint`.
bool satisfies_version_constraint(std::string_view version,
                                  const config::VersionConstraint& constraint) {
    if (constraint.op == config::VersionConstraint::None)
        return true;

    int cmp = util::compare_version(version, constraint.version);

    switch (constraint.op) {
    case config::VersionConstraint::Exact:
        return cmp == 0;
    case config::VersionConstraint::Gte:
        return cmp >= 0;
    case config::VersionConstraint::Gt:
        return cmp > 0;
    case config::VersionConstraint::Compatible: {
        // ^X.Y.Z → >= X.Y.Z, < (X+1).0.0
        if (cmp < 0) return false;
        // Parse the major version of the constraint and bump it
        auto dot = constraint.version.find('.');
        unsigned long major = dot == std::string::npos
            ? std::stoul(std::string(constraint.version))
            : std::stoul(std::string(constraint.version.substr(0, dot)));
        std::string next_major = std::to_string(major + 1) + ".0.0";
        return util::compare_version(version, next_major) < 0;
    }
    case config::VersionConstraint::Approx: {
        // ~X.Y.Z → >= X.Y.Z, < X.(Y+1).0
        if (cmp < 0) return false;
        // Parse up to the minor version and bump it
        auto dot1 = constraint.version.find('.');
        if (dot1 == std::string::npos) return cmp >= 0; // ~X → >= X
        auto dot2 = constraint.version.find('.', dot1 + 1);
        unsigned long major = std::stoul(std::string(constraint.version.substr(0, dot1)));
        unsigned long minor = std::stoul(
            std::string(constraint.version.substr(dot1 + 1, dot2 - dot1 - 1)));
        std::string next_minor = std::to_string(major) + "." +
                                 std::to_string(minor + 1) + ".0";
        return util::compare_version(version, next_minor) < 0;
    }
    default:
        return true;
    }
}

// 1.4.1: strict git-source detection for `pkg install` (see pkg.hpp).
bool is_git_install_source(std::string_view s) {
    // Loose gate first (repo semantics): local filesystem paths are excluded
    // here (drive letters / absolute / rooted paths → not a URL).
    if (!util::is_git_url(s)) return false;

    // Strip the "#<ref>" fragment before the ".git" suffix test — the fragment
    // is a git ref selector, never part of the clone URL.
    std::string base(s);
    auto hash = base.rfind('#');
    if (hash != std::string::npos) base = base.substr(0, hash);

    // SSH scp-style: git@github.com:user/repo.git
    if (base.rfind("git@", 0) == 0) return true;
    // Explicit git protocols
    auto lower = [](std::string_view v) {
        std::string r(v);
        for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return r;
    };
    std::string lbase = lower(base);
    if (lbase.rfind("git://", 0) == 0 || lbase.rfind("file://", 0) == 0) return true;
    // Any URL whose path ends ".git" (after fragment strip): https://…/repo.git
    if (base.size() >= 4 && lbase.substr(lbase.size() - 4) == ".git") return true;
    return false;
}

// ===================================================================
// Path resolution
// ===================================================================

fs::path pkg_install_dir(cli::Scope scope) {
    switch (scope) {
    case cli::Scope::Project: {
        // 1.2.0-dev.7: project scope lives under the located project root
        // (upward search); falls back to CWD when no ezmk.toml is found.
        auto root = util::locate_project_root(fs::current_path());
        return (root.value_or(fs::current_path())) / ".ezmk/pkg";
    }
    case cli::Scope::User: {
#ifdef EZMK_WIN
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (appdata) return fs::path(appdata) / "ezmk/pkg";
        return util::get_home_dir() / "AppData/Local/ezmk/pkg";
#else
        return util::get_home_dir() / ".local/ezmk/pkg";
#endif
    }
    case cli::Scope::Global:
        return util::get_exe_dir() / "pkg";
    }
    return {};
}

std::vector<fs::path> pkg_search_dirs(const std::vector<cli::Scope>& scopes) {
    std::vector<fs::path> dirs;
    for (auto s : scopes) {
        dirs.push_back(pkg_install_dir(s));
    }
    return dirs;
}

// ===================================================================
// Helpers
// ===================================================================

static std::string scope_to_string(cli::Scope scope) {
    return (scope == cli::Scope::Global) ? "global"
         : (scope == cli::Scope::User)   ? "user"
         :                                 "project";
}

// ===================================================================
// 1.3.1: install-time language-standard compatibility check.
// Semantics (design A): [project].language may declare a RANGE whose minimum
// is the effective compile standard. A package requiring a higher minimum
// than the consumer project compiles at risks failing to compile / link
// (ABI mismatch for precompiled archives). We WARN (never fail) — a strict
// error would break the existing package ecosystem (坑 4; strict switch is
// deferred to 1.4.0).
// ===================================================================

// The minimum standard a language declaration requires (parse_language's
// min_ver — an exact value fills min_ver too). 0 = unparseable/undeclared →
// treated as "no bound" (skip the check).
static int std_min_of(const std::string& language) {
    if (language.empty()) return 0;
    try {
        return config::parse_language(language).min_ver;
    } catch (...) {
        return 0;
    }
}

// Consumer context for the std-compat check: the project's minimum standard,
// its raw declared language (for messages) and the [pkg] strict_std_check
// switch (1.4.0-dev.2). nullopt min → no consumer ezmk.toml (global/user scope
// installs) or an unparseable consumer config → skip the check (with a
// warning for a malformed consumer language).
struct ConsumerStdContext {
    std::optional<int> min;   // consumer minimum standard (nullopt = no check)
    bool strict = false;      // [pkg] strict_std_check — warn → fatal
    std::string language;     // consumer's declared [project].language (messages)
};

static ConsumerStdContext consumer_std_ctx() {
    // 1.3.6: dedupe the malformed-consumer warning per process — a broken
    // consumer config used to warn once per compiled package (multi-package
    // installs flooded the console). First occurrence still warns.
    static bool g_warned_consumer_config = false;
    auto warn_once = [](const std::string& msg) {
        if (!g_warned_consumer_config) {
            g_warned_consumer_config = true;
            util::warn(msg);
        }
    };
    try {
        auto root = util::locate_project_root(fs::current_path());
        if (!root) return {};
        auto cfg = config::parse_config(*root / "ezmk.toml");
        ConsumerStdContext ctx;
        ctx.strict = cfg.pkg.strict_std_check;
        ctx.language = cfg.project.language;
        if (cfg.project.language.empty()) return ctx;  // min stays nullopt
        int min = std_min_of(cfg.project.language);
        if (min == 0) {
            warn_once("cannot check package language compatibility — consumer "
                      "[project].language is invalid: " + cfg.project.language);
            return {};
        }
        ctx.min = min;
        return ctx;
    } catch (const std::exception& e) {
        warn_once("cannot check package language compatibility: " + std::string(e.what()));
        return {};
    }
}

// Human-readable minimum standard for warning text: "C++17" / "C11" /
// "GNUC++11" from a language declaration (falls back to the raw string).
static std::string std_label(const std::string& language) {
    try {
        std::string s = config::parse_language(language).normalized_lang;
        size_t p = s.find("CPP");
        if (p != std::string::npos) s.replace(p, 3, "C++");
        return s;
    } catch (...) {
        return language;
    }
}

// Warn (default) or fatal ([pkg] strict_std_check, 1.4.0-dev.2) when the
// package's minimum standard exceeds the consumer's. precompiled=true uses the
// ABI-flavored warning wording (预编译 ABI 风险更高); the strict fatal uses a
// single escalated wording for both paths.
// 1.4.0-dev.3: the consumer context is resolved ONCE by the caller (shared with
// compile negotiation) and passed in — no second parse of the consumer config.
static void check_std_compat(const std::string& pkg_name,
                             const std::string& pkg_language,
                             bool precompiled,
                             const ConsumerStdContext& ctx) {
    int pkg_min = std_min_of(pkg_language);
    if (pkg_min == 0) return;                 // package declares no usable bound
    if (!ctx.min || pkg_min <= *ctx.min) return;  // compatible

    const std::string consumer_std =
        ctx.language.empty() ? std::to_string(*ctx.min) : std_label(ctx.language);
    const auto args = std::map<std::string, std::string>{
        {"pkg", pkg_name},
        {"pkg_std", std_label(pkg_language)},
        {"consumer_std", consumer_std},
    };

    if (ctx.strict) {
        util::fatal(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_fatal_std_mismatch,
                                    args));
    }
    util::warn(ezmk::i18n::fmt(
        precompiled ? ezmk::i18n::I18nKey::pkg_warn_std_mismatch_precompiled
                    : ezmk::i18n::I18nKey::pkg_warn_std_mismatch,
        args));
}

// 1.4.0-dev.3: 编译协商（语义 B）——见 pkg.hpp 注释。公式：
//   effective = min( max(包min, 消费者min), 能力表, 包max )
// 无消费者 / 消费者能力不足 / 任一 cap 把结果拉回包 min 以下 → 不协商。
config::LanguageInfo negotiate_package_std(const config::LanguageInfo& pkg_lang,
                                           std::optional<int> consumer_min,
                                           const toolchain::Toolchain& tc) {
    if (pkg_lang.min_ver == 0) return pkg_lang;          // 包声明不可用
    if (!consumer_min || *consumer_min <= pkg_lang.min_ver) {
        return pkg_lang;                                 // 无消费者 / 消费者更弱
    }

    int eff = *consumer_min;
    // cap 1: 工具链能力表（dev.2）——"CPP<n>" 取 <n>；解析失败不 cap（保守下限
    // 本应可解析，防御性兜底）。
    const std::string cap_str =
        toolchain::max_supported_std(tc.family, tc.version);
    if (cap_str.size() > 3) {
        try {
            int cap = std::stoi(cap_str.substr(3));
            if (cap > 0 && eff > cap) eff = cap;
        } catch (...) {}
    }
    // cap 2: 包声明区间上界（元数据承诺——超上界行为未验证，0 = 无上界）。
    if (pkg_lang.max_ver > 0 && eff > pkg_lang.max_ver) eff = pkg_lang.max_ver;
    if (eff <= pkg_lang.min_ver) return pkg_lang;        // cap 拉回包 min 以下

    // 套用协商值：std_flag / min_ver / normalized_lang 同步替换（与
    // parse_language 的构造逐字节一致，含 GNU 前缀）。
    config::LanguageInfo out = pkg_lang;
    out.min_ver = eff;
    const std::string v = std::to_string(eff);
    const bool is_cxx = (out.compiler == "g++");
    if (is_cxx) {
        out.std_flag = out.gnu_extensions ? ("-std=gnu++" + v) : ("-std=c++" + v);
        out.normalized_lang = (out.gnu_extensions ? "GNUCPP" : "CPP") + v;
    } else {
        out.std_flag = out.gnu_extensions ? ("-std=gnu" + v) : ("-std=c" + v);
        out.normalized_lang = (out.gnu_extensions ? "GNUC" : "C") + v;
    }
    return out;
}

static bool confirm(std::string_view msg, bool assume_yes = false) {
    if (assume_yes) {
        util::info(std::string(msg) + ezmk::i18n::get(ezmk::i18n::I18nKey::auto_yes));
        return true;
    }
    std::cerr << "[ezmk] " << msg << " [y/N] ";
    std::string line;
    std::getline(std::cin, line);
    return line == "y" || line == "Y" || line == "yes";
}

// 1.1.3 S3: URL 安装完整性前置确认（下载前调用）。
//  - 无 sha256：无法校验包完整性 → 警告 + 确认；
//  - 显式 http://：明文下载、易遭 MITM → 警告 + 确认（建议 https://）。
// 返回 false 表示用户取消，install 应中止且不发起下载。
bool url_integrity_confirm(const std::string& url, bool has_sha256, bool assume_yes) {
    if (!has_sha256) {
        util::warn(ezmk::i18n::get(ezmk::i18n::I18nKey::url_no_sha256_confirm));
        if (!confirm(ezmk::i18n::get(ezmk::i18n::I18nKey::url_no_sha256_confirm), assume_yes)) {
            return false;
        }
    }
    std::string lower_url = url;
    for (auto& c : lower_url) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower_url.rfind("http://", 0) == 0) {
        util::warn(ezmk::i18n::get(ezmk::i18n::I18nKey::url_http_confirm));
        if (!confirm(ezmk::i18n::get(ezmk::i18n::I18nKey::url_http_confirm), assume_yes)) {
            return false;
        }
    }
    return true;
}

fs::path detect_install_script(const fs::path& pkg_root,
                                std::string_view basename) {
    auto script_dir = pkg_root / "script";
    if (!util::file_exists(script_dir)) return {};

    // 0.9.9: .lua scripts take priority (cross-platform, sandbox-safe)
    fs::path lua = script_dir / (std::string(basename) + ".lua");
    if (util::file_exists(lua)) return lua;

    // Fallback: platform-specific scripts
#ifdef EZMK_WIN
    fs::path ps1 = script_dir / (std::string(basename) + ".ps1");
    if (util::file_exists(ps1)) return ps1;
    fs::path bat = script_dir / (std::string(basename) + ".bat");
    if (util::file_exists(bat)) return bat;
#else
    fs::path sh = script_dir / (std::string(basename) + ".sh");
    if (util::file_exists(sh)) return sh;
#endif
    return {};
}

// 0.9.10: Bundle install-hook context into a struct to reduce parameter count (9→6).
struct InstallHookContext {
    std::string pkg_name;
    fs::path    pkg_root;
    fs::path    install_path;
    std::string scope;       // "project" / "user" / "global"
};

// Run an install script with user review + confirmation.
// Returns true if execution succeeded or was skipped (continue install).
// Returns false if user chose to abort.
// 0.9.9: supports both Lua (sandbox-safe, no editor) and shell scripts.
static bool run_install_script(const fs::path& script, const fs::path& cwd,
                                bool assume_yes, std::string_view label,
                                bool is_lua,
                                const InstallHookContext& hook_ctx) {
    std::string desc = std::string(label) + " " + script.filename().string();

    util::info(ezmk::i18n::I18nKey::found_script, {{"label", desc}});

    if (is_lua) {
        // 0.9.9: Lua scripts run in a restricted sandbox (file-loading and
        // introspection functions unavailable), but the ezmk.* API still runs
        // with the current user's permissions — hence the confirmation below.
        if (!confirm(ezmk::i18n::fmt(ezmk::i18n::I18nKey::exec_question,
                                      {{"label", desc}}), assume_yes)) {
            util::info(ezmk::i18n::I18nKey::skipping, {{"label", desc}});
            return true;
        }

        util::info(ezmk::i18n::I18nKey::running_script, {{"label", desc}});
        int rc = ezmk::lua::run_install_hook_script(ezmk::lua::state(), script,
                                                     hook_ctx.pkg_name, hook_ctx.pkg_root,
                                                     hook_ctx.install_path, hook_ctx.scope);
        if (rc != 0) {
            std::string code_str = std::to_string(rc);
            std::string err_msg = ezmk::i18n::fmt(ezmk::i18n::I18nKey::script_failed,
                                                   {{"label", std::string(label)},
                                                    {"code", code_str}});
            if (!confirm(ezmk::i18n::fmt(ezmk::i18n::I18nKey::confirm_continue,
                                          {{"msg", err_msg}}), assume_yes)) {
                return false;
            }
        } else {
            util::info(ezmk::i18n::I18nKey::script_completed,
                       {{"label", std::string(label)}});
        }
        return true;
    }

    // Shell script: open in editor for review
    util::open_in_editor(script);

    if (!confirm(ezmk::i18n::fmt(ezmk::i18n::I18nKey::exec_question, {{"label", desc}}), assume_yes)) {
        util::info(ezmk::i18n::I18nKey::skipping, {{"label", desc}});
        return true; // skip but continue
    }

    util::info(ezmk::i18n::I18nKey::running_script, {{"label", desc}});
    auto res = util::run_script(script, cwd);
    if (res.exit_code != 0) {
        std::string code_str = std::to_string(res.exit_code);
        std::string err_msg = ezmk::i18n::fmt(ezmk::i18n::I18nKey::script_failed,
                                               {{"label", std::string(label)}, {"code", code_str}});
        if (!res.err.empty()) util::error(res.err);
        if (!confirm(ezmk::i18n::fmt(ezmk::i18n::I18nKey::confirm_continue, {{"msg", err_msg}}), assume_yes)) {
            return false; // abort
        }
    } else {
        util::info(ezmk::i18n::I18nKey::script_completed, {{"label", std::string(label)}});
    }
    return true;
}

// Validate a directory is a proper EazyMake package
static void validate_pkg(const fs::path& dir) {
    // ezmk.toml is always required
    if (!util::file_exists(dir / "ezmk.toml")) {
        throw std::runtime_error("package missing ezmk.toml: " + dir.string());
    }

    // Read config to determine package type
    auto cfg = config::parse_config(dir / "ezmk.toml");
    bool is_utils = (cfg.project.type == "utils");

    // include/ is always required; source dirs are optional for header-only,
    // utils, and precompiled packages.
    // 1.2.0-dev.9: src_dirs-aware — check the configured [compile].src_dirs
    // (default ["src"]) instead of a hardcoded src/ directory, so packages
    // with custom src_dirs are not rejected here. Error text keeps the "src/"
    // token for compatibility with existing diagnostics/tests.
    if (!is_utils && !cfg.project.header_only && !cfg.project.precompiled) {
        bool has_src_dir = false;
        for (auto& d : cfg.compile.src_dirs) {
            fs::path resolved = d;
            if (resolved.is_relative()) resolved = dir / resolved;
            if (util::file_exists(resolved)) { has_src_dir = true; break; }
        }
        if (!has_src_dir) {
            std::string src_dirs_str;
            for (auto& d : cfg.compile.src_dirs) {
                if (!src_dirs_str.empty()) src_dirs_str += ", ";
                src_dirs_str += d;
            }
            throw std::runtime_error(
                "package missing src/ directory (src_dirs: " + src_dirs_str +
                "): " + dir.string());
        }
    }
    if (!util::file_exists(dir / "include")) {
        // header-only packages may have only include/
        throw std::runtime_error("package missing include/ directory: " + dir.string());
    }
    // 0.9.7+: precompiled packages must have lib/ with at least one .a/.lib
    // 1.1.0-dev.2: platform-tagged files (lib<name>.<tag>.a) also covered
    if (cfg.project.precompiled) {
        if (!util::file_exists(dir / "lib")) {
            throw std::runtime_error("precompiled package missing lib/ directory: " + dir.string());
        }
        bool has_archive = false;
        for (auto& e : fs::directory_iterator(dir / "lib")) {
            auto ext = e.path().extension().string();
            if (ext == ".a" || ext == ".lib") { has_archive = true; break; }
        }
        if (!has_archive) {
            throw std::runtime_error("precompiled package has no .a/.lib in lib/: " + dir.string());
        }
    }
}

// Extract the package name from its ezmk.toml
static std::string pkg_name_from_dir(const fs::path& dir) {
    auto cfg = config::parse_config(dir / "ezmk.toml");
    return cfg.project.name;
}

// ===================================================================
// Compile a package to .a static library
// ===================================================================

// 1.1.0-dev.7: Check if a package is available in any registered repo.
// Searches across project → user → global scopes. Returns true if found.
bool package_available(std::string_view pkg_name) {
    auto result = repo::search_package(pkg_name, {
        cli::Scope::Project, cli::Scope::User, cli::Scope::Global});
    return !result.archive_path.empty();
}

// ===================================================================
// 1.2.0-dev.10: precompiled archive selection — os-arch[-compiler][-abi]
// ===================================================================

namespace {

// The <rest> of "lib<name>.<rest>.<ext>" is dash-separated:
//   [0] os   (win | linux | mac)
//   [1] arch (x64 | arm64 | x86)
//   [2..] compiler (gcc<digits> | clang<digits> | msvc<digits>) and/or
//         abi (abi<digits>); any other segment → not part of the naming scheme.
struct PrecompiledVariant {
    std::string os, arch, compiler, abi;
    std::string tag;       // the <rest> as-is (for reports)
    std::string filename;  // full filename (deterministic tie-break)
    bool recognized = false;    // fully parsed into os/arch/compiler/abi
    bool os_arch_known = false; // starts with a known os-arch pair (listed in available)
    int score = 0;              // 4 = full, 3 = same compiler, 2 = os-arch, 1 = bare
};

bool all_digits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

bool is_compiler_seg(const std::string& seg) {
    return (seg.rfind("gcc", 0) == 0 && all_digits(seg.substr(3))) ||
           (seg.rfind("clang", 0) == 0 && all_digits(seg.substr(5))) ||
           (seg.rfind("msvc", 0) == 0 && all_digits(seg.substr(4)));
}

bool is_abi_seg(const std::string& seg) {
    return seg.rfind("abi", 0) == 0 && all_digits(seg.substr(3));
}

bool is_os(const std::string& s)  { return s == "win" || s == "linux" || s == "mac"; }
bool is_arch(const std::string& s) { return s == "x64" || s == "arm64" || s == "x86"; }

std::vector<std::string> split_dash(const std::string& s) {
    std::vector<std::string> parts;
    size_t pos = 0;
    while (true) {
        auto dash = s.find('-', pos);
        if (dash == std::string::npos) { parts.push_back(s.substr(pos)); break; }
        parts.push_back(s.substr(pos, dash - pos));
        pos = dash + 1;
    }
    return parts;
}

PrecompiledVariant parse_variant(const std::string& rest, const std::string& filename) {
    PrecompiledVariant v;
    v.filename = filename;
    v.tag = rest;
    auto parts = split_dash(rest);
    if (parts.size() < 2 || !is_os(parts[0]) || !is_arch(parts[1])) return v;
    v.os = parts[0];
    v.arch = parts[1];
    v.os_arch_known = true;
    for (size_t i = 2; i < parts.size(); ++i) {
        const auto& seg = parts[i];
        if (is_compiler_seg(seg)) {
            if (!v.compiler.empty()) return v;  // two compiler segments → malformed
            v.compiler = seg;
        } else if (is_abi_seg(seg)) {
            if (!v.abi.empty()) return v;
            v.abi = seg;
        } else {
            return v;  // unknown segment → not this naming scheme
        }
    }
    v.recognized = true;
    return v;
}

// Score a candidate against the consumer's (os, arch, compiler, abi).
// A same-compiler candidate with a *different explicit abi* is ABI-incompatible
// (0) and never degrades to L3. Candidates with a different compiler or an abi
// without a compiler are never matched.
int score_variant(const PrecompiledVariant& v,
                  const std::string& plat_os, const std::string& plat_arch,
                  const std::string& compiler, const std::string& abi) {
    if (!v.recognized) return 0;
    if (v.os != plat_os || v.arch != plat_arch) return 0;
    if (!v.compiler.empty()) {
        if (compiler.empty() || v.compiler != compiler) return 0;
        if (!v.abi.empty()) {
            if (abi.empty() || v.abi != abi) return 0;  // ABI mismatch
            return 4;  // L4: full tag
        }
        return 3;  // L3: same compiler, candidate has no explicit abi (same default ABI)
    }
    if (!v.abi.empty()) return 0;  // abi without compiler → cannot confirm
    return 2;  // L2: os-arch only
}

// 1.2.0-dev.10: consumer ABI tag from toolchain defaults (zero-config).
// GCC → "abi11" (libstdc++ CXX11 ABI); Clang → macOS: "" (defaults to libc++,
// stable ABI), otherwise "abi11" (libstdc++ default); MSVC → "" (ABI decided
// by the toolset).
std::string default_abi_tag(const toolchain::Toolchain& tc) {
    switch (tc.family) {
    case toolchain::CompilerFamily::Gcc:
        return "abi11";
    case toolchain::CompilerFamily::Clang:
#ifdef EZMK_MACOS
        return "";
#else
        return "abi11";
#endif
    case toolchain::CompilerFamily::Msvc:
        return "";
    }
    return "";
}

std::string join_list(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) s += ", ";
        s += v[i];
    }
    return s;
}

// 1.2.0-dev.10: list the precompiled variant tags available in a lib/ dir —
// recognized os-arch[-compiler][-abi] tags plus bare lib<name>.a/.lib names —
// for `pkg info`. Sorted for deterministic output.
std::vector<std::string> list_precompiled_variants(const fs::path& lib_dir,
                                                   const std::string& pkg_name) {
    std::vector<std::string> out;
    if (!util::file_exists(lib_dir)) return out;
    std::string tagged_prefix = "lib" + pkg_name + ".";
    std::string bare_a  = "lib" + pkg_name + ".a";
    std::string bare_lib = "lib" + pkg_name + ".lib";
    for (auto& e : fs::directory_iterator(lib_dir)) {
        auto ext = e.path().extension().string();
        if (ext != ".a" && ext != ".lib") continue;
        std::string filename = e.path().filename().string();
        if (filename == bare_a || filename == bare_lib) {
            out.push_back(filename);
            continue;
        }
        if (filename.find(tagged_prefix) != 0) continue;
        std::string rest = filename.substr(tagged_prefix.size(),
            filename.size() - tagged_prefix.size() - ext.size());
        if (rest.empty()) continue;
        auto v = parse_variant(rest, filename);
        if (v.os_arch_known) out.push_back(v.tag);
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

// 1.2.0-dev.10: pure matching core (testable with explicit tags).
fs::path select_precompiled_variant(const fs::path& lib_dir,
                                    const std::string& pkg_name,
                                    const std::string& platform_tag,
                                    const std::string& compiler_tag,
                                    const std::string& abi_tag,
                                    bool strict) {
    auto plat = split_dash(platform_tag);
    std::string plat_os = plat.size() >= 1 ? plat[0] : "";
    std::string plat_arch = plat.size() >= 2 ? plat[1] : "";

    // 1.2.0-dev.11: missing lib/ must be a friendly "no build" error, not a
    // raw std::filesystem::filesystem_error from the directory iterator.
    if (!util::file_exists(lib_dir) || !fs::is_directory(lib_dir)) {
        throw std::runtime_error("precompiled package '" + pkg_name +
                                 "' has no lib/ directory");
    }

    std::string tagged_prefix = "lib" + pkg_name + ".";
    std::string bare_a  = "lib" + pkg_name + ".a";
    std::string bare_lib = "lib" + pkg_name + ".lib";

    std::vector<PrecompiledVariant> candidates;  // score > 0
    std::vector<std::string> available;          // report list (os-arch-known + bare)
    PrecompiledVariant bare;

    for (auto& e : fs::directory_iterator(lib_dir)) {
        auto ext = e.path().extension().string();
        if (ext != ".a" && ext != ".lib") continue;
        std::string filename = e.path().filename().string();

        if (filename == bare_a || filename == bare_lib) {
            bare.filename = filename;
            bare.recognized = true;  // bare is always a valid L1 candidate
            continue;
        }
        if (filename.find(tagged_prefix) != 0) continue;
        std::string rest = filename.substr(tagged_prefix.size(),
            filename.size() - tagged_prefix.size() - ext.size());
        if (rest.empty()) continue;  // "lib<name>..a" — malformed, skip
        auto v = parse_variant(rest, filename);
        if (v.os_arch_known) available.push_back(v.tag);
        int score = score_variant(v, plat_os, plat_arch, compiler_tag, abi_tag);
        if (score > 0) {
            v.score = score;
            candidates.push_back(std::move(v));
        }
    }
    if (!bare.filename.empty()) {
        bare.score = 1;  // L1
        candidates.push_back(std::move(bare));
        available.push_back(bare.filename);
    }

    if (candidates.empty()) {
        std::string msg = "precompiled package '" + pkg_name +
            "' has no build for platform '" + platform_tag + "'";
        if (!compiler_tag.empty())
            msg += " (toolchain " + compiler_tag +
                   (abi_tag.empty() ? "" : "-" + abi_tag) + ")";
        if (!available.empty()) {
            std::sort(available.begin(), available.end());
            msg += " — available: " + join_list(available);
        }
        throw std::runtime_error(msg);
    }

    // Best: highest score; ties → MSVC consumers prefer .lib over .a (the
    // archive format matches the toolchain), otherwise lexicographically
    // smallest filename (1.2.0-dev.11: extension preference added).
    bool prefer_lib = (compiler_tag.rfind("msvc", 0) == 0);
    auto ext_rank = [&](const PrecompiledVariant& v) -> int {
        if (!prefer_lib) return 0;
        return fs::path(v.filename).extension() == ".lib" ? 0 : 1;
    };
    auto best_it = std::max_element(candidates.begin(), candidates.end(),
        [&](const PrecompiledVariant& a, const PrecompiledVariant& b) {
            if (a.score != b.score) return a.score < b.score;
            int ra = ext_rank(a), rb = ext_rank(b);
            if (ra != rb) return ra > rb;  // max_element wants "a worse than b"
            return a.filename > b.filename;
        });
    const auto& best = *best_it;
    fs::path result = lib_dir / best.filename;

    // Degraded to L2 (os-arch) / L1 (bare) — possibly cross-toolchain ABI.
    if (best.score < 3 && !compiler_tag.empty()) {
        if (strict) {
            std::sort(available.begin(), available.end());
            util::fatal(ezmk::i18n::fmt(
                ezmk::i18n::I18nKey::precompiled_strict_mismatch,
                {{"pkg", pkg_name},
                 {"toolchain", compiler_tag},
                 {"fallback", best.filename},
                 {"available", join_list(available)}}));
        }
        std::sort(available.begin(), available.end());
        util::warn(ezmk::i18n::fmt(
            ezmk::i18n::I18nKey::precompiled_toolchain_fallback_warn,
            {{"pkg", pkg_name},
             {"toolchain", compiler_tag},
             {"fallback", best.filename},
             {"available", join_list(available)}}));
    }
    return result;
}

// 1.1.0-dev.2: Select the best precompiled archive for the current platform.
// 1.2.0-dev.10: resolves the consumer's platform/compiler/abi tags and the
// package's own precompiled_strict flag, then runs the pure matching core.
// 1.3.1: checks the package's declared minimum standard against the consumer
// project BEFORE archive selection (precompiled ABI risk — stronger wording).
fs::path select_precompiled_archive(const fs::path& lib_dir,
                                    const std::string& pkg_name) {
    auto tc = toolchain::detect_toolchain();
    std::string platform = util::detect_platform_tag();
    std::string compiler = toolchain::compiler_tag(tc);
    std::string abi = default_abi_tag(tc);

    bool strict = false;
    std::string pkg_language;
    try {
        auto cfg = config::parse_config(lib_dir.parent_path() / "ezmk.toml");
        pkg_language = cfg.project.language;
        strict = cfg.project.precompiled_strict;
    } catch (...) {
        // Malformed package config → proceed non-strict; the install/build
        // paths surface the config error where the package is processed.
    }
    if (!pkg_language.empty()) {
        check_std_compat(pkg_name, pkg_language, /*precompiled=*/true,
                         consumer_std_ctx());
    }
    return select_precompiled_variant(lib_dir, pkg_name, platform, compiler, abi, strict);
}

// 1.1.2 S2: 构造静态库归档命令。路径统一经 escape_shell_arg 转义——
// 对象路径源自归档内源文件名，可能含 `$`/反引号/空格等字符；裸引号包裹在
// POSIX `sh -c` 下可命令注入（与 util.cpp 中 git_clone/git_pull 的写法一致）。
std::string build_archive_command(bool is_msvc,
                                  const fs::path& lib_out,
                                  const std::vector<fs::path>& objects) {
    std::ostringstream cmd;
    cmd << (is_msvc ? "lib.exe /OUT:" : "ar rcs ")
        << "\"" << util::escape_shell_arg(lib_out.string()) << "\"";
    for (auto& o : objects) {
        cmd << " \"" << util::escape_shell_arg(o.string()) << "\"";
    }
    return cmd.str();
}

fs::path compile_package(const fs::path& pkg_dir,
                         const std::vector<fs::path>& dep_includes,
                         const toolchain::Toolchain& tc) {
    auto cfg = config::parse_config(pkg_dir / "ezmk.toml");
    std::string name = cfg.project.name;

    // 0.9.7+: precompiled packages — use lib/*.a directly, skip compilation
    // 1.1.0-dev.2: multi-platform — select by platform tag, fallback to bare archive
    if (cfg.project.precompiled) {
        fs::path lib_dir = pkg_dir / "lib";
        return select_precompiled_archive(lib_dir, name);
    }

    // 0.9.7+: header-only packages have no source files — skip silently.
    // 1.2.0-dev.9: short-circuit moved BEFORE source collection so a
    // header-only package without any src_dirs never triggers the
    // src_dir_missing / no_source_files fatal.
    if (cfg.project.header_only) return {};

    // 1.3.1: source-package standard compatibility check (precompiled is
    // checked inside select_precompiled_archive with stronger wording).
    // 1.4.0-dev.3: 消费者上下文一次解析——warn/fatal 校验（声明标准，协商前）
    // 与编译协商（语义 B）共享，不二次 parse 消费者配置。
    auto consumer_ctx = consumer_std_ctx();
    check_std_compat(name, cfg.project.language, /*precompiled=*/false,
                     consumer_ctx);

    // 1.4.0-dev.3: 编译协商——包按 max(包min, 消费者min) 重编（cap 到能力表
    // 与包声明上界）。协商值 ≥ 包 min → 上述校验自然不再 warn；预编译包不
    // 参与（走 select_precompiled_archive，无编译）。
    auto lang = config::parse_language(cfg.project.language);
    lang = negotiate_package_std(lang, consumer_ctx.min, tc);

    fs::path build_dir = pkg_dir / "build";
    fs::create_directories(build_dir);

    // 1.2.0-dev.9: collect sources from [compile].src_dirs (default ["src"]),
    // reusing build::collect_sources — multi-directory collection, missing-dir
    // warn+skip, filename dedup. require_main=false: packages are always
    // static libraries regardless of [project].type (package docs default to
    // "executable", which must not trigger the main.cpp requirement). An empty
    // result is a fatal error (no_source_files / src_dir_missing) — aligned
    // with project semantics: all three no-source short-circuits
    // (precompiled / header_only / utils gate) run before this point.
    auto sources = build::collect_sources(cfg.compile.src_dirs, pkg_dir,
                                          cfg.project.type,
                                          /*require_main=*/false);

    // 1.1.0: MSVC uses .lib, GCC/Clang use .a
    bool is_msvc = (tc.family == toolchain::CompilerFamily::Msvc);
    const char* lib_ext = is_msvc ? ".lib" : ".a";
    fs::path lib = build_dir / ("lib" + name + lib_ext);

    // Clean stale temps from previous crashed builds
    {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(build_dir, ec)) {
            auto& p = e.path();
            if (p.extension() == ".tmp") {
                fs::remove(p, ec);
            }
        }
    }

    // Load package cache
    fs::path cache_path = build_dir / ".pkg_cache.json";
    auto record = cache::load_record(cache_path);

    // 1.1.0: check compiler version — if changed, invalidate all caches
    std::string compiler_name = tc.cxx_compiler.filename().string();
    if (!record.compiler_version.empty() && record.compiler_version != tc.version) {
        util::info("    compiler version changed, invalidating package cache");
        record.files.clear();
    }
    record.compiler = compiler_name;
    record.compiler_version = tc.version;

    // 1.1.0: deterministic flag
    record.deterministic = cfg.compile.deterministic;

    // Check global compile options signature (1.1.2 C2: include stdlib; use_pic
    // is always false for packages — static libs).
    // 1.4.0-dev.3: pass the (negotiated) std_flag — the old "" never matched
    // the per-source signature (which always includes std_flag), so package
    // caches never hit; and a negotiation change (consumer standard) now
    // invalidates the package cache automatically.
    auto cur_sig = cache::compile_options_signature(cfg.compile, {}, lang.std_flag,
                                                    cfg.project.stdlib, false);
    // 1.1.0: deterministic build — include lockfile hash
    // 1.2.0-dev.7: lockfile resolved against the located project root
    if (cfg.compile.deterministic) {
        auto root = util::locate_project_root(fs::current_path());
        auto lock_path = (root.value_or(fs::current_path())) / "ezmk.lock";
        if (util::file_exists(lock_path)) {
            cur_sig += ":" + crypto::sha256_file(lock_path);
        }
    }
    if (record.compile_options_signature != cur_sig) {
        if (!record.compile_options_signature.empty()) {
            util::info("    compile options changed, invalidating package cache");
        }
        record.compile_options_signature = cur_sig;
        record.files.clear();
    }

    // ---- Unified compile phase ----
    cache::CompileInput cin;
    cin.sources = std::move(sources);
    cin.obj_dir = build_dir;
    cin.dep_dir = build_dir;
    cin.proj_root = pkg_dir;
    cin.compile = cfg.compile;
    cin.lang = lang;
    cin.extra_includes = dep_includes;
    cin.cache_obj_dir = build_dir;   // package: obj_dir == cache_obj_dir
    cin.disable_cache = false;       // packages always use cache
    cin.use_pic = false;             // packages are always static libs
    cin.stdlib = cfg.project.stdlib;  // 1.1.0-dev.4

    auto comp_result = cache::compile_sources(cin, record);

    // Save cache
    cache::save_record(record, cache_path);

    if (comp_result.cache_hits > 0 || comp_result.cache_misses > 0) {
        std::string summary = "    ";
        if (comp_result.cache_hits > 0) summary += std::to_string(comp_result.cache_hits) + " cached, ";
        summary += std::to_string(comp_result.cache_misses) + " compiled";
        util::info(summary);
    }

    // Archive (skip if nothing changed and .a exists)
    if (comp_result.cache_misses == 0 && util::file_exists(lib)) {
        return lib;
    }

    // Use the actual object paths returned by compile_sources
    // (handles nested source directories correctly, unlike filename-based reconstruction)

    // Archive to temp file, then atomic rename
    const char* tmp_ext = is_msvc ? ".lib.tmp" : ".a.tmp";
    fs::path lib_tmp = build_dir / ("lib" + name + tmp_ext);
    {
        std::error_code ec;
        fs::remove(lib_tmp, ec);
    }

    if (is_msvc) {
        // 1.1.0: MSVC — use lib.exe to create static library
        // 1.1.2 S2: 命令构造收敛到 build_archive_command()（路径转义）
        auto lib_res = util::run_command(
            build_archive_command(is_msvc, lib_tmp, comp_result.objects));
        if (lib_res.exit_code != 0) {
            std::error_code ec;
            fs::remove(lib_tmp, ec);
            util::error(lib_res.err);
            throw std::runtime_error("failed to create library (lib.exe) for: " + name);
        }
    } else {
        // GCC/Clang: use ar
        auto ar_res = util::run_command(
            build_archive_command(is_msvc, lib_tmp, comp_result.objects));
        if (ar_res.exit_code != 0) {
            std::error_code ec;
            fs::remove(lib_tmp, ec);
            util::error(ar_res.err);
            throw std::runtime_error("failed to create archive for: " + name);
        }
    }

    {
        std::error_code ec;
        fs::rename(lib_tmp, lib, ec);
        if (ec) {
            fs::copy_file(lib_tmp, lib, fs::copy_options::overwrite_existing, ec);
            fs::remove(lib_tmp, ec);
        }
    }

    return lib;
}

// ===================================================================
// Dependency resolution
// ===================================================================

std::vector<fs::path> resolve_dependency_order(const std::vector<fs::path>& pkg_dirs) {
    if (pkg_dirs.empty()) return {};

    // Map: package name → directory path
    std::map<std::string, fs::path> name_to_dir;
    for (auto& d : pkg_dirs) {
        name_to_dir[pkg_name_from_dir(d)] = d;
    }

    // Build adjacency list and in-degree map
    std::map<std::string, std::vector<std::string>> adj;
    std::map<std::string, int> in_degree;
    for (auto& d : pkg_dirs) {
        auto cfg = config::parse_config(d / "ezmk.toml");
        std::string name = cfg.project.name;
        if (in_degree.find(name) == in_degree.end()) in_degree[name] = 0;
        for (auto& dep : cfg.depends.libs) {
            adj[dep.name].push_back(name);
            in_degree[name]++;
        }
        // 0.2.2+: want dependencies are included if the package is installed
        for (auto& dep : cfg.depends.want) {
            if (name_to_dir.find(dep.name) != name_to_dir.end()) {
                adj[dep.name].push_back(name);
                in_degree[name]++;
            }
        }
    }

    // Check that all dependencies are satisfied
    for (auto& [name, _] : adj) {
        if (name_to_dir.find(name) == name_to_dir.end()) {
            throw std::runtime_error("missing dependency: " + name);
        }
    }

    // Kahn's algorithm
    std::deque<std::string> queue;
    for (auto& [name, deg] : in_degree) {
        if (deg == 0) queue.push_back(name);
    }

    std::vector<fs::path> sorted;
    while (!queue.empty()) {
        auto name = queue.front();
        queue.pop_front();
        sorted.push_back(name_to_dir[name]);

        for (auto& next : adj[name]) {
            if (--in_degree[next] == 0) {
                queue.push_back(next);
            }
        }
    }

    if (sorted.size() != pkg_dirs.size()) {
        throw std::runtime_error(ezmk::i18n::get(ezmk::i18n::I18nKey::circular_dep));
    }

    return sorted;
}

// ===================================================================
// Install
// ===================================================================

// 1.2.0-dev.7: Shared post-validate install processing for BOTH archive and
// directory installs: preinstall hook → dependency resolution → compilation →
// transactional copy → postinstall hook. `pkg_root` is the validated package
// directory (archive: extracted staging dir; directory: the source dir itself).
// `stage` is the archive staging area to clean up on user-cancel (empty for
// directory installs, where the source dir is never removed).
static void process_installed_pkg(const fs::path& pkg_root,
                                  const fs::path& dest_dir,
                                  cli::Scope scope,
                                  bool assume_yes,
                                  const toolchain::Toolchain& tc,
                                  const fs::path& stage) {
    validate_pkg(pkg_root);

    auto pkg_cfg = config::parse_config(pkg_root / "ezmk.toml");
    std::string pkg_name = pkg_cfg.project.name;
    util::validate_pkg_name(pkg_name);  // 1.1.3 S2: 恶意包名 → 中止安装

    // Preinstall hook
    fs::path preinstall_script = detect_install_script(pkg_root, "preinstall");
    if (!preinstall_script.empty()) {
        bool is_lua = (preinstall_script.extension() == ".lua");
        InstallHookContext hook_ctx{pkg_name, pkg_root,
                                    dest_dir / pkg_name, scope_to_string(scope)};
        if (!run_install_script(preinstall_script, dest_dir / pkg_name,
                                assume_yes, "preinstall", is_lua,
                                hook_ctx)) {
            util::info(ezmk::i18n::I18nKey::install_cancelled_user,
                       {{"hook", "preinstall"}});
            if (!stage.empty()) util::remove_all(stage);
            return;
        }
    }

    // Check for existing install
    // 1.1.2 C6: do NOT delete the old install here — dependency resolution,
    // compilation and hooks run below, and a failure would leave the package
    // uninstalled. The old version is swapped out only once the new one is
    // fully staged (see the transactional copy below).
    fs::path install_path = dest_dir / pkg_name;
    if (util::file_exists(install_path)) {
        if (!confirm(ezmk::i18n::fmt(ezmk::i18n::I18nKey::overwrite_confirm,
                     {{"pkg", pkg_name}, {"path", install_path.string()}}), assume_yes)) {
            util::info(ezmk::i18n::I18nKey::install_cancelled);
            if (!stage.empty()) util::remove_all(stage);
            return;
        }
    }

    // Collect all involved packages for dependency resolution
    std::vector<fs::path> all_pkgs = { pkg_root };

    // Check and resolve dependencies
    {
        // 1.1.0-dev.7: want interaction mode (reset per install call)
        // 0=normal (prompt), 1=accept_all, 2=deny_all
        int want_mode = 0;
        bool want_header_printed = false;

        std::set<std::string> seen = { pkg_name };
        std::deque<std::string> to_check = { pkg_name };
        while (!to_check.empty()) {
            auto cur = to_check.front();
            to_check.pop_front();

            fs::path cur_dir = pkg_root;
            if (cur != pkg_name) {
                cur_dir = dest_dir / cur;
                if (!util::file_exists(cur_dir)) {
                    throw std::runtime_error(
                        ezmk::i18n::fmt(ezmk::i18n::I18nKey::missing_dep,
                                        {{"dep", cur}}));
                }
            }

            auto cur_cfg = config::parse_config(cur_dir / "ezmk.toml");
            for (auto& dep : cur_cfg.depends.libs) {
                // 1.1.3 S2 同口径: 依赖名来自包的 ezmk.toml（不可信输入），直接拼进
                // dest_dir/<name> 会被用于解析/编译/写产物——`..`/分隔符/盘符/绝对
                // 路径会在安装目录外落盘。与根包名（1068 行）一样校验，非法即中止。
                util::validate_pkg_name(dep.name);
                if (seen.insert(dep.name).second) {
                    to_check.push_back(dep.name);
                    fs::path dep_path = dest_dir / dep.name;
                    if (util::file_exists(dep_path)) {
                        // 0.9.6+: Validate installed version against constraint
                        if (dep.constraint.op != config::VersionConstraint::None) {
                            auto dep_cfg = config::parse_config(dep_path / "ezmk.toml");
                            if (!satisfies_version_constraint(dep_cfg.project.version,
                                                              dep.constraint)) {
                                throw std::runtime_error(
                                    ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_constraint_unsatisfied,
                                                    {{"pkg", dep.name},
                                                     {"constraint", dep.constraint.version},
                                                     {"available", dep_cfg.project.version}}));
                            }
                        }
                        all_pkgs.push_back(dep_path);
                    } else {
                        // 1.1.0-dev.7: Auto-install hard dependency from registered repos
                        // 1.2.0-dev.11: guard against unbounded recursion on
                        // mutual dependencies (A⇄B).
                        if (g_auto_installing.count(dep.name)) {
                            throw std::runtime_error(
                                "circular dependency during auto-install: '" +
                                dep.name + "' is already being installed");
                        }
                        auto search = repo::search_package(dep.name, {
                            cli::Scope::Project, cli::Scope::User, cli::Scope::Global});
                        if (!search.archive_path.empty() &&
                            util::file_exists(search.archive_path)) {
                            util::info(std::string("auto-installing dependency: ") + dep.name);
                            try {
                                AutoInstallGuard guard(dep.name);
                                // Install to the same scope, skip lockfile for transitive deps
                                install(dep.name, scope, search.sha256,
                                        assume_yes, false, true);
                                // After install, verify it now exists
                                if (util::file_exists(dep_path)) {
                                    // 1.2.0-dev.11: re-validate the freshly
                                    // installed version against the caller's
                                    // constraint — auto-install used to take
                                    // the newest silently (B@^1.0 could get 2.0).
                                    if (dep.constraint.op != config::VersionConstraint::None) {
                                        auto dep_cfg = config::parse_config(
                                            dep_path / "ezmk.toml");
                                        if (!satisfies_version_constraint(
                                                dep_cfg.project.version,
                                                dep.constraint)) {
                                            throw std::runtime_error(
                                                ezmk::i18n::fmt(
                                                    ezmk::i18n::I18nKey::pkg_constraint_unsatisfied,
                                                    {{"pkg", dep.name},
                                                     {"constraint", dep.constraint.version},
                                                     {"available", dep_cfg.project.version}}));
                                        }
                                    }
                                    all_pkgs.push_back(dep_path);
                                    continue;
                                }
                            } catch (const std::exception& e) {
                                throw std::runtime_error(
                                    std::string("failed to install dependency '") +
                                    dep.name + "': " + e.what());
                            }
                        }
                        throw std::runtime_error(
                            ezmk::i18n::fmt(ezmk::i18n::I18nKey::missing_dep,
                                            {{"dep", dep.name}}));
                    }
                }
            }
            // 0.2.2+: want dependencies are optional — include if installed.
            // 1.1.0-dev.7: interactive prompt for missing optional deps (Y/N/A/D).
            for (auto& dep : cur_cfg.depends.want) {
                // 同 libs: 依赖名不可信输入 → 与根包名同口径校验（防止 `..` 越界）。
                util::validate_pkg_name(dep.name);
                if (seen.insert(dep.name).second) {
                    fs::path dep_path = dest_dir / dep.name;
                    if (util::file_exists(dep_path)) {
                        // 0.9.6+: Validate installed version against constraint
                        if (dep.constraint.op != config::VersionConstraint::None) {
                            try {
                                auto dep_cfg = config::parse_config(dep_path / "ezmk.toml");
                                if (!satisfies_version_constraint(dep_cfg.project.version,
                                                                  dep.constraint)) {
                                    util::warn(ezmk::i18n::fmt(
                                        ezmk::i18n::I18nKey::pkg_constraint_unsatisfied,
                                        {{"pkg", dep.name},
                                         {"constraint", dep.constraint.version},
                                         {"available", dep_cfg.project.version}}));
                                    continue; // skip this dep — constraint not satisfied
                                }
                            } catch (...) {
                                util::warn(std::string("failed to parse config for dependency: ") + dep.name);
                                continue;
                            }
                        }
                        to_check.push_back(dep.name);
                        all_pkgs.push_back(dep_path);
                    } else {
                        // 1.1.0-dev.7: Not installed — prompt user (interactive) or skip (-y)
                        char choice = assume_yes ? 'd' : 0;

                        // Check mode set by earlier A/D choices
                        if (want_mode == 1) choice = 'a';
                        else if (want_mode == 2) choice = 'd';

                        // Prompt if in normal mode and interactive
                        if (choice == 0) {
                            // Print header on first prompt
                            if (!want_header_printed) {
                                util::info(ezmk::i18n::get(ezmk::i18n::I18nKey::want_prompt_title));
                                want_header_printed = true;
                            }
                            util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::want_prompt_item,
                                {{"pkg", cur}, {"dep", dep.name}}));
                            std::cerr << ezmk::i18n::get(ezmk::i18n::I18nKey::want_prompt_options);
                            std::string line;
                            if (std::getline(std::cin, line)) {
                                if (!line.empty()) choice = static_cast<char>(std::tolower(line[0]));
                            }
                            if (choice != 'y' && choice != 'n' &&
                                choice != 'a' && choice != 'd') {
                                choice = 'n'; // default: skip
                            }
                        }

                        if (choice == 'a') want_mode = 1;      // accept all from now on
                        else if (choice == 'd') want_mode = 2; // deny all from now on

                        if (choice == 'y' || choice == 'a') {
                            // Try to install from repos
                            auto search = repo::search_package(dep.name, {
                                cli::Scope::Project, cli::Scope::User, cli::Scope::Global});
                            if (!search.archive_path.empty() &&
                                util::file_exists(search.archive_path)) {
                                util::info(ezmk::i18n::fmt(
                                    ezmk::i18n::I18nKey::want_auto_installing,
                                    {{"dep", dep.name}}));
                                try {
                                    install(dep.name, scope, search.sha256,
                                            assume_yes, false, true);
                                    if (util::file_exists(dep_path)) {
                                        to_check.push_back(dep.name);
                                        all_pkgs.push_back(dep_path);
                                    }
                                } catch (const std::exception& e) {
                                    util::warn(std::string("failed to install optional dependency '") +
                                               dep.name + "': " + e.what());
                                }
                            } else {
                                util::warn(std::string("optional dependency not found in repos: ") + dep.name);
                            }
                        }
                        // choice == 'n' or 'd': skip this want
                    }
                }
            }
        }
    }

    // Dependency ordering + compilation
    util::info(ezmk::i18n::I18nKey::resolving_deps);
    auto order = resolve_dependency_order(all_pkgs);

    // Build name → dir map for resolving dependency include paths
    std::map<std::string, fs::path> name_to_dir;
    for (auto& d : all_pkgs) {
        name_to_dir[config::parse_config(d / "ezmk.toml").project.name] = d;
    }

    for (auto& dir : order) {
        auto cfg = config::parse_config(dir / "ezmk.toml");
        // Skip compilation for utils packages without source files.
        // 1.2.0-dev.9: src_dirs-aware — compile only if any configured src_dir
        // exists AND contains source files. Lightweight walk (no collect_sources)
        // to avoid missing-directory warning noise for utils-only packages.
        if (cfg.project.type == "utils") {
            bool has_sources = false;
            for (auto& d : cfg.compile.src_dirs) {
                fs::path sdir = d;
                if (sdir.is_relative()) sdir = dir / sdir;
                if (util::file_exists(sdir) &&
                    !util::list_files(sdir, {".c", ".cc", ".cpp", ".cxx"}).empty()) {
                    has_sources = true;
                    break;
                }
            }
            if (!has_sources) continue;
        }
        // 0.9.7+: skip compilation for header-only packages
        if (cfg.project.header_only) {
            util::info(ezmk::i18n::I18nKey::installing_header_only,
                       {{"name", cfg.project.name}});
            continue;
        }
        // 0.9.7+: skip compilation for precompiled packages
        if (cfg.project.precompiled) {
            compile_package(dir, {}, tc);  // validates & returns lib/*.a path
            util::info(ezmk::i18n::I18nKey::installing_precompiled,
                       {{"name", cfg.project.name}});
            continue;
        }
        std::vector<fs::path> dep_includes;
        for (auto& dep : cfg.depends.libs) {
            auto it = name_to_dir.find(dep.name);
            if (it != name_to_dir.end()) {
                dep_includes.push_back(it->second / "include");
            }
        }
        // 0.2.2+: want deps also contribute include paths when installed
        for (auto& dep : cfg.depends.want) {
            auto it = name_to_dir.find(dep.name);
            if (it != name_to_dir.end()) {
                dep_includes.push_back(it->second / "include");
            }
        }
        util::info(ezmk::i18n::I18nKey::compiling_pkg,
                   {{"name", cfg.project.name}});
        compile_package(dir, dep_includes, tc);
    }

    // Copy to install directory — transactionally (1.1.2 C6).
    // Back up the old install, place the new one, and only then delete the
    // backup. If the copy fails, roll the old version back. Previously the
    // old install was deleted before staging, so any failure left the
    // package uninstalled with no recovery.
    fs::create_directories(dest_dir);
    util::info(ezmk::i18n::I18nKey::installing_to, {{"path", install_path.string()}});
    // 1.2.0-dev.11: copy-then-swap transaction. The new install is staged at
    // <name>.new while the old version stays at install_path for the whole
    // copy window — a crash mid-copy leaves the old version intact (previously
    // it was moved to a backup first, so a crash stranded the old version with
    // no install in place). The backup name is hidden and collision-free
    // (a real package could legitimately be named "foo.old").
    fs::path new_path = install_path;
    new_path += ".new";
    fs::path backup = dest_dir / (".ezmk-backup-" + pkg_name);
    { std::error_code ec; fs::remove_all(new_path, ec); }   // stale staging
    { std::error_code ec; fs::remove_all(backup, ec); }     // stale crash backup

    try {
        util::copy_recursive(pkg_root, new_path);
    } catch (...) {
        std::error_code ec;
        fs::remove_all(new_path, ec);  // drop the partial staging
        throw;
    }

    // Swap: old → backup (if present), then new → install_path.
    bool had_old = util::file_exists(install_path);
    if (had_old) {
        std::error_code ec;
        fs::rename(install_path, backup, ec);
        if (ec) {
            std::error_code ec2;
            fs::remove_all(new_path, ec2);
            throw std::runtime_error("failed to back up existing install: " +
                                     install_path.string() + " (" + ec.message() + ")");
        }
    }
    {
        std::error_code ec;
        fs::rename(new_path, install_path, ec);
        if (ec) {
            // Roll the old version back; report if the rollback itself fails —
            // the old install must not silently disappear.
            std::error_code ec2;
            fs::remove_all(install_path, ec2);  // drop partial new (best effort)
            bool restored = false;
            if (had_old) {
                std::error_code ec3;
                fs::rename(backup, install_path, ec3);
                restored = !ec3;
            }
            if (!restored) {
                throw std::runtime_error(
                    "failed to place new install AND restore old version: " +
                    install_path.string() + " (" + ec.message() + ")");
            }
            throw std::runtime_error("failed to place new install: " +
                                     install_path.string() + " (" + ec.message() + ")");
        }
    }
    { std::error_code ec; fs::remove_all(backup, ec); }  // old version gone for good

    // Postinstall hook
    fs::path postinstall_script = detect_install_script(install_path, "postinstall");
    if (!postinstall_script.empty()) {
        bool is_lua = (postinstall_script.extension() == ".lua");
        InstallHookContext hook_ctx{pkg_name, install_path,
                                    install_path, scope_to_string(scope)};
        if (!run_install_script(postinstall_script, install_path,
                                assume_yes, "postinstall", is_lua,
                                hook_ctx)) {
            util::info(ezmk::i18n::I18nKey::install_cancelled_user,
                       {{"hook", "postinstall"}});
            // Installation files are already in place; leave them
        }
    }

    util::info(ezmk::i18n::I18nKey::installed, {{"pkg", pkg_name}});
}

// 1.1.0: generate/update ezmk.lock with resolved dependency snapshot.
// Shared by archive and directory installs (project scope only, unless --no-lock).
static void maybe_write_lockfile(cli::Scope scope, bool no_lock,
                                 const toolchain::Toolchain& tc,
                                 const fs::path& dest_dir) {
    if (scope != cli::Scope::Project || no_lock) return;

    try {
        // 1.2.0-dev.7: lockfile lives under the located project root
        auto proj_root = util::locate_project_root(fs::current_path())
                            .value_or(fs::current_path());
        auto now_iso = []() -> std::string {
            auto t = std::time(nullptr);
            auto* tm = std::localtime(&t);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
            return buf;
        };

        config::Lockfile lf;
        lf.version = 1;
        lf.generated_by = "ezmk " EZMK_VERSION;
        lf.generated_at = now_iso();
        lf.toolchain = (tc.family == toolchain::CompilerFamily::Msvc) ? "msvc"
                     : (tc.family == toolchain::CompilerFamily::Clang) ? "clang" : "gcc";
        lf.toolchain_version = tc.version;
        // 1.1.2 C3: record the root project's DIRECT deps so depends_changed
        // compares direct-vs-direct (packages[] includes transitive deps).
        try {
            auto root_cfg = config::parse_config(proj_root / "ezmk.toml");
            lf.direct_deps = lockfile::direct_dep_specs(root_cfg);
        } catch (...) {
            // Unparseable root config → leave direct_deps empty
        }

        // Scan installed packages in project scope
        fs::path pkg_dir = dest_dir;
        if (util::file_exists(pkg_dir)) {
            for (auto& entry : fs::directory_iterator(pkg_dir)) {
                if (!entry.is_directory()) continue;
                auto pkg_toml = entry.path() / "ezmk.toml";
                if (!util::file_exists(pkg_toml)) continue;
                try {
                    auto pkg_cfg = config::parse_config(pkg_toml);
                    config::LockedPackage lp;
                    lp.name = pkg_cfg.project.name;
                    lp.version = pkg_cfg.project.version;
                    lp.type = pkg_cfg.project.header_only ? "header-only"
                            : pkg_cfg.project.type;
                    lp.scope = "project";
                    lp.platform = (tc.family == toolchain::CompilerFamily::Msvc) ? "windows_x86_64_msvc"
                                : "windows_x86_64_gcc";
                    for (auto& d : pkg_cfg.depends.libs) lp.dependencies.push_back(d.name);
                    for (auto& d : pkg_cfg.depends.want) lp.dependencies.push_back(d.name);

                    // 1.4.1: git-source provenance marker — written into the
                    // installed package dir by install_git_source. Git sources
                    // are pinned by their commit SHA (not an archive), so the
                    // lib hash is left empty and source/source_url/commit are
                    // recorded instead.
                    fs::path git_marker = entry.path() / ".ezmk-git-source";
                    if (util::file_exists(git_marker)) {
                        std::string content = util::file_read(git_marker);
                        auto nl = content.find('\n');
                        if (nl != std::string::npos) {
                            std::string url = content.substr(0, nl);
                            std::string commit = content.substr(nl + 1);
                            while (!commit.empty() &&
                                   (commit.back() == '\n' || commit.back() == '\r'))
                                commit.pop_back();
                            if (!url.empty() && !commit.empty()) {
                                lp.source = "git";
                                lp.source_url = url;
                                lp.commit = commit;
                                lf.packages.push_back(std::move(lp));
                                continue;   // sha256 stays empty
                            }
                        }
                    }

                    // Hash the built library — 1.2.0-dev.11: deterministic
                    // pick (shared with lockfile verify side) so record and
                    // verify hash the SAME archive.
                    auto build_dir = entry.path() / "build";
                    auto lib_file = util::find_package_archive(
                        build_dir, pkg_cfg.project.name);
                    if (!lib_file.empty()) {
                        lp.sha256 = crypto::sha256_file(lib_file);
                    }
                    lf.packages.push_back(std::move(lp));
                } catch (...) {
                    // Skip packages with broken configs
                }
            }
        }

        lockfile::save(proj_root, lf);
    } catch (...) {
        // Lockfile generation failure is non-fatal
    }
}

// 1.2.0-dev.7: Install a package directly from a source directory (no archive).
// The directory must be a valid package (ezmk.toml + include/ + src/ or
// precompiled/header-only). SHA-256 does not apply (no archive) — the caller
// emits a notice when --sha256 was requested. Shares the full post-validate
// processing with archive installs.
static void install_from_directory(const fs::path& dir, cli::Scope scope,
                                   bool assume_yes, bool no_lock) {
    util::info(ezmk::i18n::I18nKey::pkg_install_from_dir, {{"dir", dir.string()}});

    auto tc = toolchain::detect_toolchain();
    fs::path dest_dir = pkg_install_dir(scope);

    // Safety: global install confirmation (same as archive installs)
    if (scope == cli::Scope::Global) {
        if (!confirm(ezmk::i18n::get(ezmk::i18n::I18nKey::global_confirm), assume_yes)) {
            util::info(ezmk::i18n::I18nKey::install_cancelled);
            return;
        }
    }

    // No staging: the source directory IS the package root, so it is never
    // removed. Shares validate → hooks → deps → compile → copy → postinstall.
    process_installed_pkg(dir, dest_dir, scope, assume_yes, tc, {});

    // Lockfile generation (project scope only, unless --no-lock)
    maybe_write_lockfile(scope, no_lock, tc, dest_dir);
}

// 1.4.1: pkg install <git-url> — clone a git repository into a unique temp
// dir, then reuse the full directory-install chain (install_from_directory).
// `source_url` is the clone URL WITHOUT any "#ref" fragment; `ref` is the
// requested branch/tag ("" = the remote's default branch).
// `expected_commit` (--locked git reinstall): the url+commit come from
// ezmk.lock — a full clone + detached checkout of that exact commit must land
// on it, otherwise the source drifted (force-push) → fatal lock_commit_mismatch.
static void install_git_source(const std::string& source_url,
                               std::string_view ref,
                               cli::Scope scope,
                               bool assume_yes, bool no_lock,
                               const std::string* expected_commit = nullptr) {
    // git:// is plaintext (MITM) — warn + confirm, mirroring
    // url_integrity_confirm's http:// branch (1.1.3 S3 pattern).
    std::string lower_url = source_url;
    for (auto& c : lower_url) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower_url.rfind("git://", 0) == 0) {
        util::warn(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_git_plain_confirm));
        if (!confirm(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_git_plain_confirm), assume_yes)) {
            util::info(ezmk::i18n::I18nKey::install_cancelled);
            return;
        }
    }

    if (!util::git_available()) {
        util::fatal(ezmk::i18n::I18nKey::pkg_git_not_available);
    }

    // Unique temp clone dir (PID + high-res counter, like the extract staging).
    static std::atomic<uint64_t> counter{0};
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    fs::path clone_dir = fs::temp_directory_path() /
        ("ezmk_git_" + std::to_string(now_us) + "_" +
         std::to_string(counter.fetch_add(1, std::memory_order_relaxed)));

    util::info(ezmk::i18n::I18nKey::pkg_git_cloning, {{"url", source_url}});

    // Clone. Shallow by default (design: branches/tags AND the remote's
    // default branch all clone with --depth 1 — fastest path); only commit-SHA
    // pins need a full clone (a shallow clone cannot guarantee the SHA is
    // reachable) followed by a detached checkout of that exact commit.
    bool is_sha = ref.size() == 40 && ref.find_first_not_of("0123456789abcdefABCDEF") == std::string_view::npos;
    bool ok;
    if (is_sha || expected_commit) {
        ok = util::git_clone(source_url, clone_dir, "", /*shallow=*/false);
        if (ok) {
            // checkout <sha> → detached HEAD at the pinned commit
            std::string pin = expected_commit ? *expected_commit : std::string(ref);
            std::string cmd = "git -C \"" + util::escape_shell_arg(clone_dir.string()) +
                              "\" checkout --detach \"" + util::escape_shell_arg(pin) + "\"";
            auto res = util::run_command(cmd);
            ok = res.exit_code == 0;
        }
    } else {
        // branch/tag → --branch <ref> --depth 1; no ref → default branch,
        // --depth 1 (git_clone omits --branch when the branch is empty).
        ok = util::git_clone(source_url, clone_dir, ref, /*shallow=*/true);
    }
    if (!ok) {
        { std::error_code ec; fs::remove_all(clone_dir, ec); }
        if (expected_commit) {
            // The pinned commit could not be reproduced (force-push drift).
            util::fatal(ezmk::i18n::fmt(
                ezmk::i18n::I18nKey::lock_commit_mismatch,
                {{"expected", *expected_commit}, {"actual", "(unreachable)"}}));
        }
        util::fatal(std::string("failed to clone git repository: ") + source_url);
    }

    try {
        // Resolve the cloned HEAD BEFORE dropping .git — rev-parse needs the
        // repository metadata (this is the commit pin recorded for git sources).
        std::string head = util::git_head_commit(clone_dir);
        if (head.empty()) {
            throw std::runtime_error("failed to resolve HEAD of cloned repository: " + source_url);
        }
        // --locked: the clone must land exactly on the recorded commit —
        // otherwise the branch/tag was force-pushed or moved since the lockfile.
        if (expected_commit && head != *expected_commit) {
            throw std::runtime_error(ezmk::i18n::fmt(
                ezmk::i18n::I18nKey::lock_commit_mismatch,
                {{"expected", *expected_commit}, {"actual", head}}));
        }
        // The clone's .git metadata must not ride into the installed package.
        { std::error_code ec; fs::remove_all(clone_dir / ".git", ec); }
        // 1.4.1: provenance marker — install_from_directory copies the whole
        // clone dir into the installed package, so this lands at
        // <pkg>/.ezmk-git-source and lets maybe_write_lockfile re-derive
        // source="git" + source_url + commit on later lockfile regenerations
        // (the lockfile is rebuilt from installed dirs, not kept incrementally).
        util::file_write(clone_dir / ".ezmk-git-source",
                         source_url + "\n" + head + "\n");
        install_from_directory(clone_dir, scope, assume_yes, no_lock);
    } catch (...) {
        { std::error_code ec; fs::remove_all(clone_dir, ec); }
        throw;
    }
    { std::error_code ec; fs::remove_all(clone_dir, ec); }
}

void install(const std::string& pkg_file, cli::Scope scope,
             std::string_view expected_sha256,
             bool assume_yes,
             bool locked,
             bool no_lock,
             std::string_view branch_flag) {
    // 1.1.0: --locked mode — install from lockfile only
    // 1.2.0-dev.7: lockfile + config resolved against the located project root
    // 1.4.0-dev.5: --locked must actually PIN the version (previously it only
    // checked direct-dep specs and then installed the newest — silently
    // upgrading and REWRITING ezmk.lock, the exact drift --locked prevents).
    // Now: the lockfile's recorded version for this package becomes an Exact
    // constraint for the repo search, and the lockfile is never rewritten.
    std::string locked_version;    // non-empty only in --locked mode
    std::string locked_sha256;
    // 1.4.1: git-source lockfile entries are re-cloned at the recorded commit.
    std::string locked_git_url;
    std::string locked_git_commit;
    if (locked) {
        auto proj_root = util::locate_project_root(fs::current_path())
                            .value_or(fs::current_path());
        auto lf = lockfile::load(proj_root);
        if (!lf.has_value()) {
            util::fatal(ezmk::i18n::get(ezmk::i18n::I18nKey::lock_locked_missing));
        }
        if (lockfile::depends_changed(
                config::parse_config((proj_root / "ezmk.toml").string()), *lf)) {
            util::fatal(ezmk::i18n::get(ezmk::i18n::I18nKey::lock_locked_depends_mismatch));
        }
        // Find the package in the lockfile (by bare name; the CLI passes the
        // package name, not a constraint, in --locked mode). For git sources
        // the recorded source_url is the identity — a URL argument matches too.
        std::string pkg_name = pkg_file;
        if (!is_git_install_source(pkg_file)) {
            auto at = pkg_file.find('@');
            if (at != std::string::npos) pkg_name = pkg_file.substr(0, at);
        }
        bool found = false;
        for (const auto& lp : lf->packages) {
            if (lp.name == pkg_name) {
                found = true;
                if (lp.source == "git") {
                    locked_git_url = lp.source_url;
                    locked_git_commit = lp.commit;
                } else {
                    locked_version = lp.version;
                    locked_sha256 = lp.sha256;
                }
                break;
            }
        }
        // 1.4.1: --locked re-running a git URL → match the recorded source_url
        // (ignore the "#ref" fragment — the lockfile commit is authoritative).
        if (!found && is_git_install_source(pkg_file)) {
            std::string want = pkg_file;
            auto hash = want.find('#');
            if (hash != std::string::npos) want = want.substr(0, hash);
            for (const auto& lp : lf->packages) {
                if (lp.source == "git" && lp.source_url == want) {
                    locked_git_url = lp.source_url;
                    locked_git_commit = lp.commit;
                    found = true;
                    break;
                }
            }
        }
        if (!found || (locked_git_url.empty() && locked_version.empty())) {
            util::fatal(ezmk::i18n::fmt(
                ezmk::i18n::I18nKey::lock_locked_pkg_not_in_lockfile,
                {{"pkg", pkg_name}}));
        }
        // --locked never rewrites the lockfile (it is the source of truth).
        no_lock = true;
    }

    // 1.1.0: detect toolchain once for the entire install (MSVC-aware archiving)
    auto tc = toolchain::detect_toolchain();
    fs::path dest_dir = pkg_install_dir(scope);

    fs::path input(pkg_file);

    // 1.2.4: holds the repo-provided sha256 copy — expected_sha256 is a
    // string_view param and must not point into the short-lived search_result.
    std::string repo_sha;

    // 1.2.0-dev.7: directory install — a source directory is a valid package
    // source (include/ + src/ + ezmk.toml, or precompiled/header-only). No
    // archive → no SHA-256 verification (emit a notice if --sha256 was given).
    if (fs::is_directory(input)) {
        if (!expected_sha256.empty()) {
            util::warn(ezmk::i18n::I18nKey::pkg_sha256_skipped_dir);
        }
        install_from_directory(input, scope, assume_yes, no_lock);
        return;
    }

    // 1.4.1: --locked git source — the url+commit were resolved from ezmk.lock
    // above (matched by package name or by source_url). Clone at the recorded
    // commit and refuse on drift (lock_commit_mismatch).
    if (!locked_git_url.empty()) {
        if (!expected_sha256.empty()) {
            util::warn(ezmk::i18n::I18nKey::pkg_git_sha256_skipped);
        }
        install_git_source(locked_git_url, locked_git_commit, scope,
                           assume_yes, no_lock, &locked_git_commit);
        return;
    }

    // 1.4.1: git repository URL source — clone & install via the directory
    // chain. Detection order: local dir → git → archive URL → repo name.
    // Must run BEFORE the is_url heuristic: scp-style "git@host:user/repo.git"
    // (no "://") would otherwise be misread as a URL and mangled with an
    // https:// prefix; "…/repo.git" must never be downloaded as an archive.
    if (is_git_install_source(pkg_file)) {
        // A git source is pinned by its commit SHA — an explicit --sha256 has
        // no archive to verify (skip notice, mirroring directory installs).
        if (!expected_sha256.empty()) {
            util::warn(ezmk::i18n::I18nKey::pkg_git_sha256_skipped);
        }
        // 1.4.1: split "url[#ref]" — the fragment is the requested
        // branch/tag/commit (git clone rejects fragments; strip before clone
        // and pass the ref as the clone's branch selector). An explicit
        // --branch flag takes priority over the URL fragment (flag > #ref >
        // default branch).
        std::string base = pkg_file;
        std::string ref;
        auto hash = base.find('#');
        if (hash != std::string::npos) {
            ref = base.substr(hash + 1);
            base = base.substr(0, hash);
        }
        if (!branch_flag.empty()) {
            ref = std::string(branch_flag);
        }
        // Bare "github.com/user/repo.git" (no scheme, not scp/file:// style)
        // needs an explicit protocol for git clone.
        if (base.find("://") == std::string::npos && base.rfind("git@", 0) != 0) {
            base = "https://" + base;
        }
        install_git_source(base, ref, scope, assume_yes, no_lock);
        return;
    }

    // Determine if it's a URL or local file
    bool is_url = pkg_file.find("://") != std::string::npos
               || (pkg_file.find('.') != std::string::npos
                   && pkg_file.find('/') != std::string::npos
                   && !util::file_exists(fs::path(pkg_file)));

    // If no protocol, prepend https://
    std::string url;
    if (is_url && pkg_file.find("://") == std::string::npos) {
        url = "https://" + pkg_file;
    } else {
        url = pkg_file;
    }

    fs::path archive_path;

    if (is_url) {
        // 1.1.3 S3: URL 完整性前置确认（无 sha256 / 明文 http://），下载前中止
        if (!url_integrity_confirm(url, !expected_sha256.empty(), assume_yes)) {
            util::info(ezmk::i18n::I18nKey::install_cancelled);
            return;
        }
        // Download to temp
        fs::path tmp_dir = fs::temp_directory_path();
        // Extract filename from URL
        std::string fname = url;
        size_t last_slash = fname.rfind('/');
        if (last_slash != std::string::npos) fname = fname.substr(last_slash + 1);
        if (fname.empty()) fname = "package.tar.gz";
        archive_path = tmp_dir / fname;

        util::info(ezmk::i18n::I18nKey::downloading, {{"url", url}});
        util::download(url, archive_path);
    } else {
        archive_path = input;
        if (!util::file_exists(archive_path)) {
            // Not a local file or URL — try searching registered repos
            util::info(ezmk::i18n::I18nKey::searching_repos, {{"pkg", pkg_file}});
            auto search_result = [&]() {
                if (!locked_version.empty()) {
                    // --locked: pin to the exact version recorded in ezmk.lock.
                    config::VersionConstraint exact;
                    exact.op = config::VersionConstraint::Exact;
                    exact.version = locked_version;
                    return repo::search_package(pkg_file, {
                        cli::Scope::Project, cli::Scope::User, cli::Scope::Global},
                        exact);
                }
                return repo::search_package(pkg_file, {
                    cli::Scope::Project, cli::Scope::User, cli::Scope::Global});
            }();
            if (search_result.archive_path.empty() ||
                !util::file_exists(search_result.archive_path)) {
                if (!locked_version.empty()) {
                    util::fatal(ezmk::i18n::fmt(
                        ezmk::i18n::I18nKey::lock_locked_version_unavailable,
                        {{"pkg", pkg_file}, {"version", locked_version}}));
                }
                util::fatal(ezmk::i18n::I18nKey::not_found, {{"pkg", pkg_file}});
            }
            archive_path = search_result.archive_path;
            // Use sha256 from index.toml if user didn't provide one explicitly.
            // expected_sha256 is a string_view param — bind it to a local copy
            // (repo_sha) that outlives this block: search_result dies at the end
            // of the if, and a string_view into its sha256 would dangle (the
            // later SHA-256 check would read freed memory — CI-only corruption).
            if (expected_sha256.empty() && !search_result.sha256.empty()) {
                repo_sha = search_result.sha256;
                expected_sha256 = repo_sha;
            }
            // --locked: prefer the lockfile-recorded sha256 over the (possibly
            // drifted) index value — the lockfile is the source of truth.
            if (!locked_sha256.empty()) {
                repo_sha = locked_sha256;
                expected_sha256 = repo_sha;
            }
            util::info(ezmk::i18n::I18nKey::found_in_repo, {{"path", archive_path.string()}});

            // 1.2.4: directory package — the repo's `file` points at a directory
            // (index `type = "dir"`, or a bare directory path). Reuse the dev.7
            // directory-install path; a directory has no archive, so SHA-256
            // verification is skipped (notice when --sha256 / index sha256 given).
            if (fs::is_directory(archive_path)) {
                if (!expected_sha256.empty()) {
                    util::warn(ezmk::i18n::I18nKey::pkg_sha256_skipped_dir);
                }
                install_from_directory(archive_path, scope, assume_yes, no_lock);
                return;
            }
        }
    }

    // SHA-256 verification
    // 1.4.0-dev.5: local-archive sidecar auto-verification — when no explicit
    // --sha256 / index.toml sha256 was given AND the archive has a sibling
    // "<archive>.sha256" sidecar (1.3.5 pack output: "<hash>  <filename>"),
    // read it and verify. URL downloads never trust a companion sidecar
    // (坑 3: explicit --sha256 wins; a sidecar only fills an EMPTY expected
    // value, and a missing/malformed sidecar skips verification, not blocks).
    std::string sidecar_sha;
    if (expected_sha256.empty() && !is_url && !fs::is_directory(archive_path)) {
        fs::path sidecar(archive_path.string() + ".sha256");
        if (util::file_exists(sidecar)) {
            std::string content = util::file_read(sidecar);
            // Format "<hash>  <filename>\n" — take the first token (the hash).
            std::istringstream ss(content);
            std::string hash;
            ss >> hash;
            // 64 hex chars = a well-formed sha256; anything else → skip
            // (malformed sidecar is not a reason to block the install).
            if (hash.size() == 64 &&
                hash.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
                sidecar_sha = std::move(hash);
                expected_sha256 = sidecar_sha;
                util::info(ezmk::i18n::fmt(
                    ezmk::i18n::I18nKey::pkg_sha256_sidecar,
                    {{"file", sidecar.filename().string()}}));
            }
        }
    }
    if (!expected_sha256.empty()) {
        util::info(ezmk::i18n::I18nKey::verifying);
        std::string actual = crypto::sha256_file(archive_path);
        // Case-insensitive comparison
        std::string expected(expected_sha256);
        std::string actual_lower = actual;
        for (auto& c : expected) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (auto& c : actual_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (actual_lower != expected) {
            // Technical details first (untranslated), then fatal with translated message
            util::error("  expected: " + std::string(expected_sha256));
            util::error("  actual:   " + actual);
            util::fatal(ezmk::i18n::I18nKey::sha256_mismatch, {{"pkg", pkg_file}});
        }
        util::info(ezmk::i18n::I18nKey::sha256_ok);
    }

    // Safety: global install confirmation
    if (scope == cli::Scope::Global) {
        if (!confirm(ezmk::i18n::get(ezmk::i18n::I18nKey::global_confirm), assume_yes)) {
            util::info(ezmk::i18n::I18nKey::install_cancelled);
            return;
        }
    }

    // Extract to temp staging area — use PID + high-res counter to avoid collisions
    static std::atomic<uint64_t> counter{0};
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    fs::path stage = fs::temp_directory_path() /
        ("ezmk_pkg_" + std::to_string(now_us) + "_" +
         std::to_string(counter.fetch_add(1, std::memory_order_relaxed)));
    fs::create_directories(stage);

    try {
        util::info(ezmk::i18n::I18nKey::extracting);
        util::extract_archive(archive_path, stage);

        // The archive might have a top-level directory; find the actual package root
        fs::path pkg_root = stage;
        {
            std::vector<fs::path> subdirs;
            for (auto& e : fs::directory_iterator(stage)) {
                if (e.is_directory()) subdirs.push_back(e.path());
            }
            if (subdirs.size() == 1) {
                if (util::file_exists(subdirs[0] / "ezmk.toml")) {
                    pkg_root = subdirs[0];
                }
            }
        }

        // Shared post-validate processing: validate → hooks → deps → compile →
        // copy → postinstall (1.2.0-dev.7). Also shared by directory installs.
        process_installed_pkg(pkg_root, dest_dir, scope, assume_yes, tc, stage);
    } catch (...) {
        // Clean up staging on error (best-effort — a cleanup failure must not
        // mask the original error)
        { std::error_code ec; fs::remove_all(stage, ec); }
        throw;
    }

    // Cleanup temp (best-effort)
    { std::error_code ec; fs::remove_all(stage, ec); }

    // 1.1.0: generate/update ezmk.lock with resolved dependency snapshot
    maybe_write_lockfile(scope, no_lock, tc, dest_dir);
}

// ===================================================================
// Remove
// ===================================================================

void remove(const std::string& pkg_name, const std::vector<cli::Scope>& scopes) {
    util::validate_pkg_name(pkg_name);  // 1.1.3 S2: 用户输入名防自伤
    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (util::file_exists(pkg_path)) {
            util::info(ezmk::i18n::I18nKey::removing, {{"pkg", pkg_path.string()}});
            util::remove_all(pkg_path);
            return;
        }
    }
    util::error(ezmk::i18n::I18nKey::not_found, {{"pkg", pkg_name}});
}

// ===================================================================
// Search
// ===================================================================

std::vector<fs::path> search(const std::string& pkg_name,
                             const std::vector<cli::Scope>& scopes) {
    util::validate_pkg_name(pkg_name);  // 1.1.3 S2
    std::vector<fs::path> results;
    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (util::file_exists(pkg_path)) {
            results.push_back(pkg_path);
        }
    }
    return results;
}

// ===================================================================
// Info
// ===================================================================

namespace {
    std::string scope_name(cli::Scope s) {
        switch (s) {
        case cli::Scope::Project: return ezmk::i18n::get(ezmk::i18n::I18nKey::scope_project);
        case cli::Scope::User:    return ezmk::i18n::get(ezmk::i18n::I18nKey::scope_user);
        case cli::Scope::Global:  return ezmk::i18n::get(ezmk::i18n::I18nKey::scope_global);
        }
        return "unknown";
    }

    std::string format_time(const fs::path& p) {
        std::error_code ec;
        auto ftime = fs::last_write_time(p, ec);
        if (ec) return "(unknown)";

        // C++20: use clock_cast; pre-C++20 fallback
#if __cplusplus >= 202002L
        auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
#else
        // Convert file_time to system_clock by going through time_t
        auto sctp = std::chrono::system_clock::from_time_t(
            std::chrono::system_clock::to_time_t(
                std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now()
                    + std::chrono::system_clock::now())));
#endif
        auto tt = std::chrono::system_clock::to_time_t(sctp);
        auto* tm = std::localtime(&tt);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        return buf;
    }
}

void info(const std::string& pkg_name, const std::vector<cli::Scope>& scopes) {
    auto none_str = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_none);
    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (util::file_exists(pkg_path)) {
            auto cfg = config::parse_config(pkg_path / "ezmk.toml");
            util::info_line(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_name)
                            + ": " + cfg.project.name);
            util::info_line(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_version)
                            + ": " + cfg.project.version);
            std::string type_str = cfg.project.header_only
                ? ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_header_only)
                : cfg.project.type;
            util::info_line(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_type)
                            + ": " + type_str);
            util::info_line(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_language)
                            + ": " + cfg.project.language);
            util::info_line(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_scope)
                            + ": " + scope_name(scope));
            util::info_line(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_location)
                            + ": " + pkg_path.string());
            util::info_line(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_installed)
                            + ": " + format_time(pkg_path));
            // compile flags
            {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_compile_flags) + ":";
                for (auto& f : cfg.compile.flags) line += " " + f;
                if (cfg.compile.flags.empty()) line += none_str;
                util::info_line(line);
            }
            // include dirs
            {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_include_dirs) + ":";
                for (auto& d : cfg.compile.include_dirs) line += " " + d;
                if (cfg.compile.include_dirs.empty()) line += none_str;
                util::info_line(line);
            }
            // src dirs (1.2.0-dev.9)
            {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_src_dirs) + ":";
                for (auto& d : cfg.compile.src_dirs) line += " " + d;
                if (cfg.compile.src_dirs.empty()) line += none_str;
                util::info_line(line);
            }
            // hard deps
            {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_hard_deps) + ":";
                if (cfg.depends.libs.empty()) {
                    line += none_str;
                } else {
                    for (auto& d : cfg.depends.libs) {
                        line += " " + d.name;
                        if (d.constraint.op != config::VersionConstraint::None)
                            line += "@" + d.constraint.version;
                    }
                }
                util::info_line(line);
            }
            // optional deps
            {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_optional_deps) + ":";
                if (cfg.depends.want.empty()) {
                    line += none_str;
                } else {
                    for (auto& d : cfg.depends.want) {
                        line += " " + d.name;
                        if (d.constraint.op != config::VersionConstraint::None)
                            line += "@" + d.constraint.version;
                    }
                }
                util::info_line(line);
            }
            // link flags
            {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_link_flags) + ":";
                for (auto& f : cfg.link.flags) line += " " + f;
                if (cfg.link.flags.empty()) line += none_str;
                util::info_line(line);
            }
            // link dirs
            {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_link_dirs) + ":";
                for (auto& d : cfg.link.link_dirs) line += " " + d;
                if (cfg.link.link_dirs.empty()) line += none_str;
                util::info_line(line);
            }
            // system targets
            {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_system_targets) + ":";
                for (auto& t : cfg.link.system_targets) line += " " + t;
                if (cfg.link.system_targets.empty()) line += none_str;
                util::info_line(line);
            }

            // Show tools for utils packages
            if (cfg.project.type == "utils") {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_tools) + ":";
                if (cfg.utils.tools.empty()) {
                    line += none_str;
                } else {
                    for (size_t i = 0; i < cfg.utils.tools.size(); ++i) {
                        if (i > 0) line += ",";
                        line += " " + cfg.utils.tools[i];
                    }
                }
                util::info_line(line);
            }

            // 0.2.5+: Show declared utils permissions, if any.
            if (cfg.utils.permissions.has_value()) {
                const auto& pm = *cfg.utils.permissions;
                util::info_line(ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_permissions) + ":");
                auto print_list = [&](const char* label,
                                      const std::vector<std::string>& v) {
                    if (v.empty()) return;
                    std::string line = std::string("    ") + label + ":";
                    for (auto& e : v) line += " " + e;
                    util::info_line(line);
                };
                print_list("read", pm.read);
                print_list("read-deny", pm.read_deny);
                print_list("write", pm.write);
                print_list("write-deny", pm.write_deny);
                print_list("run", pm.run);
                print_list("run-deny", pm.run_deny);
                util::info_line("    (unlisted access will prompt at runtime)");
            }
            fs::path build_dir = pkg_path / "build";
            if (util::file_exists(build_dir)) {
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_artifacts) + ":";
                bool found = false;
                for (auto& f : fs::directory_iterator(build_dir)) {
                    auto ext = f.path().extension().string();
                    if (ext == ".a" || ext == ".dll" || ext == ".so") {
                        line += " " + f.path().filename().string();
                        found = true;
                    }
                }
                if (!found) line += none_str;
                util::info_line(line);
            }
            // 1.2.0-dev.10: precompiled packages — list available lib/ variants
            if (cfg.project.precompiled) {
                auto variants = list_precompiled_variants(pkg_path / "lib", cfg.project.name);
                std::string line = ezmk::i18n::get(ezmk::i18n::I18nKey::pkg_info_precompiled_variants) + ":";
                if (variants.empty()) {
                    line += none_str;
                } else {
                    for (auto& t : variants) line += " " + t;
                }
                util::info_line(line);
            }
            return;
        }
    }
    util::error(ezmk::i18n::I18nKey::not_found, {{"pkg", pkg_name}});
}

// 0.2.3+
void list(const std::vector<cli::Scope>& scopes) {
    for (auto scope : scopes) {
        std::string scope_label = (scope == cli::Scope::Project) ? "project" :
                                  (scope == cli::Scope::User) ? "user" : "global";
        util::info(ezmk::i18n::I18nKey::pkg_list_title, {{"scope", scope_label}});

        fs::path dir = pkg_install_dir(scope);
        if (!util::file_exists(dir)) {
            util::info(ezmk::i18n::I18nKey::pkg_list_none);
            continue;
        }

        std::vector<std::string> names;
        for (auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                names.push_back(entry.path().filename().string());
            }
        }
        std::sort(names.begin(), names.end());

        if (names.empty()) {
            util::info(ezmk::i18n::I18nKey::pkg_list_none);
            continue;
        }

        for (auto& name : names) {
            fs::path pkg_path = dir / name;
            auto toml = pkg_path / "ezmk.toml";
            if (util::file_exists(toml)) {
                try {
                    auto cfg = config::parse_config(toml);
                    std::string line = ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_list_item,
                        {{"name", cfg.project.name},
                         {"version", cfg.project.version},
                         {"type", cfg.project.type}});
                    if (cfg.project.type == "utils" && !cfg.utils.tools.empty()) {
                        line += " (tools:";
                        for (size_t i = 0; i < cfg.utils.tools.size(); ++i) {
                            if (i > 0) line += ",";
                            line += " " + cfg.utils.tools[i];
                        }
                        line += ")";
                    } else if (!cfg.depends.libs.empty()) {
                        line += " (depends:";
                        for (size_t i = 0; i < cfg.depends.libs.size(); ++i) {
                            if (i > 0) line += ",";
                            line += " " + cfg.depends.libs[i].name;
                        }
                        line += ")";
                    }
                    util::info(line);
                } catch (...) {
                    util::warn(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_list_parse_error,
                                                {{"name", name}}));
                }
            } else {
                util::info(ezmk::i18n::fmt(ezmk::i18n::I18nKey::pkg_list_no_toml,
                                            {{"name", name}}));
            }
        }
    }
}

// 0.2.3+
void update(const std::string& pkg_name, const std::vector<cli::Scope>& scopes) {
    // Find installed package in specified scopes (first match wins)
    cli::Scope found_scope = cli::Scope::Project;
    fs::path found_pkg_path;
    std::string installed_version;
    bool found = false;

    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        fs::path pkg_path = dir / pkg_name;
        if (!util::file_exists(pkg_path)) continue;

        auto cfg = config::parse_config(pkg_path / "ezmk.toml");
        found_scope = scope;
        found_pkg_path = pkg_path;
        installed_version = cfg.project.version;
        found = true;
        break;
    }

    if (!found) {
        util::error(ezmk::i18n::I18nKey::not_found, {{"pkg", pkg_name}});
        return;
    }

    // Search registered repos for the package (all scopes)
    auto search_result = repo::search_package(pkg_name, {
        cli::Scope::Project, cli::Scope::User, cli::Scope::Global});

    if (search_result.archive_path.empty() ||
        !util::file_exists(search_result.archive_path)) {
        util::info(ezmk::i18n::I18nKey::pkg_update_no_updates, {{"pkg", pkg_name}});
        return;
    }

    std::string repo_version = search_result.version;

    // Version comparison — semantic numeric comparison
    if (util::compare_version(repo_version, installed_version) == 0) {
        util::info(ezmk::i18n::I18nKey::pkg_update_up_to_date,
                   {{"pkg", pkg_name}, {"version", installed_version}});
        return;
    }

    util::info(ezmk::i18n::I18nKey::pkg_update_updating,
               {{"pkg", pkg_name}, {"old", installed_version}, {"new", repo_version}});

    // Use the install flow to handle download, verify, compile, and replace
    std::string sha256_hint;
    if (!search_result.sha256.empty()) {
        sha256_hint = search_result.sha256;
    }

    // Call install with the archive path found in the repo
    // Since install() accepts local paths, we pass the archive_path directly
    install(search_result.archive_path.string(), found_scope, sha256_hint, false);
}

// 0.2.4+
void update_all(const std::vector<cli::Scope>& scopes) {
    // Note: users should run 'ezmk repo update' first to refresh repo indices.
    int updated = 0;
    int up_to_date = 0;
    int failed = 0;

    for (auto scope : scopes) {
        fs::path dir = pkg_install_dir(scope);
        if (!util::file_exists(dir)) continue;

        for (auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;
            std::string pkg_name = entry.path().filename().string();

            // Read installed version
            auto toml = entry.path() / "ezmk.toml";
            if (!util::file_exists(toml)) continue;

            std::string installed_version;
            try {
                auto cfg = config::parse_config(toml);
                installed_version = cfg.project.version;
            } catch (...) {
                util::warn(std::string("failed to parse config for package: ") + pkg_name + " — skipping");
                ++failed;
                continue;
            }

            // Search repos for latest version
            auto result = repo::search_package(pkg_name, {
                cli::Scope::Project, cli::Scope::User, cli::Scope::Global});

            if (result.archive_path.empty() ||
                !util::file_exists(result.archive_path)) {
                ++up_to_date;
                continue;
            }

            // Compare versions
            if (util::compare_version(result.version, installed_version) <= 0) {
                ++up_to_date;
                continue;
            }

            // Update available — install latest
            util::info(ezmk::i18n::I18nKey::pkg_update_updating,
                       {{"pkg", pkg_name},
                        {"old", installed_version},
                        {"new", result.version}});

            std::string sha256_hint;
            if (!result.sha256.empty()) sha256_hint = result.sha256;

            try {
                install(result.archive_path.string(), scope, sha256_hint, false);
                ++updated;
            } catch (...) {
                util::warn(std::string("failed to update package: ") + pkg_name);
                ++failed;
            }
        }
    }

    // Summary
    if (updated > 0 || up_to_date > 0 || failed > 0) {
        std::string summary = std::to_string(updated) + " updated";
        if (up_to_date > 0) summary += ", " + std::to_string(up_to_date) + " already up-to-date";
        if (failed > 0) summary += ", " + std::to_string(failed) + " failed";
        util::info(summary);
    }
}

} // namespace ezmk::pkg
