#include "ezmk/util.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef EZMK_WIN
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <winhttp.h>
#elif defined(EZMK_MACOS)
  #include <csignal>
  #include <mach-o/dyld.h>
  #include <unistd.h>
  #include <sys/wait.h>
#else
  #include <csignal>
  #include <unistd.h>
  #include <sys/wait.h>
#endif

// ---- miniz (C API, compiled together) ----
// NOTE (1.1.2 S1): do NOT define MINIZ_NO_TIME / MINIZ_NO_ARCHIVE_WRITING_APIS
// here. The vendor sources (miniz_zip.c) are compiled WITHOUT those macros, so
// mz_zip_archive_file_stat would have a different layout (m_time field present
// vs absent) across TUs — mz_zip_reader_file_stat() writes the full struct and
// would corrupt extract_zip()'s local stat (shifted fields + out-of-bounds).
extern "C" {
#include "miniz.h"
}
#include "miniz_zip.h"
#include "miniz_tinfl.h"

namespace ezmk::util {

// ===================================================================
// Logging
// ===================================================================

// 0.2.3+: Global mutex for thread-safe console output during parallel builds.
static std::mutex g_log_mutex;

void info(std::string_view msg)  {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (supports_color())
        std::cerr << color::green << "[ezmk] " << color::reset << msg << "\n";
    else
        std::cerr << "[ezmk] " << msg << "\n";
}
void warn(std::string_view msg)  {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (supports_color())
        std::cerr << color::yellow << "[ezmk warn] " << color::reset << msg << "\n";
    else
        std::cerr << "[ezmk warn] " << msg << "\n";
}
void error(std::string_view msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (supports_color())
        std::cerr << color::red << "[ezmk error] " << color::reset << msg << "\n";
    else
        std::cerr << "[ezmk error] " << msg << "\n";
}

void fatal(std::string_view msg) {
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        if (supports_color())
            std::cerr << color::red << "[ezmk fatal] " << color::reset << msg << "\n";
        else
            std::cerr << "[ezmk fatal] " << msg << "\n";
    }
    throw ezmk::fatal_error(msg);
}

// 0.9.8+: structured info line — same prefix as info() but without color,
// intended for multi-line data output (pkg info, repo info, repo list).
void info_line(std::string_view msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cerr << "[ezmk] " << msg << "\n";
}

// ---- I18n-aware logging overloads ----

void info(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args) {
    info(ezmk::i18n::fmt(key, args));
}
void warn(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args) {
    warn(ezmk::i18n::fmt(key, args));
}
void error(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args) {
    error(ezmk::i18n::fmt(key, args));
}
void fatal(ezmk::i18n::I18nKey key, const std::map<std::string, std::string>& args) {
    fatal(ezmk::i18n::fmt(key, args));
}

// ===================================================================
// Color support
// ===================================================================

namespace color {
    const char* reset = "\033[0m";
    const char* green = "\033[32m";
    const char* yellow = "\033[33m";
    const char* red = "\033[31m";
    const char* cyan = "\033[36m";
    const char* bold = "\033[1m";
    const char* dim = "\033[2m";
}

static bool g_console_initialized = false;

// 0.2.6+: global color policy, set once from --color=<mode> (or left Auto).
static ColorMode g_color_mode = ColorMode::Auto;

void set_color_mode(ColorMode mode) {
    g_color_mode = mode;
#ifdef EZMK_WIN
    // Forcing color on may target a legacy conhost without VT100 enabled;
    // try to turn it on so escape codes render instead of showing as garbage.
    if (mode == ColorMode::Always) {
        init_console();
    }
#endif
}

ColorMode get_color_mode() {
    return g_color_mode;
}

void init_console() {
    if (g_console_initialized) return;
    g_console_initialized = true;

#ifdef EZMK_WIN
    // Enable VT100 processing on Windows 10+
    HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }
#endif
}

bool supports_color() {
    // Explicit --color=always/never (0.2.6+) overrides everything, including
    // the NO_COLOR environment variable (matching git/ls conventions).
    if (g_color_mode == ColorMode::Always) return true;
    if (g_color_mode == ColorMode::Never) return false;

    // ColorMode::Auto — respect NO_COLOR convention: https://no-color.org/
    const char* no_color = std::getenv("NO_COLOR");
    if (no_color && no_color[0] != '\0') return false;

    // Check if stderr is a terminal
#ifdef EZMK_WIN
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr == INVALID_HANDLE_VALUE || hErr == nullptr) return false;
    DWORD mode = 0;
    return GetConsoleMode(hErr, &mode) != 0;
#else
    return isatty(STDERR_FILENO);
#endif
}

bool stderr_is_tty() {
#ifdef EZMK_WIN
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr == INVALID_HANDLE_VALUE || hErr == nullptr) return false;
    DWORD mode = 0;
    return GetConsoleMode(hErr, &mode) != 0;
#else
    return isatty(STDERR_FILENO);
#endif
}

// 0.9.6+: Progress line with \r for in-place refresh.
// Thread-safe: uses the same g_log_mutex as info/warn/error.
void progress(std::string_view msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (supports_color())
        std::cerr << "\r" << color::green << "[ezmk] " << color::reset << msg << std::flush;
    else
        std::cerr << "\r[ezmk] " << msg << std::flush;
}

void progress_newline() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cerr << "\n";
}

std::string color_msg(const char* color, std::string_view msg) {
    if (!supports_color()) return std::string(msg);
    std::string result;
    result.reserve(strlen(color) + msg.size() + strlen(color::reset) + 1);
    result += color;
    result += msg;
    result += color::reset;
    return result;
}

// ===================================================================
// Filesystem
// ===================================================================

bool file_exists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

