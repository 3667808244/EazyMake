# 7. 监视模式与钩子

## 监视模式

`ezmk project watch` 会在你编辑源文件、头文件或 `ezmk.toml` 时自动重新构建：

```bash
$ ezmk project watch
=== Build successful: build/hello ===
watching for changes... (Ctrl-C to stop)
```

- 监视 `src_dirs`、`include_dirs` 和 `ezmk.toml`。
- 快速连续编辑会被合并（300 毫秒防抖）。
- 编辑 `ezmk.toml` 会清除缓存并执行完整重新构建。
- 构建失败**不会**停止监视循环——修复后保存即可重试。

有用的标志（与 `build` 的构建选项相同）：

```bash
$ ezmk project watch --profile debug -j4
$ ezmk project watch --no-build-on-start   # 等待首次变更再构建，而不是立即构建
```

## 构建钩子

钩子是在构建生命周期的特定节点运行的 Lua 脚本。在 `ezmk.toml` 中声明（路径相对于项目根目录）：

```toml
[hooks]
pre_build  = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
```

| 钩子 | 触发时机 |
|---|---|
| `pre_build` | 编译之前 |
| `post_build` | 链接成功后 |
| `on_failure` | 构建出错时 |

每个钩子接收一个 `ctx` 表：

```lua
-- scripts/post.lua
ezmk.info("built: " .. ctx.output)
ezmk.info("root:  " .. ctx.project_root)
if ctx.profile ~= "" then
    ezmk.info("profile: " .. ctx.profile)
end
```

- `ctx.output` — 构建产物的路径
- `ctx.project_root` — 项目根目录
- `ctx.profile` — 当前激活的 profile 名称（无则为空）

钩子在和工具脚本相同的沙箱中运行（没有 `os`/`io`；使用 `ezmk.*` API）。找不到钩子脚本时会警告并跳过——不会导致构建失败。钩子仅作用于你的项目，包编译不受影响。

下一章：[工具 →](08-utils.md)
