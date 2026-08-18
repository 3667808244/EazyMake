# 11. 导入 CMake 项目

已有用 CMake 管理的项目想改用 EazyMake 构建？实验性的 `ezmk project import`
命令可以把标准的 `CMakeLists.txt` 一步转换为全新的 `ezmk.toml`。

## 导入一个示例项目

先准备一个典型的小型 CMake 项目：

```bash
$ mkdir /tmp/hello-cmake && cd /tmp/hello-cmake
$ cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(hello VERSION 1.0.0 LANGUAGES CXX)

set(SRCS src/main.cpp)
add_executable(hello ${SRCS})
target_include_directories(hello PRIVATE include)
target_compile_definitions(hello PRIVATE GREETING="hi")
EOF
$ mkdir -p src include
$ cat > src/main.cpp <<'EOF'
#include <cstdio>
int main() { std::printf("%s\n", GREETING); return 0; }
EOF
```

导入它：

```bash
$ ezmk project import --from cmake
```

这会生成 `ezmk.toml`（若文件已存在则默认拒绝，需加 `--overwrite`）。看看生成了什么：

```bash
$ cat ezmk.toml
```

可以看到 `[project]` 头（name/version/language）、带 `src_dirs = ["src"]` 和
`include_dirs = ["include"]` 的 `[compile]` 段（来自 `${SRCS}` /
`target_include_directories` 展开），以及含 `GREETING` 的 `[compile.macros]`。

## 构建并运行

```bash
$ ezmk build
$ ezmk run
hi
```

源目录、头文件路径、编译定义全部来自 CMakeLists 的映射。导入后 **`ezmk.toml` 是
唯一事实源**：直接编辑它，不要再回头改 `CMakeLists.txt`。

## 哪些映射、哪些跳过、哪些拒绝

| 情形 | 行为 |
|---|---|
| `project`、`add_executable`、`add_library`、`target_sources`、`target_include_directories`、`target_compile_definitions`、`target_compile_options`、`target_link_libraries` | 映射到 `ezmk.toml` |
| `set(...)` + `${VAR}`（顶层、常量） | 单层展开；未解析的留 `# TODO: 未解析的参数` |
| `find_package(Boost 1.82)` | 写成 `[depends]` 下**注释掉的** `# lib = ["boost@1.82"]` |
| `if(WIN32)` / `if(UNIX)` … | 取**当前平台**对应的分支 |
| `add_custom_command`、`function()`、`$<...>` 生成器表达式、`pkg_check_modules` | **导入中止**（不产出任何文件） |

被拒绝的项目，手动迁移步骤见
[`docs/zh/migrate-from-cmake.md`](../../../docs/zh/migrate-from-cmake.md)（用 Lua
`[hooks]` 复刻自定义步骤）。

> 💡 想直接跑完整示例？运行 `ezmk example cmake-interop` 生成可 `export cmake` 的项目（示例列表见 [`examples/README.md`](../../../examples/README.md)）。

## 下一步

- 为 clangd/LSP 重新生成 `compile_commands.json`：`ezmk project cc`。
- 逐条阅读生成的 `# TODO:` 注释，取消注释/调整 `[depends]`。
- 完整的支持/拒绝清单见 `docs/zh/migrate-from-cmake.md`。