std::string file_read(const fs::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    f.seekg(0);
    std::string s(static_cast<size_t>(sz), '\0');
    f.read(s.data(), sz);
    return s;
}

bool file_write(const fs::path& p, std::string_view content) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) {
        error(std::string("cannot write: ") + p.string());
        return false;
    }
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f) {
        error(std::string("write failed: ") + p.string());
        return false;
    }
    return true;
}

void create_directories(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

void remove_all(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

void copy_recursive(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::copy(from, to, fs::copy_options::recursive, ec);
}

// 1.1.2 C1: atomic move of a build artifact into place. rename first; if that
// fails (e.g. the target is locked by a running exe / antivirus on Windows),
// fall back to copy_file + remove the temp. If both fail, fatal — a caller that
// ignores this would otherwise report success with a stale/missing artifact.
void atomic_rename(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec) return;
    ec.clear();
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    std::error_code rm_ec;
    fs::remove(from, rm_ec);
    if (ec) {
        fatal("failed to move build output into place: " + to.string() +
              " (" + ec.message() + ")");
    }
}

// 1.1.2 C5: TOML double-quoted string literal with escaping.
std::string toml_quote(std::string_view s) {
    std::string r;
    r.reserve(s.size() + 2);
    r += '"';
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\t': r += "\\t";  break;
            case '\r': r += "\\r";  break;
            default:   r += c;      break;
        }
    }
    r += '"';
    return r;
}

std::vector<fs::path> list_files(const fs::path& dir,
                                 const std::vector<std::string>& exts) {
    std::vector<fs::path> result;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        auto ext = entry.path().extension().string();
        for (auto& e : exts) {
            if (ext == e) {
                result.push_back(entry.path());
                break;
            }
        }
    }
    return result;
}

// 1.1.0-dev.2: Returns a simplified platform tag (e.g. "win-x64", "linux-x64", "mac-arm64").
// Used for precompiled package file matching and index.toml platform filtering.
// Distinct from repo.cpp's build_platform_key() which uses "os_arch_toolchain" triplets.
std::string detect_platform_tag() {
    std::string os;
#ifdef EZMK_WIN
    os = "win";
#elif defined(EZMK_MACOS)
    os = "mac";
#else
    os = "linux";
#endif

    std::string arch;
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
    arch = "x64";
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    arch = "arm64";
#elif defined(__i386__) || defined(__i686__) || defined(_M_IX86)
    arch = "x86";
#else
    arch = "unknown";
#endif

    return os + "-" + arch;
}

fs::path get_home_dir() {
#ifdef EZMK_WIN
    // 0.2.3+: Check HOME first for Git Bash / MSYS2 compatibility
    const char* home = std::getenv("HOME");
    if (home) return fs::path(home);
    home = std::getenv("USERPROFILE");
    if (home) return fs::path(home);
    const char* homeDrive = std::getenv("HOMEDRIVE");
    const char* homePath  = std::getenv("HOMEPATH");
    if (homeDrive && homePath) return fs::path(std::string(homeDrive) + homePath);
    return fs::path("C:/Users");
#else
    const char* home = std::getenv("HOME");
    if (home) return fs::path(home);
    return fs::path("/tmp");
#endif
}

fs::path get_exe_dir() {
#ifdef EZMK_WIN
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    if (len > 0) return fs::path(std::string(buf, len)).parent_path();
    return fs::current_path();
#elif defined(EZMK_MACOS)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0)
        return fs::path(std::string(buf)).parent_path();
    return fs::current_path();
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) return fs::path(std::string(buf, n)).parent_path();
    return fs::current_path();
#endif
}

// ===================================================================
// Archive creation (1.1.0-dev.2)
// ===================================================================

