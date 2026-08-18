// with-tests 示例 — Catch2 测试（教程 09「测试你的项目」）。
//
// 只需写 TEST_CASE：ezmk test 会自动生成 Catch2 v3 兼容的主程序，
// 并链接 catch2 包提供的实现（libcatch2.a）。
// 过滤：ezmk test --filter "add works"；详细输出：ezmk test -V。
#include <catch2/catch_all.hpp>

// 被测函数来自 src/add.cpp（测试链接排除 main.cpp，函数放独立源文件）。
int add(int a, int b);

TEST_CASE("add works", "[math]") {
    REQUIRE(add(2, 3) == 5);
    REQUIRE(add(0, 0) == 0);
}

TEST_CASE("add handles negatives", "[math]") {
    REQUIRE(add(-1, 1) == 0);
    REQUIRE(add(-2, -3) == -5);
}
