#pragma once

// 0.2.5+ — GNU-style option parsing layer.
//
// Provides a single tokenizer (parse_options) that all subcommand parsers in
// cli.cpp share, replacing the per-command hand-written flag matching. Supports:
//   * long options   --word / --word=value / --word value
//   * short options  -x, grouping -xyz (== -x -y -z), attached value -j4
//   * grouped value must trail: -vj4 == -v -j 4
//   * "--" terminates option parsing; everything after is positional
//   * a lone "-" is a positional (stdin convention)
//   * options and positionals may be freely interleaved (GNU permutation)
//
// See plans/0.2.5.md §2 for the full specification.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ezmk::cli {

// One option declaration. A command builds a table of these and hands it to
// parse_options().
struct OptionSpec {
    char        short_name;   // short flag char, '\0' if none
    std::string long_name;    // long flag name without "--", empty if none
    bool        takes_arg;    // whether the option consumes a value
};

// Result of parsing: recognized options (in the order they appeared) plus the
// leftover positional arguments.
struct ParsedOptions {
    // key = long_name, or string(1, short_name) when there is no long name.
    // For switches (takes_arg == false) the value is an empty string.
    std::vector<std::pair<std::string, std::string>> options;
    std::vector<std::string> positionals;

    // True if the option appeared at least once.
    bool has(std::string_view name) const;
    // Value of the last occurrence, or nullopt if the option is absent.
    std::optional<std::string> value(std::string_view name) const;
    // Number of times the option appeared (supports repeated flags).
    int count(std::string_view name) const;
};

// Tokenize argv[begin, argc) per the GNU rules above.
// On an unknown option, a missing value, or a value passed to a switch,
// reports a localized, command-scoped error via util::fatal (throws
// ezmk::fatal_error). cmd_name is used in those messages, e.g.
// "ezmk project build".
ParsedOptions parse_options(int argc, char** argv, int begin,
                            const std::vector<OptionSpec>& spec,
                            std::string_view cmd_name);

} // namespace ezmk::cli
