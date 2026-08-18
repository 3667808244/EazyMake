# 仓库管理

EazyMake 的仓库是一个 **git 仓库**。用户通过 `ezmk repo add <git_url>` 注册仓库，工具自动 `git clone` 到本地缓存，之后 `pkg install` 可按包名（而非 URL）安装。

> **为什么 clone 到本地缓存？** 这样 `pkg install` 按名搜索时读取的是磁盘上的本地副本——克隆一次后离线也可用，后续 `repo update` 只是 `git pull`，只拉取变更的差异。

---

## 仓库结构

一个 EazyMake 仓库是一个 git 仓库，根目录包含：

```
<repo>.git/
  index.toml           # 仓库元数据 + 包索引（必需）
  packages/            # 包归档文件存放目录（含普通库包和 utils 工具包）
    foo-0.1.0.zip
    bar-1.2.0.tar.gz
    bar-1.1.0.tar.gz
    ezmk-cc-0.1.0.zip  # utils 工具包也是普通包，放在 packages/ 下
    ...
```

### 设计理由

| 特性     | git 方案                    | 静态目录方案               |
| -------- | --------------------------- | -------------------------- |
| 增量更新 | `git pull`，只拉取差异      | 重新下载整个 `index.toml`  |
| 版本管理 | git log / tag 天然追溯      | 需自行维护版本文件         |
| 托管     | GitHub、GitLab、Gitee、自建 | 需要文件服务器             |
| 离线     | clone 后本地即可用          | 同样可以，但获取过程需手动 |

---

## `index.toml` 格式

```toml
[repo]
name = "my-repo"
description = "My project's package repository"

[[packages]]
name = "foo"
version = "0.1.0"
file = "packages/foo-0.1.0.zip"
sha256 = "a1b2c3d4e5f6..."   # 可选，但强烈建议提供

[[packages]]
name = "bar"
version = "1.2.0"
file = "packages/bar-1.2.0.tar.gz"
sha256 = "d4e5f6a7b8c9..."

[[packages]]
name = "bar"
version = "1.1.0"
file = "packages/bar-1.1.0.tar.gz"
sha256 = "g7h8i9j0k1l2..."

[[packages]]
name = "ezmk-cc"
version = "0.1.0"
file = "packages/ezmk-cc-0.1.0.zip"
sha256 = "hsiqno182bl2..."

# 1.2.4+：目录包 —— file 指向仓库内目录（免打包），无 sha256
[[packages]]
name = "mylib"
version = "0.2.0"
type = "dir"
file = "packages/mylib/"
```

### `[repo]` section

| 字段          | 类型   | 必须 | 说明                            |
| ------------- | ------ | ---- | ------------------------------- |
| `name`        | string | 是   | 仓库名称，用于注册后的标识      |
| `description` | string | 否   | 仓库描述，`ezmk repo list` 展示 |

### `[[packages]]` section（可重复）

| 字段      | 类型   | 必须 | 说明                              |
| --------- | ------ | ---- | --------------------------------- |
| `name`    | string | 是   | 包名称                            |
| `version` | string | 是   | 包版本，建议 SemVer               |
| `file`    | string | 是   | 包归档或目录相对于仓库根目录的路径 |
| `type`    | string | 否   | `"dir"` = 目录包（1.2.4+）；省略 = 归档包 |
| `sha256`  | string | 否   | 归档的 SHA-256 校验值（建议提供；目录包省略） |

> **为什么 `sha256` 可选但建议提供？** 校验值让 `pkg install` 验证归档完整性，因此公共仓库应提供。保留可选，是为了不卡住信任自身文件的简单内部仓库。

> **目录包（1.2.4+）：** `type = "dir"`（或 `file` 直接指向目录）时，`pkg install` 走**文件夹安装**（同 `pkg install <目录>`：校验 `ezmk.toml` + 源码编译/安装），**无归档 sha256 校验**。适合 header-only / 源码包免打包托管（git 仓库克隆即得源码，改动即时生效）；`local` 类型仓库最适用。归档包行为完全不变。

同一包的多个版本通过重复 `[[packages]]`、`name` 相同而 `version` 不同来表示。`pkg install` 默认安装最新版本。

