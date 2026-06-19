#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace ezmk::crypto {

// SHA-256 of a string.
std::string sha256(std::string_view data);

// SHA-256 of a file's contents. Returns empty string if file cannot be read.
std::string sha256_file(const std::filesystem::path& p);

} // namespace ezmk::crypto
