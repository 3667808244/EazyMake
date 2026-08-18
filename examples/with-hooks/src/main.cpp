// with-hooks 示例 — 构建钩子（教程 07「监视模式与钩子」）。
//
// 生成：ezmk example with-hooks
// 构建：cd with-hooks && ezmk build -v
//       → 观察 pre_build/post_build 钩子输出的信息（-v 显示钩子执行）。
#include <iostream>

int main() {
    std::cout << "Hello from with-hooks!" << std::endl;
    return 0;
}
