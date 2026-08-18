// with-tests 示例 — ezmk 内置框架测试（教程 09「测试你的项目」）。
//
// 零依赖：每个测试文件编译为独立可执行文件，退出码即判定（0 = 通过，非零 = 失败）。
// 运行：ezmk test 或 ezmk test -f ezmk；过滤：ezmk test --filter basic。
// 想用 Catch2 框架：ezmk pkg install catch2 后把 ezmk.toml 的 framework 改 "catch2"。
#include <cstdlib>

// 被测函数来自 src/add.cpp（测试链接排除 main.cpp，函数放独立源文件）。
int add(int a, int b);

int main() {
    if (add(2, 3) != 5) return 1;
    if (add(0, 0) != 0) return 1;
    if (add(-1, 1) != 0) return 1;
    if (add(-2, -3) != -5) return 1;
    return 0;   // 全部通过
}
