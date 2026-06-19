# EazyMake 0.1.3 更新计划

---

## 1. 实现 `repo` 子命令

CLI 解析已完成（`cli.cpp`），`main.cpp` 中目前是占位桩。需要新建 `src/repo.cpp` + `include/ezmk/repo.hpp` 实现核心逻辑。

### 1.1 仓库模型

EazyMake 的仓库是一个 **git 仓库**，包含 `index.toml`（元数据 + 包索引）和 `packages/` 目录（包归档文件）。用户通过 `ezmk repo add <git_url>` 注册仓库，工具自动 `git clone` 到本地缓存。

```
<repo>.git/
  index.toml           # 仓库元数据 + 包索引
  packages/
    foo-0.1.0.zip
    bar-1.2.0.tar.gz
    bar-1.1.0.tar.gz
    ...
```

**为什么用 git：**
- 天然支持版本控制——`git pull` 增量更新索引和包文件，无需重新下载整个仓库
- 分布式托管——GitHub、GitLab、Gitee、自建 Git 服务器都可以作为仓库源
- 无需专用服务端——`git clone` 就是获取仓库的全部操作
- git tag 可用于标记稳定版本快照

### 1.2 `index.toml` 格式

```toml
[repo]
name = "my-repo"
description = "My project's package repository"

# 每个包一个 section
[[packages]]
name = "foo"
version = "0.1.0"
file = "packages/foo-0.1.0.zip"
sha256 = "a1b2c3..."

[[packages]]
name = "bar"
version = "1.2.0"
file = "packages/bar-1.2.0.tar.gz"
sha256 = "d4e5f6..."

[[packages]]
name = "bar"
version = "1.1.0"
file = "packages/bar-1.1.0.tar.gz"
sha256 = "g7h8i9..."
```

### 1.3 仓库注册信息与本地缓存

已注册的仓库列表保存在：
- **全局**：`<ezmk_install_dir>/repo/list.toml`
- **用户**：`~/.local/ezmk/repo/list.toml`
- **项目**：`.ezmk/repo/list.toml`

`list.toml` 格式：

```toml
[[repos]]
name = "my-repo"
url = "git@github.com:user/ezmk-repo.git"    # git clone URL
branch = "main"                                # 跟踪的分支（默认 main）
last_update = "2026-06-19T12:00:00Z"

[[repos]]
name = "community"
url = "https://gitee.com/example/ezmk-repo.git"
branch = "main"
last_update = ""
```

**本地缓存路径**（`git clone` 的目标目录）：

| 作用域 | 本地缓存路径 |
|---|---|
| 全局 | `<ezmk_install_dir>/repo/.cache/<repo_name>/` |
| 用户 | `~/.local/ezmk/repo/.cache/<repo_name>/` |
| 项目 | `.ezmk/repo/.cache/<repo_name>/` |

每个仓库 clone 到对应作用域的 `.cache/` 下，以仓库名称为子目录。`ezmk repo update` 在此目录执行 `git pull`。

### 1.4 子命令行为

| 命令 | 行为 |
|---|---|
| `ezmk repo add [-p\|-u\|-g] <git_url> [--name <name>] [--branch <branch>]` | 注册仓库并 `git clone` 到本地缓存。`--name` 省略时从 URL 推断（取路径末尾，去掉 `.git`）。`--branch` 默认 `main`。默认作用域 `-p` |
| `ezmk repo remove [-p\|-u\|-g] <name>` | 移除注册并删除本地缓存目录。默认 `-pug`（所有作用域） |
| `ezmk repo update [-p\|-u\|-g] [<name>]` | 在本地缓存目录执行 `git pull` 拉取最新。`<name>` 省略时更新所有。默认 `-pug` |
| `ezmk repo list [-p\|-u\|-g]` | 列出已注册仓库（名称、git URL、分支、最后更新时间）。默认 `-pug` |

### 1.5 与 `pkg install` 集成

`ezmk pkg install -p <pkg_name>` 的新查找顺序：
1. 当前目录的本地路径 / 显式 URL（和现在一样）
2. 已注册仓库的本地缓存中按名称搜索（按项目 → 用户 → 全局顺序查找 repo，在 clone 的 `index.toml` 中查找包名）
3. 仍未找到 → 报错

此集成是 0.1.3 的关键目标——注册仓库后只需包名即可安装。

### 1.6 本地路径仓库兼容

