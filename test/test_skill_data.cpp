// Unit tests for the embedded user-project skill table (1.5.0-dev.8).
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/skill_data.hpp"

#include <cstring>
#include <set>
#include <string>

TEST_CASE("embedded agent skills: four ezmk-user-* entries",
          "[skill][1.5.0-dev.8]") {
    std::size_t count = 0;
    const ezmk::skills::Skill* skills = ezmk::skills::all(count);
    REQUIRE(skills != nullptr);
    REQUIRE(count == 4);

    std::set<std::string> names;
    for (std::size_t i = 0; i < count; ++i) {
        REQUIRE(skills[i].name != nullptr);
        REQUIRE(skills[i].description != nullptr);
        REQUIRE(skills[i].content != nullptr);
        REQUIRE(std::strlen(skills[i].description) > 0);
        names.insert(skills[i].name);
        const std::string content = skills[i].content;
        REQUIRE(content.rfind("---\n", 0) == 0);
        REQUIRE(content.find("name: " + std::string(skills[i].name)) !=
                std::string::npos);
    }
    REQUIRE(names.count("ezmk-user-build") == 1);
    REQUIRE(names.count("ezmk-user-config") == 1);
    REQUIRE(names.count("ezmk-user-pkg") == 1);
    REQUIRE(names.count("ezmk-user-test") == 1);
}
