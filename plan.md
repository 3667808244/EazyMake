# EazyMake 1.4.4 执行计划

> **状态：✅ 阶段一~六完成（实现收口，2026-09-19）**——本文档把设计文档 §4 转成可勾选的七阶段清单。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)；2.0.0 的移除清单见 [`plans/2.0.x/REMOVALS.md`](plans/2.0.x/REMOVALS.md)（本版**不执行**其中任何一项）。
>
> 详细设计：[**1.4.4.md**](plans/1.4.x/1.4.4.md)。主题：**历史遗留清理**——1.4.3 发布后全仓扫描出的"小而确定"的债：`install.ps1 -DryRun` 缺陷、`test/` 7 条编译告警、`.gitignore` 过期条目、过期 TODO、`release.yml` 僵尸 job、`check_docs_sync` 未接线。
>
> **范围边界**：**零功能新增、零 CLI 行为变更、零配置语义变更、公共 API 无破坏性变更**；不放宽也不移除任何弃用面（`[test].flags` / `ezmk utils cc` 等留 2.0.0）。明确不做：`zh-TW` 补键、`[install].sharedir`、本地残留文件清理、文件监视 SKIP、man/CLI 正文（设计 §3.8）。
>
> **⛔ 发布门槛**：① 阶段清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 **1099 用例 / 6348 断言**，1.4.3 发布态）；④ 附加门槛：`check_man_sync.py` 通过、`groff -man -Tutf8 -z -ww` 4 页零告警、i18n **406** 键三向一致、新增的 docs-sync CI 步通过。
>
> **版本决策**：dev 阶段二进制版本号**保持 1.4.3**（1.2.x/1.3.x/1.4.1~1.4.3 补丁先例均不提前 bump），正式发布 commit 按 workflow §3 置 1.4.4（`build.sh` fallback + `include/ezmk/version.hpp` + 4 页 `.TH` 日期），tag `v1.4.4`。

---

## 1 背景

1.4.3 发布后对仓库做了一次系统的历史遗留扫描，六类结论与归属见设计 §1 的表。本版取其中**不需要新功能即可收口**的五类：用户可见缺陷（`install.ps1 -DryRun`）、质量口径缺口（`test/` 告警）、仓库卫生（`.gitignore`）、过期承诺（`cli.cpp` TODO）、流程僵尸与未接线检查（`macos-x64` job、`check_docs_sync`）。其余各有归属：弃用面与兼容垫片 → 2.0.0；`sharedir` → 2.0.0 决策项；本地残留不属仓库内容。

## 2 目标

| 组 | 优先级 | 覆盖 |
|----|--------|------|
| 用户可见缺陷（M-01/M-07） | P0 | `install.ps1 -DryRun` 修复 + windows job 冒烟（零副作用、零网络） |
| 质量口径（M-02） | P0 | `test/` 7 条编译告警清零，使"首方代码零告警"覆盖 `src/` + `include/ezmk/` + `test/` |
| 仓库卫生（M-03/M-04） | P1 | `.gitignore` 过期条目/去重/分组；过期 TODO 改为不挂版本的已知限制 |
| 流程与 CI（M-05/M-06/M-07） | P0/P1 | `macos-x64` 默认跳过（`if:` 条件）；`check_docs_sync` 接入 ubuntu job |
| 文档与收口（M-08/M-09） | P0 | CHANGES / skill / formula 注释 / 索引 / 状态 + 明确不做项 |

## 3 执行阶段（每阶段一个 commit，阶段间全量回归）

### 阶段一：`install.ps1 -DryRun` 修复 + windows CI 冒烟（M-01/M-07，对应设计 §3.1/§3.7）

- [x] 根因修复：`Main` 的 `$tempDir` 改为**两种模式都计算展示路径**，仅非 dry-run 时 `New-Item`（现状 `install.ps1:520-524` 在 dry-run 下留空 → `Invoke-BinaryDownload:197` 的 `Join-Path` 抛 `EmptyStringNotAllowed` 中止）
- [x] 预览完整性：`Register-OfficialRepo:378-381`、`Preinstall-OfficialUtils:415-418`、`Confirm-Installation:442-445` 三处把 `Test-DryRun` 分支提到 `Test-Path $ezmkBin` 早退**之前**（dry-run 下目标二进制必然不存在，现状会误报"跳过"）
- [x] 防御加固：`Invoke-BinaryDownload` 开头补空 `DestDir` 断言（`Write-Die`），避免同类回归以晦涩报错形式出现
- [x] 本地实跑（PowerShell 5.1）：`.\install.ps1 -DryRun` 与 `.\install.ps1 -Version v1.4.3 -DryRun` → **退出码 0**、打印全部 `Would …`、**未创建任何文件/目录**、未改 PATH、未联网
- [x] 非 dry-run 行为零变化复核（路径/顺序/提示文案；可用 `-InstallDir <temp>` 对着已下载资产跑一遍真实安装）
- [x] CI：windows job 新增 `shell: pwsh` 步骤跑 `install.ps1 -DryRun -InstallDir <temp>`，断言退出码 0 + 目标目录**未被创建** + 输出含 `[DRY RUN] Would download to:` / `Would install:` 关键行

