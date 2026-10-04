# EazyMake 1.4.8 执行计划

> **状态：✅ 已发布（2026-10-04，tag `v1.4.8`；Release run `37165812937` success）**——本文档把设计文档 §4 转成可勾选的分阶段清单。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)；2.0.0 的移除清单见 [`plans/2.0.x/REMOVALS.md`](plans/2.0.x/REMOVALS.md)。
>
> 详细设计：[**1.4.8.md**](plans/1.4.x/1.4.8.md)。主题：**内嵌依赖与 CI 依赖更新**——miniz 2.2.0 → 3.1.2、Lua 5.4.7 → 5.4.9、Catch2 3.8.0 → 3.16.0，以及 GitHub Actions 主版本（checkout / upload-artifact → v7，action-gh-release → v3）。
>
> **范围边界**：只升依赖与 CI；**零功能新增、零 CLI/配置语义变更、公共 API 无破坏性变更**；不放宽也不移除任何弃用面（归 2.0.0）。
>
> **⛔ 发布门槛**：① 阶段清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试**无新增失败**（本机实测基线 **1133 用例 / 6525 断言 / 9 个 `test_integration_git.cpp` 环境性失败**，见 `build/upstream/BASELINE-NOTES.md`；用例数只增不减）；④ 附加门槛：`python scripts/check_man_sync.py` 通过、`groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警、i18n 三向一致（**412** 键）、`bash scripts/check_docs_sync.sh` 通过；⑤ `ci.yml` 在 push 上全绿。

---

## 1 背景

1.4.7 发布后盘点全仓外部依赖：内嵌库落后上游（miniz 跨两条大版本线、Lua 落后 2 个补丁且 5.4 线已终结、Catch2 落后 8 个 minor），CI action 落后 3 个大版本（Node 20 → Node 24 运行时迁移）。miniz 直接承载不可信输入（`pkg install` 解归档、`project pack --format zip` 产归档），其 3.x 修复正好落在这两条路径上，因此列为 P0。详见设计 §1。

## 2 目标

| 组 | # | 覆盖 | 优先级 |
|----|---|------|--------|
| 基线 | D-00 | vendor 文件清单 + sha256；上游同版本 diff 隔离本地改动 | P0 |
| miniz | D-01 | 4 `.c` + 5 `.h` → **3.1.2**（保留 `miniz_export.h` 静态桩） | P0 |
| Lua | D-02 | 32 `.c` + 27 `.h` → **5.4.9**（重施 `linit.c` 沙箱补丁） | P0 |
| Catch2 | D-03 | 合并头 → **3.16.0**（`catch2.hpp` + `catch2_impl.cpp`） | P1 |
| CI | D-04 | checkout / upload-artifact → **v7**、action-gh-release → **v3** | P1 |
| 文档 | D-05 | `docs/{zh,en}/technical.md` 版本串 + `CHANGES.md` + 计划索引 | P0 |
| 验证 | D-06 | 全量零回归 + zip/tar.gz + Lua 沙箱 + `ezmk test` 解析 + CI 绿 | P0 |

## 3 执行阶段

### 阶段零：基线落库与上游拉取（D-00）

- [x] 记录当前 vendor 文件清单与 sha256（清单落盘 `build/upstream/vendor-baseline.sha256`）
- [x] 拉取上游 miniz `3.1.2`、Lua `5.4.9`、Catch2 `v3.16.0`；另拉 Lua `5.4.7` 原版用于 diff
- [x] 上游逐文件比对（本机 MSYS2 缺 diffutils，用 Python 等价实现）：Lua **仅 `linit.c` 一处本地改动**；miniz 与 2.2.0 内容相同（仅 CRLF）；upstream-only 文件 `lua.c` / `luac.c` / `lua.hpp` / `Makefile`
- [x] 回归：建立本机基线（1133 用例 / 6525 断言 / 9 个 git 集成环境失败，已登记 `build/upstream/BASELINE-NOTES.md`）

### 阶段一：miniz 3.1.2（D-01）

- [x] `src/vendor/miniz.c` / `miniz_tdef.c` / `miniz_tinfl.c` / `miniz_zip.c` 替换
- [x] `include/vendor/miniz.h` / `miniz_common.h` / `miniz_tdef.h` / `miniz_tinfl.h` / `miniz_zip.h` 替换；`miniz_export.h` 保留
- [x] 编译通过（`mz_zip_archive_file_stat` 等结构/签名无变化，一次通过）
- [x] zip 端到端（pack → install → 消费编译）+ tar.gz 端到端 + 非 ASCII / 畸形归档用例（集成套件全通过）
- [x] 回归：全量 1133 用例 / 6525 断言，失败集合与基线完全一致（9 个 git 集成环境失败，无新增）

### 阶段二：Lua 5.4.9（D-02）

- [x] `src/vendor/lua/*.c`（32）替换（不含 `lua.c` / `luac.c`）
- [x] `include/vendor/lua/*.h`（27）替换（含 `ljumptab.h` / `lopnames.h`）
- [x] 重新施加 `src/vendor/lua/linit.c` 的 `io` / `os` 移除补丁（逐字保留原注释）
- [x] `build/ezmk-lua` 冒烟（Lua 5.4 正常执行）+ Lua 钩子 / `ezmk utils` / 沙箱用例（集成套件全通过）
- [x] 回归：全量 1133 用例 / 6525 断言，失败集合与基线完全一致（无新增）

### 阶段三：Catch2 3.16.0（D-03）

- [x] `catch_amalgamated.hpp` → `include/vendor/catch2.hpp`；`catch_amalgamated.cpp` → `src/vendor/catch2_impl.cpp`（保留 `#include "catch2.hpp"` 与 amalgamated `main()`）
- [x] `build/test_ezmk --help` 确认 `--verbosity high` 等旗标仍在
- [x] `ezmk test` 摘要解析（`src/build.cpp:2292-2352` 两条格式）端到端（集成套件通过）
- [x] **默认顺序变更处置**：Catch2 ≥ 3.16 `--order` 默认 `rand`（3.8 为 `decl`）→ `build.sh` 两个测试调用补 `--order decl` 恢复确定性（设计 §3.3 第 5 条）
- [x] 回归：全量 1133 用例 / 6525 断言，失败集合与基线完全一致（9 个 git 集成环境失败，无新增）

### 阶段四：CI action 主版本（D-04）

- [x] `actions/checkout@v4` → `@v7`（`ci.yml` 4 处 + `release.yml` 4 处）
- [x] `actions/upload-artifact@v4` → `@v7`（`ci.yml:165` / `ci.yml:233`）
- [x] `softprops/action-gh-release@v2` → `@v3`（`release.yml` 4 处）
- [x] push 触发 `ci.yml` 全绿 —— run `37166102625` success（Windows / Ubuntu / man pages / zsh completions 全绿，含升级后的 `actions/checkout@v7`；`ci.yml:347` 的 `cp -r man` 断言行未触碰）
- [x] 复核 `macos-13`：官方 2025-09-19 公告该镜像关闭（retired）；该 job 默认 `if: vars.ENABLE_MACOS_X64` 跳过，本版不改行为，登记为延后项

### 阶段五：文档 / 版本串 / 收口（D-05 / D-06）

- [x] `docs/zh/technical.md:13,17` + `docs/en/technical.md:13,17` 三处版本串同步（Lua 5.4.9 / miniz 3.1.2 + 11.3.2 / Catch2 v3.16）
- [x] `CHANGES.md` 新增 1.4.8 条目
- [x] 计划索引状态（`plans/1.4.x/README.md` / `plans/README.md` / 根 `plan.md`）
- [x] 门槛复核：`check_man_sync.py` 通过、groff 4 页零告警、i18n 412 键三向一致、docs-sync（本机缺 MSYS `diff`/`find` → 用 Python 等价校验 en↔zh 文件配对，通过；CI 跑脚本）
- [x] 回归：全量 1133 用例 / 6525 断言，失败集合与基线完全一致（9 个 git 集成环境失败，无新增）

### 发布阶段：1.4.8 正式发布

- [x] 版本号 `1.4.8`（`build.sh:74` 的 `EZMK_VERSION` fallback）
- [x] `git tag v1.4.8` → Release（run `37165812937` success）；7 资产 digest 与 `assets[].digest` 逐一一致，Linux/macOS tar 含 `man/` 4 页、Windows zip 不含，`ezmk.exe.sha256` 与 `ezmk.exe` digest 一致，`ezmk.exe version` → 1.4.8
- [x] 三渠道分发（Homebrew tap `e02f45f`（真实 digest）/ pacman `makepkg -fd` 出包 `ea9c8839…`（包内 version 1.4.8、man 4 页）/ winget PR [#446349](https://github.com/microsoft/winget-pkgs/pull/446349)，其 CI 与版主审批为发布后跟进项）
- [x] Release 侧 action 升级（checkout v7 / action-gh-release v3）在本次发布验证（release run success）

## 4 关键设计决策

- **只换源码、不改行为**：三个内嵌库都是静态内嵌的第三方代码，替换后靠全量回归 + 端到端用例证明无行为漂移；唯一保留的本地改动是既有的 Lua `linit.c` 沙箱补丁。
- **miniz 优先**：它是唯一直接处理不可信输入（外部归档）且跨度最大的依赖；3.1.2 的中央目录偏移溢出与 `tinfl` 死循环修复与 `pkg install` 直接相关。
- **Lua 停在 5.4.9**：5.4 线终版、纯补丁；5.5 的破坏性变更与 `LUA_COMPAT_5_3` 语义变化另立 2.0 评估。
- **Catch2 用官方 amalgamation 覆盖**：仓库现有布局就是 amalgamation 两个文件，直接覆盖最小化 diff；升级后必须复核 `ezmk test` 的控制台摘要解析。
- **CI 保留浮点主标签**：与仓库现状一致（不改成 commit SHA 固定）；升级只动主版本号，不动工作流结构。

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| miniz 2.2.0 → 3.1.2 | 内部；zip 产物字节可能变化（UTF-8 flag） | 端到端用例；产物 hash 现算 |
| Lua 5.4.7 → 5.4.9 | 内部；无公共 API 变化 | 重施 `linit.c` 补丁 + 沙箱用例 |
| Catch2 3.8.0 → 3.16.0 | 仅测试 | 复核 `ezmk test` 摘要解析；用户项目分支不动 |
| Actions 主版本 | 仅 CI（Node 24） | push 触发 `ci.yml`；Release 侧随发布验证 |
| 公共 API / CLI / 配置 | 无变化 | API 稳定性承诺不变 |

## 6 延后项

- **Lua 5.5**（破坏性变更 + `LUA_COMPAT_5_3` 语义差异）——2.0 前评估。
- **官方仓库 `ezmk-repo` 的 `packages/` 版本更新**（catch2 3.6.0 / lua 5.4.7 / nlohmann_json 3.11.3 等）——分发内容，另立计划。
- **`macos-13` runner 退役**——该 job 默认跳过，本版不改行为。
- **action 固定到 commit SHA**（供应链加固）——如需再单独立项。