// 1.1.0-dev.2: Create a .tar.gz archive from a source directory.
// Uses miniz for gzip compression (raw deflate) + hand-rolled ustar tar.
void create_targz(const fs::path& source_dir, const fs::path& output_file) {
    // --- Step 1: walk source_dir and collect sorted entries ---
    struct TarEntry {
        std::string name;
        std::vector<uint8_t> content;
        bool is_dir = false;
    };
    std::vector<TarEntry> entries;
    for (auto& e : fs::recursive_directory_iterator(source_dir)) {
        TarEntry te;
        auto rel = fs::relative(e.path(), source_dir);
        te.name = rel.generic_string();
        // tar convention: forward slashes
        std::replace(te.name.begin(), te.name.end(), '\\', '/');
        if (e.is_directory()) {
            te.is_dir = true;
            if (!te.name.empty() && te.name.back() != '/') te.name += '/';
        } else {
            std::string raw = file_read(e.path());
            te.content.assign(raw.begin(), raw.end());
        }
        entries.push_back(std::move(te));
    }
    std::sort(entries.begin(), entries.end(),
              [](const TarEntry& a, const TarEntry& b) { return a.name < b.name; });

    // --- Step 2: build tar byte stream (ustar format) ---
    std::vector<uint8_t> tar;
    auto pad512 = [&]() {
        size_t rem = tar.size() % 512;
        if (rem) tar.insert(tar.end(), 512 - rem, 0);
    };

    for (auto& entry : entries) {
        // Split name into prefix+name if > 100 chars (ustar extension)
        std::string hdr_name = entry.name;
        std::string hdr_prefix;
        if (hdr_name.size() > 100) {
            // Find a '/' within the first ~155 chars to split at
            auto slash = hdr_name.find('/', hdr_name.size() > 155 ? hdr_name.size() - 155 : 0);
            if (slash != std::string::npos && slash < 155) {
                hdr_prefix = hdr_name.substr(0, slash);
                hdr_name = hdr_name.substr(slash + 1);
            }
        }
        if (hdr_name.size() > 100) hdr_name.resize(100);

        std::array<char, 512> hdr{};
        auto set_field = [&](int off, int len, const std::string& v) {
            for (size_t i = 0; i < v.size() && i < (size_t)len - 1; ++i)
                hdr[off + i] = v[i];
        };
        auto set_octal = [&](int off, int len, size_t v) {
            char buf[32];
            int n = snprintf(buf, sizeof(buf), "%zo", v);
            // right-align: pad with '0's on the left
            int start = len - 1 - n;
            if (start < 0) start = 0;
            for (int i = 0; i < n && (start + i) < len - 1; ++i)
                hdr[off + start + i] = buf[i];
            // null terminate
            hdr[off + len - 1] = '\0';
        };

        set_field(0, 100, hdr_name);
        set_octal(100, 8, entry.is_dir ? 0755 : 0644); // mode
        set_octal(108, 8, 0);                           // uid
        set_octal(116, 8, 0);                           // gid
        set_octal(124, 12, entry.content.size());       // size
        set_octal(136, 12, 0);                          // mtime = 0 (deterministic)
        hdr[156] = entry.is_dir ? '5' : '0';            // typeflag
        set_field(157, 100, "");                        // linkname
        set_field(257, 6, "ustar");                     // magic
        set_field(263, 2, "00");                        // version
        set_field(265, 32, "");                         // uname
        set_field(297, 32, "");                         // gname
        set_field(329, 8, "");                          // devmajor
        set_field(337, 8, "");                          // devminor
        set_field(345, 155, hdr_prefix);                // prefix

        // Checksum: sum all bytes with chksum field treated as 8 spaces
        for (int i = 148; i < 156; ++i) hdr[i] = ' ';
        unsigned sum = 0;
        for (auto c : hdr) sum += static_cast<unsigned char>(c);
        set_octal(148, 7, sum); // 6 octal digits + null = 7 bytes, null at 155

        tar.insert(tar.end(), hdr.begin(), hdr.end());
        if (!entry.is_dir && !entry.content.empty()) {
            tar.insert(tar.end(), entry.content.begin(), entry.content.end());
            pad512();
        }
    }
    // Two zero blocks mark end of archive
    tar.insert(tar.end(), 1024, 0);

    // --- Step 3: gzip compress with miniz (raw deflate) ---
    // Gzip header: ID1=0x1f, ID2=0x8b, CM=0x08(deflate), FLG=0, MTIME=0, XFL=0, OS=255(unknown)
    const uint8_t gzip_hdr[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};

    z_stream strm{};
    // -MZ_DEFAULT_WINDOW_BITS = raw deflate (no zlib/adler32 wrapper)
    mz_deflateInit2(&strm, MZ_DEFAULT_COMPRESSION, Z_DEFLATED,
                    -MZ_DEFAULT_WINDOW_BITS, 8, Z_DEFAULT_STRATEGY);

    mz_ulong bound = mz_deflateBound(&strm, static_cast<mz_ulong>(tar.size()));
    std::vector<uint8_t> deflated(bound);

    strm.next_in = tar.data();
    strm.avail_in = static_cast<mz_uint>(tar.size());
    strm.next_out = deflated.data();
    strm.avail_out = static_cast<mz_uint>(deflated.size());

    int rc = mz_deflate(&strm, MZ_FINISH);
    if (rc != MZ_STREAM_END) {
        mz_deflateEnd(&strm);
        throw std::runtime_error("gzip compression failed (rc=" + std::to_string(rc) + ")");
    }
    mz_deflateEnd(&strm);

    size_t deflated_size = strm.total_out;
    deflated.resize(deflated_size);

    // --- Step 4: write gzip file (header + deflated data + footer) ---
    // Gzip footer: CRC32 (4 bytes LE) + uncompressed size (4 bytes LE, mod 2^32)
    mz_ulong crc = mz_crc32(MZ_CRC32_INIT, tar.data(), tar.size());
    mz_uint32 isize = static_cast<mz_uint32>(tar.size());

    std::vector<uint8_t> gz;
    gz.insert(gz.end(), gzip_hdr, gzip_hdr + sizeof(gzip_hdr));
    gz.insert(gz.end(), deflated.begin(), deflated.end());
    gz.push_back(static_cast<uint8_t>(crc & 0xff));
    gz.push_back(static_cast<uint8_t>((crc >> 8) & 0xff));
    gz.push_back(static_cast<uint8_t>((crc >> 16) & 0xff));
    gz.push_back(static_cast<uint8_t>((crc >> 24) & 0xff));
    gz.push_back(static_cast<uint8_t>(isize & 0xff));
    gz.push_back(static_cast<uint8_t>((isize >> 8) & 0xff));
    gz.push_back(static_cast<uint8_t>((isize >> 16) & 0xff));
    gz.push_back(static_cast<uint8_t>((isize >> 24) & 0xff));

    // Atomic write
    fs::create_directories(output_file.parent_path());
    auto tmp = output_file.string() + ".tmp";
    {
        std::ofstream of(tmp, std::ios::binary);
        if (!of) throw std::runtime_error("cannot create: " + tmp);
        of.write(reinterpret_cast<const char*>(gz.data()), gz.size());
        if (!of) throw std::runtime_error("write failed: " + tmp);
    }
    std::error_code ec;
    fs::rename(tmp, output_file, ec);
    if (ec) throw std::runtime_error("rename failed: " + output_file.string());
}

// ===================================================================
// Archive extraction
// ===================================================================

// 1.1.2 S1: 解压输出大小上限，防止 zip-bomb（解压后数据超过该值直接拒绝）。
// tar.gz 路径先把整个解压结果读进内存，故必须有界。
constexpr size_t kMaxDecompressedSize = size_t(1) << 30; // 1 GiB

