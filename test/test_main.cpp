// EazyMake test suite entry point (Catch2 v3)
// This is the ONLY file that includes catch2.hpp without
// CATCH_AMALGAMATED_CUSTOM_MAIN — it provides the implementation.
#include "catch2.hpp"
#include "ezmk/i18n.hpp"

namespace {
    // 1.4.12: initialize i18n when the TEST RUN starts, not in a static
    // constructor. Static-init order across translation units is unspecified,
    // so the old bootstrap could run before locale_data.cpp's map was built and
    // leave the suite with the "{key}" fallback (observed with GCC on Arch).
    struct I18nBootstrap : Catch::EventListenerBase {
        using EventListenerBase::EventListenerBase;
        void testRunStarting(Catch::TestRunInfo const&) override {
            ezmk::i18n::init("en");
        }
    };
}
CATCH_REGISTER_LISTENER(I18nBootstrap)
