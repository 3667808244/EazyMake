---
name: ezmk-workflow
description: EazyMake 开发发布工作流 —— dev / pre / 正式发布三阶段的执行步骤、提交约定、发布门槛与分发渠道。
---

# EazyMake Workflow（开发发布工作流）

版本从开发到发布的完整流程，分三个阶段：dev（功能开发）、pre（发布前收口）、正式版发布。提交约定统一用 `type(scope): 描述` 格式。

## 1 dev 阶段开发

> 目标：把当前版本设计文档拆成可勾选的阶段，逐阶段落地并自检，全部完成后收口、全量回归、更新文档。对应 `1.x.0-dev.N` 子版本。

1. **确认当前版本**：读 `plans/README.md` 的「当前执行」节，确定当前推进的系列/子版本（如 1.2.0 → dev.N）。
2. **生成执行计划**：根据该版本设计文档 `plans/<series>/<version>.md`，在仓库根目录编写 `plan.md` —— 头部链接回设计文档，把设计文档的「执行步骤」转成带 `[ ]` 勾选框的阶段清单，标注每个阶段的优先级与范围边界。
3. **提交计划文件**：`git commit -m "docs(<version>): 新增 dev.N 执行计划"`。
4. **循环执行阶段**，直到计划所有阶段完成（可并行开发的阶段可多 Agent 并行开发）：
    1. **选定一个阶段**，先加载对应 skill：改 `src/`/`build.sh` → `ezmk-build`；跑/写测试 → `ezmk-test`；改架构/配置/CLI → `ezmk-codebase`；改翻译 → `ezmk-i18n`。
    2. **按内容类型执行并自检**：
        - **代码变更**：
            1. `bash build.sh` 编译通过（生成 locale/logo/version 头后编译全部源文件）
            2. `bash build.sh test` 跑快速测试（跳过 `[integration]`）
            3. 测试未通过则修正后重跑，直到通过才算完成该阶段
        - **翻译变更（i18n）**：三处同步 —— `include/ezmk/i18n_keys.def`（X-macro 键）+ `locale/en.json` + `locale/zh.json`，改完 `bash build.sh` 重建
        - **文档变更**：
            1. 检查文档间相互引用（链接、命令示例、路径、版本号）是否一致
            2. 检查内容是否有错谬（与实现对齐）
    3. **提交该阶段**：`git commit -m "chore: 阶段N完成 — <简述>"`（约定：每个阶段一提交，不把无关阶段合并到一个 commit）
5. **全量回归**：`bash build.sh test-all` 跑单元 + 集成测试，要求零回归（对比基线用例/断言数）。
6. **更新计划文档**：`plan.md` 勾选 `[x]`；`plans/README.md` 把该版本从「当前执行」移到「已完成」；必要时更新系列 README 与依赖图。
7. **更新 changelog**：`CHANGES.md` 新增该版本条目（新增 / 行为变更 / 文档 / 已知限制）。
8. **核对发布门槛**（⛔ 见 `plans/1.1.x/1.1.0-pre.3.md`）：① 计划清单全部完成或明确收口 ② 公共 API 无破坏性变更 ③ 全量测试零回归 —— 门槛未满足即停止，禁止带着未收口项进入下一子版本。
9. **提交并推送**：文档齐全后 `git push`。

## 2 pre 阶段开发

> 目标：dev 功能全部落地后的**发布前收口** —— 用户触达打磨、全量文档检查、缺陷收集与未实现项补全、发布流水线（如 pacman 分发）。对应 `1.x.0-pre.N` 子版本。

1. **确认 dev 收口**：所有 dev.N 阶段完成（`plan.md` 全勾选、无悬空项、全量回归零回归），否则退回 dev 阶段。
2. **用户触达打磨**：顶层别名、`--help` 重组、README 精简；在 `CHANGES.md` 落 API 稳定性承诺。
3. **全量文档检查**：
    1. 逐份核对 `docs/en|zh/`、`README.md`、tutorial、skill 与实现是否一致
    2. 检查命令示例、路径、版本号、配置节引用是否可复现
    3. i18n 三向一致性：`scripts/check_i18n.py`（`i18n_keys.def` + 两份 JSON）
