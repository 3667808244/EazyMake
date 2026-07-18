#include "ezmk/util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>

#ifdef EZMK_WIN
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <winhttp.h>
#elif defined(EZMK_MACOS)
  #include <mach-o/dyld.h>
  #include <unistd.h>
  #include <sys/wait.h>
#else
  #include <unistd.h>
  #include <sys/wait.h>
#endif

// ---- miniz (C API, compiled together) ----
#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_WRITING_APIS
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
// Archive extraction
// ===================================================================

void extract_zip(const fs::path& archive, const fs::path& dest) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archive.string().c_str(), 0)) {
        throw std::runtime_error("failed to open ZIP: " + archive.string());
    }
    mz_uint num = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < num; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        fs::path out = dest / stat.m_filename;
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
    out.resize(compressed.size() * 4);
    size_t out_pos = 0;

    tinfl_decompressor inflator{};
    tinfl_init(&inflator);

    size_t in_pos = data_off;
    while (in_pos < src_len) {
        size_t in_bytes = src_len - in_pos;
        size_t out_bytes = out.size() - out_pos;
        if (out_bytes == 0) {
            out.resize(out.size() * 2);
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
            fs::path outpath = dest / name;
            fs::create_directories(outpath.parent_path());
            if (off + fsize <= out.size()) {
                std::ofstream fout(outpath, std::ios::binary);
                fout.write(reinterpret_cast<const char*>(out.data() + off), fsize);
            }
        } else if (typeflag == '5') {
            // Directory
            fs::create_directories(dest / name);
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

ProcResult run_command(const std::string& cmd) {
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

    if (CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                       0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWriteOut);
        CloseHandle(hWriteErr);

        char buf[4096];
        DWORD n;
        while (ReadFile(hReadOut, buf, sizeof(buf) - 1, &n, nullptr) && n > 0) {
            result.out.append(buf, n);
        }
        while (ReadFile(hReadErr, buf, sizeof(buf) - 1, &n, nullptr) && n > 0) {
            result.err.append(buf, n);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
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
    int rc = std::system(cmd2.c_str());
    if (WIFEXITED(rc)) result.exit_code = WEXITSTATUS(rc);
    else result.exit_code = rc;

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

    // Change to cwd, run script, restore
    if (ext == ".sh") {
#ifdef EZMK_WIN
        cmd << "bash ";
#else
        cmd << "bash ";
#endif
        cmd << "\"" << escape_shell_arg(script.string()) << "\"";
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

    // Build command with cd
    std::ostringstream full_cmd;
#ifdef EZMK_WIN
    full_cmd << "cd /d \"" << escape_shell_arg(cwd.string()) << "\" && " << cmd.str();
#else
    full_cmd << "cd \"" << escape_shell_arg(cwd.string()) << "\" && " << cmd.str();
#endif

    return run_command(full_cmd.str());
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
