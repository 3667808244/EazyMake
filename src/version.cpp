#include "ezmk/util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

namespace ezmk::util {

// 1.4.6 Q-15: parse one numeric version component. The config validator accepts
// arbitrarily long digit runs, so a raw std::stoul can throw out_of_range and
// abort with a non-diagnostic message. Saturates at ULONG_MAX instead.
unsigned long parse_version_component(std::string_view s) {
    unsigned long v = 0;
    const unsigned long max = std::numeric_limits<unsigned long>::max();
    for (char c : s) {
        if (c < '0' || c > '9') break;
        unsigned long d = static_cast<unsigned long>(c - '0');
        if (v > (max - d) / 10) return max;
        v = v * 10 + d;
    }
    return v;
}

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
        // 1.4.11 Q4-18: saturating parse — an arbitrarily long digit run used to
        // wrap around instead of clamping.
        size_t seg_start = pos;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            ++pos;
        }
        unsigned long val = parse_version_component(
            s.substr(seg_start, pos - seg_start));
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

// 1.4.2 F-27: extract the pre-release segment ("1.2.0-rc.1+build" → "rc.1").
static std::string_view pre_release_part(std::string_view s) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') return {};  // build metadata first → no pre-release
        if (s[i] == '-') {
            auto end = s.find('+', i + 1);
            return end == std::string_view::npos ? s.substr(i + 1)
                                                 : s.substr(i + 1, end - i - 1);
        }
    }
    return {};
}

// 1.5.0: semantic-versioning precedence for the dot-separated pre-release
// identifiers: numeric identifiers compare numerically (rc.9 < rc.10),
// numeric < alphanumeric, and a larger identifier set wins when all preceding
// identifiers are equal. The previous plain string compare ordered rc.10
// before rc.9.
static int compare_prerelease_ids(std::string_view a, std::string_view b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        size_t ai_end = a.find('.', i);
        if (ai_end == std::string_view::npos) ai_end = a.size();
        size_t bj_end = b.find('.', j);
        if (bj_end == std::string_view::npos) bj_end = b.size();
        const std::string_view ai = a.substr(i, ai_end - i);
        const std::string_view bj = b.substr(j, bj_end - j);

        const bool a_num = !ai.empty() &&
            std::all_of(ai.begin(), ai.end(),
                        [](char c) { return c >= '0' && c <= '9'; });
        const bool b_num = !bj.empty() &&
            std::all_of(bj.begin(), bj.end(),
                        [](char c) { return c >= '0' && c <= '9'; });
        if (a_num && b_num) {
            const unsigned long av = parse_version_component(ai);
            const unsigned long bv = parse_version_component(bj);
            if (av != bv) return av < bv ? -1 : 1;
        } else if (a_num != b_num) {
            return a_num ? -1 : 1;   // numeric identifiers rank lower
        } else if (ai != bj) {
            return ai < bj ? -1 : 1;
        }
        i = (ai_end >= a.size()) ? a.size() : ai_end + 1;
        j = (bj_end >= b.size()) ? b.size() : bj_end + 1;
    }
    const bool a_more = i < a.size();
    const bool b_more = j < b.size();
    if (a_more == b_more) return 0;
    return a_more ? 1 : -1;   // more identifiers outrank a prefix
}

int compare_version_precedence(std::string_view a, std::string_view b) {
    const int core = compare_version(a, b);
    if (core != 0) return core;
    const std::string_view pa = pre_release_part(a);
    const std::string_view pb = pre_release_part(b);
    if (pa.empty() && pb.empty()) return 0;
    if (pa.empty()) return 1;    // release outranks a pre-release
    if (pb.empty()) return -1;
    return compare_prerelease_ids(pa, pb);
}

} // namespace ezmk::util