> **为什么在索引中保留每个版本？** 这样 `pkg install` 默认取最新版、`pkg update` 可平滑升级——需要旧版本的项目也仍能找到。

---

## 仓库注册与本地缓存

### 注册表 `list.toml`

已注册的仓库列表存储路径：

| 作用域 | 路径                                 |
| ------ | ------------------------------------ |
| 全局   | `<ezmk_install_dir>/repo/list.toml`  |
| 用户   | `~/.local/ezmk/repo/list.toml`       |
| 项目   | `<project_dir>/.ezmk/repo/list.toml` |

格式：

```toml
[[repos]]
name = "my-repo"
url = "git@github.com:user/ezmk-repo.git"
type = "git"
branch = "main"
last_update = "2026-06-19T12:00:00Z"

[[repos]]
name = "community"
url = "https://gitee.com/example/ezmk-repo.git"
type = "git"
branch = "main"
last_update = ""

[[repos]]
name = "local-dev"
url = "E:/packages/my-dev-repo"
type = "local"
last_update = "2026-06-19T10:00:00Z"
```

| 字段          | 说明                                                              |
| ------------- | ----------------------------------------------------------------- |
| `name`        | 仓库唯一标识名                                                    |
| `url`         | git clone URL（`type = "git"`）或本地目录路径（`type = "local"`） |
| `type`        | `"git"` 或 `"local"`                                              |
| `branch`      | 跟踪的分支，`type = "git"` 时有效，默认 `main`                    |
| `last_update` | 最后一次 `update` 的时间                                          |

### 本地缓存路径

`git clone` 的目标目录：

| 作用域 | 缓存路径                                       |
| ------ | ---------------------------------------------- |
| 全局   | `<ezmk_install_dir>/repo/.cache/<repo_name>/`  |
| 用户   | `~/.local/ezmk/repo/.cache/<repo_name>/`       |
| 项目   | `<project_dir>/.ezmk/repo/.cache/<repo_name>/` |

对于 `type = "local"` 的仓库，没有 `.cache/` 目录——直接使用 `url` 指向的本地路径。

> **为什么本地仓库不设缓存？** 目录已在本机存在，clone 是多余的——ezmk 直接就地读取其 `index.toml`，改动即时生效（对离线镜像或正在开发的仓库很方便）。

---

## 子命令

### `ezmk repo add`

注册一个新仓库，并 clone 到本地缓存。

```
ezmk repo add [-p|-u|-g] <url> [--name <name>] [--branch <branch>]
```

| 参数                | 说明                                                                 |
| ------------------- | -------------------------------------------------------------------- |
| `-p`                | 项目作用域（默认）                                                   |
| `-u`                | 用户作用域                                                           |
| `-g`                | 全局作用域                                                           |
| `<url>`             | git clone URL（如 `https://github.com/user/repo.git`）或本地目录路径 |
| `--name <name>`     | 仓库名称（可选）。省略时从 URL 推断：取路径末尾，去掉 `.git`         |
| `--branch <branch>` | 跟踪的分支，仅对 git 仓库有效，默认 `main`                           |

**示例**：

```bash
# 注册 GitHub 仓库（默认项目作用域）
ezmk repo add git@github.com:user/ezmk-repo.git

# 注册并自定义名称和分支
ezmk repo add -u https://github.com/example/repo.git --name community --branch stable

# 注册本地目录（开发调试用）
ezmk repo add -p E:/packages/my-dev-repo --name local-dev

# 全局作用域
ezmk repo add -g https://gitee.com/org/public-repo.git
```

**行为**：
1. 解析 URL 类型——含 `://` 或 `git@` 前缀且不含 `://` → git 仓库；纯路径（以 `/` 或盘符开头）→ 本地目录
2. 对于 git 仓库：
   - 在对应作用域的 `.cache/` 下创建子目录
   - 执行 `git clone --branch <branch> <url> <cache_path>`
   - 如果 clone 失败 → 报错并清理
3. 对于本地目录：
   - 验证 `index.toml` 存在且格式正确
   - 不 clone，直接记录路径
