# EazyMake 1.2.1 执行计划

> **状态：已发布**（2026-08-18，tag `v1.2.1`，GitHub Release + Homebrew tap + winget PR #419171 + pacman PKGBUILD 已同步）。1.2.x 系列路线图见 [`plans/1.2.x/README.md`](plans/1.2.x/README.md)。
>
> 详细设计：[**1.2.1.md**](plans/1.2.x/1.2.1.md)。本计划为 1.2.0 正式发布后的第一个补丁子版本：**按项目类型差异化模板生成**——`ezmk project new` 的 `static`/`shared` 类型改为生成 `include/<name>.hpp` + `src/<name>.cpp` 库骨架（不再生成无意义的 `main.cpp`），`executable`/`utils` 保持现状；默认配置模板追加注释掉的 `[test]` 示例节。
>
> **范围边界**：只改 `project new` 的模板生成（`src/project.cpp` + `src/config.cpp` 模板 + 测试 + 文档），**公共 API 无破坏性变更**（`create_project()` 签名与 CLI 不变）；`ezmk.toml` 模板仅追加注释 `[test]` 示例节（解析零影响）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（1.2.0 基线 775 用例 / 3554 断言 → 本版 785 / 3629 全绿）。

---

## 1 背景

- `ezmk project new` 对 `executable`/`static`/`shared` 生成**完全相同**的 `src/main.cpp`（Hello world），且从不生成头文件——库项目带着无意义的 `main.cpp`，`--type` 对生成物零影响。
- 需要按类型差异化：库项目生成「公共头文件 + 实现」骨架，符合 `include_dirs = ["include"]` 默认配置的惯例示范。
- 默认配置无 `[test]` 节示范，用户想跑测试需自行查文档手写配置——发现成本高。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `executable`：保持 `src/main.cpp`（Hello world）不变，不生成头文件 | P0 |
| 2 | `static`/`shared`：生成 `include/<name>.hpp` + `src/<name>.cpp` 库骨架，**不生成 main.cpp** | P0 |
| 3 | `utils`：保持现状（无 C++ 代码） | P0 |
| 4 | 项目名净化：头文件名保留原名；namespace `-`/`.`/空格 → `_` | P1 |
| 5 | 测试：集成断言各类型文件集合 + 净化 + 可编译；全量零回归 | P0 |
| 6 | 默认配置模板追加**注释掉的 `[test]` 示例节**（降低发现成本，TOML 解析零影响） | P1 |
| 7 | 文档：cli.md + 教程 02/03 + default_create + CHANGES.md 1.2.1 条目 | P1 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：模板改造（4.1 + 4.2）

- [x] **1.1 分支生成**：`create_project()` 按 `project_type` 分支——executable 原路径；static/shared 生成 `include/<name>.hpp`（`#pragma once` + `namespace <ns>` + `greeting()` 示例）+ `src/<name>.cpp`（实现）；utils 不变
- [x] **1.2 净化 helper**：`sanitize_namespace()`（`-`/`.`/空格 → `_`）+ 单测
- [x] **1.3 默认配置模板**：`write_default_config()` 追加注释掉的 `[test]` 示例节（§3.5 内容，字段与 TestConfig 一致、不含已弃用 flags）

### 阶段二：测试（4.3）

- [x] **2.1 集成测试**：`project new --type static/shared` → hpp+cpp 存在、main.cpp 不存在；`--type executable` → main.cpp 存在、无 .h；`--type utils` → 无 cpp + `utils/` 目录；含 `-` 项目名 → `namespace my_lib`；新库项目（static/shared）`build` 通过
- [x] **2.2 单测**：库骨架文件集合与内容、净化、executable 无头文件、utils 无 C++ 代码、默认模板注释 `[test]` 节存在性（含零解析影响）
- [x] **2.3 全量回归**：`bash build.sh test-all` 零失败（785 / 3629，1 跳过为既有环境限制）

### 阶段三：文档（4.4）

- [x] **3.1 cli.md**：`project new` 模板说明（按类型生成物 + 注释 `[test]` 节）
- [x] **3.2 教程**：02（库模板说明）、03（默认配置展示同步）
- [x] **3.3 default_create.md**：默认模板展示同步（profile + 注释 `[test]` 节 + 库骨架说明）
- [x] **3.4 CHANGES.md**：1.2.1 条目

### 阶段四：收口（4.5）

- [x] **4.1 收口**：plan.md 全勾选 + 设计文档勾选 + `plans/1.2.x/README.md` 状态更新 + 发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| static/shared 同骨架 | 库类型区分在配置层（`ezmk.toml` `type`），模板骨架无需区分 |
| 头文件名保留原名 | 文件系统允许 `-`/`.`；仅 C++ namespace 需净化（标识符限制） |
| `#pragma once` | 规避 guard 宏净化；与仓库头文件惯例一致 |
| 示例 API 用 `const char*` | 最小模板，避免引入 `<string>` 依赖 |
| executable 不生成 .h | 可执行文件无公共头文件需求，保持最小 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| static/shared 新项目无 main.cpp | 仅新建项目；旧项目零变化 | 文档同步说明 |
| 新增 hpp+cpp 生成物 | 纯新增 | 与默认 include_dirs 衔接 |
| 公共 API / CLI | 无变化 | `create_project()` 签名与 `project new` 不变 |

## 6 延后项（明确收口）

- **header-only 项目类型 / C 语言模板 / 更多模板选项**：归 2.0.0 或后续补丁评估，本版只做最小差异化。
