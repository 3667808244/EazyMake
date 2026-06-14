#include "ezmk/util.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef EZMK_WIN
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <winhttp.h>
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

void info(std::string_view msg)  { std::cerr << "[ezmk] " << msg << "\n"; }
void warn(std::string_view msg)  { std::cerr << "[ezmk warn] " << msg << "\n"; }
void error(std::string_view msg) { std::cerr << "[ezmk error] " << msg << "\n"; }

void fatal(std::string_view msg) {
    std::cerr << "[ezmk fatal] " << msg << "\n";
    std::exit(1);
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

void file_write(const fs::path& p, std::string_view content) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) {
        error(std::string("cannot write: ") + p.string());
        return;
    }
    f.write(content.data(), content.size());
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
    const char* home = std::getenv("USERPROFILE");
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
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) return fs::path(std::string(buf, n)).parent_path();
    return fs::current_path();
#endif
}

// ===================================================================
// SHA-256
// ===================================================================

namespace {

constexpr std::array<uint32_t, 64> SHA256_K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z)  { return (x & y) ^ (~x & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t gam0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t gam1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16)
             | (uint32_t(block[i * 4 + 2]) << 8)  | uint32_t(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        w[i] = gam1(w[i - 2]) + w[i - 7] + gam0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + sig1(e) + ch(e, f, g) + SHA256_K[i] + w[i];
        uint32_t t2 = sig0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

} // anonymous namespace

std::string sha256(std::string_view data) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };

    const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
    size_t len = data.size();

    // Process full blocks
    size_t i = 0;
    for (; i + 64 <= len; i += 64) {
        sha256_transform(state, bytes + i);
    }

    // Padding
    uint8_t last[128];
    size_t remaining = len - i;
    std::memcpy(last, bytes + i, remaining);
    last[remaining] = 0x80;
    remaining++;

    size_t total_bits = len * 8;
    if (remaining <= 56) {
        std::memset(last + remaining, 0, 64 - remaining);
    } else {
        std::memset(last + remaining, 0, 128 - remaining);
        sha256_transform(state, last);
        std::memset(last, 0, 64);
    }

    // Append length in big-endian
    for (int j = 0; j < 8; ++j) {
        last[56 + j] = static_cast<uint8_t>(total_bits >> (56 - j * 8));
    }
    sha256_transform(state, last);

    // Hex output
    std::string hex;
    hex.reserve(64);
    for (uint32_t s : state) {
        for (int j = 24; j >= 0; j -= 8) {
            hex += "0123456789abcdef"[(s >> j) & 0xf];
        }
    }
    return hex;
}

std::string sha256_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    f.seekg(0);
    std::string data(static_cast<size_t>(sz), '\0');
    f.read(data.data(), sz);
    return sha256(data);
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
    // Fallback: use curl command
    std::string cmd = "curl -sL -o '" + dest.string() + "' '" + url + "'";
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
    }
    CloseHandle(hReadOut);
    CloseHandle(hReadErr);
#else
    // Use popen + redirect stderr
    std::string cmd2 = cmd + " 2>&1";
    FILE* pipe = popen(cmd2.c_str(), "r");
    if (!pipe) return result;

    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        result.out += buf;
    }
    result.exit_code = pclose(pipe);
    if (WIFEXITED(result.exit_code)) result.exit_code = WEXITSTATUS(result.exit_code);
#endif
    return result;
}

// ===================================================================
// Cross-platform
// ===================================================================

std::string native_path(const fs::path& p) {
    auto s = p.generic_string();
#ifdef EZMK_WIN
    std::replace(s.begin(), s.end(), '/', '\\');
#endif
    return s;
}

} // namespace ezmk::util