4. 写入 `list.toml`
5. 如同名仓库已存在 → 报错（如需覆盖先 `remove`）

### `ezmk repo remove`

移除已注册的仓库，并删除本地缓存。

```
ezmk repo remove [-p|-u|-g] <name>
```

| 参数     | 说明             |
| -------- | ---------------- |
| `-p`     | 项目作用域       |
| `-u`     | 用户作用域       |
| `-g`     | 全局作用域       |
| `<name>` | 要移除的仓库名称 |

- 默认作用域：`-pug`（在所有作用域中查找并移除第一个匹配项）
- 移除时删除缓存目录（`.cache/<name>/`），但不影响 `type = "local"` 的原始路径
- 如未找到 → 报错

**示例**：

```bash
ezmk repo remove my-repo           # 所有作用域
ezmk repo remove -g community      # 仅全局作用域
```

### `ezmk repo update`

更新仓库索引——对 git 仓库执行 `git pull`，对本地目录重新读取。

```
ezmk repo update [-p|-u|-g] [<name>]
```

| 参数           | 说明                                       |
| -------------- | ------------------------------------------ |
| `[-p\|-u\|-g]` | 作用域，默认 `-pug`                        |
| `<name>`       | 仓库名称（可选）。省略时更新所有已注册仓库 |

**行为**：
1. **git 仓库**（`type = "git"`）：
   - 在缓存目录执行 `git pull origin <branch>`
   - 如果本地缓存不存在（之前 clone 失败或手动删除），重新 `git clone`
   - `git pull` 失败 → 警告但不报错（网络问题不应阻断构建）
2. **本地目录**（`type = "local"`）：
   - 重新读取 `index.toml`
3. 更新 `list.toml` 中的 `last_update` 时间戳

**示例**：

```bash
ezmk repo update                    # 更新全部
ezmk repo update -p my-repo        # 仅更新项目作用域的 my-repo
```

### `ezmk repo list`

列出已注册的仓库及其状态。

```
ezmk repo list [-p|-u|-g]
```

| 参数           | 说明                    |
| -------------- | ----------------------- |
| `[-p\|-u\|-g]` | 筛选作用域，默认 `-pug` |

**示例输出**：

```
Repositories (project scope):
  my-repo       git@github.com:user/repo.git (main)    2026-06-19 12:00
  local-dev     E:/packages/my-dev-repo (local)         2026-06-19 10:00

Repositories (user scope):
  community     https://gitee.com/example/repo.git (stable)  2026-06-19 08:30

Repositories (global scope):
  (none)
```

---

## 与 `pkg install` 集成

注册仓库后，可通过包名（而非完整 URL）安装：

```bash
# 旧方式：每次都要提供完整 URL
ezmk pkg install -p https://raw.githubusercontent.com/user/repo/main/packages/foo.zip

# 新方式：注册后按名称安装
ezmk repo add -p git@github.com:user/ezmk-repo.git --name my-repo
ezmk pkg install -p foo
```

### 查找顺序

当 `pkg install` 的参数既不是本地文件路径、也不是带 `://` 的 URL 时：

1. 按**项目 → 用户 → 全局**的顺序查找注册表 `list.toml`
2. 在每个作用域内按注册顺序遍历仓库
3. 对于每个仓库，读取其 `index.toml`（git 仓库读本地缓存，本地仓库读源路径）
4. 在 `[[packages]]` 中按 `name` 搜索
5. 若同一包有多个版本，取版本号最高者
6. 从仓库的 `packages/` 目录获取归档文件（git 仓库读本地缓存，需安装）
7. 若所有仓库都未找到 → 报错

> **为什么跨仓库取最高版本？** 版本比较覆盖所有已注册仓库而非仅第一个匹配，这样 `pkg install <name>` 无论最新版在哪个仓库都能拿到。

### 注意事项

- 首次 `pkg install foo` 前确保已执行 `ezmk repo update`，否则可能使用旧的 `index.toml`
- 推荐在 `ezmk project build` 前自动执行 `ezmk repo update --pug`（可选，可在后续版本加入）
- 仓库中的包如果有 `sha256`，安装时必须校验

