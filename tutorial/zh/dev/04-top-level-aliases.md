# 10. 顶层别名（快速参考）

自 1.1.0 起，最常用的 `project` 操作也可作为顶层命令使用。完全省略 `project` 关键字：

| 顶层别名 | 完整形式 |
|---|---|
| `ezmk build` | `ezmk project build` |
| `ezmk run` | `ezmk project run` |
| `ezmk clean` | `ezmk project clean` |
| `ezmk watch` | `ezmk project watch` |
| `ezmk install` | `ezmk project install` |
| `ezmk test` | `ezmk project test` |
| `ezmk pack` | `ezmk project pack` |

两种形式**完全等价**——所有选项和参数行为一致：

```bash
$ ezmk build --profile release -j8          # 别名形式
$ ezmk project build --profile release -j8  # 完整形式——完全相同
```

## 何时使用哪种

- **日常使用**——用短的顶层形式：`ezmk build`、`ezmk run`、`ezmk test`、`ezmk watch`。
- **脚本与习惯**——完整的 `ezmk project <action>` 形式保持稳定，他人阅读时也更清晰。

## 其他别名族

还有两层用于加快输入：

- **双字母简写**（`0.2.6+`）：`ezmk pb` → `project build`，`ezmk pt` → `project test`，`ezmk ki` → `pkg install`，…… 仅作用于命令位置——`ezmk project pb` 仍是未知子命令。
- **作用域标志**：`pkg`/`repo` 的 `-p`（项目）、`-u`（用户）、`-g`（全局）。

完整的别名表见 [`docs/zh/cli.md`](../../../docs/zh/cli.md#命令简写026)。

教程到此结束。你现在已掌握：安装、初始化、配置、增量构建、构建配置与并行、使用包、监视、钩子、clangd 集成、运行测试，以及 ezmk 提供的所有命令形式。
