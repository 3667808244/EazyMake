# EazyMake 1.4.5 执行计划

> **状态：✅ 已发布（tag `v1.4.5`，2026-10-02）—— 阶段一~七全部完成**。本文档把设计文档 §4 转成可勾选的七阶段清单。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)；2.0.0 的移除清单见 [`plans/2.0.x/REMOVALS.md`](plans/2.0.x/REMOVALS.md)（本版**不执行**其中任何一项，但已**新增**两条回退供其登记消费 R-03/R-04）。
>
> 详细设计：[**1.4.5.md**](plans/1.4.x/1.4.5.md)。主题：**生成物格式统一**——把两个「ezmk 全权生成、用户不该手改」的文件从 TOML 改为 JSON：`ezmk.lock` → `ezmk.lock.json`、`list.toml` → `list.json`（三作用域）。旧格式**继续可读**，首次成功写入时**自动迁移**（写新 → 删旧）。
>
> **范围边界**：只有这一个变更面（两个文件 + 连带路径 / 文档 / 测试 / zsh 补全）。不改 `ezmk.toml` / `ezmk-workspace.toml`（人写配置）、不改 `index.toml`（仓库侧作者手写）、不改 lockfile 任何**字段语义**（`sha256` 别名照旧双写、`version` 仍 1、`--locked` / `deterministic` 判定逻辑不变）。**公共 API 无破坏性变更**（只新增函数，`repo::list_toml_path()` 保留为兼容别名）。明确不做项见设计 §3.13。
>
> **⛔ 发布门槛**：① 阶段清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 **1099 用例 / 6349 断言**，1.4.4 发布态，4 跳过）且新增用例只增不减；④ 附加门槛：`python scripts/check_man_sync.py` 通过、`groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警、i18n **406 → 412** 三向一致、`bash scripts/check_docs_sync.sh` 通过。
>
> **版本决策**：dev 阶段二进制版本号**保持 1.4.4**（1.2.x/1.3.x/1.4.1~1.4.4 补丁先例均不提前 bump），正式发布 commit 按 workflow §3 置 1.4.5（`build.sh` fallback + `include/ezmk/version.hpp` + 4 页 `.TH` 日期），tag `v1.4.5`。

---

## 1 背景

EazyMake 的内部生成物目前格式混杂：`record.json` / `links.json` / `compile_commands.json` / `.vscode` 三件套早已是 JSON，只有 `ezmk.lock` 与 `list.toml` 还是 TOML——而这两个恰恰是"ezmk 自己写、自己也读、文档明确请勿手改"的文件（TOML 的真正职责是人写配置：`ezmk.toml` / `ezmk-workspace.toml` / `index.toml`）。它们的写出全靠手写字符串拼接 + `util::toml_quote` 逐处转义（1.1.2 C5 / 1.4.2 F-04 都是为补这类写入器而生），且是仓储里少数**非原子写**的生成物。本版把这两个文件 JSON 化并升级为原子写，同时用"双读 + 单向迁移"保证升级无感。详见设计 §1。

## 2 目标

| 组 | 优先级 | 覆盖 |
|----|--------|------|
| lockfile JSON 化（M-01/M-02/M-04） | P0 | `ezmk.lock.json` 写入（原子写）+ 旧 `ezmk.lock` 回退读取 + 自动迁移 + 三处缓存签名改用 `active_path()` |
| 注册表 JSON 化（M-03/M-08） | P0 | `list.json`（全局/用户/项目）+ 双读 + 迁移 + zsh 补全双分支 |
| 公共 helper 与 API 兼容（M-05/M-06） | P1/P0 | `util::atomic_write_text()` 上提；`lockfile::*` 签名不变；`repo_list_path()` / `legacy_repo_list_path()` 新增、`list_toml_path()` 保留 |
| i18n 与文档（M-07/M-10） | P0 | 5 个迁移键三向一致；docs 6 文件 + tutorial + README 中英 + man 2 页 + skills 4 处 + CHANGES 1.4.5 |
| 测试固化（M-09） | P0 | 双格式对拍等价 + 迁移 + 缓存"确定性签名跟随当前生效 lockfile" + F-24 双路径 + 保留旧 TOML 夹具 |
| 索引与登记（M-11/M-12） | P0 | `plans/2.0.x/REMOVALS.md` 登记 R-03/R-04；索引与根 `plan.md` 状态更新；明确不做项 |

## 3 执行阶段（每阶段一个 commit，阶段间全量回归）

### 阶段一：lockfile JSON 化（M-01/M-02/M-04，对应设计 §3.1~§3.6）

- [x] `include/ezmk/lockfile.hpp` / `src/lockfile.cpp`：新增 `lockfile_path(proj_root)`（= `ezmk.lock.json`）、`legacy_lockfile_path(proj_root)`（= `ezmk.lock`）、`active_path(proj_root)`（JSON 优先 → 旧文件 → 默认新名）；`load` / `save` / `verify` / `depends_changed` / `direct_dep_specs` **签名不变**
- [x] `save()` 改为 `nlohmann::json` 序列化 + `dump(2)`：`metadata` 对象（`version` 仍 **1**、`generated_by` / `generated_at` / `toolchain` / `toolchain_version` / `direct_deps`）+ `packages` 数组，键名与旧 TOML **逐字一致**（含 `sha256` 别名继续双写）
- [x] 空字段策略沿用旧写入器：`lib_sha256` / `archive_sha256` / `commit` 为空**不写出**；`direct_deps` / `dependencies` 空写 `[]`
- [x] `save()` 改用原子写（**注**：`util::atomic_write_text` 本阶段即落地——旧写入器是直写 `util::file_write`，原子写是本节验收的一部分；阶段三只剩 `workspace.cpp` 收敛），**新文件写入成功后**才 `fs::remove` 旧文件（best-effort，失败仅警告）；`--no-lock` / 非项目作用域行为不变
- [x] `load()` 双读：`ezmk.lock.json` → JSON 路径；否则旧 `ezmk.lock` → 保留 TOML 回退解析（字段读取代码不动）+ `lock_legacy_detected` 提示；两文件共存 → JSON 生效 + `lock_legacy_stale` 警告；解析失败仍为"警告 + 当作无 lockfile"
- [x] **缓存签名三处同改**（设计 §3.6，本版最易踩的坑）：`src/build.cpp:1035`、`src/cache.cpp:317`、`src/pkg.cpp:949` 全部改用 `lockfile::active_path(proj_root)`，保存侧与校验侧算法逐字节一致（对照 1.4.2 的同类缺陷 `CHANGES.md:430`）
- [x] 回归：全量零失败（**1106 用例 / 6412 断言**，4 跳过；基线 1099/6349 → +7 用例 / +63 断言）

### 阶段二：仓库注册表 JSON 化（M-03/M-08，对应设计 §3.7）

- [x] `include/ezmk/repo.hpp` / `src/repo.cpp`：新增 `repo_list_path(scope)`（= `list.json`）与 `legacy_repo_list_path(scope)`（= `list.toml`）；`list_toml_path()` **保留**并转发到 `legacy_repo_list_path()`（删除归 2.0.0，REMOVALS R-04）；顺带抽出 `repo_dir(scope)`，让三个路径函数与 `cache_dir()` 不再各自复制作用域分支
- [x] `save_repo_list()` 写 `list.json`（顶层 `{"version": 1, "repos": [...]}`，键名与旧 `[[repos]]` 一致；`type == "git"` 时才写 `branch`）+ 原子写 + 写后删旧 `list.toml`
- [x] `load_repo_list()` 双读（`list.json` 优先 → `list.toml` 回退 + `repo_list_legacy_detected`；双文件共存 → `repo_list_legacy_stale` 警告），**两条路径共用 `admit_entry()` 的 1.4.2 F-24 名字安全校验**（手改旧文件不得绕过）
- [x] `res/ezmk.zsh:_ezmk_repo_names()` 双分支：`.ezmk/repo/list.json` 优先（`grep -E '"name"\s*:'`），文件不存在回退旧 `list.toml`（静态补全脚本装在用户机上不随升级更新，不能只认新名）；grep/sed 提取已在两种格式上实测（zsh 本机不可用，语法未跑 `zsh -n`）
- [x] `repo add/remove/update` 三个调用点行为不变（只有序列化与路径改变）；`repo list/info` 读取路径同改
- [x] 回归：全量零失败（**1111 用例 / 6452 断言**，4 跳过；阶段一后 1106/6412 → +5 用例 / +40 断言）

### 阶段三：原子写 helper 上提（M-05，对应设计 §3.5）

- [x] `include/ezmk/util.hpp` / `src/util.cpp`：`util::atomic_write_text()` 公共 helper（**已在阶段一落地**——`file_write` 之上加 tmp → `atomic_rename`，失败返回 false 并清理临时文件）
- [x] `src/workspace.cpp:521` 的文件内实现改为调用公共版本（更名为 `write_workspace_config`，保留"用户文件写入失败必须响亮中止"的 `util::fatal` 语义；`ezmk-workspace.toml` 的文本级拼接与二进制写行为零变化）
- [x] 复核两个新写入器（lockfile / 注册表）全部走该 helper；回归零失败（**1111 用例 / 6452 断言**，与阶段二持平——纯重构）

### 阶段四：i18n + man + 文档 + skill（M-07/M-10，对应设计 §3.8/§3.11）

- [x] `include/ezmk/i18n_keys.def` + `locale/en.json` + `locale/zh.json`：迁移相关新键 **6** 个（`lock_legacy_detected` / `lock_migrated` / `lock_legacy_stale` / `repo_list_legacy_detected` / `repo_list_migrated` / `repo_list_legacy_stale`；**比计划多一个**——注册表的"双文件共存"与 lockfile 对称处理），键数 **406 → 412**，`python scripts/check_i18n.py` 三向一致（`zh-TW` 变体只翻译差异键，其**既有** lock_* 消息也同步改名）
- [ ] （本版明确不做）i18n 化两处硬编码英文解析失败消息（`src/lockfile.cpp` / `src/repo.cpp`）：它们由两条读取路径共用且都带文件名上下文，收益低；留作后续收口（见 §6 延后项）
- [x] `man/ezmk.1`（`--locked` / `--no-lock` / FILES 段）与 `man/ezmk.toml.5`（版本约束 / 确定性构建段）正文文件名更新 + 迁移一句；`python scripts/check_man_sync.py` 通过 + `groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警
- [x] `docs/{en,zh}` **7 个文件**（`config_file.md`（Lockfile 小节 + JSON 示例块 + 迁移/降级说明 + 补 `scope` 语义）、`cli.md`、`repo.md`（注册表 + JSON 示例 + 迁移说明）、`technical.md`（目录树 + 补 `ezmk.lock.json` 行）、`glossary.md`、`pkg.md`、**`package_authoring.md`（计划清单遗漏，本轮补齐）**）；`bash scripts/check_docs_sync.sh` 通过（docs 15 / tutorial 16 同名同集合）
- [x] **示例口径修正**：`source` 是**来源类型**（`repo`/`url`/`local`/`archive`/`git`）、具体来源在 `source_url`（与 `src/pkg.cpp:1601`/`:2012` 一致）——设计文档 §3.1 与 4 处文档示例同批改正
- [x] `tutorial/{en,zh}/packages/02-version-lockfile.md`：标题 / "(TOML)" → "(JSON)" / 示例块 / `git add ezmk.lock.json`
- [x] `README.md:198` / `README_ZH.md:197` 高级特性表行
- [x] skills **5 个文件**：`ezmk-codebase`（目录树 / config 行 / lockfile 模块行与 API / 注册表三路径 / Lockfile 小节）、`ezmk-repo`（注册表小节）、`ezmk-user-pkg`（lockfile workflow 示例块 TOML→JSON + `git add` 行 + `source` 口径）、`ezmk-user-config`、`ezmk-test`
- [x] 顺带（服务文档保真）：lockfile / 注册表写入改用 `nlohmann::ordered_json`，使落盘字段顺序与文档示例一致（`record.json` 保持 `json` 不变）

