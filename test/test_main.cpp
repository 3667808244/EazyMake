// EazyMake test suite entry point (Catch2 v3)
// This is the ONLY file that includes catch2.hpp without
// CATCH_AMALGAMATED_CUSTOM_MAIN — it provides the implementation.
#include "catch2.hpp"
#include "ezmk/i18n.hpp"

// Initialize i18n before any tests run (Catch2 registers test cases
// at static-init time, but they don't execute until main() runs).
// This ensures all modules see initialized i18n strings.
namespace {
    struct I18nBootstrap {
        I18nBootstrap() { ezmk::i18n::init("en"); }
    } g_i18n_bootstrap;
}

