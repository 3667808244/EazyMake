# EazyMake 示例

本目录是 `ezmk example` 内置示例的**单一事实源**：构建 EazyMake 时，
`scripts/embed_examples.py` 会把这些文件嵌入二进制，用户 `ezmk example <name>`
即可生成到当前目录（离线可用、与版本同源）。

```bash
ezmk example list        # 列出全部示例
ezmk example hello       # 生成到 ./hello/
ezmk example hello -o /tmp   # 生成到 /tmp/hello/
```

> 每个示例都是完整可构建的项目；生成后 `cd <name> && ezmk build` 即可。
> 构建产物（`build/`、`.ezmk/`、`*.o`）已被仓库根 `.gitignore` 覆盖，
> 直接在仓库内构建也不会污染 git。

| 示例 | 一句话 | 对应教程 | 运行方式 |
|------|--------|----------|----------|
| `hello` | 最简可执行项目 | [01 安装与验证](../tutorial/zh/basic/01-install.md) / [02 第一个项目](../tutorial/zh/basic/02-first-project.md) | `ezmk example hello && cd hello && ezmk build && ezmk run` |
| `greeter` | 静态库骨架（公共头文件 + 实现） | [03 理解 ezmk.toml](../tutorial/zh/basic/03-config.md) / [05 构建配置](../tutorial/zh/basic/05-profiles-parallel.md) | `ezmk example greeter && cd greeter && ezmk build` → `build/libgreeter.a` |
| `with-packages` | 依赖 + 版本约束（`fmt@^10.0`）+ lockfile | [06 使用包](../tutorial/zh/packages/01-packages.md) / [12 版本约束](../tutorial/zh/packages/02-version-lockfile.md) | `ezmk example with-packages && cd with-packages && ezmk run`（首次构建自动装 fmt，需网络） |
| `with-tests` | Catch2 测试（`ezmk test`） | [09 测试你的项目](../tutorial/zh/dev/03-test.md) | `ezmk example with-tests && cd with-tests && ezmk pkg install catch2 -y && ezmk test`（首次装 catch2，需网络） |
| `with-hooks` | 构建钩子（pre/post Lua 脚本） | [07 监视模式与钩子](../tutorial/zh/dev/01-watch-hooks.md) | `ezmk example with-hooks && cd with-hooks && ezmk build -v` |
| `cmake-interop` | CMake 导出 / 导入互操作 | [11 导入 CMake 项目](../tutorial/zh/interop/01-import-cmake.md) | `ezmk example cmake-interop && cd cmake-interop && ezmk project export cmake` |

> 示例文件为**中文注释**（中文为项目文档基准语言）。本目录与教程章节一一对应，
> 教程每章尾部会提示「运行 `ezmk example <name>` 获取完整示例」。
