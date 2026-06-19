#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace ezmk::crypto {

// SHA-256 of a string.
std::string sha256(std::string_view data);

// SHA-256 of a file's contents (streaming: reads in 64 KiB chunks).
// Returns empty string if file cannot be read.
std::string sha256_file(const std::filesystem::path& p);

// Streaming SHA-256 state for incremental hashing.
class Sha256Stream {
public:
    Sha256Stream();
    void update(const void* data, size_t len);
    std::string finalize();
    // Also expose the raw bytes before hex-encoding
    void finalize_raw(uint8_t out[32]);
private:
    uint32_t state_[8];
    uint8_t buf_[64];
    size_t buf_len_;
    uint64_t total_bits_;
};

} // namespace ezmk::crypto
