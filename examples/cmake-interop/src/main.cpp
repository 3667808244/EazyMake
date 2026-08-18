// cmake-interop 示例 — CMake 互操作（教程 11「导入 CMake 项目」）。
//
// 生成：ezmk example cmake-interop
// 导出：cd cmake-interop && ezmk project export cmake
//       → 生成 CMakeLists.txt（单向快照：之后以 ezmk.toml 为唯一事实源）
// 反向导入：把现有 CMakeLists.txt 转成 ezmk.toml → ezmk project import
//           （实验性，标准写法可映射；自定义命令/生成器表达式会被事务性拒绝）
#include <iostream>

int main() {
    std::cout << "Hello from cmake-interop!" << std::endl;
    return 0;
}
