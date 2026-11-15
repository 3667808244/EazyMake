# EazyMake 1.5.0-pre.2 执行计划

> **状态：📝 待执行（2026-11-15）** —— 详细设计见 [plans/1.5.x/1.5.0-pre.2.md](plans/1.5.x/1.5.0-pre.2.md)；系列索引见 [plans/1.5.x/README.md](plans/1.5.x/README.md)。dev.12 已收口（1242/7922；CI run `43609114462` 全绿），本计划是 1.5.0 正式发布前的第二个（最后一个）pre 检查点，聚合 dev.8~dev.12。
>
> **回归基线**：dev.12 全量 `test-all`（`EZMK_TEST_OFFLINE=1`）**1242 / 7922 / 8 跳过 / 0 失败**；pre.1 为 1219/6979。本版不得低于 1242/7922。
>
> **范围**：聚合复核、pre.1 延后项重新裁定、文档/用户触达、i18n/skills、CHANGES/API 稳定性、发布流水线预检、全量回归与门槛预核对。**不新增功能、不改公共 API、不升版本号、不打 tag、不更新分发 manifest。**

## 1 背景

dev.8~dev.12 在 pre.1 之后交付了 agent skills、编译器/连接器输出策略、工具链/缓存/网络/安装前缀配置、首次运行模板，以及 55 项审核修复。dev 阶段已全部收口，但 pre.1 的 14 项延后账本尚未回填、dev.8~12 的已知限制未汇总，且已确认两处文档事实漂移（`[project].name` 必填、`network_timeout` 的 git 300 s 兜底）。本计划只做聚合、核对、回填与发布门槛预检，不重复代码审核。

## 2 执行阶段

### 阶段一：聚合复核与延后项裁定（P0）

- [ ] dev.8~dev.12 收口矩阵核对（计划勾选、CHANGES、CI run、全量基线）
- [ ] pre.1 的 14 项延后清单逐项填结论（已解决 / 1.5.x / 2.0.0 / 明确不做）
- [ ] 汇总 dev.8~dev.12 已知限制（macOS kqueue、MSVC shared 导出宏、A-23、C-06/C-07/C-12/D-06）
- [ ] 更新设计文档 §3.1 账本与 CHANGES 已知限制

### 阶段二：文档与用户触达（P0）

- [ ] `[project].name` 必填：`docs/{en,zh}/project_config.md` + 示例/教程核对
- [ ] `network_timeout`：HTTP 0=不超时、git 300 s 兜底、非 TTY `GIT_TERMINAL_PROMPT=0` 写入 en/zh user_config
- [ ] 首次运行模板与 `EZMK_NO_AUTO_CONFIG` 文档核对
- [ ] README / README_ZH 增加 1.5.0 能力速览（agent skills / 输出策略 / 工具链 / 网络 / install prefix）
- [ ] 递归 watch、`--locked`、MSVC shared 导出宏说明复核
- [ ] man / zsh / help 交叉核对；`check_man_sync` + groff 零告警
- [ ] `check_docs_sync` / `check_config_docs` 全绿

### 阶段三：i18n / CHANGES / API（P0）

- [ ] `check_i18n`（427 键）+ all-keys 测试；zh-TW 继承
- [ ] dev.8~12 新增用户文案的中文语义核对
- [ ] CHANGES 新增 1.5.0-pre.2 条目（新增/行为变更/修复/已知限制）
- [ ] API 稳定性核对 + 1.5.0 聚合要点草稿
- [ ] README_ZH 与英文同步

### 阶段四：发布流水线预检与 CI 加固（P0/P1）

- [ ] install.sh / install.ps1 本地 smoke（版本、man、zsh、`-DryRun`）
- [ ] PKGBUILD / Homebrew / winget 仍指向 1.4.11（确认，不发布）
- [ ] release.yml 矩阵/产物清单核对（macos-x64 skip、man 位置、Windows 不含 man）
- [ ] `test/test_integration_helpers.hpp` 的 `run_ezmk` 增加 300 s 超时（dev.12 A-22 余项）+ 集成回归
- [ ] 评估 `ubuntu-latest` 迁移（pin 24.04 或记录）
- [ ] push 后 CI 全绿并记录 run ID

### 阶段五：全量回归与发布门槛预核对（P0）

- [ ] `bash build.sh test-all`（`EZMK_TEST_OFFLINE=1`）零失败，≥ **1242 / 7922 / 8 跳过 / 0 失败**
- [ ] 门槛①：阶段一~六全部完成或延后登记
- [ ] 门槛②：公共 API 无破坏性变更；行为变更（name 必填等）已在 CHANGES 逐条写明
- [ ] 门槛③：全量零回归 + CI 在最后一次 push 上绿色
- [ ] 发布清单预检（版本/日期/man/分发脚本 ready；不含版本号与 tag 变更）

### 阶段六：状态回填与收口（P0）

- [ ] `plan.md` 勾选、dev.12/系列 README/plans README 状态
- [ ] CHANGES pre.2 收口条目
- [ ] 明确 1.5.0 正式版下一步（1.5.0 计划立项、版本号/tag/三渠道分发）
- [ ] 提交并推送

## 3 发布门槛

进入 1.5.0 正式发布前必须同时满足：

1. 本计划各阶段全部勾选，或延后项在设计文档 §3.1/§3.5 明确登记；
2. pre.1 的 14 项延后 + dev.8~dev.12 已知限制全部有去向；
3. P1 零残留，P2 修复或明确收口；
4. 全量 `test-all` 零失败，不低于 1242/7922；
5. CI 在最后一次 push 上绿色；
6. 公共 API 无破坏性变更，行为变更已写入 CHANGES；
7. 发布流水线预检完成，分发 manifest 仍指向 1.4.11（正式发布时升级）。

## 4 备注

- 本版**不修改**版本号、不打 tag、不更新分发渠道；这些留给 1.5.0 正式版计划。
- 正式版聚合计划 `plans/1.5.x/1.5.0.md` 在 pre.2 收口后立项。
- 每个阶段完成后更新本文件勾选并提交；阶段提交信息遵循 `type(scope): 描述`。