// 1.1.2 S1: 把归档条目名解析为 dest 内的安全目标路径。
// 归档条目名是不可信输入，须同时防三种逃逸：
//   - `..` 分量（`../evil`、`..\evil`）——`\` 与 `/` 一律视为分隔符处理；
//   - 绝对路径（以 `/` 或 `\` 开头，POSIX 下 operator/ 会整体替换 dest）；
//   - 盘符 / UNC（`C:\...`、`\\server\share`，仅 Windows 有意义，统一拒绝）。
// 最后再经 lexically_normal 做一次目录边界兜底检查，任何一步违规抛 runtime_error。
static fs::path safe_extract_path(const fs::path& dest, std::string_view entry) {
    std::string name(entry);

    // 盘符（`C:`）与 UNC 前缀（`\\server` / `//server`）
    if (name.size() >= 2 && std::isalpha(static_cast<unsigned char>(name[0])) && name[1] == ':') {
        throw std::runtime_error("unsafe path in archive: " + name);
    }
    if (name.compare(0, 2, "\\\\") == 0 || name.compare(0, 2, "//") == 0) {
        throw std::runtime_error("unsafe path in archive: " + name);
    }

    // 绝对路径
    if (!name.empty() && (name.front() == '/' || name.front() == '\\')) {
        throw std::runtime_error("unsafe path in archive: " + name);
    }

    // 按 `/` 与 `\` 切分，拒绝任何 `..` 分量（`.`. 分量无害，normalize 时消除）
    std::string component;
    for (size_t i = 0; i <= name.size(); ++i) {
        char c = (i < name.size()) ? name[i] : '\0';
        if (i == name.size() || c == '/' || c == '\\') {
            if (component == "..") {
                throw std::runtime_error("unsafe path in archive: " + name);
            }
            component.clear();
        } else {
            component += c;
        }
    }

    // 兜底：normalize 后必须仍在 dest 之内（前缀 + 目录边界，防上述检查漏判）
    fs::path joined = (dest / name).lexically_normal();
    std::string jp = joined.generic_string();
    std::string dp = dest.lexically_normal().generic_string();
    if (jp.size() < dp.size() ||
        jp.compare(0, dp.size(), dp) != 0 ||
        (jp.size() > dp.size() && jp[dp.size()] != '/')) {
        throw std::runtime_error("unsafe path in archive: " + name);
    }
    return joined;
}

void extract_zip(const fs::path& archive, const fs::path& dest) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archive.string().c_str(), 0)) {
        throw std::runtime_error("failed to open ZIP: " + archive.string());
    }
    mz_uint num = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < num; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        fs::path out = safe_extract_path(dest, stat.m_filename);
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            fs::create_directories(out);
        } else {
            fs::create_directories(out.parent_path());
            if (!mz_zip_reader_extract_to_file(&zip, i, out.string().c_str(), 0)) {
                mz_zip_reader_end(&zip);
                throw std::runtime_error("failed to extract: " + std::string(stat.m_filename));
            }
        }
    }
    mz_zip_reader_end(&zip);
}

// Gzip header parsing: returns offset to start of deflate data
static size_t skip_gzip_header(const uint8_t* data, size_t len) {
    if (len < 10 || data[0] != 0x1f || data[1] != 0x8b || data[2] != 0x08) {
        throw std::runtime_error("not a valid gzip file");
    }
    size_t pos = 10;
    uint8_t flags = data[3];
    if (flags & 0x04) { // FEXTRA
        if (pos + 2 > len) throw std::runtime_error("truncated gzip header");
        uint16_t xlen = data[pos] | (uint16_t(data[pos + 1]) << 8);
        pos += 2 + xlen;
    }
    if (flags & 0x08) { // FNAME
        while (pos < len && data[pos] != 0) ++pos;
        ++pos; // skip null
    }
    if (flags & 0x10) { // FCOMMENT
        while (pos < len && data[pos] != 0) ++pos;
        ++pos;
    }
    if (flags & 0x02) { // FHCRC
        pos += 2;
    }
    return pos;
}