### 阶段五：测试固化（M-09，对应设计 §3.9）

- [x] **双格式对拍等价用例（本版最关键）**：同一份 lockfile 的 TOML 文本与 JSON 文本（两个包：repo 静态库 + local header-only）→ 两个读取路径各产出 `config::Lockfile` → metadata 6 字段 + 每个 package 的 12 字段**逐字段 `REQUIRE` 相等**，并对 `depends_changed` 断言行为一致
- [x] 双读回退用例（仅有旧 `ezmk.lock` 时逐字段加载 + `active_path()` 解析：旧文件优先 → 新文件优先）；迁移写入用例（新文件存在且可解析 + 旧文件被删 + round-trip 相等）；双文件共存用例（JSON 生效 + 警告）；解析失败用例（损坏 JSON → 警告 + 视为无 lockfile）
- [x] 空字段不写出用例（`lib_sha256` / `archive_sha256` / `commit` 空 → JSON 无该键；`dependencies` / `direct_deps` 空 → `[]`；读回仍为空）
- [x] `test_cache.cpp`：**确定性签名跟随当前生效的 lockfile** —— 旧 TOML 状态下命中 → 迁移到 JSON 后旧签名失效（恰一次重编）→ 按新文件重新签名后再次命中
- [x] `test_repo.cpp`：注册表迁移 + 双读一致 + F-24 在**两条路径**都生效（旧 TOML 夹具与 JSON 夹具各一份）+ 路径断言改为 `repo_list_path(...).filename() == "list.json"` 与 `list_toml_path() == legacy_repo_list_path()` + local 条目不写 `branch`
- [x] `test_lockfile.cpp` 既有 TOML 夹具：写入侧断言改 JSON，**读取侧的两处旧 TOML 夹具按夹具纪律保留**（双读路径的覆盖）
- [x] **夹具纪律**：`test/test_integration.cpp:938-941` 的旧 TOML lockfile 夹具**保持不动**（strict 模式读旧格式的端到端回归）；`test/test_integration_git.cpp` 路径改新名 + JSON 内容断言（`"source": "git"` / `"commit": "…"`）
- [x] 回归：全量零失败，**1113 用例 / 6491 断言**（4 跳过；基线 1099/6349 → **+14 用例 / +142 断言**，只增不减）

