# EazyMake 1.4.1 执行计划

> **状态：dev 阶段完成**（2026-09-01 全量 1020/5970 零回归，实现收口）。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.1.md**](plans/1.4.x/1.4.1.md)。本计划为 1.4.0 正式发布后的**第一个补丁版本**（单主题：`pkg install` 支持 git 仓库 URL），对照 1.3.1~1.3.6 的单功能补丁惯例。
>
> **范围边界**：只做 git URL 安装支持——识别（`git@` / `git://` / `file://` / `.git` 后缀）→ 克隆 → ref 定位（`#<ref>` / `--branch`，分支/标签浅克隆、commit 全量）→ 复用 `install_from_directory` 安装 → lockfile 记录 `source="git"` + `commit` + `--locked` 校验。子模块递归 / 仓库子目录 / `pkg update` git 语义**明确不做**。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 1003 用例 / 5835 断言，1.4.0 发布态；1.4.1 dev 后 1020/5970 零回归）。
>
> **版本决策**：dev 阶段二进制版本号保持 1.4.0（1.2.x/1.3.x 补丁先例不提前 bump），正式发布 commit 按 workflow §3 bump 1.4.1。

---

## 1 背景

- `pkg install` 支持名称（仓库）/ 本地归档 / 本地目录 / 归档 URL 四种来源，**git 仓库 URL 不支持**：`git@github.com:user/repo.git` 被 `is_url` 启发式误判为 URL 补 `https://` 前缀后下载失败；`https://github.com/user/repo.git` 被当作归档下载、解压失败（`src/pkg.cpp` 1611-1623）。
- 基础设施已齐备：`util::git_available()/git_clone()/git_pull()`（`util.cpp` Git helpers）、`repo.cpp` 私有 `is_git_url()`、`install_from_directory()`（`pkg.cpp:1519`）目录安装全链路。缺口仅为：来源判定 + 克隆落地 + ref 定位 + lockfile 记录。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `pkg install` 识别 git 仓库 URL 并克隆安装，复用 `install_from_directory` 全链路 | P0 |
| 2 | ref 定位：`#<ref>` 片段 + `--branch <ref>`（分支/标签/commit SHA），浅克隆策略 | P0 |
| 3 | lockfile 记录 git 源（`source="git"` + `source_url` + 可选 `commit` 字段）；`--locked` 校验 commit | P0 |
| 4 | 完整性语义：commit 为指纹、`--sha256` 跳过提示、`git://` 明文警告 | P1 |
| 5 | 文档三向 + i18n 三向（+5 key，`check_i18n.py` 通过） | P0 |
| 6 | 全量回归 `bash build.sh test-all` 零失败（基线 1003/5835）；CI push 绿色 | P0 |

## 3 执行阶段

### 阶段一：git 来源检测 + 克隆安装链路（对应设计 §3.1/3.2）

- [x] **1.1** `util::is_git_url()` 提取（**保留 repo 宽松语义**：`git@`/任意 `://`/含点含斜杠 + 本地路径排除，repo add 行为零变化），`repo.cpp` 改调 util 版本（repo add 兼容性回归测试覆盖）；`pkg.cpp` 用严格子集判定（剥 `#ref` 后 `.git` 结尾 / `git@` / `git://` / `file://`）
- [x] **1.2** `pkg.cpp` 检测顺序插入 git 分支：目录 → **git** → 归档 URL → 名称搜索；判定短路不误伤 `.zip`/`.tar.gz`
- [x] **1.3** 克隆 → 临时目录 → `install_from_directory` 全链路；git 不可用 → fatal；`util::git_head_commit()` helper（`rev-parse HEAD`）
- [x] **1.4** 单测：`is_git_install_source` 判定表（命中：`git@`/`git://`/`file://`/`.git`/`.git#ref`；不误判：`.zip`/`.tar.gz`/`foo.zip#x`/无协议非 git）

### 阶段二：ref 定位与克隆策略（对应设计 §3.2）

- [x] **2.1** `#<ref>` 片段解析（最后一个 `#` 后全部，克隆前剥离）+ `--branch <ref>` flag（优先级：flag > 片段 > 默认分支）
- [x] **2.2** `util::git_clone` 加 `shallow` 参数（默认 false，repo 调用零变化）：分支/标签/默认分支 `--depth 1`；commit SHA 全量克隆 + checkout
- [x] **2.3** 集成测试：`#tag` / `#<sha>` / `--branch` / 默认分支四形态安装，lockfile commit 与 `rev-parse HEAD` 一致

### 阶段三：lockfile commit + `--locked` 校验（对应设计 §3.3）