void extract_targz(const fs::path& archive, const fs::path& dest) {
    // Read compressed file
    std::string compressed = file_read(archive);
    if (compressed.empty()) throw std::runtime_error("cannot read archive: " + archive.string());

    const auto* src = reinterpret_cast<const uint8_t*>(compressed.data());
    size_t src_len = compressed.size();

    size_t data_off = skip_gzip_header(src, src_len);

    // Decompress with tinfl — gzip uses raw deflate (no zlib header).
    // The gzip header was already stripped by skip_gzip_header.
    std::vector<uint8_t> out;
    // 1.1.2 S1: 初始容量与增长都封顶，超限抛错（防 zip-bomb / OOM）
    size_t initial = compressed.size() * 4;
    out.resize(std::min<size_t>(std::max<size_t>(initial, 1), kMaxDecompressedSize));
    size_t out_pos = 0;

    tinfl_decompressor inflator{};
    tinfl_init(&inflator);

    size_t in_pos = data_off;
    while (in_pos < src_len) {
        size_t in_bytes = src_len - in_pos;
        size_t out_bytes = out.size() - out_pos;
        if (out_bytes == 0) {
            size_t new_size = std::min<size_t>(out.size() * 2, kMaxDecompressedSize);
            if (new_size <= out_pos) {
                throw std::runtime_error("gzip decompression exceeds size limit");
            }
            out.resize(new_size);
            out_bytes = out.size() - out_pos;
        }

        tinfl_status st = tinfl_decompress(
            &inflator,
            src + in_pos, &in_bytes,
            out.data(), out.data() + out_pos, &out_bytes,
            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF
        );

        in_pos += in_bytes;
        out_pos += out_bytes;

        if (st == TINFL_STATUS_DONE) break;
        if (st == TINFL_STATUS_FAILED) {
            throw std::runtime_error("gzip decompression failed");
        }
    }

    out.resize(out_pos);

    // Parse tar from decompressed data
    // Tar format: 512-byte header blocks, each file entry has:
    //   name[100] | mode[8] | uid[8] | gid[8] | size[12] | mtime[12] | chksum[8] | typeflag[1] | linkname[100] | ...
    //   File data follows, padded to 512 bytes.
    //   End of archive: two consecutive zero-filled 512-byte blocks.

    auto octal_to_size = [](const char* s, size_t len) -> size_t {
        size_t v = 0;
        for (size_t i = 0; i < len && s[i] && s[i] != ' '; ++i) {
            v = (v << 3) | (s[i] - '0');
        }
        return v;
    };

    size_t off = 0;
    while (off + 512 <= out.size()) {
        const uint8_t* blk = out.data() + off;

        // Check for end-of-archive (all zeros)
        bool all_zero = true;
        for (int i = 0; i < 512; ++i) {
            if (blk[i] != 0) { all_zero = false; break; }
        }
        if (all_zero) {
            // Check for second zero block
            bool all_zero2 = true;
            if (off + 1024 <= out.size()) {
                for (int i = 512; i < 1024; ++i) {
                    if (out[off + i] != 0) { all_zero2 = false; break; }
                }
            }
            if (all_zero2) break; // normal end
            break;
        }

        std::string name(reinterpret_cast<const char*>(blk), std::min(size_t(100), out.size() - off));
        name = name.c_str(); // trim at null
        size_t fsize = octal_to_size(reinterpret_cast<const char*>(blk + 124), 12);
        char typeflag = static_cast<char>(blk[156]);

        off += 512;

        if (typeflag == '0' || typeflag == '\0') {
            // Regular file
            fs::path outpath = safe_extract_path(dest, name);
            fs::create_directories(outpath.parent_path());
            if (off + fsize <= out.size()) {
                std::ofstream fout(outpath, std::ios::binary);
                fout.write(reinterpret_cast<const char*>(out.data() + off), fsize);
            }
        } else if (typeflag == '5') {
            // Directory
            fs::create_directories(safe_extract_path(dest, name));
        }
        // Skip data, rounded up to 512
        off += (fsize + 511) & ~511ULL;
    }
}

void extract_archive(const fs::path& archive, const fs::path& dest) {
    auto ext = archive.extension().string();
    auto name = archive.filename().string();
    // Handle .tar.gz
    auto ends_with = [](const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (ends_with(name, ".tar.gz") || ends_with(name, ".tgz")) {
        extract_targz(archive, dest);
    } else if (ext == ".zip") {
        extract_zip(archive, dest);
    } else {
        throw std::runtime_error("unsupported archive format: " + archive.string());
    }
}

// ===================================================================
// HTTP download
// ===================================================================

void download(std::string_view url_sv, const fs::path& dest) {
    std::string url(url_sv);

#ifdef EZMK_WIN
    // Parse URL
    bool https = false;
    std::string host, path = "/";

    auto starts_with = [](const std::string& s, const std::string& prefix) {
        return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
    };
    if (starts_with(url, "https://")) {
        https = true;
        url = url.substr(8);
    } else if (starts_with(url, "http://")) {
        url = url.substr(7);
    }

    size_t slash = url.find('/');
    if (slash != std::string::npos) {
        path = url.substr(slash);
        host = url.substr(0, slash);
    } else {
        host = url;
    }

    std::wstring whost(host.begin(), host.end());
    std::wstring wpath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(
        L"EazyMake/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) throw std::runtime_error("WinHttpOpen failed");

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(),
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("WinHttpConnect failed");
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("WinHttpOpenRequest failed");
    }

    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("WinHttpSendRequest failed");
    }

    ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("WinHttpReceiveResponse failed");
    }

    // Check HTTP status code — reject non-2xx responses
    {
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX)) {
            if (statusCode < 200 || statusCode >= 300) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error(
                    "download failed: HTTP " + std::to_string(statusCode) +
                    " for " + std::string(url_sv));
            }
        }
        // If QueryHeaders fails, continue anyway (conservative — the server
        // might not support this query, but we already have a response)
    }

    // Read response
    fs::create_directories(dest.parent_path());
    std::ofstream fout(dest, std::ios::binary);

    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    char buf[8192];
    do {
        dwSize = 0;
        if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            DWORD toRead = (dwSize < sizeof(buf)) ? dwSize : sizeof(buf);
            if (WinHttpReadData(hRequest, buf, toRead, &dwDownloaded)) {
                fout.write(buf, dwDownloaded);
            }
        }
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

#else
    // Fallback: use curl command — escape single quotes in both URL and dest path.
    // Single-quote style: break out of quotes, insert escaped quote, re-enter quotes.
    auto escape_sq = [](const std::string& s) -> std::string {
        std::string r;
        for (char c : s) {
            if (c == '\'') r += "'\\''";
            else r += c;
        }
        return r;
    };
    std::string escaped_url = escape_sq(url);
    std::string escaped_dest = escape_sq(dest.string());
    std::string cmd = "curl -sL -o '" + escaped_dest + "' '" + escaped_url + "'";
    auto res = run_command(cmd);
    if (res.exit_code != 0) {
        throw std::runtime_error("download failed: " + res.err);
    }
#endif
}

// ===================================================================
// Process
// ===================================================================

// 1.1.2: int overload (backward compat) forwards to the RunOptions version.
ProcResult run_command(const std::string& cmd, int timeout_sec) {
    RunOptions opts;
    opts.timeout_sec = timeout_sec;
    return run_command(cmd, opts);
}

