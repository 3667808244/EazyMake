# EazyMake 1.2.0-dev.7 执行计划

> **状态：已完成**（2026-08-15，全量测试 695 用例 / 3234 断言零失败）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.7.md`](plans/1.2.0/1.2.0-dev.7.md)。本计划为 1.2.0 系列第七个开发子版本：**本地包源 + 项目向上查找**——聚合两个相互独立的改进：① `ezmk pkg install <dir>` 从文件夹（源目录）直接安装包；② `ezmk.toml` 自 CWD 向上查找（最多 5 层父目录）。
>
> **范围边界**：仅新增 `pkg install` 目录入参形态 + 内部项目根定位；不新增 CLI flag / 配置字段；找不到 `ezmk.toml` 时回退 CWD（行为不变）。不触碰 dev.1~dev.6 的命令构造/导出/导入逻辑。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯新增入参形态 + 内部定位）；③ 全量测试零回归（Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

1. **从文件夹安装包缺口**：`ezmk pkg install` 只接受归档文件（`.zip`/`.tar.gz`）、URL、或已注册仓库的包名。开发/调试本地包时，必须先打包成归档才能安装，无法直接从源目录（`include/` + `src/` + `ezmk.toml`）安装。
2. **项目定位缺口**：`parse_config("ezmk.toml")` 仅在当前工作目录查找。进入项目子目录后运行 `ezmk build`/`ezmk test` 等会因找不到 `ezmk.toml` 而失败，无法像 git 一样"向上查找"。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `ezmk pkg install <dir>`：参数为已存在目录时直接安装（复用 `validate_package_dir()` + 与归档同一套后处理） | P0 |
| 2 | 目录安装跳过 SHA-256（无归档）并显式提示；全局安装确认保留 | P1 |
| 3 | `util::locate_project_root()`：自 CWD 向上查找含 `ezmk.toml` 的目录，最多 5 层；找到返回，否则回退 CWD | P0 |
| 4 | `main.cpp`/`pkg.cpp`/`cache.cpp`/`lockfile.cpp` 统一改用定位根 + 绝对路径 `parse_config` | P0 |
| 5 | 找不到时错误提示清晰（「5 层内未找到 ezmk.toml」）；无配置场景（`project new`）行为不变 | P1 |
| 6 | 单测 + 集成覆盖两条新能力；全量测试零回归 | P0 |
| 7 | i18n 新 key（en/zh）+ 文档（pkg.md / cli.md / README / CHANGES.md） | P1 |

## 3 执行阶段

### 阶段一：`pkg install <dir>` 目录安装

- [x] **1.1 入口分支**（2.1）：`install()` 在「URL → 本地文件 → 仓库名」判断前加 `fs::is_directory(input)` 分支，分流到 `install_from_directory`
- [x] **1.2 后处理抽取**（2.2）：`install_from_directory()` 先 `validate_package_dir(dir)`，再把解压后处理（钩子 → 依赖 → 编译 → 复制）抽为可复用段，目录分支复用（`pkg_root = dir`，不复制不打包）；作用域语义与归档一致
- [x] **1.3 SHA-256/确认**（2.3）：目录安装无归档，`--sha256` 忽略并提示（i18n 新 key）；全局安装确认保留
- [x] **1.4 i18n**（2.4）：`install_from_dir` / `sha256_skipped_dir` 等 key 三向一致（`.def` + en/zh JSON），`scripts/check_i18n.py` 通过；`bash build.sh` 编译通过

### 阶段二：`ezmk.toml` 向上查找

- [x] **2.1 工具函数**（3.1）：`util::locate_project_root(start, max_up=5)`（`src/util.hpp`/`util.cpp`）——start 为第 0 层，最多检查到第 5 层父目录；找到返回目录，否则 `nullopt`
- [x] **2.2 main.cpp 接入**（3.2）：各命令（build/run/test/watch/project/cc 等）统一 `auto root = locate_project_root(cwd).value_or(cwd)`，`parse_config((root/"ezmk.toml").string())`，`proj_root = root`
- [x] **2.3 pkg/cache/lockfile 接入**（3.3）：`fs::current_path()` 改定位根；`pkg_install_dir(Project)` 的 `.ezmk/pkg` 路径随根
- [x] **2.4 错误提示**（3.4）：找不到时提示「5 层内未找到 ezmk.toml」（i18n 新 key）；无配置场景回退 CWD 行为不变

### 阶段三：测试

- [x] **3.1 单测**（2.5 + 3.5）：目录安装成功 / 非法结构拒绝；`locate_project_root` 0/1/5/6 层、无 toml
- [x] **3.2 集成测试**（2.5 + 3.5）：建临时包目录 → `ezmk pkg install <dir>` → `pkg list` 可见；子目录内 `ezmk build` 成功、5 层边界、回退
- [x] **3.3 全量回归**：`bash build.sh test-all` 零回归

### 阶段四：文档收口

- [x] **4.1 文档**（2.6 + 3.6）：`docs/en|zh/pkg.md` 补「从文件夹安装」小节；`docs/en|zh/cli.md` + README 说明向上查找行为；`CHANGES.md` dev.7 条目
- [x] **4.2 收口**：本计划勾选 `[x]`；`plans/1.2.0/README.md` dev.7 状态「待实现 → 已完成」；发布门槛复核

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 不新增 flag / 配置字段 | 目录入参靠 `fs::is_directory` 自动识别；项目定位为纯内部逻辑 |
| 目录与归档同一套后处理 | 抽出可复用段，保证目录安装行为与归档完全一致 |
| SHA-256 跳过 + 提示 | 目录无归档，`--sha256` 忽略并显式提示，不静默 |
| 最多 5 层 + 回退 CWD | 常量 `PROJECT_ROOT_MAX_UP = 5` 防误扫 home/系统根；未找到保持现状行为 |
| `locate_project_root()` 单一入口 | 收敛查找逻辑，避免各命令重复实现 |
| 找到才改变 `proj_root` | 无配置场景（`project new`）不受影响 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `pkg install <dir>` 目录安装 | 纯新增入参形态 | 文件/URL/仓库名路径不变；仅当参数是已存在目录时识别 |
| SHA-256 对目录安装跳过 | 仅目录安装场景 | `--sha256` 忽略 + 提示 |
| `ezmk.toml` 向上查找 | 找到时 `proj_root` 变为父目录 | 未找到回退 CWD，行为不变；无新 flag/配置字段 |
| 新增 i18n key | 纯新增 | `.def` 一行 + 两份 JSON，`check_i18n.py` 校验 |

## 6 延后项

- pacman 分发属发布流水线，已拆至 `plans/1.2.0/1.2.0-pre.1.md`。
- 更智能的项目根定位（如到 home 或 `.git` 根）属 2.0.0 之后增强，不在本版。
- dev.9 的包 `[compile].src_dirs`/`include_dirs` 配置收敛，随 dev.7 的本地目录安装共用路径，另行推进。
