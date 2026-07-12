# 1. 安装与验证

## 安装

在 Linux、macOS 或 MSYS2（Windows）上，一行命令即可安装：

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | bash
```

这会从源码构建 `ezmk` 并安装到 `$HOME/.local/bin`。要安装到其他位置：

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh | PREFIX=/usr/local bash
```

如果想先查看脚本内容（对任何 `curl | bash` 操作都建议这么做）：

```bash
curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/install.sh -o install.sh
less install.sh
bash install.sh
```

> **裸 Windows（无 MSYS2）：**从
> [GitHub Release](https://github.com/3667808244/EazyMake/releases) 下载预编译的 `ezmk.exe`，并将其放到你的 `PATH` 中。

## 环境要求

安装脚本会替你检查以下各项：

- `git`、`bash`
- C++17 编译器（`g++` 或 `clang++`）
- `python3`（仅构建时需要；如果缺失，界面语言会回退到英文 —— 构建仍然可以完成）

## 验证

```bash
$ ezmk version
EazyMake 0.9.0
```

如果提示找不到命令，可能是因为 `$HOME/.local/bin` 不在你的 `PATH` 中。添加它：

```bash
export PATH="$HOME/.local/bin:$PATH"   # 把这行放到 ~/.bashrc 或 ~/.zshrc 中
```

## 语言与颜色

- 使用 `EZMK_LANG=zh` 或 `EZMK_LANG=en` 设置界面语言。
- 使用 `--color=always` / `--color=never` 强制启用或禁用 ANSI 颜色（默认为 `auto`）。

```bash
$ EZMK_LANG=zh ezmk help    # 中文帮助文本
```

下一步：[你的第一个项目 →](02-first-project.md)