#ifdef EZMK_WIN
// Build an environment block (double-null-terminated "K=V\0...") that is the
// current process environment with `extra` applied (keys added or replaced).
// Returns an empty vector when extra is empty → caller passes nullptr (inherit).
// The child gets a private environment, so no parent-global mutation / race.
static std::vector<char> build_env_block(const std::map<std::string, std::string>& extra) {
    std::vector<char> block;
    if (extra.empty()) return block;
    char* env = GetEnvironmentStringsA();
    if (env) {
        for (char* p = env; *p; p += strlen(p) + 1) {
            std::string entry(p);
            auto eq = entry.find('=');
            std::string key = (eq == std::string::npos) ? entry : entry.substr(0, eq);
            if (extra.find(key) == extra.end()) {
                block.insert(block.end(), entry.begin(), entry.end());
                block.push_back('\0');
            }
            // else: replaced by `extra` below
        }
        FreeEnvironmentStringsA(env);
    }
    for (auto& [k, v] : extra) {
        std::string entry = k + "=" + v;
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back('\0');
    }
    block.push_back('\0');
    return block;
}
#endif

ProcResult run_command(const std::string& cmd, const RunOptions& opts) {
    ProcResult result{};
#ifdef EZMK_WIN
    HANDLE hReadOut, hWriteOut, hReadErr, hWriteErr;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    CreatePipe(&hReadOut, &hWriteOut, &sa, 0);
    SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&hReadErr, &hWriteErr, &sa, 0);
    SetHandleInformation(hReadErr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWriteOut;
    si.hStdError  = hWriteErr;

    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    // 1.1.2: optional working directory + private environment block
    std::string cwd_str = opts.cwd.empty() ? std::string() : opts.cwd.string();
    LPCSTR cwd_ptr = cwd_str.empty() ? nullptr : cwd_str.c_str();
    std::vector<char> env_block = build_env_block(opts.env);
    LPVOID env_ptr = env_block.empty() ? nullptr : env_block.data();

    if (CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                       0, env_ptr, cwd_ptr, &si, &pi)) {
        CloseHandle(hWriteOut);
        CloseHandle(hWriteErr);

        // Drain whatever is currently available without blocking (non-blocking
        // reads via PeekNamedPipe). A normal child that exits flushes its own
        // handles; a child that spawns grandchildren may hold the pipe open
        // longer than we wait — but we must never block here, or a hung child
        // would defeat the timeout.
        auto drain_pipe = [](HANDLE hPipe, std::string& sink) {
            for (;;) {
                DWORD avail = 0;
                if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &avail, nullptr)) break;
                if (avail == 0) break;
                char buf[4096];
                DWORD n = avail > sizeof(buf) - 1 ? sizeof(buf) - 1 : avail;
                if (!ReadFile(hPipe, buf, n, &n, nullptr) || n == 0) break;
                sink.append(buf, n);
            }
        };

        auto deadline = std::chrono::steady_clock::now();
        if (opts.timeout_sec > 0) deadline += std::chrono::seconds(opts.timeout_sec);
        for (;;) {
            drain_pipe(hReadOut, result.out);
            drain_pipe(hReadErr, result.err);

            DWORD wr = WaitForSingleObject(pi.hProcess, 25);
            if (wr == WAIT_OBJECT_0) break;           // child exited
            if (wr != WAIT_TIMEOUT) break;            // wait failed
            if (opts.timeout_sec > 0 && std::chrono::steady_clock::now() >= deadline) {
                TerminateProcess(pi.hProcess, 1);
                WaitForSingleObject(pi.hProcess, INFINITE);
                result.timed_out = true;
                break;
            }
        }
        // Final drain: the child has exited, capture whatever remains buffered.
        drain_pipe(hReadOut, result.out);
        drain_pipe(hReadErr, result.err);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exit_code = static_cast<int>(exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        // CreateProcess failed — command not found or not executable
        result.exit_code = 1;
    }
    CloseHandle(hReadOut);
    CloseHandle(hReadErr);
