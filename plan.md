# EazyMake 1.5.0 正式版发布计划

> **状态：📝 待执行（2026-11-15）** —— 详细设计见 [plans/1.5.x/1.5.0.md](plans/1.5.x/1.5.0.md)；系列索引见 [plans/1.5.x/README.md](plans/1.5.x/README.md)。聚合 dev.1~dev.12 + pre.1 + pre.2，按 workflow §3 执行冻结 → 版本/文档 → tag/Release → 三渠道分发 → 收尾。
>
> **回归基线**：全量 test-all（EZMK_TEST_OFFLINE=1）**1243 / 7928 / 8 跳过 / 0 失败**；CI run **43702080867** 全绿。发布门槛：≥ 1243/7928、CI 绿、公共 API 无破坏性变更、发布清单逐项核对。
>
> **范围**：正式版聚合与发布。**不新增功能、不改公共 API**；已知限制（macOS kqueue、MSVC shared 导出宏、A-23、C-06/C-07/C-12/D-06）按设计文档 §6 登记。

## 1 背景

1.5.0 在 1.4.11 之上新增用户级/全局级 config.toml 配置层、agent skills、输出策略、工具链/缓存/网络/安装前缀配置、首次运行模板，并完成 55 项审核修复。dev.1~dev.12 与 pre.1/pre.2 已全部收口；本计划负责正式版发布。

## 2 执行阶段

### 阶段一：冻结与回归（P0）

- [ ] 确认工作区干净、最后一次 push 的 CI 绿（run 43702080867）
- [ ] 全量 bash build.sh test-all（EZMK_TEST_OFFLINE=1）零失败，≥ **1243 / 7928 / 8 跳过 / 0 失败**

### 阶段二：版本、CHANGES 与索引（P0）

- [ ] build.sh 的 EZMK_VERSION fallback → 1.5.0；构建重新生成 include/ezmk/version.hpp
- [ ] CHANGES.md 新增 1.5.0 聚合条目（新增/行为变更/修复/已知限制）
- [ ] plans/README.md、系列 README、本 plan.md 状态切换
- [ ] 文档中的当前版本引用核对
- [ ] 提交 release: 1.5.0 并 push；CI 绿

### 阶段三：tag + Release（P0）

- [ ] git tag v1.5.0 + git push origin v1.5.0
- [ ] 创建 GitHub Release（published）触发 release.yml
- [ ] 核对 7 个资产（windows zip/exe/sha256 ×2、linux tar.gz、macos-arm64 tar.gz）与 digest
- [ ] 核对 man 放置（Linux/macOS 有、Windows 无）、解包后 ezmk version = 1.5.0
- [ ] macos-x64 skipped 属预期；记录 Release run

### 阶段四：三渠道分发（P0）

- [ ] Homebrew：publish/homebrew/ezmk.rb 1.5.0 + 真实 sha256 + tap 同步 + brew install 冒烟
- [ ] winget：publish/winget/e/ezmk/1.5.0 三件套 + winget validate + PR
- [ ] pacman：publish/arch/PKGBUILD 1.5.0 + 源码 tarball digest + makepkg 验证
- [ ] install.sh / install.ps1 对 v1.5.0 冒烟

### 阶段五：收尾与验收（P0）

- [ ] CHANGES 1.5.0“发布”节回填 tag/Release digest/三渠道 commit
- [ ] plans/README.md 移入已完成、路线图推进
- [ ] 发布后验收（版本/man/zsh/资产/渠道）
- [ ] 明确 2.0.0 为下一破坏性窗口

## 3 发布门槛

1. 全量 test-all 零失败，不低于 1243/7928；
2. CI 在最后一次 push 上绿色；
3. 公共 API 无破坏性变更，行为变更（[project].name 必填等）已在 CHANGES 写明；
4. Release 资产/ digest / man 放置 / 版本号核对通过；
5. 三渠道与 install 脚本验证通过；
6. 任一失败回退 tag/Release 后修复，禁止带病发布。

## 4 备注

- 本版不改公共 API；1.5.x 补丁线承接 C-06/C-07/C-12/D-06，2.0.0 承接 A-23 与破坏性移除。
- 发布完成后更新 CHANGES / plans README / 系列 README 的最终状态与 digest。
