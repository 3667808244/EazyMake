#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "ezmk/cli.hpp"
#include "ezmk/config.hpp"
#include "ezmk/toolchain.hpp"

namespace ezmk::pkg {
namespace fs = std::filesystem;

// ---- Path resolution ----
// Get the install directory for a given scope.
fs::path pkg_install_dir(cli::Scope scope);

// Resolve search paths for multiple scopes, in order.
std::vector<fs::path> pkg_search_dirs(const std::vector<cli::Scope>& scopes);

// ---- Package operations ----
// Install a package from a local file or URL into the given scope.
// expected_sha256: if non-empty, verify archive hash before extracting.
// assume_yes: skip all interactive prompts (for CI/scripts).
// locked: 1.1.0 — install from lockfile only, error on mismatch.
// no_lock: 1.1.0 — skip lockfile generation.
void install(const std::string& pkg_file, cli::Scope scope,
             std::string_view expected_sha256 = {},
             bool assume_yes = false,
             bool locked = false,
             bool no_lock = false);

// Remove a package: search scopes in order, delete the first match.
void remove(const std::string& pkg_name, const std::vector<cli::Scope>& scopes);

// Search for a package across scopes, returning paths where found.
std::vector<fs::path> search(const std::string& pkg_name,
                             const std::vector<cli::Scope>& scopes);

// Show information about a package (its ezmk.toml contents).
void info(const std::string& pkg_name, const std::vector<cli::Scope>& scopes);

// 0.2.3+: List all installed packages across the given scopes.
void list(const std::vector<cli::Scope>& scopes);

// 0.2.3+: Update an installed package to the latest version from registered repos.
void update(const std::string& pkg_name, const std::vector<cli::Scope>& scopes);

// 0.2.4+: Update all installed packages across the given scopes.
void update_all(const std::vector<cli::Scope>& scopes);

// ---- Dependency resolution ----
// Topologically sort a list of package directories by their [depends].lib order.
// Throws if a cycle is detected or a dependency is missing.
std::vector<fs::path> resolve_dependency_order(const std::vector<fs::path>& pkg_dirs);

// ---- Compile a package to a static library (.a / .lib) ----
// Returns the path to the compiled library file.
// dep_includes: extra -I paths for dependencies' include/ directories.
// tc: the detected toolchain (controls archiver selection + output extension).
fs::path compile_package(const fs::path& pkg_dir,
                         const std::vector<fs::path>& dep_includes = {},
                         const toolchain::Toolchain& tc = {});

// 1.1.2 S2: 构造静态库归档命令（MSVC: lib.exe /OUT:，GCC/Clang: ar rcs）。
// 所有路径经 util::escape_shell_arg 转义——对象路径源自归档内源文件名，
// 可能含 `$`/反引号/空格等字符，裸引号在 POSIX `sh -c` 下可命令注入。
std::string build_archive_command(bool is_msvc,
                                  const fs::path& lib_out,
                                  const std::vector<fs::path>& objects);

// 1.1.3 S3: URL 安装完整性前置确认。返回 false 表示用户取消（install 应中止）。
//  - 无 sha256 → 无法校验包完整性，警告 + 确认；
//  - 显式 http:// → 明文下载有 MITM 风险，警告 + 确认（建议 https://）。
// assume_yes 为 true 时（-y）全部跳过，行为与现有确认逻辑一致。
bool url_integrity_confirm(const std::string& url, bool has_sha256, bool assume_yes);

// 1.4.1: strict git-source detection for `pkg install` (a STRICT subset of
// util::is_git_url — archive URLs must never match). True only for:
//   - SSH scp-style:  "git@github.com:user/repo.git"
//   - explicit scheme: "git://host/repo.git" / "file:///tmp/repo.git"
//   - a URL (scheme or bare "host/user/repo") whose path ends ".git" after
//     stripping any "#ref" fragment: "https://github.com/user/repo.git[#v1]"
// Archive URLs (".zip" / ".tar.gz" / ".tgz"), local names and local paths
// are never git sources.
bool is_git_install_source(std::string_view s);

// 0.9.6+ — Check if a package version satisfies a version constraint.
// Returns true if `version` satisfies `constraint`.
bool satisfies_version_constraint(std::string_view version,
                                  const config::VersionConstraint& constraint);

// 0.9.10: Detect an install script in the package's script/ directory.
// Priority: .lua (cross-platform) → platform-specific fallback (.ps1/.bat on
// Windows, .sh on Linux/macOS). Returns the path if found, empty path otherwise.
fs::path detect_install_script(const fs::path& pkg_root,
                                std::string_view basename);

// 1.1.0-dev.7: Check if a package is available in any registered repo.
// Returns true if at least one registered repo has the package in its index.
bool package_available(std::string_view pkg_name);

// 1.1.0-dev.2: Select the precompiled archive matching the current platform
// from a lib/ directory. Priority: exact platform tag match (lib<name>.<tag>.a)
// > bare fallback (lib<name>.a). Throws std::runtime_error if no match.
// 1.2.0-dev.10: platform identifier extended to os-arch[-compiler][-abi] with
// a 4-level ABI-safe priority; signature unchanged (wraps
// select_precompiled_variant with the real platform/toolchain/strict flag).
fs::path select_precompiled_archive(const fs::path& lib_dir,
                                     const std::string& pkg_name);

// 1.2.0-dev.10: Pure matching core — selects the best precompiled variant for
// explicit consumer tags (platform "os-arch", compiler "gcc13", abi "abi11").
// 4-level priority: full (L4) > same-compiler (L3) > os-arch (L2) > bare (L1);
// same-compiler candidates with a different explicit abi are skipped
// (ABI-incompatible); ties broken by lexicographically smallest filename.
// Degradation to L2/L1 emits a warning when compiler_tag is non-empty, or a
// fatal error when strict is set. Exposed for deterministic unit tests — the
// public select_precompiled_archive() wraps it with the real environment
// (detect_platform_tag + detect_toolchain + the package's precompiled_strict).
fs::path select_precompiled_variant(const fs::path& lib_dir,
                                    const std::string& pkg_name,
                                    const std::string& platform_tag,
                                    const std::string& compiler_tag,
                                    const std::string& abi_tag,
                                    bool strict);

// 1.4.0-dev.3: Compile negotiation (semantics B) — a source package is compiled
// at max(pkg_min, consumer_min) instead of its own declared minimum, capped by
// the toolchain capability table (max_supported_std, dev.2) and the package's
// declared range upper bound (metadata promise). Returns the negotiated
// LanguageInfo (std_flag / min_ver / normalized_lang replaced); returns the
// original when there is no negotiation (no consumer / consumer weaker than the
// package / the caps pull the result back to the package minimum or below).
// Pure — no I/O. Precompiled packages never negotiate (no compilation).
config::LanguageInfo negotiate_package_std(const config::LanguageInfo& pkg_lang,
                                           std::optional<int> consumer_min,
                                           const toolchain::Toolchain& tc);

} // namespace ezmk::pkg
