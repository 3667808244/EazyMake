# 6. 使用包

包是从源码预构建的静态库（`.a`），你的项目链接时会用到它们。你可以从**仓库**（包含 `index.toml` 的 git 仓库）获取包，也可以直接从文件或 URL 安装。

## 添加仓库

```bash
$ ezmk repo add https://github.com/example/ezmk-repo.git --name example
$ ezmk repo update            # git pull 拉取最新索引
$ ezmk repo list
$ ezmk repo info example      # 查看可用的包和版本
```

你也可以使用本地目录作为仓库（`type = "local"`）。

## 安装包

```bash
$ ezmk pkg install fmt         # 从已注册的仓库按名称安装
$ ezmk pkg install ./fmt.zip   # 从本地压缩包安装
$ ezmk pkg install https://.../fmt.tar.gz --sha256 <hash>   # 验证完整性
```

### 作用域

包的安装位置由作用域标志控制：

| 标志 | 作用域 | 路径 |
|---|---|---|
| `-p` | 项目（默认） | `.ezmk/pkg/` |
| `-u` | 用户 | `~/.local/ezmk/pkg/` |
| `-g` | 全局 | `<install_dir>/pkg/`（需要确认） |

```bash
$ ezmk pkg install -u fmt      # 为当前用户安装
```

## 声明依赖

在 `ezmk.toml` 中引用包：

```toml
[depends]
lib  = ["fmt"]      # 必需依赖
want = ["spdlog"]   # 可选依赖（仅当已安装时才使用）
```

现在构建——`ezmk` 会解析依赖链，将每个包编译为静态库并链接进来：

```bash
$ ezmk project build
```

在代码中使用：

```cpp
#include <fmt/core.h>
int main(){ fmt::print("Hello {}!\n", "packages"); }
```

## 管理已安装的包

```bash
$ ezmk pkg list                # 查看已安装的包（加 -p/-u/-g 过滤作用域）
$ ezmk pkg update fmt          # 从仓库更新某个包
$ ezmk pkg update --all        # 更新全部
$ ezmk pkg remove fmt          # 卸载
```

> 构建时加上 `--auto-update` 会先执行 `ezmk repo update --pug`，确保包名解析基于最新的索引。

详见 [`docs/pkg.md`](../../docs/zh/pkg.md) 和 [`docs/repo.md`](../../docs/zh/repo.md)。

下一章：[监视模式与钩子 →](07-watch-hooks.md)