### 阶段二：测试代码 7 条编译告警清零（M-02，对应设计 §3.2）

- [x] `test/test_build.cpp:73`：补齐聚合初始化（**必须 C++17 合法**，不得使用 designated initializers——1.4.2 F-10 刚移除过）
- [x] `test/test_cache.cpp:711`：修 `-Wcomment`（`//` 注释行尾反斜杠导致的续行）
- [x] `test/test_integration.cpp:2399/2404/2419/2475`：循环变量 `const std::string&` → `std::string_view`（`-Wrange-loop-construct`，4 处同构）
- [x] `test/test_lua.cpp:53`：删除已无调用点的 `lua_dostring_safe`（或加 `[[maybe_unused]]` 并注明理由）
- [x] 严格旗标下复核：`CXXFLAGS='-std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2' bash build.sh test` → `src/` + `include/ezmk/` + `test/` **零告警**（vendor 除外）
- [x] 回归：断言数保持 **1099 用例 / 6348 断言**（若数字变化必须说明原因）

### 阶段三：`.gitignore` 卫生（M-03，对应设计 §3.3）

- [x] 删除已被跟踪文件的忽略条目：`plan.md`、`include/ezmk/version.hpp`（ignore 对 tracked 文件无效，留着掩盖真实状态）
- [x] 删除已消失条目：`stdout_in_linux_vm.txt`
- [x] 去重 `.claude/*` + `!.claude/skills/`（现状首尾各一组）
- [x] 按用途分组重排：生成物 / 构建产物 / 本地临时 / IDE·OS（保留 `build/`、`.ezmk/`、生成物、`.dsh/` 的真实忽略）
- [x] 复核：`git status --porcelain` 干净；`git status --porcelain --ignored` 中生成物仍为 `!!`、`plan.md`/`version.hpp` 不再被忽略

### 阶段四：过期 TODO 与"挂版本承诺"扫查（M-04，对应设计 §3.4）

- [x] `src/cli.cpp:1144-1146`：改写为**不挂版本**的已知限制（argv 由 OS 保证 NUL 结尾、不可能含嵌入 NUL，故无需防御；明确它不是待办）
- [x] 全仓扫同类"挂版本承诺"（`归 1.`、`将在 1.`、`TODO(1.`、`待 1.`）→ 指向已发布版本的一并改写为事实陈述或指向 `plans/2.0.x/REMOVALS.md`
- [x] **不改**历史溯源注释（`// 1.2.0-dev.11: …` 这类记录实现时间的注释，与承诺区分）
- [x] 复核：`grep -rn "归 1\." src/ include/ezmk/` 无命中

### 阶段五：CI 与发布流程卫生（M-05/M-06/M-07，对应设计 §3.5/§3.6）

- [x] `release.yml`：`macos-x64` job 加 `if: vars.ENABLE_MACOS_X64 == 'true'`（未设置 → 空串 → **skipped**，run 不再长期 `queued`），并补注释说明如何开启
- [x] `release.yml` YAML 校验：`npx --yes js-yaml` 解析通过；确认新增/改动的 `name:` 一律带引号（1.4.3 曾因步骤名含 `: ` 导致 0 秒失败）
- [x] ubuntu job 新增 `Docs: en/zh file parity` 步 → `bash scripts/check_docs_sync.sh`（当前 `docs` 15/15、`tutorial` 16/16）
- [x] `CONTRIBUTING.md`：把该检查从"人工清单"更新为"CI 已接线，本地可预检"；windows 冒烟步骤写入测试说明
- [x] 本地 dry-run：用 `build/ci.json`（js-yaml 产物）复跑新增步骤的 shell 片段（受限沙箱下 MSYS2 bash 起不来，以 CI 为准）

### 阶段六：文档与收口（M-08，对应设计 §3.7/§5）

- [x] `.claude/skills/ezmk-publish/SKILL.md`：§2.3 与坑位表补"`macos-x64` 默认跳过，需要时开 `ENABLE_MACOS_X64`"；`publish/homebrew/ezmk.rb` 头部注释同步 Intel Mac 口径
- [x] `.claude/skills/ezmk-workflow`：发布流程 §3.3 的产物核对补一句"`macos-x64` 默认 skipped 属预期"
- [x] `CHANGES.md` 新增 1.4.4 条目（修复 / 卫生 / 文档 / 已知限制），`(未发布)` 占位由发布 commit 回填日期
- [x] `plans/1.4.x/README.md`、`plans/README.md`、根 `plan.md` 状态与索引更新
- [x] 门槛复核：清单完成 + API 无破坏 + 1099/6348 零回归 + `check_man_sync.py`/groff/i18n/docs-sync 四项附加门槛

