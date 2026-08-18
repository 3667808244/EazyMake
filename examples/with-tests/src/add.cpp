// with-tests 示例 — 被测函数实现。
// 独立于 main.cpp：ezmk test 链接测试目标文件时排除项目的 main.cpp（避免 main 冲突），
// 因此被测函数放在单独源文件才能被测试链接到。
int add(int a, int b) {
    return a + b;
}