- [x] **3.1** `LockfilePackage` 新增可选 `commit` 字段（load/save 兼容：空串缺省、非空才写出、lockfile version 保持 1）
- [x] **3.2** git 源写入 `source="git"` + `source_url` + `commit`；sha256 留空
- [x] **3.3** `--locked`：按 lockfile 的 source_url+commit 克隆，`rev-parse HEAD` 不一致 → fatal（`lock_commit_mismatch`）；lockfile 不重写
- [x] **3.4** 测试：commit 匹配成功 / 篡改后失败 / 旧 lockfile（无 commit 字段）正常解析

### 阶段四：i18n 三向（对应设计 §3.4）

- [x] **4.1** 5 个新 key（`pkg_git_cloning` / `pkg_git_not_available` / `pkg_git_sha256_skipped` / `pkg_git_plain_confirm` / `lock_commit_mismatch`）入 `i18n_keys.def` + en/zh JSON（zh-TW 继承回退）
- [x] **4.2** `python scripts/check_i18n.py` 通过（键数 397 → 402）

### 阶段五：测试补齐 + 收口（对应设计 §3.6/§4）

- [x] **5.1** 集成补齐：`file://` 端到端（fixture `git init` + tag + 提交）、`--sha256` 跳过提示、`git://` 明文确认（`-y` 跳过）
- [x] **5.2** 全量回归 `bash build.sh test-all` 零失败（**1003 用例 / 5835 断言**基线，零回归）；CI 工作流覆盖核对
- [x] **5.3** 文档：docs/en|zh `pkg.md`「Package Sources」新增 Git 小节 + 检测顺序、`cli.md` `pkg install` 参数表（git URL / `--branch`）、README 速览核对、`.claude/skills/ezmk-user-pkg/SKILL.md` 同步
- [x] **5.4** `CHANGES.md` 1.4.1 条目（新增 / 行为变更 / 测试 / 已知限制）
- [x] **5.5** `plans/1.4.x/README.md` / `plan.md` / `plans/README.md` 状态更新（全勾选）
- [x] **5.6** 发布门槛复核（⛔：① 清单全部完成或明确收口 ② 公共 API 无破坏性变更 ③ 全量零回归）——满足后按 workflow 正式发布（bump 1.4.1）

## 4 关键设计决策

| 决策 | 结论 | 理由 |
|------|------|------|
| 补丁范围 | 单主题：git URL 安装 | 用户需求聚焦；对照 1.3.1~1.3.5 单功能补丁惯例 |
| git 判定 | `.git` 后缀（剥 `#ref` 后）+ `git@`/`git://`/`file://` 前缀 | 最短可判定规则，零误伤归档 URL；判定表单测锁定 |
| 安装路径 | 克隆 → `install_from_directory` 复用 | 目录安装全链路（校验/钩子/依赖/编译/拷贝/lockfile）零新代码 |
| ref 语法 | `#<ref>` 片段 + `--branch` flag | `#` 是 URL fragment 天然分隔，无歧义；不采用 `@ref`（与 SSH `git@` 冲突） |
| 浅克隆 | 分支/标签/默认 `--depth 1`；commit SHA 全量 | SHA 在浅克隆下可能不可达，正确性优先 |
| lockfile | 新增可选 `commit` 字段，version 保持 1 | 向后兼容旧 lockfile；git 源以 commit 为可复现指纹 |
| `--locked` | 克隆后比对 `rev-parse HEAD` | 防 tag/branch force-push 漂移，与「lockfile 是唯一真相」语义一致 |
| `pkg update` | git 源提示重新安装，不做升级逻辑 | git 源无语义版本；自动刷新分支会静默升级，与 `--locked` 冲突 |

## 5 兼容性矩阵

| 变更 | 影响 | 说明 |
|------|------|------|
| 来源检测插入 git 分支 | 新增来源 | 仅 `.git` 后缀 / 特殊前缀命中时生效；既有 4 种来源行为不变 |
| `util::git_clone` 加 `shallow` | 内部签名 | 默认 false，repo.cpp 调用零变化 |
| `util::is_git_url()` 提取 | 内部重构 | repo add 行为对齐 + 回归测试锁定 |
| `LockfilePackage.commit` | 格式扩展 | 可选字段，旧 lockfile 兼容；非空才写出 |
| 新 flag `--branch` / 新 i18n key | 纯增量 | 不改变既有 flag/key 语义 |
| 公共 API | **无破坏性变更** | 纯增量补丁 |

## 6 延后项（裁定表完整版见设计文档 §3.8）

- **明确不做**：子模块递归（`--recurse-submodules`）、仓库子目录选择（git 仓库根即包根）、`pkg update` 的 git 版本语义（提示重新安装）、`git+https://` 等 scheme 别名、`[depends]` 中的 git URL
- **不影响**：1.4.0 裁定表「收口 1.5.x」项（launch 透传 / 语义 C / workspace 相关）继续按原归宿
