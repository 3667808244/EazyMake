# EazyMake 1.2.1 执行计划

> **状态：执行中**（2026-08-17 启动）。1.2.x 系列路线图见 [`plans/1.2.x/README.md`](plans/1.2.x/README.md)。
>
> 详细设计：[**1.2.1.md**](plans/1.2.x/1.2.1.md)。本计划为 1.2.0 正式发布后的第一个补丁子版本：**按项目类型差异化模板生成**——`ezmk project new` 的 `static`/`shared` 类型改为生成 `include/<name>.hpp` + `src/<name>.cpp` 库骨架（不再生成无意义的 `main.cpp`），`executable`/`utils` 保持现状。
>
> **范围边界**：只改 `project new` 的模板生成（`src/project.cpp` + 测试 + 文档），**公共 API 无破坏性变更**（`create_project()` 签名与 CLI 不变）；`ezmk.toml` 模板不动。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（1.2.0 基线 775 用例 / 3554 断言）。

---

## 1 背景

- `ezmk project new` 对 `executable`/`static`/`shared` 生成**完全相同**的 `src/main.cpp`（Hello world），且从不生成头文件——库项目带着无意义的 `main.cpp`，`--type` 对生成物零影响。
- 需要按类型差异化：库项目生成「公共头文件 + 实现」骨架，符合 `include_dirs = ["include"]` 默认配置的惯例示范。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `executable`：保持 `src/main.cpp`（Hello world）不变，不生成头文件 | P0 |
| 2 | `static`/`shared`：生成 `include/<name>.hpp` + `src/<name>.cpp` 库骨架，**不生成 main.cpp** | P0 |
| 3 | `utils`：保持现状（无 C++ 代码） | P0 |
| 4 | 项目名净化：头文件名保留原名；namespace `-`/`.`/空格 → `_` | P1 |
| 5 | 测试：集成断言各类型文件集合 + 净化 + 可编译；全量零回归 | P0 |
| 6 | 文档：cli.md + CHANGES.md 1.2.1 条目 | P1 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：模板改造（4.1）

- [ ] **1.1 分支生成**：`create_project()` 按 `project_type` 分支——executable 原路径；static/shared 生成 `include/<name>.hpp`（`#pragma once` + `namespace <ns>` + `greeting()` 示例）+ `src/<name>.cpp`（实现）；utils 不变
- [ ] **1.2 净化 helper**：`sanitize_namespace()`（`-`/`.`/空格 → `_`）+ 单测

### 阶段二：测试（4.2）

- [ ] **2.1 集成测试**：`project new --type static/shared` → hpp+cpp 存在、main.cpp 不存在；`--type executable` → main.cpp 存在、无 .h；`--type utils` → 无 cpp + `utils/` 目录；含 `-` 项目名 → `namespace my_lib`；新库项目 `build` 通过
- [ ] **2.2 全量回归**：`bash build.sh test-all` 零失败（基线 775/3554）

### 阶段三：文档（4.3）

- [ ] **3.1 cli.md**：`project new` 模板说明（按类型生成物）
- [ ] **3.2 CHANGES.md**：1.2.1 条目

### 阶段四：收口（4.4）

- [ ] **4.1 收口**：plan.md 全勾选 + 设计文档勾选 + `plans/1.2.x/README.md` 状态更新 + 发布门槛复核（API 无破坏性变更 + 全量零回归）

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
