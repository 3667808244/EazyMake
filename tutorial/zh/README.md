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

## 约定

- Shell 代码片段假定使用 Linux/macOS/MSYS2。在裸 Windows 上，使用 GitHub Release 中预编译的
  `ezmk.exe`。
- `$` 表示你输入的命令；不带 `$` 的行是输出。
- 每个命令都有简写别名（例如 `ezmk pb` = `ezmk project build`）—— 参见
  [`docs/cli.md`](../../docs/zh/cli.md#command-shorthands-026)。
