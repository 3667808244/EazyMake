# EazyMake 1.3.2 执行计划

> **状态：已完成**。1.3.x 系列路线图见 [`plans/1.3.x/README.md`](plans/1.3.x/README.md)。
>
> 详细设计：[**1.3.2.md**](plans/1.3.x/1.3.2.md)。本计划为 1.3.0 发布后的**补丁版本**：`ezmk test --report <fmt>[:<path>]` 产**机器可读测试报告**（JUnit XML 写文件，交给已有仪表盘/CI 渲染）——non-goals「原生单元测试仪表盘」条款（形态 A/B 拒绝）的**形态 C 替代方案**。Catch2 路径透传 vendor 自带 reporter（`-r <fmt>::out=<file>`），EZMK 内置框架配**最小 JUnit 发射器**，两路对称。
>
> **范围边界**：本版只做**发射**（报告写文件），不做 UI/历史/图表（形态 B）；不加 `[test]` 配置字段（CI 命令里写 flag 即可）；失败门禁不变（报告是附加产物，`ezmk test` 退出码语义不动）。**公共 API 无破坏性变更**（纯新增 flag）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 876 用例 / 5091 断言，1.3.1 后实测；原基线 863/5007）。

---

## 1 背景

- `ezmk test`（`src/build.cpp` `run_tests()` `:1813-2315`）只输出控制台摘要文本，CI 无法直接消费；绕过 ezmk 直接跑测试二进制会脱节（不自动构建、不注入依赖包 include/宏/`-stdlib`）。
- 1.3.0-dev.5 确立"消费命令总是先增量构建"——报告出口应挂在 `ezmk test` 之后。
- 两框架不对称：**Catch2**（vendor v3）自带 `-r junit/xml/json` reporter；**EZMK 内置框架**（`build.cpp:2201-2315`）无 reporter 概念——报告出口必须两路对称。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `ezmk test --report <fmt>[:<path>]`：JUnit XML 写文件；缺省路径 `<proj_root>/.ezmk/test-results/junit.xml` | P0 |
| 2 | Catch2 路径：`test_cmd` 追加 `-r <fmt>::out=<file>`；控制台 reporter 保持默认 → 现有摘要解析零回归 | P0 |
| 3 | EZMK 框架路径：最小 JUnit XML 发射器（逐文件 PASS/FAIL/TIMEOUT/编译失败/链接失败 → `<testsuite>`/`<testcase>`/`<failure>`/`<error>`） | P0 |
| 4 | 报告写文件不碰 stdout（坑 1）；XML 全量转义（坑 2）；路径 shell 转义（坑 4）；输出截断（坑 5） | P0 |
| 5 | `ezmk workspace test` 透传 `--report`：每成员写自己的报告文件 | P1 |
| 6 | 测试：两框架端到端 + 文件内容断言（PASS/FAIL/TIMEOUT → XML）；全量零回归 | P0 |
| 7 | 文档：cli.md / README / CHANGES.md 1.3.2 条目；non-goals「仪表盘」条款的替代方案引用 | P1 |
| 8 | 明确不做：历史/耗时对比/抖动分析/HTML 渲染（形态 B）；`[test]` 配置字段归后续 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：CLI

- [x] **1.1 `CliArgs` 加 `test_report`**（`include/ezmk/cli.hpp`）+ `--report` 解析（`src/cli.cpp` test spec）：`<fmt>[:<path>]`，fmt 非空校验；新增 i18n key `cli_err_invalid_report`
- [x] **1.2 workspace 侧**（P1 前置）：`WorkspaceOptions.test_report` + `workspace_cmd_spec()` 加 `--report` + `parse_workspace_opts` 读取；build/clean/list 下拒绝（新 key）

### 阶段二：Catch2 路径

- [x] **2.1 `run_tests` 加 `test_report` 参数**（`build.hpp` + `main.cpp` 调用点）；报告 spec 解析（首个 `:` 分割 fmt/path；缺省路径 `proj_root/.ezmk/test-results/junit.xml`；相对路径按 proj_root 解析）
- [x] **2.2 `test_cmd` 追加 `-r <fmt>::out=<file>`**（路径 `escape_shell_arg`）；控制台 reporter 保持默认
- [x] **2.3 回归测试**：stdout 摘要不变 + 报告文件生成 + 失败时含 `<failure>` + `--filter` 组合