为方便本地开发和离线使用，也支持本地目录作为仓库源（不需要是 git 仓库）：

```
ezmk repo add -p /path/to/local/repo --name local-dev
```

此时：
- `add`：只记录路径，不 clone（本身就是本地目录）
- `update`：直接读取本地目录的 `index.toml`，无需网络
- git 仓库和本地目录在 `list.toml` 中用 `type` 字段区分：`type = "git"` 或 `type = "local"`

### 1.7 实现要点

- **新增文件**：`src/repo.cpp`、`include/ezmk/repo.hpp`
- **依赖**：需要系统安装 `git` 命令行工具在 PATH 中
- `main.cpp`：用 `ezmk::repo::add/remove/update/list` 替换占位桩
- `pkg.cpp`：在 `install` 查找逻辑中增加仓库搜索回退（读取本地缓存中的 `index.toml`）
- `util.cpp`：新增 `git_clone(url, dest, branch)`、`git_pull(dest)`、`git_last_commit_time(dest)` 辅助函数
- 安全：`index.toml` 中提供 `sha256` 的包，安装时校验
- `list.toml` 增加 `type` 字段（`"git"` / `"local"`），与 0.1.2 的 `RepoOptions` 兼容

---

## 2. 代码整理

当前代码已经能工作，但存在可维护性问题，0.1.3 应逐步改善。

### 2.1 CLI 解析重构（`src/cli.cpp`）

**问题**：`parse()` 函数 ~235 行，深度嵌套 `if/else`，难以新增子命令。

**方案**：按命令组分拆为独立函数：
```
parse_project_args(argc, argv) → CliArgs
parse_pkg_args(argc, argv)     → CliArgs
parse_repo_args(argc, argv)    → CliArgs
```
`parse()` 只做命令组路由。同时引入 `--help` 子命令支持（`ezmk pkg --help`）。

### 2.2 工具模块拆分（`src/util.cpp` → 多文件）

**问题**：`util.cpp` 606 行，包含日志、文件系统、SHA-256、压缩解压、HTTP 下载、进程管理六种职责。

**方案**：不急于拆成多个编译单元（避免过早抽象），但至少在 `util.cpp` 内部用清晰的 section 注释分区，并将 SHA-256 独立为 `src/crypto.cpp` + `include/ezmk/crypto.hpp`，因为它有 ~150 行纯算法代码。

备选：如果后续有更多算法需求（MD5、CRC32），`crypto` 模块可以直接扩展。

### 2.3 JSON 解析替换（`src/cache.cpp`）

**问题**：`cache.cpp` 包含 ~170 行手写 JSON 解析器，仅用于读 `record.json`。脆弱、难扩展。

