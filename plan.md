# EazyMake 1.3.3 执行计划

> **状态：执行中**。1.3.x 系列路线图见 [`plans/1.3.x/README.md`](plans/1.3.x/README.md)。
>
> 详细设计：[**1.3.3.md**](plans/1.3.x/1.3.3.md)。本计划为 1.3.0 发布后的**补丁版本**：为 1.3.0 的 **`workspace` 命令组**补齐双字母简写——`wl`/`wb`/`wt`/`wc` → `workspace list/build/test/clean`（`kAliases` 表加 4 行，沿用「组首字母 + 子命令首字母」规则，与 p/k/r 一致；无任何 `w*` 键冲突）。
>
> **范围边界**：刻意不做——`completions/_ezmk` 加简写（与 p/k/r 一致的既有设计决定）、`w` 单字母（workspace 有子命令，单字母歧义）、**`example` 组简写（定死边界，不留后续评估）**。`-w` 是 project 命令的参数位 flag（重定向），与命令位置的 `w*` 简写正交。**公共 API 无破坏性变更**（纯增量别名）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 895 用例 / 5185 断言，1.3.2 后实测；原基线 863/5007）。

---

## 1 背景

- 0.2.6 起 `kAliases` 表（`src/cli.cpp:1005-1030`）为 **project / pkg / repo** 提供双字母简写（`pb`/`ki`/`ra`…），单字母 `u`/`h`/`v` 覆盖 utils/help/version；1.3.0 新增 **`workspace` 命令组**却无对应简写——`ezmk workspace build` 只能全拼，或绕道 `ezmk build -w`（需先记住"project 命令 + `-w`"的间接形态）。
- 简写只在**命令位置**生效（`ezmk workspace wb` 仍报未知子命令）；`--verbose` 记录展开（`wb → workspace build`）。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `wl`/`wb`/`wt`/`wc` → `workspace list/build/test/clean`（`kAliases` 加 4 行） | P0 |
| 2 | 无冲突：`kAliases` 现有键无任何 `w*` 占用；与 `pw`（project watch）首字母不同、与 `-w` flag 语义正交 | P0 |
| 3 | 单测（`test/test_cli.cpp`）：展开断言 + 命令位置限定 + `--verbose` 展开记录 + 既有简写回归 | P0 |
| 4 | 文档：cli.md（en/zh）简写说明 + CHANGES.md 1.3.3 条目 | P1 |
| 5 | 刻意不做：`completions/_ezmk` 加简写；`w` 单字母；`example` 组简写（定死边界） | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：别名表

- [ ] **1.1 `kAliases` 加 4 行**（`src/cli.cpp:1016-1027` 区域）：`wl`/`wb`/`wt`/`wc` → `workspace list/build/test/clean`；展开逻辑零改动（`shorthand_expansion`/下游 `Command::Workspace*` 分发自动生效）

### 阶段二：单测

- [ ] **2.1 展开断言**（`test/test_cli.cpp`）：`wl`/`wb`/`wt`/`wc` → `Command::WorkspaceList/Build/Test/Clean`
- [ ] **2.2 命令位置限定**：`ezmk workspace wb` → 未知子命令错误
- [ ] **2.3 `--verbose` 展开记录**：`wb → workspace build`；既有 p/k/r/u/h/v 简写断言回归（无键冲突）

### 阶段三：文档

- [ ] **3.1 cli.md（en/zh）**：命令简写说明补 workspace 4 个（现状只描述 p/k/r 简写）
- [ ] **3.2 CHANGES.md**：1.3.3 条目
- [ ] **3.3 无新 i18n key**（全部复用既有错误消息）——`.def`/JSON/`locale_data.cpp` 零改动

### 阶段四：收口

- [ ] **4.1 全量零回归**（基线 895/5185）
- [ ] **4.2 文档收口**：plan.md 勾选；`plans/1.3.x/README.md` 状态更新
- [ ] **4.3 发布门槛复核**：API 无破坏性变更 + 计划清单收口（`example` 组/`w` 单字母/补全明确不做）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 沿用「组首字母 + 子命令首字母」 | `wl`/`wb`/`wt`/`wc`，与 p/k/r 同款规则 |
| 简写只在命令位置生效 | 展开先行于一切解析；`ezmk workspace wb` 仍报未知子命令 |
| `w` 单字母不引入 | workspace 有 4 个子命令，单字母无法表达（区别于 `u`/`h`/`v` 的"组即命令"模式） |
| `example` 组定死不做 | 低频一次性命令 + `e`/`ex` 形态易与 shell 常见命令混淆；不留后续评估 |
| 补全不加简写 | `completions/_ezmk` 与既有设计决定一致（刻意不补全简写） |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `wl`/`wb`/`wt`/`wc` 新简写 | 纯新增 | `kAliases` 无 `w*` 键，零冲突 |
| `ezmk workspace wb`（组内位置） | 行为不变 | 仍报未知子命令 |
| `-w` flag | 不受影响 | 参数位 flag 与命令位置简写正交 |
| `completions/_ezmk` | 不加 | 与既有简写设计决定一致 |
| 公共 API | 无破坏性变更 | 纯增量 |

## 6 延后项（明确收口）

- **`example` 组简写**：定死边界不做（低频一次性 + 形态易混淆），不留后续评估。
- **`w` 单字母**：不引入（workspace 有子命令，单字母歧义）。
- **`completions/_ezmk` 加简写**：与既有设计决定一致，刻意不补全简写。
- **`-w` 与 `w*` 简写组合的文档示例**：归 1.4.0 或后续评估。
- **2.0.0**：保持破坏性变更窗口，与本版解耦。
