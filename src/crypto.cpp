#include "ezmk/crypto.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace ezmk::crypto {

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

// ===================================================================
// Streaming SHA-256
// ===================================================================

Sha256Stream::Sha256Stream()
    : buf_len_(0), total_bits_(0) {
    state_[0] = 0x6a09e667; state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372; state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f; state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab; state_[7] = 0x5be0cd19;
}

void Sha256Stream::update(const void* data, size_t len) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    total_bits_ += static_cast<uint64_t>(len) * 8;

    // If there's buffered data, fill the buffer first
    if (buf_len_ > 0) {
        size_t to_fill = 64 - buf_len_;
        if (len < to_fill) {
            std::memcpy(buf_ + buf_len_, bytes, len);
            buf_len_ += len;
            return;
        }
        std::memcpy(buf_ + buf_len_, bytes, to_fill);
        sha256_transform(state_, buf_);
        bytes += to_fill;
        len -= to_fill;
        buf_len_ = 0;
    }

    // Process full 64-byte blocks
    while (len >= 64) {
        sha256_transform(state_, bytes);
        bytes += 64;
        len -= 64;
    }

    // Buffer remaining
    if (len > 0) {
        std::memcpy(buf_, bytes, len);
        buf_len_ = len;
    }
}

void Sha256Stream::finalize_raw(uint8_t out[32]) {
    // Padding
    uint8_t last[128];
    size_t remaining = buf_len_;
    std::memcpy(last, buf_, remaining);
    last[remaining] = 0x80;
    remaining++;

    if (remaining <= 56) {
        std::memset(last + remaining, 0, 64 - remaining);
    } else {
        std::memset(last + remaining, 0, 128 - remaining);
        sha256_transform(state_, last);
        std::memset(last, 0, 64);
    }

    // Append bit length as 64-bit big-endian
    for (int j = 0; j < 8; ++j) {
        last[56 + j] = static_cast<uint8_t>(total_bits_ >> (56 - j * 8));
    }
    sha256_transform(state_, last);

    for (int i = 0; i < 8; ++i) {
        uint32_t s = state_[i];
        out[i * 4]     = static_cast<uint8_t>(s >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(s >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(s >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(s);
    }
}

std::string Sha256Stream::finalize() {
    uint8_t raw[32];
    finalize_raw(raw);
    std::string hex;
    hex.reserve(64);
    for (int i = 0; i < 32; ++i) {
        hex += "0123456789abcdef"[raw[i] >> 4];
        hex += "0123456789abcdef"[raw[i] & 0xf];
    }
    return hex;
}

// ===================================================================
// Convenience wrappers
// ===================================================================

std::string sha256(std::string_view data) {
    Sha256Stream s;
    s.update(data.data(), data.size());
    return s.finalize();
}

std::string sha256_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};

    Sha256Stream s;
    char buf[65536]; // 64 KiB chunks
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
        s.update(buf, static_cast<size_t>(f.gcount()));
    }
    return s.finalize();
}

} // namespace ezmk::crypto
