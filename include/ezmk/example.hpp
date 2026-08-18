#pragma once

#include <string>
#include <vector>

namespace ezmk::example {

// 1.2.3: `ezmk example` 内置示例的嵌入表类型。
// 数据由 scripts/embed_examples.py 构建期生成到 src/example_data.cpp（gitignore）。
struct ExampleFile {
    const char* path;
    const char* content;
};

struct Example {
    const char* name;
    const char* description;
    std::vector<ExampleFile> files;
};

// 全部内置示例（构建期从 examples/ 源目录嵌入）。
const std::vector<Example>& embedded_examples();

} // namespace ezmk::example
