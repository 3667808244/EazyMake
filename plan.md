# EazyMake 1.2.0-dev.5 执行计划

> **状态：已完成**（2026-08-14，全量测试 683 用例 / 3170 断言零失败）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.5.md`](plans/1.2.0/1.2.0-dev.5.md)。本计划为 1.2.0 系列第五个开发子版本：**catch2 v3 测试主程序兼容**——修复 `ezmk test` 在 catch2 v3（官方仓库当前版本 3.6.0）下无法链接的问题。
>
> **范围边界**：`ezmk test` 的 `test_main.cpp` 生成逻辑（`src/build.cpp` `run_tests`）——v3 多头路径改显式 `main` + `Catch::Session().run()`；v2 vendor 单头路径保留 `CATCH_CONFIG_MAIN`。纯内部生成逻辑，不新增命令/flag、不触碰公共 API、无新 i18n key。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯内部生成逻辑）；③ 全量测试零回归（Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

官方仓库（`ezmk-repo`，github/gitee 镜像）的 **catch2 3.6.0 是 v3**，与 v2 的测试主程序机制不同：

- **catch2 v2**：`#define CATCH_CONFIG_MAIN` 后包含 `catch.hpp`，头文件会生成 `main`。
- **catch2 v3**：**已移除 `CATCH_CONFIG_MAIN`**，`main` 由库的 `catch_main.cpp` 提供或必须由消费方显式定义。

`ezmk test`（`src/build.cpp` `run_tests`）生成的 `test_main.cpp` 固定为 `#define CATCH_CONFIG_MAIN` + `#include <catch2/catch_all.hpp>`，在 v3 下该文件**不产生任何 `main`**，测试链接无入口点（本机表现为 mingw 报 `undefined reference to WinMain`）。这是 `ezmk test` + 官方 catch2 包链路断掉的根本原因。

> **前置**：catch2 3.6.0 包内容已在 `ezmk-repo` 修复（`3b74cc1`：107 个实现 .cpp 平铺进 `src/`，`libcatch2.a` 含 106 成员 / 24939 实现符号）。本计划只修 EazyMake 侧 CLI 的测试主程序生成。

---

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | v3 多头路径生成 **v3 兼容 main**：`#include <catch2/catch_session.hpp>` + 显式 `int main` 调 `Catch::Session().run(argc, argv)`（不再依赖 v2 的 `CATCH_CONFIG_MAIN`） | P0 |
| 2 | v2 vendor 单头路径（`include/vendor/catch2.hpp`）**保持原逻辑**（`CATCH_CONFIG_MAIN`），不回归 | P0 |
| 3 | 集成测试：建项目 + `[depends] lib=["catch2"]` + test 源 → `ezmk test` 端到端跑通（链接 + 运行 + 断言） | P0 |
| 4 | 全量测试零回归 | P0 |

---

## 3 执行阶段

### 阶段一：test_main 生成改造

- [x] **1.1 前置确认**（4.0）：官方仓库 catch2 v3 包已修复（`ezmk-repo` `3b74cc1`）；gitee 镜像同步后本地 `official` 可 `repo update` 获取；或先用 `local-test`（本机 checkout）验证
- [x] **1.2 test_main 生成改造**（4.1）：`src/build.cpp` `run_tests` v3 分支 → `#include <catch2/catch_session.hpp>` + 显式 `int main` 调 `Catch::Session().run(argc, argv)`；v2 分支保留 `CATCH_CONFIG_MAIN`
- [x] **1.3 本地验证**（4.2）：建项目 + `[depends] lib=["catch2"]` + test 源 → `ezmk test` 链接/运行/断言通过（用修复后的 catch2 包）

### 阶段二：测试

- [x] **2.1 集成测试**（4.3）：`test/test_integration.cpp` 新增用例（依赖已装 catch2；离线则 SKIP）
- [x] **2.2 全量回归**（4.4）：`bash build.sh test-all` 零回归（683 用例 / 3170 断言）

### 阶段三：文档收口

- [x] **3.1 文档**（4.5）：`CHANGES.md` 1.2.0-dev.5 条目（口径：`ezmk test` catch2 v3 兼容）
- [x] **3.2 收口**（4.6）：`plans/1.2.0/README.md` 状态更新（dev.5 完成）；esvm 侧回退 vendor + `[depends]` 验证由 esvm 项目执行

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **v3 显式 `main` + `Catch::Session().run()`** | v3 已移除 `CATCH_CONFIG_MAIN`；改用 v2/v3 均有的稳定 API `Catch::Session().run(argc, argv)`，未来 catch2 升级无需再改（已本地验证：链接 `libcatch2.a` 成功，运行输出 `All tests passed`） |
| **v2 vendor 单头保留 `CATCH_CONFIG_MAIN`** | `include/vendor/catch2.hpp` 仍为 v2，`CATCH_CONFIG_MAIN` 有效，行为不变防回归 |
| **项目 `main.cpp` 排除不变** | `run_tests` 收集 `project_objs` 时已排除 `main.o`/`main.obj`（line 1651-1654），测试链接只有一个入口，不与项目 main 重复定义 |
| **`has_user_main` 不变** | 用户测试源自带 `int main(` / `CATCH_CONFIG_MAIN` 时跳过 test_main 生成（line 1417-1426），行为不变 |
| **纯内部生成逻辑** | 无新增 i18n key、无公共 API/CLI 变更 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| v3 多头 test_main 生成改显式 main | 修复前无 main 链接失败；修复后正常 | 显式 `main` + `Catch::Session().run()`，稳定 API |
| v2 vendor 单头路径 | 无 | 保留 `CATCH_CONFIG_MAIN`，行为不变 |
| 用户测试源自带 main（`has_user_main`） | 无 | 跳过 test_main 生成，不变 |
| 项目 `src/main.cpp` | 无 | 已在 `project_objs` 排除 `main.o`，不变 |
| 公共 API / CLI | 无 | 纯内部生成逻辑，无接口变更 |

---

## 6 延后项

- **esvm 验证**：dev.5 落地后，esvm 回退 vendor catch2、改回 `[depends] lib=["catch2^3.6.0"]`，验证整条 `ezmk test` 链路（由 esvm 项目执行，不在本计划内）。
- **dev.6 构建耗时统计 / dev.7 本地包源+向上查找 / dev.8 钩子运行时**：1.2.0 系列后续子版本，独立并行。
- **catch2 包修复后的 SHA-256 校验**：官方仓库 catch2 包修复前 SHA-256 不一致（本地旧缓存 `47e5e6a0` vs 远程 `25420401`）已在换 gitee 镜像后一致，不再有强制 `--sha256` 需要（顺带确认项）。