### 阶段七：正式发布（workflow §3，对照 1.4.3 流程）

- [ ] 版本定稿：`build.sh` fallback + `include/ezmk/version.hpp` → 1.4.4；4 页 `.TH` 日期 = 发布日期；`CHANGES.md` 日期回填
- [ ] tag `v1.4.4`（annotated）+ GitHub Release（notes 取 CHANGES 1.4.4 节）+ 产物核对（linux/macos tar 含 `man/` 4 页、Windows zip 不含；digest 与 `assets[].digest` 一致）
- [ ] 三渠道：Homebrew（本地副本 + tap，真实 digest）/ pacman（`PKGBUILD` pkgver + 源码 tarball digest + `makepkg -fd` 出包）/ winget（split manifest + PR；CLA 账户已签）
- [ ] **验证阶段五的成果**：Release run 不再长期 `queued`（`gh run watch --exit-status` 能真正代表结果）
- [ ] 发布记录：`CHANGES.md`「发布」小节 + `publish/release-notes-1.4.4.md` + 索引状态

---

## 4 关键设计决策

- **缺陷优先于卫生**：阶段一是本版唯一"用户可见"的修复（README 记载的 `-DryRun` 用法实跑报错），其余为口径与流程卫生——排序按"用户影响 > 工具产出可信度 > 注释/规则整洁"。
- **预览契约写死**：dry-run 必须"退出码 0 + 零副作用 + 零网络 + 完整打印"，并**用 CI 断言锁定**（否则修复会在下次改动中悄悄退化）。
- **测试告警必须真清**：不接受"在 CI 里加 `-Wno-*`"这种把闸门调松的做法；改法限定 C++17（避免重蹈 1.4.2 F-10 的 designated-initializer 覆辙）。
- **僵尸 job 保留定义、默认跳过**：删掉会丢掉"runner 一旦可用就能出包"的能力；`if: vars.…` 既让 run 正常结束，又保留开关。
- **`check_docs_sync` 接入而非退役**：它是唯一能防"en 加了文档、zh 忘了"的机械检查（内容质量仍靠人工），成本是一步 bash。
- **过期承诺改陈述、不改行为**：`cli.cpp` 的 TODO 改写不引入任何代码路径（"检测截断"本无防御对象）。
- **不动 man/CLI/配置正文**：本版零行为变更，`check_man_sync.py` 预期无感；仅发布 commit 更新 `.TH` 日期。
- **与 2.0.0 严格分工**：本版只清"不需要弃用到期"的债；弃用面与决策项留 [`plans/2.0.x/REMOVALS.md`](plans/2.0.x/REMOVALS.md)。

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|---|---|---|
| `install.ps1 -DryRun` 修复 | 预览从"报错"变为"可用"；真实安装路径不变 | CI 冒烟锁定零副作用 |
| `test/` 告警修法 | 仅测试代码表达形式 | 回归对照 1099/6348 |
| `.gitignore` 条目删除/重排 | 对已跟踪文件无影响；新克隆行为不变 | `git status --ignored` 复核 |
| `src/cli.cpp` 注释改写 | 无行为变化 | — |
| `release.yml` `macos-x64` 默认跳过 | 资产集合不变（本就无该资产）；run 状态恢复正常 | 需要时开 `ENABLE_MACOS_X64` |
| ubuntu job 新增 docs-sync 步 | 仅新增断言；当前 en↔zh 集合已一致 | 与 CONTRIBUTING 口径同步 |
| 公共 API / CLI / 配置语义 | **无变更** | 门槛②满足 |
| Windows（`install.ps1` 真实安装 / winget） | 无变化 | — |

## 6 延后项

- **`locale/zh-TW.json` 的 32 个未译键**：变体继承是既定设计，补齐属增强而非清理；留待专门的语言变体计划（设计 §3.8）。
- **`[install].sharedir`（解析但不生效）**：实现属新功能、移除影响配置兼容性 → 归 [`plans/2.0.x/REMOVALS.md`](plans/2.0.x/REMOVALS.md) **D-07** 拍板。
- **2.0.0 弃用面与兼容垫片**（`[test].flags`、`ezmk utils cc`、lockfile `sha256` 别名、`[compile].include_dir` 单数、`-V`、`util::run_executable(int)`、`[utils.permissions]` 缺省、旧式 shell 钩子、repo `[platform]` 缺省、未知键策略）：见 2.0.0 清单 §2/§3，本版一律不动。
- **本地残留文件**（根目录 `temp.md`/`sessions.md`/`build_err.txt`/`*.eml`、`build/pkg-arch/…`）：未被跟踪，不属仓库内容。
- **`test_file_watcher.cpp` 的 7 处环境相关 SKIP**：需在 Linux/CI 环境判定，本版不动。
- **`test_workspace.cpp:301` 的 Catch2 `WARN`**：刻意的运行期诊断信息，非编译告警。
