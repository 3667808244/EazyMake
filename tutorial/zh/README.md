# EazyMake 教程

一份从零到上手使用的 `ezmk` 实战指南。每一章都建立在上一章的基础上，每章结尾都有你可以直接运行的命令。

本教程教你**如何完成实际工作**。要了解精确定义和完整的选项说明，请参阅 [`docs/`](../../docs/zh/) 参考文档（尤其是 [`docs/cli.md`](../../docs/zh/cli.md)）。

## 章节

1. [安装与验证](01-install.md)
2. [你的第一个项目](02-first-project.md)
3. [理解 `ezmk.toml`](03-config.md)
4. [增量构建与缓存](04-cache.md)
5. [构建配置与并行编译](05-profiles-parallel.md)
6. [使用包](06-packages.md)
7. [监视模式与钩子](07-watch-hooks.md)
8. [Utils 工具（clangd 集成）](08-utils.md)
9. [测试你的项目](09-test.md)
10. [顶层别名（快速参考）](10-top-level-aliases.md)
11. [导入 CMake 项目](11-import-cmake.md)
12. [语义化版本约束与确定性构建](12-version-lockfile.md)
13. [第三方与私有仓库](13-third-party-repos.md)
14. [多平台多工具链预编译共包](14-precompiled-packages.md)

## 约定

- Shell 代码片段假定使用 Linux/macOS/MSYS2。在裸 Windows 上，使用
  [PowerShell 安装脚本](01-install.md#windows原生无需-msys2) (`install.ps1`)
  或从 [GitHub Release](https://github.com/3667808244/EazyMake/releases) 下载预编译的 `ezmk.exe`。
- `$` 表示你输入的命令；不带 `$` 的行是输出。
- 每个命令都有简写别名（例如 `ezmk pb` = `ezmk project build`）—— 参见
  [`docs/cli.md`](../../docs/zh/cli.md#command-shorthands-026)。
- 大多数 `project` 操作也提供**顶层别名**（`ezmk build`、`ezmk run`、`ezmk clean`、`ezmk watch`、`ezmk install`、`ezmk test`、`ezmk pack`）——本教程使用这些短形式；完整 `ezmk project <action>` 形式与之等价。