### 阶段六：变更日志与收口（M-10/M-11/M-12，对应设计 §3.11/§3.12/§3.13）

- [x] `CHANGES.md` 新增 1.4.5 条目（**`(未发布)` 占位**，发布 commit 回填日期）：变更 / **自动迁移（无需手动操作）** / **降级说明**（1.4.4 及更早读不到 `*.json`：`deterministic = true` 致命、注册表显示为空需重新 `repo add`）/ 新增 API（`lockfile::active_path` / `repo::repo_list_path` / `util::atomic_write_text`）/ 测试 / 明确不做
- [x] `plans/2.0.x/REMOVALS.md`：登记 **R-03**（旧格式读取回退：`ezmk.lock` TOML / `list.toml` TOML → 2.0.0 移除并给明确迁移报错）与 **R-04**（`repo::list_toml_path()` 别名 → 2.0.0 删除）；§7 边界补"1.4.5 的方向是**新增**回退，与不移除垫片的约束同向"（**计划阶段已随计划文档一并提交**）
- [x] `plans/1.4.x/README.md`（版本表 + 依赖关系 + 跨版本关注点 + 回归基线 + i18n 实测 412）、`plans/README.md`（当前执行 / 目录结构 / 系列条目 / 版本表 / mermaid 节点）、根 `plan.md` 状态更新
- [x] 门槛复核：清单完成 + API 无破坏 + 全量 **1113/6491** 零失败 + `check_man_sync.py` / groff 4 页 / i18n **412** / docs-sync 四项附加门槛（见下"阶段六门槛复核实测"）

