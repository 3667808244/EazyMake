# EazyMake 1.4.0-pre.1 执行计划

> **状态：执行中（计划已建）**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.0-pre.1.md**](plans/1.4.x/1.4.0-pre.1.md)。本计划为 1.4.0 系列第一个 pre-release 检查点，主题为**发布前收口**——用户触达打磨 + 全量文档检查 + 缺陷收集与未实现项补全 + 1.4.0 聚合 changelog + 发布门槛预核对。dev.1 ~ dev.7 全部落地（**988 用例 / 5770 断言**零回归）。
>
> **范围边界**：发布流水线（winget/Homebrew/pacman）不在本版，收口到正式发布阶段（workflow §3）；只规划 pre.1 一个检查点，收口不干净时按 1.2.0 机制追加 pre.2。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 988 用例 / 5770 断言，1.4.0-dev.7 后实测）。

---

## 1 背景

- 与 1.3.0-pre.1 不同：1.4.0 各 dev 实现时**同步写了文档**，不存在大规模文档缺口——pre.1 的重心是**核对与收口**。
- 未做的收口项：用户触达打磨（别名总表 / `--help` / README 速览）；`CHANGES.md` API 稳定性承诺仅补了 dev.5/dev.7 两条；跨文档一致性未做整仓核对；dev 已知限制/跟进项散落未聚合裁定；无 1.4.0 聚合 changelog；发布门槛未预核对。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 用户触达打磨：别名总表集中核对、`--help` 输出复核、README 命令速览核对 | P0 |
| 2 | `CHANGES.md` API 稳定性承诺扩展（1.4.0 全部新增 API） | P0 |
| 3 | 全量文档检查：docs/en\|zh、README、tutorial、skill 与实现逐份核对；i18n 三向一致 | P0 |
| 4 | 缺陷收集与未实现项补全：dev 已知限制 + dev.6 P2 + 1.3.6 遗留聚合裁定表 | P0 |
| 5 | `CHANGES.md` 1.4.0 聚合条目 | P0 |
| 6 | 全量回归 `bash build.sh test-all` 零失败；CI push 绿色 | P0 |
| 7 | 发布门槛预核对（⛔ 三条件） | P0 |

## 3 执行阶段

### 阶段一：全量文档核对（对应设计 §3.3）

- [ ] **1.1** 命令示例可复现：docs/zh\|en/cli.md、README.md\|README_ZH.md、tutorial/** 与 `--help` 输出、顶层别名表一致
- [ ] **1.2** 路径/文件名：`.vscode/` 三件套、`ezmk-workspace.toml`、`locale/*.json`、docs 互链锚点有效
- [ ] **1.3** 版本号：文档中版本引用（1.3.x → 1.4.0 语境）与 CHANGES.md 一致
- [ ] **1.4** 配置节引用：`[pkg] strict_std_check`、区间语法 + 编译协商、`[depends] workspace` 缺省值与实现一致
- [ ] **1.5** i18n 三向一致：`python scripts/check_i18n.py` 通过
- [ ] **1.6** skill 文档（`.claude/skills/*`）与实现一致（workspace watch/scan、`project export vscode`）

### 阶段二：用户触达打磨（对应设计 §3.1）

- [ ] **2.1** 顶层别名总表核对：`kAliases` 全表 vs 文档简写表 vs `--help` 三处逐行比对（`ww`/`ws` 确认已入三处）
- [ ] **2.2** `--help` 输出复核：`ezmk --help` / `workspace` / `project` / `pkg` 与 cli.md 命令表一致（flag / 缺省值 / `-w` 家族）
- [ ] **2.3** README 命令速览核对（zh/en）：`project export vscode`、`workspace watch`、`workspace scan` 均在列

### 阶段三：缺陷裁定与修复（对应设计 §3.4）

- [ ] **3.1** 聚合裁定表逐项最终裁定（修复 / 收口 1.5.x / 收口 2.0.0 / 明确不做），每项落状态
- [ ] **3.2** 修复项（建议清单）：`[compile].language` 超能力配置期校验、跨盘符 cache 键碰撞、`extract_zip` 大小上限、`lua_to_json` 循环表递归、export C 侧 `std_capability_note`、死代码/硬编码英文清理
- [ ] **3.3** 修复项带单元/集成测试；i18n 三向一致；文档「实现为准」复核

### 阶段四：收口

- [ ] **4.1** `CHANGES.md` API 稳定性承诺扩展（§3.2 清单：export vscode / strict_std_check / 编译协商 / CMake 映射 / watch 透传 / workspace watch / tgz 别名 / sha256 边车 / workspace scan）
- [ ] **4.2** `CHANGES.md` 1.4.0 聚合条目（dev.1 ~ dev.7 + pre.1 汇总）
- [ ] **4.3** 全量回归 `bash build.sh test-all` 零失败（基线 988/5770）；CI push 绿色核对
- [ ] **4.4** `plans/1.4.x/README.md` / `plan.md` / `plans/README.md` 状态更新（pre.1 全勾选）
- [ ] **4.5** 发布门槛预核对（⛔：清单完成/收口 + API 无破坏性变更 + 全量零回归）——未满足即停止，禁止进入正式发布

## 4 关键设计决策

| 决策 | 结论 | 理由 |
|------|------|------|
| 检查点数量 | 只规划 pre.1（同 1.3.0） | 各 dev 已同步写文档、收口质量好；收口不干净时按 1.2.0 机制追加 pre.2 |
| 重心 | 核对与收口，非新写文档 | dev 期间文档已同步；pre.1 做整仓一致性核对 + 缺陷裁定 |
| 缺陷去向 | 聚合裁定表（修复 / 1.5.x / 2.0.0 / 不做） | 散落各 dev 的「已知限制/跟进项」一次性定归宿，防止悬空 |
| 发布流水线 | 不在 pre.1 | 需真实 Release 验证，收口到正式发布阶段（workflow §3） |
| CI | 只核对 push 绿色，不扩工作流 | ci.yml / release.yml 已有，dev 阶段每次全量回归通过 |

## 5 兼容性矩阵

| 变更 | 影响 | 说明 |
|------|------|------|
| 文档更新 / 用户触达打磨 | 纯文档 | 只增不改既有语义 |
| CHANGES.md 稳定性承诺扩展 | 声明扩展 | 承诺已稳定实现的能力（全量回归锁定），非新增行为 |
| 缺陷修复（裁定「修复」项） | 行为修正 | 每项带回归测试；不引入破坏性变更 |
| 公共 API | 无破坏性变更 | 若某修复被迫改变公共行为 → 收口延后，不进 1.4.0 |

## 6 延后项（裁定表完整版见设计文档 §3.4）

- **收口 1.5.x**：launch 参数透传 / 多配置调试 / 语义 C / `[test]` 场景协商 / CMake features 全量映射 / `CXX_STANDARD_REQUIRED` 严格化 / workspace watch `--` 透传 / watch profile 热切换 / 测试链接缺包归档 / watcher 风暴 / import `add_library` 误判 / `scan --prune` / build.cpp·pkg.cpp 重构 / Catch2 结构化解析
- **收口 2.0.0 前**：cli.cpp 命令组拆文件
- **明确不做**：`project cc`/`export cmake` 改造、`--resolve`/`--glob`/`-o` 对 vscode 目标、CLI `--strict`、成员依赖自动推断
