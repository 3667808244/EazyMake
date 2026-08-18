# 13. 第三方与私有仓库

官方仓库预注册了常用包，但你的团队可能需要自己的包源：内部库、私有分支、离线镜像。EazyMake 的仓库系统基于 git——`ezmk repo add <url>` 注册，`ezmk repo update` 刷新索引。

## 注册一个仓库

```bash
$ ezmk repo add git@github.com:your-org/ezmk-repo.git
$ ezmk repo add -u https://github.com/example/repo.git --name community --branch stable
$ ezmk repo add -p E:/packages/my-dev-repo --name local-dev
```

| 参数 | 说明 |
| ---- | ---- |
| `-p` / `-u` / `-g` | 项目 / 用户 / 全局作用域（默认项目） |
| `<url>` | git clone URL 或本地目录路径 |
| `--name <name>` | 仓库名（省略时从 URL 推断：取路径末尾去 `.git`） |
| `--branch <branch>` | 跟踪分支，仅 git 仓库有效，默认 `main` |

- **git 仓库**：ezmk 克隆到对应作用域的缓存目录，之后可按包名安装。
- **本地目录**：验证 `index.toml` 后**直接就地读取**，改动即时生效——适合开发中的包或离线镜像。

## index.toml：仓库的包索引

仓库根目录必须有 `index.toml`：

```toml
[repo]
name = "my-repo"
description = "My project's package repository"

[[packages]]
name = "foo"
version = "0.1.0"
file = "packages/foo-0.1.0.zip"
sha256 = "a1b2c3d4e5f6..."   # 可选，但强烈建议提供
```

- 同一包的多个版本通过重复 `[[packages]]` 条目表示（`name` 相同、`version` 不同）。
- `pkg install` 默认取最高版本；不满足约束的版本会被跳过。

### `[platform]`：按平台映射归档路径（1.1.0+）

不同平台需要不同归档时，用 `[platform]` 给平台键映射路径前缀：

```toml
[platform]
windows_x86_64_msvc = "win/msvc"
windows_x86_64      = "win/gcc"
linux_x86_64_gcc    = "linux/gcc"
darwin_arm64_clang  = "mac/clang"
```

键格式为 `{os}_{arch}[_{toolchain}]`（`os`：`windows` / `linux` / `darwin`；`toolchain`：`gcc` / `clang` / `msvc`）。ezmk 先试含工具链的三键，再退回两键，命中的值作为该平台归档的路径前缀。

## 刷新索引

`repo add` 之后（以及仓库更新后）刷新缓存索引：

```bash
$ ezmk repo update
```

> **为什么首次安装前要先 `repo update`？** 按名搜索读的是缓存中的 `index.toml`——即上次 clone 或 update 时的快照。`repo update` 做 `git pull` 刷新它，否则索引可能过期。

## 私有仓库认证

`repo add` / `repo update` 通过 `git clone` / `git pull` 子进程执行，继承 ezmk 进程的完整环境与 git 全局配置——SSH key、凭据助手、CI token 都按你习惯的方式配好 git 即可，ezmk 照常拉取。

## 易错点

- **`-p/-u/-g` 只选一个**：`pkg install` 和 `repo add` 只接受一个作用域标志。
- **本地目录不 clone、无版本快照**：改动即生效，适合开发调试；对外分发请走 git 仓库。
- **首次按名安装前先 `repo update`**：否则用的是注册时的旧索引快照。