#else
    // Use temporary files to capture stdout and stderr separately.
    // Respect $TMPDIR; fall back to /tmp (POSIX).
    const char* tmpdir = std::getenv("TMPDIR");
    std::string tmp_prefix = tmpdir ? std::string(tmpdir) : "/tmp";
    if (!tmp_prefix.empty() && tmp_prefix.back() != '/') tmp_prefix += '/';

    std::string out_tmpl = tmp_prefix + "ezmk_stdout_XXXXXX";
    std::string err_tmpl = tmp_prefix + "ezmk_stderr_XXXXXX";
    // mkstemp modifies the buffer in-place; use &out_tmpl[0] (C++11 guarantees
    // contiguous storage for std::string).
    int out_fd = mkstemp(&out_tmpl[0]);
    int err_fd = mkstemp(&err_tmpl[0]);
    if (out_fd < 0 || err_fd < 0) {
        if (out_fd >= 0) { close(out_fd); unlink(out_tmpl.c_str()); }
        if (err_fd >= 0) { close(err_fd); unlink(err_tmpl.c_str()); }
        return result;
    }

    // Wrap the user command in a brace group so our stdout/stderr redirections
    // apply to the whole command. Appending "1>out 2>err" directly composes
    // incorrectly with any fd redirection inside `cmd` (e.g. `echo x >&2` would
    // otherwise land in the stdout capture because the later `1>out` overrides
    // the user's `>&2`). The group makes the outer redirections authoritative.
    std::string cmd2 = "{ " + cmd + " ; } 1>" + out_tmpl + " 2>" + err_tmpl;

    pid_t pid = fork();
    if (pid == 0) {
        // Child: apply cwd + extra env BEFORE exec — fork() copy-on-write means
        // these only affect the child, so no parent-global mutation / race.
        // (1.1.2 C4/C7: this is how run_script cwd and SOURCE_DATE_EPOCH reach
        // the child without process-global setenv in a multi-threaded build.)
        if (!opts.cwd.empty() && chdir(opts.cwd.string().c_str()) != 0) {
            _exit(126); // chdir failed
        }
        for (auto& [k, v] : opts.env) {
            setenv(k.c_str(), v.c_str(), 1);
        }
        // Bind stdout/stderr to the temp files, then exec the shell.
        dup2(out_fd, STDOUT_FILENO);
        dup2(err_fd, STDERR_FILENO);
        close(out_fd);
        close(err_fd);
        execl("/bin/sh", "sh", "-c", cmd2.c_str(), (char*)nullptr);
        _exit(127); // exec failed
    } else if (pid > 0) {
        close(out_fd);
        close(err_fd);

        int status = 0;
        if (opts.timeout_sec > 0) {
            // Poll waitpid (WNOHANG) against a deadline; SIGKILL on timeout.
            auto deadline = std::chrono::steady_clock::now()
                + std::chrono::seconds(opts.timeout_sec);
            bool timed_out = false;
            for (;;) {
                pid_t r = waitpid(pid, &status, WNOHANG);
                if (r == pid) break;               // reaped normally
                if (r < 0) break;                  // waitpid error
                if (std::chrono::steady_clock::now() >= deadline) {
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);      // reap the zombie
                    result.exit_code = 1;
                    result.timed_out = true;
                    timed_out = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (!timed_out && WIFEXITED(status)) {
                result.exit_code = WEXITSTATUS(status);
            } else if (!timed_out) {
                result.exit_code = status;
            }
        } else {
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
            else result.exit_code = status;
        }
    } else {
        // fork failed
        result.exit_code = 1;
        close(out_fd);
        close(err_fd);
    }

    // Read stdout
    {
        std::ifstream fout(out_tmpl);
        if (fout) {
            std::ostringstream ss;
            ss << fout.rdbuf();
            result.out = ss.str();
        }
    }
    // Read stderr
    {
        std::ifstream ferr(err_tmpl);
        if (ferr) {
            std::ostringstream ss;
            ss << ferr.rdbuf();
            result.err = ss.str();
        }
    }

    close(out_fd);
    close(err_fd);
    unlink(out_tmpl.c_str());
    unlink(err_tmpl.c_str());
#endif
    return result;
}

// ===================================================================
// Git helpers
// ===================================================================

bool git_available() {
    auto res = run_command("git --version");
    return res.exit_code == 0;
}

bool git_clone(const std::string& url, const fs::path& dest, std::string_view branch) {
    std::ostringstream cmd;
    cmd << "git clone --branch " << escape_shell_arg(branch)
        << " \"" << escape_shell_arg(url) << "\" \""
        << escape_shell_arg(dest.string()) << "\"";
    auto res = run_command(cmd.str());
    if (res.exit_code != 0) {
        error(std::string("git clone failed: ") + res.err);
        return false;
    }
    return true;
}

bool git_pull(const fs::path& repo_dir, std::string_view branch) {
    std::ostringstream cmd;
    cmd << "git -C \"" << escape_shell_arg(repo_dir.string())
        << "\" pull origin " << escape_shell_arg(branch);
    auto res = run_command(cmd.str());
    if (res.exit_code != 0) {
        error(std::string("git pull failed: ") + res.err);
        return false;
    }
    return true;
}