#### 阶段六门槛复核实测（2026-09-27）

| 门槛 | 实测 |
|------|------|
| ① 清单完成/收口 | 阶段一~五全部 `[x]`；阶段四唯一未做项（两处硬编码解析消息 i18n 化）**明确不做**并记入 §6 延后项 |
| ② API 无破坏 | 仅**新增** `lockfile::lockfile_path/legacy_lockfile_path/active_path`、`repo::repo_list_path/legacy_repo_list_path`、`util::atomic_write_text`；`lockfile::load/save/verify/depends_changed` 签名不变；`repo::list_toml_path()` 保留为旧路径别名（2.0.0 移除，REMOVALS R-04） |
| ③ 全量零回归 | `bash build.sh test-all` → **1113 用例 / 6491 断言，0 失败**（4 跳过；基线 1.4.4 发布态 1099/6349 → +14 用例 / +142 断言） |
| ④a man | `python scripts/check_man_sync.py` → `OK: man pages are in sync …`；`groff -man -Tutf8 -z -ww man/ezmk.1 man/ezmk-lua.1 man/ezmk.toml.5 man/ezmk-workspace.toml.5` → 退出码 0、零告警 |
| ④b i18n | `python scripts/check_i18n.py` → `i18n_keys.def / en / zh` 各 **412** 键 + `zh-TW` 变体一致 |
| ④c docs | `bash scripts/check_docs_sync.sh` → `docs (15 files matched)` + `tutorial (16 files matched)` + `All checks passed` |

### 阶段七：正式发布（workflow §3，对照 1.4.4 流程）

