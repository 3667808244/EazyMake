// with-tests 示例 — 项目代码（教程 09「测试你的项目」）。
//
// 生成：ezmk example with-tests
// 安装依赖：cd with-tests && ezmk pkg install catch2 -y     （需网络）
// 运行测试：ezmk test
// 构建运行：ezmk build && ezmk run
#include <iostream>

// 被测函数实现在 src/add.cpp（测试链接会排除 main.cpp，函数不能放这里）。
int add(int a, int b);

int main() {
    std::cout << "2 + 3 = " << add(2, 3) << std::endl;
    return 0;
}
