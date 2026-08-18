# EazyMake 1.2.4 执行计划

> **状态：已完成**（2026-08-18 执行完毕，待发布）。1.2.x 系列路线图见 [`plans/1.2.x/README.md`](plans/1.2.x/README.md)。
>
> 详细设计：[**1.2.4.md**](plans/1.2.x/1.2.4.md)。本计划为 1.2.x 稳定线补丁：**仓库文件夹包支持**——官方仓库目前只能托管归档包（`file` → `archive_path` → `extract_archive`），而 `pkg install <dir>` 的文件夹安装（dev.7）只作用于用户手传目录。本版打通「仓库托管目录包」：按名安装解析出的路径为目录时复用 `install_from_directory`；`index.toml` 增可选 `type = "dir"` 标注（sha256 语义区分——目录包无归档 hash，跳过校验）；归档包零影响。header-only/源码包可免打包、以 git 目录形式托管。**纯增量，公共 API 无破坏性变更**。
>
> **范围边界**：只改 `src/pkg.cpp`（按名安装目录分支）+ `src/repo.cpp`（index `type` 字段解析）+ 测试 + 文档；命令/配置不变，归档包行为不变。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 791 用例 / 3746 断言）。

---

## 1 背景

- 仓库只能托管归档包（`file` → `extract_archive`），无目录分支；`pkg install <dir>` 的文件夹安装只作用于用户手传目录、不经过仓库解析。
- header-only / 源码包想以「目录」形式托管在仓库（免打包、开发中包即拉即用）时，仓库机制不支持。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 按名安装路径支持目录包：repo 解析出的包路径为目录时，走 `install_from_directory`（复用 dev.7） | P0 |
| 2 | `index.toml` 目录包标注：`type = "dir"`（可选字段），sha256 校验语义区分（目录包无归档 hash，跳过） | P0 |
| 3 | 兼容：既有归档包零影响（目录/归档分支互斥） | P0 |
| 4 | 测试：local 仓库目录包端到端 + 归档包回归；文档 + CHANGES.md 1.2.4 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：安装分支（4.1，src/pkg.cpp）

- [x] **1.1 按名安装目录分支**：`search_result.archive_path` 消费入口（统一入口）加 `fs::is_directory(archive_path) → install_from_directory(archive_path, ...)`；归档分支（`extract_archive`）不动、互斥；1846/1908 消费点经 `install(路径)` 自动复用 1388 目录分支

### 阶段二：index 解析（4.2，src/repo.cpp）

- [x] **2.1 `[[packages]].type` 可选字段**：`"dir"` 或省略（省略 = 归档包，向后兼容）；`type = "dir"` 时 sha256 可省略且不参与校验
- [x] **2.2 `find_package_archive` 对 dir 包返回目录路径**（`file_exists` 已兼容目录）；版本/约束/依赖解析零改动；validate/info 天然兼容

### 阶段三：测试（4.3）

- [x] **3.1 集成测试**：local 仓库 index 含 dir 包 → `pkg install <name>` 成功（目录分支触发 + 源码编译归档 + 无 sha256 不报错）；file 指向缺失目录 → repo add 友好报错
- [x] **3.2 全量回归**：`bash build.sh test-all` 零失败（793/3755，基线 791/3746）

### 阶段四：文档（4.4）

- [x] **4.1 repo.md**（目录包格式 / 校验语义 / 示例，zh/en）+ CHANGES.md 1.2.4 条目

### 阶段五：收口（4.5）

- [x] **5.1 plan.md 全勾选** + 设计文档勾选 + `plans/1.2.x/README.md` 状态更新 + 发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 复用 `install_from_directory` | dev.7 的目录结构校验 + 编译/安装逻辑不变，仅把入口从「用户手传目录」扩展到「repo 解析出的目录」 |
| `type = "dir"` 可选标注 | 省略 = 归档包（向后兼容）；dir 包 sha256 省略且跳过校验 |
| `is_directory` 兜底 | `type` 省略但 `file` 指向目录 → 自动走目录分支 |
| 归档/目录互斥 | `extract_archive` 不动，零影响 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 按名安装加目录分支 | 归档包零影响 | `is_directory` 与归档互斥；既有索引不变 |
| `[[packages]].type = "dir"` 可选字段 | 纯新增 | 省略 = 归档包（向后兼容） |
| dir 包 sha256 跳过 | 仅 dir 包 | 归档包 sha256 校验不变 |
| 公共 API | 无破坏性变更 | index 字段纯增量；命令/配置不变 |

## 6 延后项（明确收口）

- **目录包内容哈希/增量同步语义**（非归档 sha256）：归 2.0.0 或后续评估；本版以 `is_directory` + 可选 `type` 标注的最小实现为准。
- **官方仓库是否切换 header-only 包为目录形式**：由仓库维护决定，本版只提供能力。