std::string git_last_commit_time(const fs::path& repo_dir) {
    std::ostringstream cmd;
    cmd << "git -C \"" << escape_shell_arg(repo_dir.string())
        << "\" log -1 --format=%cI";
    auto res = run_command(cmd.str());
    if (res.exit_code != 0) return {};
    // Trim trailing newline
    auto s = res.out;
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

// ===================================================================
// Editor & script execution
// ===================================================================

std::string find_editor() {
    // 0.2.3+: Check EDITOR and VISUAL environment variables (cross-platform)
    const char* editor_env = std::getenv("EDITOR");
    if (editor_env && editor_env[0] != '\0') return editor_env;
    const char* visual_env = std::getenv("VISUAL");
    if (visual_env && visual_env[0] != '\0') return visual_env;
#ifdef EZMK_WIN
    // Fallback: notepad (always available on Windows)
    return "notepad";
#else
    // Try editors in order: vim, nano, emacs
    for (const char* editor : {"vim", "nano", "emacs"}) {
        std::string cmd = std::string("command -v ") + editor + " > /dev/null 2>&1";
        if (std::system(cmd.c_str()) == 0) {
            return editor;
        }
    }
    return {}; // none found
#endif
}

void open_in_editor(const fs::path& file) {
    std::string editor = find_editor();
    if (editor.empty()) {
        warn(ezmk::i18n::I18nKey::no_editor);
        return;
    }
    info(ezmk::i18n::I18nKey::opening_editor,
         {{"file", file.string()}, {"editor", editor}});
#ifdef EZMK_WIN
    std::string cmd = editor + " \"" + escape_shell_arg(file.string()) + "\"";
#else
    std::string cmd = editor + " \"" + escape_shell_arg(file.string()) + "\" < /dev/tty > /dev/tty 2>&1";
#endif
    auto res = run_command(cmd);
    if (res.exit_code != 0 && !res.err.empty()) {
        warn(ezmk::i18n::I18nKey::editor_error, {{"msg", res.err}});
    }
}

ProcResult run_script(const fs::path& script, const fs::path& cwd) {
    auto ext = script.extension().string();
    std::ostringstream cmd;

    if (ext == ".sh") {
        cmd << "bash \""
            << escape_shell_arg(script.string()) << "\"";
    } else if (ext == ".ps1") {
        cmd << "powershell -ExecutionPolicy Bypass -File \""
            << escape_shell_arg(script.string()) << "\"";
    } else if (ext == ".bat") {
        cmd << "cmd /c \"" << escape_shell_arg(script.string()) << "\"";
    } else {
        ProcResult bad;
        bad.exit_code = 1;
        bad.err = "unsupported script extension: " + ext;
        return bad;
    }

    // 1.1.2 C4: cwd is passed via RunOptions (Windows lpCurrentDirectory /
    // POSIX chdir-after-fork), NOT a "cd <cwd> && ..." shell prefix. On Windows
    // `cd` is a cmd builtin, not an executable — CreateProcessA would fail to
    // spawn it, so every install script (.sh/.ps1/.bat) failed to run.
    RunOptions opts;
    opts.cwd = cwd;
    return run_command(cmd.str(), opts);
}

std::string escape_shell_arg(std::string_view s) {
    std::string r;
    r.reserve(s.size() + 8); // small reserve for occasional escapes
    for (char c : s) {
        if (c == '"' || c == '\\' || c == '`' || c == '$')
            r += '\\';
        r += c;
    }
    return r;
}

// ===================================================================
// Link syntax (1.1.0-dev.5)
// ===================================================================

LinkRef parse_link_syntax(std::string_view raw) {
    LinkRef result;
    const std::string_view prefix = "@link:";

    // Trim leading whitespace
    auto start = raw.find_first_not_of(" \t");
    if (start == std::string_view::npos) return result;
    raw = raw.substr(start);

    // Must start with "@link:"
    if (raw.size() < prefix.size() || raw.substr(0, prefix.size()) != prefix) {
        return result; // not a link reference
    }

    auto remaining = raw.substr(prefix.size());
    if (remaining.empty()) return result; // "@link:" with no name

    // Find the first '/' to split name from sub-path
    auto slash_pos = remaining.find('/');
    std::string_view name_part, sub_part;
    if (slash_pos != std::string_view::npos) {
        name_part = remaining.substr(0, slash_pos);
        sub_part = remaining.substr(slash_pos + 1); // may be empty for trailing slash
    } else {
        name_part = remaining;
        // sub_part stays empty
    }

    // Validate name: [A-Za-z0-9_-]+
    if (name_part.empty()) return result;
    for (char c : name_part) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return result;
        }
    }

    result.name = std::string(name_part);
    result.sub_path = std::string(sub_part);
    return result;
}

// ===================================================================
// Utils / Lua plugin discovery
// ===================================================================

fs::path find_utils_script(const std::string& name) {
    // Search for <name>.lua across project → user → global scope.
    // Uses the same scope paths as pkg_install_dir() in pkg.cpp.

    std::string script_file = name + ".lua";

    // Helper: scan a directory for <pkg>/utils/<name>.lua
    auto scan_dir = [&](const fs::path& pkg_root) {
        if (!fs::exists(pkg_root)) return fs::path();
        for (auto& pkg_entry : fs::directory_iterator(pkg_root)) {
            if (!pkg_entry.is_directory()) continue;
            auto candidate = pkg_entry.path() / "utils" / script_file;
            if (fs::exists(candidate)) return candidate;
        }
        return fs::path();
    };

    // 1) Project scope
    fs::path project_dir = fs::current_path() / ".ezmk/pkg";
    auto found = scan_dir(project_dir);
    if (!found.empty()) return found;

    // 2) User scope
#ifdef EZMK_WIN
    const char* appdata = std::getenv("LOCALAPPDATA");
    fs::path user_dir = appdata ? fs::path(appdata) / "ezmk/pkg"
                                : get_home_dir() / "AppData/Local/ezmk/pkg";
#else
    fs::path user_dir = get_home_dir() / ".local/ezmk/pkg";
#endif
    found = scan_dir(user_dir);
    if (!found.empty()) return found;

    // 3) Global scope: same as pkg_install_dir(Global) = get_exe_dir() / "pkg"
    fs::path global_dir = get_exe_dir() / "pkg";
    found = scan_dir(global_dir);
    if (!found.empty()) return found;

    // 4) Development fallback: project source pkg/ dir (for testing during development)
    fs::path dev_pkg_dir = fs::current_path() / "pkg";
    found = scan_dir(dev_pkg_dir);
    if (!found.empty()) return found;

    return {};
}

// ---- Fuzzy matching (0.9.4+) ----

// Levenshtein distance — standard dynamic programming algorithm.
static int levenshtein_distance(const std::string& a, const std::string& b) {
    size_t n = a.size(), m = b.size();
    std::vector<int> prev(m + 1), cur(m + 1);
    for (size_t j = 0; j <= m; ++j) prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= n; ++i) {
        cur[0] = static_cast<int>(i);
        for (size_t j = 1; j <= m; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = std::min({cur[j - 1] + 1, prev[j] + 1, prev[j - 1] + cost});
        }
        prev.swap(cur);
    }
    return prev[m];
}

std::vector<std::string> closest_match(
    const std::string& input,
    const std::vector<std::string>& candidates,
    int max_distance)
{
    std::vector<std::pair<int, std::string>> matches;
    for (const auto& c : candidates) {
        int d = levenshtein_distance(input, c);
        if (d <= max_distance) {
            matches.emplace_back(d, c);
        }
    }
    std::sort(matches.begin(), matches.end());
    std::vector<std::string> result;
    result.reserve(matches.size());
    for (auto& [d, s] : matches) result.push_back(std::move(s));
    return result;
}

} // namespace ezmk::util