### 阶段三：EZMK 路径

- [x] **3.1 逐文件结果收集**：EZMK 循环内记录 fname/elapsed/状态（PASS/FAIL/TIMEOUT/编译失败/链接失败）+ stdout/stderr 摘要
- [x] **3.2 最小 JUnit 发射器**：`<testsuites>` → 每文件 `<testsuite>` → `<testcase>`；失败/超时 `<failure>`、编译/链接失败 `<error>`；**XML 全量转义**（`& < > " '`）+ stdout/stderr 截断（4KB/条）+ temp→rename 原子写
- [x] **3.3 格式门禁**：EZMK 下非 `junit` 格式 → 显式报错提示用 Catch2 框架（新 i18n key）
- [x] **3.4 单测**：PASS/FAIL/TIMEOUT 三态 → XML 内容断言；转义/截断断言

### 阶段四：workspace 透传（P1）

- [x] **4.1 `run_member` 透传**（`workspace_build.cpp`）：action == "test" 时成员子命令追加 `--report <value>`，每成员写自己的 `.ezmk/test-results/junit.xml`
- [x] **4.2 集成测试**：双成员各写各的报告文件；失败汇总语义不变

### 阶段五：文档 + 收口

- [x] **5.1 文档**：`docs/en|zh/cli.md` test 命令 `--report` 节；README 命令表；`CHANGES.md` 1.3.2 条目；non-goals「仪表盘」条款核对（替代方案 = 本版）
- [x] **5.2 全量零回归**（基线 876/5091）
- [x] **5.3 文档收口**：plan.md 勾选；`plans/1.3.x/README.md` 状态更新
- [x] **5.4 发布门槛复核**：API 无破坏性变更 + 计划清单收口（历史/图表/`[test]` 字段明确延后）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 只做发射（形态 C） | 报告写文件交给已有仪表盘/CI 渲染；UI/历史/图表（形态 A/B）是 non-goals 拒绝项 |
| Catch2 透传 vendor reporter | `-r <fmt>::out=<file>` 零成本；控制台 reporter 保持默认，摘要解析零回归 |
| EZMK 最小发射器 | 循环内已有全部数据，汇总成 JUnit XML；`<error>` 区分编译/链接失败 |
| 缺省路径项目级 | `<proj_root>/.ezmk/test-results/junit.xml`，可 gitignore，不污染 `build/` |
| XML 全量转义 | `& < > " '`，含 stdout/stderr 截断内容（坑 2/5） |
| 原子写 | temp → `util::atomic_rename`（对齐 cache/record.json 惯例） |
| 格式门禁 | EZMK 仅 `junit`；`json` 等显式提示用 Catch2 框架（坑 3，避免"换框架丢格式"陷阱） |
| 失败门禁不变 | 报告是附加产物，`ezmk test` 退出码语义不动 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `ezmk test --report` | 纯新增 flag | 无参数时行为完全不变 |
| `.ezmk/test-results/` 新目录 | 纯新增 | 项目级、可 gitignore；不触碰 `build/` |
| Catch2 路径追加 `-r` | 仅 `--report` 时生效 | 控制台 reporter 保持默认，摘要解析零回归 |
| `run_tests` 签名加参 | 内部 API | `main.cpp` 调用点同步；无外部调用者 |
| workspace 成员子命令追加 flag | 仅 test + `--report` 时 | 汇总/退出码语义不变 |
| 公共 API | 无破坏性变更 | 纯增量 |

## 6 延后项（明确收口）

- **历史 / 耗时对比 / 抖动分析 / HTML 渲染**：形态 B，non-goals 拒绝；外部工具。
- **`[test]` 配置字段**（声明式报告路径）：归 1.4.0 或后续评估。
- **EZMK 侧 JSON 格式**：归 1.4.0 或后续评估。
- **2.0.0**：保持破坏性变更窗口（`[test].flags` / `ezmk utils cc` 移除等），与本版解耦。
