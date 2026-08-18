// with-packages 示例 — 依赖 + 版本约束（教程 06「使用包」/ 12「版本约束与确定性构建」）。
//
// 生成：ezmk example with-packages
// 安装依赖：cd with-packages && ezmk pkg install fmt -y     （需网络）
// 运行：ezmk build && ezmk run
// 锁文件：ezmk build 首次成功会生成 ezmk.lock；之后 ezmk build --locked 复现构建。
#include <fmt/core.h>

int main() {
    fmt::print("Hello from with-packages! fmt version: {}\n", FMT_VERSION);
    return 0;
}
