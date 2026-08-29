# EazyMake 1.4.0-pre.1 执行计划

> **状态：收口完成（全量零回归 1003/5835）**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.0-pre.1.md**](plans/1.4.x/1.4.0-pre.1.md)。本计划为 1.4.0 系列第一个 pre-release 检查点，主题为**发布前收口**——用户触达打磨 + 全量文档检查 + 缺陷收集与未实现项补全 + 1.4.0 聚合 changelog + 发布门槛预核对。dev.1 ~ dev.7 全部落地（**988 用例 / 5770 断言**零回归）。
>
> **范围边界**：发布流水线（winget/Homebrew/pacman）不在本版，收口到正式发布阶段（workflow §3）；只规划 pre.1 一个检查点，收口不干净时按 1.2.0 机制追加 pre.2。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 988 用例 / 5770 断言，1.4.0-dev.7 后实测 → pre.1 修复后重跑确认零回归）。
>
> **版本决策（F1，--help 审计）**：pre.1 阶段二进制版本号**保持 1.3.6**（1.2.0/1.3.0 系列 pre 阶段先例均不提前 bump，bump 只发生在正式发布 commit）；1.4.0 正式发布时按 workflow §3.2 置正式版本号。

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

- [x] **1.1** 命令示例可复现：docs/zh\|en/cli.md、README.md\|README_ZH.md、tutorial/** 与 `--help` 输出、顶层别名表一致（4 份审计 + `--help` 交叉核对，P0=0）
- [x] **1.2** 路径/文件名：`.vscode/` 三件套、`ezmk-workspace.toml`、`locale/*.json`、docs 互链锚点有效（程序化全量链接校验 0 缺失；3 断锚 + install.sh 用法 + 跨语言链接已修）
- [x] **1.3** 版本号：文档中版本引用（1.3.x → 1.4.0 语境）与 CHANGES.md 一致（17 项：README/tutorial/config_file/CHANGES 已修；46 处 `dev.N+` 标注正式版归一化为 `1.4.0+`，已记录）
- [x] **1.4** 配置节引用：`[pkg] strict_std_check`、区间语法 + 编译协商、`[depends] workspace` 缺省值与实现一致（P0/P1=0，5 项 P2 已修）
- [x] **1.5** i18n 三向一致：`python scripts/check_i18n.py` 通过（396 → 397 键 + zh-TW 变体）
- [x] **1.6** skill 文档（`.claude/skills/*`）与实现一致（12 个 skill 修复 37 项：6 P0 + 12 P1 + 19 P2）

### 阶段二：用户触达打磨（对应设计 §3.1）

- [x] **2.1** 顶层别名总表核对：`kAliases` 全表（35 条）vs 文档简写表 vs `--help` 三处比对——docs/zh、docs/en、README 通过；README_ZH 4 处缺陷（缺 workspace watch / `-w` 注释 / 特性表 / tgz）已修
- [x] **2.2** `--help` 输出复核：与 cli.md 命令表一致（P0=0；help 侧 4 项已修：zh 英文残留 i18n 化、`--verbose` 措辞统一、长行排版空格）
- [x] **2.3** README 命令速览核对（zh/en）：`project export vscode`、`workspace watch`、`workspace scan` 均在列（ZH 补 export vscode / watch）

### 阶段三：缺陷裁定与修复（对应设计 §3.4）

- [x] **3.1** 聚合裁定表逐项最终裁定（27 项：修复 5 / 收口 1.5.x 17 / 收口 2.0.0 前 1 / 明确不做 4，见设计 §3.4 与 plan.md §6）
- [x] **3.2** 修复项：`[compile].language` 超能力配置期警告（C/C++ 序数比较 + i18n 键）、跨盘符 cache 键碰撞（`safe_relative` 5 处替换）、`extract_zip` 解压大小上限（per-entry + 总量 1 GiB）、`lua_to_json` 循环表检测（`lua_topointer` + set）、export C 侧 `std_capability_note`（`max_supported_c_std` + C89/99/11/17 序数）
- [x] **3.3** 修复项带单元/集成测试（+15 用例）；i18n 三向一致（397 键）；文档「实现为准」复核

### 阶段四：收口

- [x] **4.1** `CHANGES.md` API 稳定性承诺扩展（1.4.0-pre.1 全量块：export vscode / strict_std_check / 编译协商 / CMake 映射 / watch 透传 / workspace watch / tgz 别名 / sha256 边车 / workspace scan）
- [x] **4.2** `CHANGES.md` 1.4.0 聚合条目（dev.1 ~ dev.7 + pre.1 汇总，pre.1 草稿；正式发布时定稿）
- [x] **4.3** 全量回归 `bash build.sh test-all` 零失败（**1003 用例 / 5835 断言**，dev.7 基线 988/5770，+15 用例/+65 断言；2 跳过为既有环境限制）；CI 工作流核对（ci.yml 的 ubuntu test-all + windows test + zsh 补全三 job 覆盖本版改动面）
- [x] **4.4** `plans/1.4.x/README.md` / `plan.md` / `plans/README.md` 状态更新（pre.1 全勾选，已提交）
- [x] **4.5** 发布门槛预核对（⛔：① 清单全部完成或明确收口 ✓ ② 公共 API 无破坏性变更 ✓（纯增量 + 修复，无破坏性变更）③ 全量零回归 ✓（1003/5835））——**门槛满足，可进入 1.4.0 正式发布**

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
