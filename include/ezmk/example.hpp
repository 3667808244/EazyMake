#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ezmk::example {
namespace fs = std::filesystem;

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

// 1.2.3: `ezmk example list` — 列出全部内置示例（名称 + 一句话说明）。
void list_examples();

// 1.2.3: `ezmk example <name>` — 按嵌入表生成示例到 <output_dir>/<name>/。
// 目标目录已存在或示例名未知 → 抛 ezmk::fatal_error。
void create_example(const std::string& name, const fs::path& output_dir = ".");

} // namespace ezmk::example
