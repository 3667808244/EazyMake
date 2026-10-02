# EazyMake 1.4.5 — 生成物格式统一（lockfile / 仓库注册表 JSON 化）

EazyMake 的内部生成物从此只用两种格式：**人写的配置是 TOML，ezmk 生成的是 JSON**。本版把最后两个例外改过来——`ezmk.lock` → **`ezmk.lock.json`**、`list.toml` → **`list.json`**（全局 / 用户 / 项目三作用域），并顺带把这两个少数还在直写的生成物升级为**原子写**。**旧格式继续可读、升级无感**：首次成功写入时自动迁移（写新文件 → 删旧文件）。**零 CLI 行为变更、零配置语义变更、公共 API 无破坏性变更。**

### 变更

- **`ezmk.lock` → `ezmk.lock.json`**：改用 JSON 序列化，形状与旧 TOML 一一对应（`metadata` + `packages`，键名逐字一致），字段顺序固定为写入顺序，便于在版本库里 review diff；空的可选字段（`lib_sha256` / `archive_sha256` / `commit`）不写出。**字段语义完全不变**：`sha256` 仍是 `lib_sha256` 的旧别名（继续双写）、`version` 仍为 1、`--locked` 与 `deterministic` 的判定逻辑不变。
- **`list.toml` → `list.json`**（三作用域）：`{"version": 1, "repos": [...]}`；`type = "local"` 的条目不写 `branch`。
- **原子写**：两个文件此前是直写，现在走 tmp → rename（半截 lockfile 在 `deterministic = true` 下是致命错误）；`ezmk-workspace.toml` 的写入也收敛到同一实现。
- **确定性构建的缓存签名**：`deterministic = true` 时 lockfile 的**内容**哈希是编译签名的一部分，三处调用点统一改为解析"当前生效的 lockfile"（不再硬编码文件名）——修复的是"某一侧取不到文件 → 签名永不等 → 每次构建全量重编"这一缺陷形态（1.4.2 曾修过同类问题）。迁移会因换格式改变内容，带来**恰一次**全量重编，之后增量命中。
- **注册表读取的安全约束不因回退路径放宽**：JSON 与旧 TOML 两条读取路径共用同一份入口校验（1.4.2 的"名字不得逃出缓存树"在旧格式文件上同样生效）。
- **zsh 补全**：仓库名补全同时识别 `list.json` 与旧 `list.toml`（补全脚本静态安装在用户机上，不随升级自动更新）。

### 迁移（无需手动操作）

- 升级后第一次读取旧文件会打印一条提示；下一次写入（`ezmk pkg install` / `ezmk repo add|remove|update`）即写入新文件并删除旧文件。`ezmk build` 只做只读校验，**不会**改盘上文件。
- **git 视角**：`ezmk.lock` 的删除 + `ezmk.lock.json` 的新增应在同一次 commit 里完成。请勿继续保留旧文件（双文件共存时以 `ezmk.lock.json` 为准，并对陈旧文件给出警告）。

### ⚠️ 降级注意

1.4.4 及更早版本只识别 `ezmk.lock` / `list.toml`。回退到旧版本时：

- `deterministic = true` 的项目会因"缺少 lockfile"报**致命错误** —— 从版本库里取回 TOML 文件即可；
- 仓库注册表会显示为空（已安装的包不受影响，重新 `ezmk repo add` 恢复）。

### 新增 API

`lockfile::lockfile_path()` / `legacy_lockfile_path()` / `active_path()`；`repo::repo_list_path()` / `legacy_repo_list_path()`（`list_toml_path()` 保留为旧路径别名，2.0.0 移除）；`util::atomic_write_text()`。

### 文档

`docs/{en,zh}` 7 个文件（`config_file.md` 的 Lockfile 小节整块换成 JSON 示例 + 迁移/降级说明、`repo.md` 注册表 JSON 示例 + 迁移说明、`cli.md`、`technical.md`、`glossary.md`、`pkg.md`、`package_authoring.md`）、教程 Pkg 02、`README.md` / `README_ZH.md`、man 2 页、`.claude/skills` 5 个文件。

**测试**：全量 **1113 用例 / 6491 断言零失败**（基线 1099/6349），其中包含**双格式对拍**用例（同值的 TOML 与 JSON 文本逐字段相等）与确定性签名用例。

**安装**：`install.sh`（Linux/macOS）、`install.ps1`（Windows）、`winget install EazyMake.EazyMake`、`brew install 3667808244/eazymake/ezmk`、`publish/arch/PKGBUILD`（Arch/MSYS2）。

**完整变更**：见 [`CHANGES.md`](https://github.com/3667808244/EazyMake/blob/main/CHANGES.md) 的 1.4.5 节；设计与执行计划见 [`plans/1.4.x/1.4.5.md`](https://github.com/3667808244/EazyMake/blob/main/plans/1.4.x/1.4.5.md)。