**方案**：引入单个头文件 JSON 库（如 [nlohmann/json](https://github.com/nlohmann/json) 的 `json.hpp`）放到 `include/vendor/`，替换手写解析器和序列化器。这样：
- 删除 `json_escape`、`json_to_record`、`record_to_json`、`json_skip_*`、`json_read_string`、`json_expect`、`json_skip_value` 等 ~200 行临时代码
- 缓存记录的读写变成 ~30 行
- `repo` 模块的 `index.toml` 已经是 TOML（用 toml++），不需要 JSON，但缓存格式保持一致可读性更好

如果不想引入新依赖，也可以用 toml++ 将 `record.json` 改为 `record.toml`——toml++ 已经在项目里。

### 2.4 `using namespace` 清理

**问题**：所有 `.hpp` 头部都有 `namespace fs = std::filesystem;`，会污染任何包含这些头文件的翻译单元。

**方案**：将 `namespace fs = std::filesystem;` 从头文件移到 `.cpp` 实现文件中（或移到 `namespace ezmk::xxx { }` 内部）。头文件中使用完整的 `std::filesystem::`。

### 2.5 错误处理增强

**问题**：很多地方用 `util::fatal()` 直接 `exit(1)`，但调用栈中间可能有临时文件未清理。

**方案**：
- 将 `fatal` 改为抛 `ezmk::error` 异常（继承 `std::runtime_error`）
- `main()` 的 catch 块统一清理 `.ezmk/temp/` 临时目录
- 保留 `fatal` 仅用于无法恢复的场景（如编译器未找到）

### 2.6 包管理模块整理（`src/pkg.cpp`）

**问题**：557 行混合了安装、卸载、搜索、信息、依赖解析、编译六种职责。

**方案**（低优先级，可延后到 0.1.4）：
- `install` 逻辑保持，但将依赖解析（拓扑排序）独立为 `internal::resolve_deps()`
- 也可以不做——当前规模尚可，过早拆分反而增加复杂度

---

## 3. 文档完善

### 3.1 `docs/repo.md`

仓库系统的设计文档，描述仓库结构、`index.toml` 格式、子命令用法、与 `pkg` 的集成方式。

### 3.2 更新 `CLAUDE.md`

新增 `repo` 子命令到 CLI 表格。

---

## 4. 版本发布

- 版本号：`0.1.3`
- 更新 `main.cpp` 中的版本字符串
- commit message: `0.1.3: repo subcommand implementation and code cleanup`

---

## 5. 实现进度

### ✅ 已完成（2026-06-19）

| 任务 | 状态 | 说明 |
|---|---|---|
| `docs/repo.md` | ✅ | git-based 仓库设计文档 |
| `include/ezmk/repo.hpp` | ✅ | 仓库模块头文件：`RepoEntry` 结构体、`add/remove/update/list` 声明、`search_package` 集成接口 |
| `src/repo.cpp` | ✅ | 仓库核心实现：`load_repo_list`/`save_repo_list`（toml++ 读写 `list.toml`）、`add`（git clone + 注册）、`remove`（删缓存 + 注销）、`update`（git pull）、`list`（打印）、`search_package`（按名搜索包） |
| CLI 类型更新 (`cli.hpp`) | ✅ | `RepoOptions` 增加 `scopes`、`url`、`name`、`branch` 字段；移除 `std::optional` |
| CLI 解析更新 (`cli.cpp`) | ✅ | repo 四个子命令支持 `-p/-u/-g` 作用域参数、`--name`、`--branch` 选项；更新 `print_usage` 帮助文本 |
| Git 辅助函数 (`util.hpp/cpp`) | ✅ | `git_available()`、`git_clone()`、`git_pull()`、`git_last_commit_time()` |
| `main.cpp` 集成 | ✅ | 替换 repo 占位桩为实际调用；引入 `ezmk/repo.hpp`；版本号更新为 `0.1.3` |
| `pkg.cpp` 集成 | ✅ | `install` 增加仓库搜索回退：非本地文件/URL 时在已注册仓库中按名称搜索 |
| `CLAUDE.md` 更新 | ✅ | 新增 Repository management 章节、更新 CLI 表格、修正构建命令 |

| 2.1 CLI 解析重构 | ✅ | `parse()` 拆分为 `parse_project_args` / `parse_pkg_args` / `parse_repo_args` + 共享辅助 `parse_scope_and_value`。`parse()` 从 361 行减至 320 行，顶层变为 5 行路由 |
| 2.2 SHA-256 独立 | ✅ | 抽取 `src/crypto.cpp` + `include/ezmk/crypto.hpp`，`util.cpp` 减少 118 行 |
| 2.3 JSON 替换 | ✅ | 引入 nlohmann/json (`include/vendor/nlohmann_json.hpp`)，删除手写 JSON 解析器 ~160 行，`cache.cpp` 从 406 行减至 246 行 |
| 2.4 `namespace fs` 清理 | ✅ | 7 个头文件的 `namespace fs = std::filesystem;` 全部从文件作用域移入各命名空间内部 |
| 2.5 错误处理增强 | ✅ | `fatal()` 从 `exit(1)` 改为 `throw ezmk::fatal_error`；`main()` 捕获后清理 `.ezmk/temp/` |
| 版本比较修复 | ✅ | `repo.cpp` 版本比较从字符串字典序改为按 `.` 拆分后逐段数值比较 |

### ⏳ 待完成（0.1.4）

| 任务 | 优先级 | 说明 |
|---|---|---|
| pkg.cpp 整理 | 低 | 依赖解析独立、模块职责分离 |
| 缓存原子写入包编译 | 中 | `pkg.cpp` 中 `compile_package` 也应使用原子写入 |
| 测试框架 | 低 | 引入单元测试 |

### 新增/变更文件清单

```
include/ezmk/crypto.hpp       — SHA-256 模块头文件
include/ezmk/repo.hpp          — 仓库模块头文件
include/vendor/nlohmann_json.hpp — nlohmann/json 单头文件
src/crypto.cpp                 — SHA-256 实现
src/repo.cpp                   — 仓库模块实现
docs/repo.md                   — 仓库设计文档
```
