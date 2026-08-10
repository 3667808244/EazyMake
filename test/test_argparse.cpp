// Unit tests for argparse.cpp — the GNU-style option tokenizer.
// 1.1.3 T2: directly exercise parse_options (long/short options, grouping,
// "--" pass-through, error cases, and ParsedOptions::has/value).
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/argparse.hpp"
#include "ezmk/util.hpp"

#include <string>
#include <vector>

using namespace ezmk::cli;

// Build a char** argv from a token vector and run parse_options from `begin`.
static ParsedOptions run_parse(std::vector<std::string> tokens,
                               const std::vector<OptionSpec>& spec,
                               int begin = 1) {
    std::vector<char*> argv;
    for (auto& t : tokens) argv.push_back(t.data());
    return parse_options(static_cast<int>(argv.size()), argv.data(), begin,
                         spec, "test cmd");
}

// ---- long options ----

TEST_CASE("parse_options: long flag --flag", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "--flag"}, {{'\0', "flag", false}});
    REQUIRE(p.has("flag"));
    REQUIRE(p.positionals.empty());
}

TEST_CASE("parse_options: long option --key=value", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "--key=val"}, {{'\0', "key", true}});
    REQUIRE(p.value("key").has_value());
    REQUIRE(*p.value("key") == "val");
}

TEST_CASE("parse_options: long option --key value", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "--key", "val"}, {{'\0', "key", true}});
    REQUIRE(p.value("key").has_value());
    REQUIRE(*p.value("key") == "val");
}

// ---- short options ----

TEST_CASE("parse_options: short option -x", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "-x"}, {{'x', "", false}});
    REQUIRE(p.has("x"));
}

TEST_CASE("parse_options: short grouping -xyz", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "-xyz"},
                       {{'x', "", false}, {'y', "", false}, {'z', "", false}});
    REQUIRE(p.has("x"));
    REQUIRE(p.has("y"));
    REQUIRE(p.has("z"));
}

TEST_CASE("parse_options: short attached value -j4", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "-j4"}, {{'j', "", true}});
    REQUIRE(p.value("j").has_value());
    REQUIRE(*p.value("j") == "4");
}

// ---- pass-through / positional handling ----

TEST_CASE("parse_options: -- terminates option parsing", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "--flag", "--", "pos1", "pos2"},
                       {{'\0', "flag", false}});
    REQUIRE(p.has("flag"));
    REQUIRE(p.positionals.size() == 2);
    REQUIRE(p.positionals[0] == "pos1");
    REQUIRE(p.positionals[1] == "pos2");
}

TEST_CASE("parse_options: lone dash is a positional", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "-"}, {});
    REQUIRE(p.positionals.size() == 1);
    REQUIRE(p.positionals[0] == "-");
}

TEST_CASE("parse_options: options and positionals interleave", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "pos1", "--flag", "pos2"},
                       {{'\0', "flag", false}});
    REQUIRE(p.has("flag"));
    REQUIRE(p.positionals.size() == 2);
    REQUIRE(p.positionals[0] == "pos1");
    REQUIRE(p.positionals[1] == "pos2");
}

// ---- error cases ----

TEST_CASE("parse_options: missing value throws fatal_error", "[argparse][1.1.3]") {
    REQUIRE_THROWS_AS(run_parse({"cmd", "--key"}, {{'\0', "key", true}}),
                      ezmk::fatal_error);
}

TEST_CASE("parse_options: unknown long option throws fatal_error", "[argparse][1.1.3]") {
    REQUIRE_THROWS_AS(run_parse({"cmd", "--bogus"}, {}), ezmk::fatal_error);
}

TEST_CASE("parse_options: value passed to a switch throws fatal_error", "[argparse][1.1.3]") {
    REQUIRE_THROWS_AS(run_parse({"cmd", "--flag=val"}, {{'\0', "flag", false}}),
                      ezmk::fatal_error);
}

TEST_CASE("parse_options: unknown short option throws fatal_error", "[argparse][1.1.3]") {
    REQUIRE_THROWS_AS(run_parse({"cmd", "-q"}, {}), ezmk::fatal_error);
}

// ---- accessors ----

TEST_CASE("parse_options: has/value on absent option", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd"}, {{'\0', "flag", false}, {'\0', "key", true}});
    REQUIRE_FALSE(p.has("flag"));
    REQUIRE_FALSE(p.value("key").has_value());
}

TEST_CASE("parse_options: last occurrence of value wins", "[argparse][1.1.3]") {
    auto p = run_parse({"cmd", "--key=a", "--key=b"}, {{'\0', "key", true}});
    REQUIRE(*p.value("key") == "b");
}
