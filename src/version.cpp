#include "ezmk/util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace ezmk::util {

// Compare two semantic version strings (major.minor.patch).
// Strips pre-release tags (-alpha, -rc1) and build metadata (+build).
int compare_version(std::string_view a, std::string_view b) {
    // Helper: parse the next numeric segment from s starting at pos.
    // Updates pos to after the segment delimiter ('.' or end-of-string or '-' or '+').
    auto parse_seg = [](std::string_view s, size_t& pos) -> unsigned long {
        // Skip leading non-digits (shouldn't happen for well-formed versions).
        while (pos < s.size() && !std::isdigit(static_cast<unsigned char>(s[pos]))) {
            ++pos;
        }
        if (pos >= s.size()) return 0;
        // Collect digits.
        unsigned long val = 0;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            val = val * 10 + static_cast<unsigned long>(s[pos] - '0');
            ++pos;
        }
        // Advance past the delimiter ('.', '-', '+', or end).
        if (pos < s.size() && s[pos] == '.') {
            ++pos;
        }
        return val;
    };

    // Strip pre-release and build metadata for comparison purposes.
    // Find the first '-' or '+' that isn't part of a digit.
    auto strip_extra = [](std::string_view s) -> std::string_view {
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '-' || s[i] == '+') {
                return s.substr(0, i);
            }
        }
        return s;
    };

    std::string_view a_clean = strip_extra(a);
    std::string_view b_clean = strip_extra(b);

    size_t pa = 0, pb = 0;
    while (pa < a_clean.size() || pb < b_clean.size()) {
        unsigned long va = parse_seg(a_clean, pa);
        unsigned long vb = parse_seg(b_clean, pb);
        if (va < vb) return -1;
        if (va > vb) return 1;
    }
    return 0;
}

} // namespace ezmk::util