> **为什么首次安装前要先 `repo update`？** 按名搜索读取的是缓存中的 `index.toml`——即上次 clone 或 update 时的快照。`repo update`（`git pull`）会刷新它，否则索引可能过期。

---

## CI/CD 里的私有仓库

`ezmk repo add` / `repo update` 通过 `git clone` / `git pull` 子进程执行。这些子进程**继承 ezmk 进程的完整环境变量 + git 的全局配置**，因此私有仓库的认证走 git 的标准机制——按你 CI 已经习惯的方式配置 git，ezmk 就能照常拉取。

> ezmk 没有专门的 `--credential-helper` / `--git-config` 参数。这不是障碍：任何 git 层面的认证（SSH 密钥、credential helper、`http.extraHeader`）都会被 clone/pull 子进程继承。

### SSH URL（推荐）

用 SSH 远程地址（`git@host:org/repo.git`），并给 Runner 配置 SSH 密钥。`git` 会使用环境里的 SSH agent（或 `~/.ssh/`）：

```yaml
# GitHub Actions — 前一步把 deploy key 加载进 ssh-agent
- name: Set up SSH
  uses: webfactory/ssh-agent@v0.9.0
  with:
    ssh-private-key: ${{ secrets.EZMK_REPO_DEPLOY_KEY }}

- name: Register private repo
  run: ezmk repo add -u git@github.com:your-org/ezmk-repo.git
```

### HTTPS URL

使用 credential helper，或把认证头注入 git 全局配置。clone 子进程读取的配置与手动 `git clone` 完全相同：

```bash
# GitLab CI — 用 CI_JOB_TOKEN 通过 extra header 认证
git config --global http.https://gitlab.internal/.extraHeader "Authorization: Bearer ${CI_JOB_TOKEN}"
ezmk repo add -u https://gitlab.internal/team/ezmk-repo.git
```

```yaml
# GitHub Actions — 用 PAT（或同组织内用 GITHUB_TOKEN）通过 header 认证
- name: Register private repo
  run: |
    git config --global http.https://github.com/.extraHeader "Authorization: Bearer ${{ secrets.EZMK_REPO_TOKEN }}"
    ezmk repo add -u https://github.com/your-org/ezmk-repo.git
```

> **注意**：`actions/checkout` 的 credential helper 只覆盖被 checkout 的那个仓库——一个**不同**的私有仓库（比如你的包仓库）需要按上面单独配 SSH 密钥或 header。

### 本地路径（无需认证）

已经在 Runner 文件系统上的仓库不需要 clone、也不需要认证——直接注册为 `type = "local"`：

```bash
ezmk repo add -p ./vendor/ezmk-repo --name internal
```

---

## 安全

仓库相关的安全条款(全局注册无需确认、全局安装二次确认、`sha256` 校验、`git clone`/`pull` 失败处理)已集中到 [`safety.md`](safety.md)。

---

## 目录约定总览

| 路径                                     | 说明                |
| ---------------------------------------- | ------------------- |
| `<ezmk_install_dir>/repo/list.toml`      | 全局仓库注册表      |
| `<ezmk_install_dir>/repo/.cache/<name>/` | 全局仓库 clone 缓存 |
| `~/.local/ezmk/repo/list.toml`           | 用户仓库注册表      |
| `~/.local/ezmk/repo/.cache/<name>/`      | 用户仓库 clone 缓存 |
| `.ezmk/repo/list.toml`                   | 项目仓库注册表      |
| `.ezmk/repo/.cache/<name>/`              | 项目仓库 clone 缓存 |

---

## 设计决策

- **基于 git**：利用 git 的增量传输、版本追溯、分布式托管能力，无需自建服务端
- **本地路径兼容**：`type = "local"` 支持开发中的本地仓库目录，不与 git 强制绑定
- **不实现依赖解析服务端**：依赖解析完全在客户端完成（拓扑排序在 `pkg.cpp` 中已有），仓库只负责"提供包"
- **SHA-256 可选**：建议提供但非强制——内部仓库可省略，公共仓库应提供
- **每个作用域独立 clone**：项目、用户、全局的仓库缓存独立，避免权限和并发冲突
