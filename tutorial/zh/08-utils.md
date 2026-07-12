# 8. 工具（clangd 集成）

`ezmk utils <name>` 运行基于 Lua 的工具。有些是内置的，有些来自已安装的 `type = "utils"` 包。

## 内置工具：`ezmk utils cc`

生成 `compile_commands.json`，让编辑器和语言服务器（clangd）理解你的构建配置：

```bash
$ ezmk utils cc
generated compile_commands.json
$ ezmk utils cc -o build/compile_commands.json   # 自定义输出路径
```

将 clangd 指向该文件（如果文件在项目根目录下，通常会自动识别），你就能获得与 `ezmk.toml` 中标志和 include 目录匹配的准确补全、跳转定义和诊断信息。

## 安装更多工具

工具以包的形式发布：

```bash
$ ezmk pkg install some-utils-pkg
$ ezmk utils <tool-name> [args...]
```

工具名之后的所有参数都会透传给工具。

## 编写自己的工具（快速体验）

工具包是一个 `type = "utils"` 的项目，并包含一个列出工具的 `[utils]` 表；每个工具对应 `utils/<name>.lua`。脚本使用沙箱化的 `ezmk.*` API（23 个函数：项目信息、编译选项、文件系统、进程、日志、JSON、路径）：

```lua
-- utils/hello.lua
ezmk.info("project: " .. ezmk.project_name())
for _, src in ipairs(ezmk.list_sources()) do
    ezmk.info("  " .. src)
end
```

```bash
$ ezmk project new mytools --type utils
$ ezmk utils hello
```

沙箱移除了 `os`/`io`；通过 `ezmk.run()` 运行外部命令，`ezmk.file_write()` 会拒绝写入项目根目录之外的路径。包还可以通过 `[utils.permissions]` 进一步限制访问权限。

详见 [`docs/utils.md`](../../docs/zh/utils.md) 了解完整的插件 API 和权限模型。

---

以上就是全部教程内容。现在你可以创建项目、配置构建、增量编译、使用 profile 和并行编译、引入包、监视文件变更、挂载构建钩子，以及与 clangd 集成。关于任何命令或选项的确切语义，[`docs/cli.md`](../../docs/zh/cli.md) 是权威参考。
