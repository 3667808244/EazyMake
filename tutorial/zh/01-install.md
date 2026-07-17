# 1. 安装与验证

## 安装

### Linux / macOS / MSYS2

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

### Windows（原生，无需 MSYS2）

下载并运行 PowerShell 安装脚本 — 无需编译器或 git：

```powershell
# 推荐方式：先审阅再运行
# 1. 打开 https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1
# 2. 另存为 install.ps1，然后执行：
.\install.ps1

# 或一行命令（方便，但建议先审阅）：
irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex
```

这会从 GitHub Releases 下载预编译的 `ezmk.exe`，校验 SHA-256，
安装到 `%LOCALAPPDATA%\ezmk\bin`，并配置用户 `PATH`。
可用参数自定义：

```powershell
.\install.ps1 -Version "0.9.5"           # 安装指定版本
.\install.ps1 -InstallDir "D:\tools\ezmk" # 自定义安装目录
.\install.ps1 -DryRun                     # 预览操作，不做实际更改
.\install.ps1 -NoPath                     # 跳过 PATH 配置
```

> **MSYS2 用户：**请使用上面的 `install.sh` 方式（使用 g++ 从源码构建）。

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
