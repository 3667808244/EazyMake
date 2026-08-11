# EazyMake 1.2.0-dev.5 执行计划

> **状态：待执行**（承接 dev.2 已完成，2026-08-11；前置 catch2 包修复已推送 ezmk-repo）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.5.md`](plans/1.2.0/1.2.0-dev.5.md)。本计划为 1.2.0 系列第五个开发子版本：**`ezmk test` catch2 v3 测试主程序兼容**——修复 `run_tests` 生成的 test_main 依赖 v2 的 `CATCH_CONFIG_MAIN`（v3 已移除）导致链接无入口的缺陷。
>
> **范围边界**：只改 `src/build.cpp` `run_tests` 的 test_main 生成（v3 分支改显式 `main` + `Catch::Session().run()`，v2 vendor 分支保留原逻辑）。不新增命令、不弃用、不触碰公共 API。catch2 包内容修复（`ezmk-repo` `3b74cc1`）为前置、已交付。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯内部生成逻辑）；③ 全量测试零回归（Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

官方仓库 catch2 3.6.0 是 **v3**，而 v3 **已移除 `CATCH_CONFIG_MAIN`**（v2 机制），`main` 由库的 `catch_main.cpp` 提供或必须显式定义。`ezmk test`（`run_tests`）生成的 test_main 固定为 `#define CATCH_CONFIG_MAIN` + `#include <catch2/catch_all.hpp>`，在 v3 下**不产生任何 `main`** → 测试链接无入口（mingw 报 `undefined reference to WinMain`）。即使 catch2 包内容已修复，`ezmk test` 链路仍断。

> **前置（已交付）**：catch2 3.6.0 包内容修复（`ezmk-repo` `3b74cc1`：107 实现 .cpp 平铺进 `src/`，`libcatch2.a` 含 106 成员 / 24939 实现符号）。本计划只修 EazyMake 侧 CLI。

---

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | v3 多头路径生成 **v3 兼容 main**：`#include <catch2/catch_session.hpp>` + 显式 `int main` 调 `Catch::Session().run(argc, argv)` | P0 |
| 2 | v2 vendor 单头路径（`include/vendor/catch2.hpp`）**保持原逻辑**（`CATCH_CONFIG_MAIN`），不回归 | P0 |
| 3 | 集成测试：建项目 + `[depends] lib=["catch2"]` + test 源 → `ezmk test` 端到端跑通（链接 + 运行 + 断言） | P0 |
| 4 | 全量测试零回归 | P0 |

---

## 3 执行阶段

### 阶段一：test_main 生成改造

- [x] **1.1 方案验证**：显式 `int main` + `Catch::Session().run(argc, argv)` 链接 `libcatch2.a` 运行通过（`All tests passed`，1 断言）——v3 兼容方案可行
- [ ] **1.2 代码改造**（4.1）：`src/build.cpp` `run_tests` test_main 生成——v3 多头分支 → `#include <catch2/catch_session.hpp>` + 显式 `int main`；v2 vendor 分支保留 `CATCH_CONFIG_MAIN`
- [ ] **1.3 回归确认**（4.1）：`has_user_main` / 项目 `main.o` 排除逻辑不变；`write_needed` 内容比对逻辑适配新内容

### 阶段二：端到端验证

- [ ] **2.1 本地验证**（4.2）：建项目 + `[depends] lib=["catch2"]` + test 源 → `ezmk test` 链接/运行/断言通过（用修复后的 catch2 包，local-test 或 gitee 同步后 official）
- [ ] **2.2 集成测试**（4.3）：`test/test_integration.cpp` 新增用例（依赖已装 catch2；离线 SKIP）
- [ ] **2.3 全量回归**（4.4）：`bash build.sh test-all` 零回归（基线 668 用例 / 3074 断言）

### 阶段三：文档与收口

- [ ] **3.1 文档**（4.5）：`CHANGES.md` 1.2.0-dev.5 条目（口径：`ezmk test` catch2 v3 兼容）
- [ ] **3.2 发布门槛预检**：① 清单全部完成或明确收口；② 公共 API 无破坏性变更（纯内部生成逻辑）；③ 全量测试零回归
- [ ] **3.3 交接推进**：dev.5 收口 → `plans/1.2.0/README.md` 状态更新（dev.5 完成）；esvm 侧回退 vendor + `[depends] lib=["catch2^3.6.0"]` 验证整条 `ezmk test` 链路（由 esvm 项目执行）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **v3 显式 main** | v3 多头路径生成 `#include <catch2/catch_session.hpp>` + `int main(...){ return Catch::Session().run(argc, argv); }`——不再依赖已移除的 `CATCH_CONFIG_MAIN` |
| **v2 路径不回归** | vendor 单头（`include/vendor/catch2.hpp`）保留 `#define CATCH_CONFIG_MAIN`，v2 行为不变 |
| **项目 main 已排除** | `run_tests` 收集 `project_objs` 时跳过 `main.o`/`main.obj`（line 1651-1654），测试链接只有一个入口——无需改动 |
| **`has_user_main` 不变** | 用户测试源自带 `int main(` / `CATCH_CONFIG_MAIN` 则跳过 test_main 生成 |
| **无 i18n 变更** | 纯内部生成逻辑，无新用户可见文案 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| v3 多头 test_main 生成改显式 main | 修复前无 main 链接失败；修复后正常 | 显式 `main` + `Catch::Session().run()`，v2/v3 均有的稳定 API |
| v2 vendor 单头路径 | 无 | 保留 `CATCH_CONFIG_MAIN`，行为不变 |
| 用户测试源自带 main（`user_has_main`） | 无 | 跳过 test_main 生成，不变 |
| 项目 `src/main.cpp` | 无 | 已在 `project_objs` 排除 `main.o`，不变 |
| 公共 API / CLI | 无 | 纯内部生成逻辑，无接口变更 |

---

## 6 延后项

- **esvm 全链路验证**：dev.5 落地后，esvm 回退 vendor catch2、改回 `[depends] lib=["catch2^3.6.0"]` 验证整条 `ezmk test` 链路——由 esvm 项目执行，不在本计划范围。
- **dev.3 默认模板 Profile / dev.4 CMake 导入**：1.2.0 系列后续子版本，独立并行。
- **承接 1.1.3 延后项**（归 1.2.0）：安装钩子权限门控（S1b）、watcher 静默死亡 / kqueue 缺口、宽窄字符路径 / i18n 二次替换、CLI 参数嵌入 NUL 完整防御、OVERLAPPED 池实例化重构——均不在 dev.5 范围。
