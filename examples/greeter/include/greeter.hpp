// greeter 示例 — 静态库骨架（教程 03「理解 ezmk.toml」/ 05「构建配置与并行编译」）。
//
// 生成：ezmk example greeter
// 构建：cd greeter && ezmk build          → 产出 build/libgreeter.a
// 消费：把本目录作为依赖安装（ezmk pkg install <目录>），或在其他项目 [depends] 引用。
#pragma once

// greeter — 示例公共 API。
// 替换为你的库接口：头文件放 include/，实现放 src/。

namespace greeter {

// 示例函数：返回一条问候消息。
const char* greeting();

} // namespace greeter
