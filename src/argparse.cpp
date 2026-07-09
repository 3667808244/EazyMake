#include "ezmk/argparse.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <cstring>

namespace ezmk::cli {

// ---- ParsedOptions accessors ----

bool ParsedOptions::has(std::string_view name) const {
    for (const auto& [k, v] : options) {
        (void)v;
        if (k == name) return true;
    }
    return false;
}

std::optional<std::string> ParsedOptions::value(std::string_view name) const {
    std::optional<std::string> result;
    for (const auto& [k, v] : options) {
        if (k == name) result = v; // last occurrence wins
    }
    return result;
}

int ParsedOptions::count(std::string_view name) const {
    int n = 0;
    for (const auto& [k, v] : options) {
        (void)v;
        if (k == name) ++n;
    }
    return n;
}

namespace {

const OptionSpec* find_long(const std::vector<OptionSpec>& spec,
                            std::string_view name) {
    for (const auto& s : spec) {
        if (!s.long_name.empty() && s.long_name == name) return &s;
    }
    return nullptr;
}

const OptionSpec* find_short(const std::vector<OptionSpec>& spec, char c) {
    for (const auto& s : spec) {
        if (s.short_name != '\0' && s.short_name == c) return &s;
    }
    return nullptr;
}

// Canonical key under which a matched option is stored: prefer the long name,
// fall back to the single short character.
std::string canonical_key(const OptionSpec& s) {
    if (!s.long_name.empty()) return s.long_name;
    return std::string(1, s.short_name);
}

[[noreturn]] void err_unknown(std::string_view opt, std::string_view cmd) {
    util::fatal(i18n::I18nKey::cli_unknown_option,
                {{"opt", std::string(opt)}, {"cmd", std::string(cmd)}});
}

[[noreturn]] void err_missing(std::string_view opt, std::string_view cmd) {
    util::fatal(i18n::I18nKey::cli_missing_value,
                {{"opt", std::string(opt)}, {"cmd", std::string(cmd)}});
}

} // namespace

ParsedOptions parse_options(int argc, char** argv, int begin,
                            const std::vector<OptionSpec>& spec,
                            std::string_view cmd_name) {
    ParsedOptions out;
    bool after_dashdash = false;

    for (int i = begin; i < argc; ++i) {
        std::string tok = argv[i];

        // Everything after the first "--" is positional.
        if (after_dashdash) {
            out.positionals.push_back(tok);
            continue;
        }

        // "--" terminates option parsing (only the first one).
        if (tok == "--") {
            after_dashdash = true;
            continue;
        }

        // A lone "-" is a positional (stdin convention).
        if (tok == "-" || tok.empty() || tok[0] != '-') {
            out.positionals.push_back(tok);
            continue;
        }

        // ---- long option: --name / --name=value ----
        if (tok.size() >= 2 && tok[1] == '-') {
            std::string body = tok.substr(2); // strip "--"
            std::string name = body;
            std::optional<std::string> inline_val;
            auto eq = body.find('=');
            if (eq != std::string::npos) {
                name = body.substr(0, eq);
                inline_val = body.substr(eq + 1);
            }

            const OptionSpec* s = find_long(spec, name);
            if (!s) err_unknown("--" + name, cmd_name);

            std::string key = canonical_key(*s);
            if (s->takes_arg) {
                if (inline_val) {
                    out.options.emplace_back(key, *inline_val);
                } else {
                    if (i + 1 >= argc) err_missing("--" + name, cmd_name);
                    out.options.emplace_back(key, argv[++i]);
                }
            } else {
                if (inline_val) {
                    // A switch was given a value (--flag=x): reject explicitly.
                    util::fatal(i18n::I18nKey::cli_unknown_option,
                                {{"opt", "--" + name + "="},
                                 {"cmd", std::string(cmd_name)}});
                }
                out.options.emplace_back(key, std::string());
            }
            continue;
        }

        // ---- short option(s): -x, -xyz, -j4, -vj4 ----
        for (size_t p = 1; p < tok.size(); ++p) {
            char c = tok[p];
            const OptionSpec* s = find_short(spec, c);
            if (!s) err_unknown(std::string("-") + c, cmd_name);

            std::string key = canonical_key(*s);
            if (s->takes_arg) {
                // Value is the rest of this token if any, else the next token.
                std::string rest = tok.substr(p + 1);
                if (!rest.empty()) {
                    out.options.emplace_back(key, rest);
                } else {
                    if (i + 1 >= argc) err_missing(std::string("-") + c, cmd_name);
                    out.options.emplace_back(key, argv[++i]);
                }
                break; // consumed the remainder of the token
            }
            out.options.emplace_back(key, std::string());
        }
    }

    return out;
}

} // namespace ezmk::cli