- [x] 版本定稿（commit `e29ae82`）：`build.sh` fallback + `include/ezmk/version.hpp` → 1.4.5（实测 `./build/ezmk version` → `EazyMake 1.4.5`）；4 页 `.TH` 日期 = **2026-10-02**；`CHANGES.md` 日期回填；`docs/{en,zh}/config_file.md` 的 `EZMK_VERSION` 与 `README{,_ZH}.md` 的 winget `-Version` 示例同步 1.4.5；新增 `publish/release-notes-1.4.5.md`；定稿后全量 **1113/6491 零失败**
- [x] **BOM 修复（commit `65c7af3`）**：定稿时用 PowerShell 改 4 页 man 的 `.TH` 日期写入了 UTF-8 BOM——CI 的 man 静态 lint 抓到（本地 groff / `check_man_sync.py` 不报），且实测渲染会在标题前多一行 `ï»¿`。按 workflow §3.5「回退 tag 并修复」：删 Release + tag → 在修复 commit 上重建 → 重做三渠道。修复后 4 页首 3 字节 `2e 5c 22`、本地按 CI 的 6 项 grep 复现 0 命中、CI run `36996122783` **全绿**
- [x] tag `v1.4.5`（annotated，**回退后**指向 `65c7af3`，tag 对象 `68e214d`）+ GitHub Release 重建（notes 取 `publish/release-notes-1.4.5.md`）：`release.yml` run **`36997151873` success**（`version` / `linux-x64` / `macos-arm64` / `windows-x64` 全绿，`macos-x64` **skipped**，与 1.4.4 一致）；7 资产全量下载核对——digest 与 `assets[].digest` **逐一一致**（zip `a2b45465…` / linux `faca5023…` / macos `66c82770…` / exe `0b2c4c4a…` / lua `4bdfdcf2…`）、两个 `.sha256` 边车与对应 digest 一致；linux/macos tar 含 `man/` 4 页（**无 BOM**、groff 零告警、渲染首行即标题）且 ELF 版本串 `1.4.5`、Windows zip 仅 `_ezmk` + 两个 exe（不含 `man/`）、下载的 `ezmk.exe version` → `EazyMake 1.4.5`（首次发布 run `36993548272` 对应已作废的 `e29ae82`）
- [x] 三渠道（**回退后重做**）：**Homebrew** tap `3667808244/homebrew-eazymake` commit `206a4e4`（macos-arm64 `66c82770…` / linux-x64 `faca5023…` 真实 digest，仓库副本同步；首次 `09b304c` 作废）/ **pacman** `publish/arch/PKGBUILD` → v1.4.5（源码 digest `0849809f…`，codeload == archive）+ MSYS2 MINGW64 `makepkg -fd` 出包 `eazymake-1.4.5-1-x86_64.pkg.tar.zst`（sha256 `fb4980eb…`，`sha256sums` 通过，包内 `ezmk.exe version` → 1.4.5、man 4 页无 BOM 且零告警、`_ezmk` 落位；首次 `25bbabf7…` 作废）/ **winget** split manifests（`publish/winget/e/ezmk/1.4.5/`）`winget validate` 通过 + PR [`microsoft/winget-pkgs#445627`](https://github.com/microsoft/winget-pkgs/pull/445627)（`InstallerSha256` 已更新为 `a2b45465…` 并触发 CI 重跑；`license/cla` pass；`07/08/09` 长跑 check 与版主审批为发布后跟进项）
- [x] 发布记录：`CHANGES.md` 1.4.5「发布」小节（含**回退原因与流程反思**：首次先建 Release 后查 CI，违反 §3.1「tag 前确认 CI 绿」）+ [`publish/release-notes-1.4.5.md`](publish/release-notes-1.4.5.md) + `plans/1.4.x/README.md` / `plans/README.md` / 根 `plan.md` 状态更新（1.4.5 移入「已完成」，mermaid 节点 `v145p` → done）

---

## 4 关键设计决策

- **只换容器，不换结构**：JSON 形状与旧 TOML 一一对应（`metadata` + `packages`），键名逐字一致、`version` 仍 1、`sha256` 别名继续双写——目的是让"迁移是否等价"可以用**逐字段对拍**机械验证（阶段五），而不是靠人工核对。
- **双读 + 单向迁移，而不是硬切换**：旧文件照读、只在新文件写入成功后删旧文件；升级无感，代价只有"降级不可读"这一条，用文档写死。硬切换（只读 JSON）属破坏性变更，按仓库惯例归 2.0.0。
- **文件名与格式同时改**：`list.toml` 里放 JSON 名实不符；`ezmk.lock` 无扩展名，改名零阻力。文件名成为格式的唯一判别依据，老版本读新文件时表现为"文件不存在"（可理解的报错）而非"解析失败"。
- **删旧文件必须在新文件落地之后**：任何失败路径都宁可留下双文件（下次读取走 JSON + 陈旧警告），绝不在写入未成功时丢 lockfile。
- **缓存签名用"当前生效路径"而非"新名字"**：`deterministic` 下 lockfile 的**内容**哈希是编译签名的一部分，三处（保存/校验/包缓存）必须取同一份文件，否则某一侧在迁移后取不到文件、签名永不等，重演 1.4.2 的缺陷；迁移因换格式改变内容而带来**恰一次**全量重编，用测试锁定"仅此一次"。
- **回退路径不得降级安全**：1.4.2 F-24 的名字校验在旧格式读取路径上同样生效（手改的旧 `list.toml` 一样能塞 `../evil`），两条路径各有一份用例。
- **静态补全脚本必须双分支**：`res/ezmk.zsh` 装在用户机上不会随升级更新，"只认新名"会让老用户补全直接失效。
- **顺手升级原子写**：两个文件原本是仓储里少数非原子写的生成物；半截 lockfile 在 `deterministic = true` 下是致命错误，借写入器重写一并修掉（新 helper 上提为 `util::atomic_write_text`）。
- **不碰字段语义、不碰弃用面**：本版**新增**兼容回退而非移除任何东西，与 `REMOVALS.md` §7「1.4.x 不放宽不移除弃用面」的硬约束同向；`sha256` 别名（D-01）、`index.toml`、`ezmk.toml` 一律不动。

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|---|---|---|
| `ezmk.lock` → `ezmk.lock.json` | 升级无感（旧文件照读）；首次写入后 git 视角为一次"删除 + 新增"重命名 | 迁移提示 + CHANGES 说明，用户只需 commit |
| **降级 1.4.5 → ≤1.4.4** | 老版本读不到 `*.json`：`deterministic = true` **致命**；非确定性仅告警 | 文档显式写明；可从旧 commit 取回 TOML lockfile |
| `list.toml` → `list.json` | 升级无感；降级后老版本注册表为空（已安装包不受影响，重新 `repo add` 可恢复） | 文档写明（注册表是本地状态文件） |
| 缓存签名改用 `active_path()` | 迁移当天一次全量重编（换格式必然改变 lockfile 内容 → 内容哈希变化） | 单测锁定"仅一次，之后命中" |
| `lockfile::load/save/verify/depends_changed` | 签名与语义不变 | 门槛② |
| `repo::list_toml_path()` | 保留（返回旧路径）；新增 `repo_list_path()` / `legacy_repo_list_path()` | 删除归 2.0.0（R-04） |
| `util::atomic_write_text()` | 新增公共 helper；`workspace.cpp` 行为不变 | 纯增量 |
| i18n | 406 → 412（en/zh/def 三向一致） | `check_i18n.py` |
| lockfile 字段 / `--locked` / `deterministic` / `[depends]` 语义 | **无变更** | 门槛② |
| 公共 API / CLI 行为 / 配置语义 | 无破坏性变更（仅新增 + 生成物格式） | 门槛② |

## 6 延后项

- **`index.toml`**（仓库侧索引）：作者手写、生态已固化，不在本版范围（REMOVALS D-08 口径）。
- **`ezmk.toml` / `ezmk-workspace.toml`**：人写配置保留 TOML（`toml++` 依赖不退役；`util::toml_quote` 仍被 `config.cpp` / `import.cpp` / `workspace.cpp` 使用，无死代码）。
- **旧格式读取回退的移除** 与 **`repo::list_toml_path()` 的删除**：归 2.0.0，已在 REMOVALS 登记 R-03/R-04。
- **lockfile `sha256` 旧别名**（REMOVALS D-01）：本版继续双写，结论留 2.0.0 拍板。
- **`pkg remove` 不重写 lockfile** 的既有行为：不在本版顺手扩大范围（属既有语义，非本次格式变更引入）。
- **`repo info --json`**（`plans/0.x.x/0.2.5.md:717` 曾提出）：注册表 JSON 化后技术阻力下降，可在 2.x 重估。
- **两处硬编码英文解析失败消息的 i18n 化**（`failed to parse ezmk.lock: ` / `failed to parse repo list: `）：由两条读取路径共用且都带文件名上下文，本版不做；日后若要收口，需 +2 键并同批更新索引口径。
- **其余非原子写生成物的统一**（`compile_db.cpp` / `export.cpp` / `cache.cpp` 各自的 tmp → rename 手写片段）：本版只新增公共 helper 并用于新写入器 + `workspace.cpp`，不做全仓替换以控制 diff。