4. **缺陷收集与未实现项补全**：
    1. 系统性排查代码/文档缺陷，汇总成带优先级/状态的清单
    2. 修复测试系统、工具、文档缺陷；补 CI 测试工作流
    3. 需真实 Release 验证的发布流水线项，明确收口到正式版
5. **发布流水线**（如 pacman 分发）：
    1. 编写 `publish/arch/PKGBUILD`（`pkgname=eazymake`，安装 `ezmk` 到 `/usr/bin/` + `_ezmk` 补全到 `zsh/site-functions`）
    2. 本机 MSYS2 + 远程 Arch Linux `makepkg` 生成并验证产物（`makepkg -si` 后 `ezmk version`）
    3. AUR 账户未开通 → 不提交 AUR，以「仓库内 `publish/arch/PKGBUILD` 自取 + `makepkg -si`」为主，AUR 延后
6. **全量回归**：`bash build.sh test-all` 零回归；CI 在 push 上绿色。
7. **发布门槛预核对**（⛔ 见 `plans/1.1.x/1.1.0-pre.3.md`）：实现清单全部完成/收口 + API 兼容 + 零回归。
8. **更新计划文档与 changelog**：`plan.md` 勾选、`plans/README.md` 状态、`CHANGES.md` pre 条目。
9. **提交并推送**。

## 3 正式版发布

> 目标：pre 各类型（用户触达 / 文档检查 / 缺陷收集 / 发布流水线）收口后，聚合开发阶段全部交付，走发布清单 → 打 tag 触发 Release → 分发渠道 → 收尾。对应 `1.x.0` 正式版，发布计划见 `plans/<series>/<version>.md`。

### 3.1 冻结与回归

1. 合并所有收尾提交，`git status` 干净
2. 本地全量回归：`bash build.sh test-all` 零回归
3. 确认 CI 工作流在最后一次 push 上绿色

### 3.2 版本与文档

4. `CHANGES.md` 新增 `1.x.0` 条目（汇总开发阶段交付 + pre 收口）
5. 版本号置为正式版：`include/ezmk/version.hpp` / `build.sh` 的 `EZMK_VERSION` fallback
6. `plans/README.md` 更新：该版本移入「已完成」，路线图推进

### 3.3 打 tag 触发 Release

7. `git tag v1.x.0` + `git push origin v1.x.0`
8. 创建 GitHub Release → `release.yml` 触发多平台构建/打包（windows-x64 / linux-x64 / macos-x64 / macos-arm64）
9. 人工核对产物：`res/ezmk.zsh` 拷贝为 `_ezmk`、二进制可运行（`./ezmk version`）

### 3.4 分发渠道（加载 `ezmk-publish` skill）

10. **Homebrew**：从 Release 资产 digest（`gh api .../releases/tags/<tag>`）取真实 sha256，填 `homebrew-eazymake/ezmk.rb`（双处同步），`brew install` 冒烟
11. **winget**：填 split manifest（version/installer/defaultLocale），提交 `microsoft/winget-pkgs` PR（审批为发布后跟进项，不阻塞发布）
12. **pacman**：`publish/arch/PKGBUILD` 自取 + `makepkg -si`（本机 MSYS2 + 远程 Arch Linux 验证；AUR 延后）
13. **install.sh / install.ps1** 真机回归（含 zsh 补全）

### 3.5 收尾

14. `CHANGES.md` / `plans/README.md` 最终更新
15. 发布门槛最终核对（实现完整 + API 兼容 + 零回归）
16. 任一不满足 → 回退 tag 并修复，禁止带病发布
