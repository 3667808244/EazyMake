# EazyMake 1.3.6 执行计划

> **状态：已完成**。1.3.x 系列路线图见 [`plans/1.3.x/README.md`](plans/1.3.x/README.md)。
>
> 详细设计：[**1.3.6.md**](plans/1.3.x/1.3.6.md)。本计划为 1.3.0 发布后的**补丁版本**，主题为**代码质量收口**（承接 1.3.5 后质量分析结论）——`-Wall -Wextra` 清零、`parse_language` 错误文案与 `ver_map` 一致、`consumer_std_min` 校验去噪、归档遍历与运行逻辑去重、`run_tests` 机械拆分（~550 行单函数）、测试文件按主题拆分。**纯重构零新功能**：不引入任何新 flag/配置/命令/i18n key（仅文案微调）；每个重构都有既有测试锁定行为。
>
> **范围边界**：**明确不做**——build.cpp/pkg.cpp 全面重构（1.4.0+）、cli.cpp 命令组拆文件（2.0.0 前）、Catch2 结构化解析（1.4.0）、1.3.1~1.3.5 延后功能项。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 908 用例 / 5283 断言，1.3.5 后实测）。

---

## 1 背景

- 1.3.5 收口后质量分析结论：`-Wall -Wextra` 仅 2 处警告、`parse_language` 文案缺 `98/03/26`、`consumer_std_min` 多包刷屏、`create_targz`/`create_zip` 遍历重复、`project run` 与 watch `--run` 运行逻辑重复、`run_tests` ~550 行单函数、`test_integration.cpp` 3752 行。
- 本版把这些**低成本高价值**项落地，每个重构由既有测试锁定，风险可控。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `-Wall -Wextra` 编译警告清零（2 处） | P0 |
| 2 | `parse_language` 错误文案与 `ver_map` 一致 | P0 |
| 3 | `consumer_std_min` 校验去噪（每进程只 warn 一次） | P0 |
| 4 | 归档遍历共享（`collect_stage_entries`，1.3.5 等价性测试锁定） | P0 |
| 5 | 运行逻辑共享（`run_executable`，`project run` 与 watch `--run` 复用） | P0 |
| 6 | `run_tests` 机械拆分（按框架拆 helper，纯搬运） | P1 |
| 7 | 测试文件拆分（helpers 共享头 + 按主题拆，用例/断言数不变） | P1 |
| 8 | 明确不做：全面重构 / cli 拆文件 / 结构化解析 / 延后功能项 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：小修

- [x] **1.1 警告清零**：`detect_catch2` 删未用参数 `depends`（`build.cpp:1750` + `build.hpp` 声明 + 调用点）；`export.cpp:130` 补括号
- [x] **1.2 文案一致**：`parse_language` 错误文案从 `ver_map` 键生成（或补全 98/03/26）
- [x] **1.3 校验去噪**：`consumer_std_min` 消费者 language 非法 → 每进程只 warn 一次（static flag）
- [x] **1.4 单测**：文案含 98/03/26；去噪只 warn 一次

### 阶段二：归档遍历共享

- [x] **2.1 `collect_stage_entries()`**（`util.cpp` + `util.hpp`）：递归遍历 → 相对路径 → `/` 分隔 → 目录尾 `/` → 排序（从 `create_targz`/`create_zip` Step 1 提取）
- [x] **2.2 两归档器复用**：`create_targz`/`create_zip` 消费共享结果（zip 写侧 `safe_zip_name` 校验保留）；1.3.5 等价性集成测试全绿 + `collect_stage_entries` 单测（子目录/空文件/排序）

### 阶段三：运行逻辑共享

- [x] **3.1 `run_executable(exe, args, warn_on_nonzero)`**（`main.cpp` static helper）：`running` key + 命令组装 + `run_command` + 回显；`warn_on_nonzero=false` → 返回退出码（run 现状），`true` → 警告不退出（watch 现状）
- [x] **3.2 `ProjectRun` 与 watch `run_watched_exe` 改调 helper**；`project run` 与 watch 集成测试全绿

### 阶段四：`run_tests` 机械拆分

- [x] **4.1 拆分**：`prepare_test_compile_input` / `run_catch2_tests` / `run_ezmk_tests` 提取（**纯搬运**，不改行为/变量名语义）；`run_tests` 变薄调度
- [x] **4.2 复核**：`git diff -w` 确认纯搬运；全量回归（含报告/过滤/缓存/失败门禁用例）

### 阶段五：测试文件拆分

- [x] **5.1 `test_integration_helpers.hpp`**：提取共享 helpers（`run_ezmk`/`find_*`/`poll_log`/1.3.5 pack helpers 等）
- [x] **5.2 按主题拆 `test_integration_*.cpp`**（core/pkg/workspace/report…）；用例/断言数不变

### 阶段六：文档 + 收口

- [x] **6.1 CHANGES.md**：1.3.6 条目
- [x] **6.2 全量零回归**（基线 908/5283）
- [x] **6.3 文档收口**：plan.md 勾选；`plans/1.3.x/README.md` 状态更新；发布门槛复核（API 无破坏性变更 + 纯重构零回归）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 纯重构零新功能 | 不引入新 flag/配置/命令/i18n key（仅文案微调）；公共 API 无破坏性变更 |
| 机械搬运纪律（run_tests） | 不改变量名/顺序/字符串；`git diff -w` 复核 + 全量回归锁定 |
| 既有测试锁定行为 | 等价性（1.3.5）/集成（watch/run/pack）/全量回归——每个重构都有对应锁定 |
| `detect_catch2` 删参 | 无外部调用者，删参干净（不落 `[[maybe_unused]]`） |
| 文案从 `ver_map` 生成 | 单一数据源，杜绝再次漂移 |
| 去噪 = 进程级一次 | 只改重复 warn 的噪音，不改首次语义 |
| 测试文件拆分纯组织 | 用例/断言数必须不变（可量化的零回归） |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `detect_catch2` 删参 | 内部函数签名 | 同步声明与唯一调用点 |
| 错误文案/警告次数 | 输出文本 | 仅文案/去噪，无语义变化；测试同步 |
| 归档/运行 helper 提取 | 内部重构 | 行为由既有等价性/集成测试锁定 |
| `run_tests` 拆分 | 内部重构 | 纯搬运；全量回归锁定 |
| 测试文件拆分 | 测试组织 | 用例/断言数不变 |
| 公共 API | **无破坏性变更** | 纯内部重构 + 文案修复 |

## 6 延后项（明确收口）

- **build.cpp/pkg.cpp 全面重构**、**Catch2 结构化解析**：归 1.4.0。
- **cli.cpp 命令组拆文件**（`parse_*` 1272 行单文件）：2.0.0 前评估。
- **1.3.1~1.3.5 延后功能项**：`--run` 参数透传 / `workspace watch` / `tgz` 别名 / sha256 边车自动校验——归 1.4.0。
- **2.0.0**：保持破坏性变更窗口，与本版解耦。
