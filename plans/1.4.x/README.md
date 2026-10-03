# 1.4.x — 功能版本系列（调试配置自动化 + 语言标准收尾）

> 1.4.x 系列 = **1.4.0 功能版本**（dev.1 调试配置生成 → dev.2 工具链能力表 + 校验严格化 → dev.3 编译协商 → dev.4 CMake 互操作补全 → dev.5 功能收口 → dev.6 代码质量审计 → **dev.7 workspace scan（用户确认插队）** → pre.1 发布前收口）—— **2026-08-30 已发布 ✅（tag `v1.4.0`）**；**1.4.1 补丁版本**（`pkg install` 支持 git 仓库 URL）—— **2026-09-03 已发布 ✅（tag `v1.4.1`）**；**1.4.2 补丁版本（代码质量审计修复第二轮）—— ✅ 已发布（2026-09-13，tag `v1.4.2`；全量 1099/6342 零回归；分发完成：Release 7 资产（真实 digest 已回填 homebrew/winget）+ pacman 本机 makepkg 出包 + winget PR #434086 已合并上线）**；**1.4.3 补丁版本（man 手册 4 页：`ezmk.1` + `ezmk-lua.1` + `ezmk.toml.5` + `ezmk-workspace.toml.5`）—— ✅ 已发布（2026-09-19，tag `v1.4.3`；全量 1099/6348 零回归；三渠道分发完成：Release 7 资产（真实 digest 已回填 homebrew/winget）+ pacman 出包 + winget PR #437604 已合并上线）**（见 [1.4.3.md](1.4.3.md)）；**1.4.4 补丁版本（历史遗留清理：`install.ps1 -DryRun` / 测试告警 / `.gitignore` / 过期 TODO / `macos-x64` job / docs-sync 接线）—— ✅ 已发布（2026-09-26，tag `v1.4.4`；全量 1099/6349 零回归，4 跳过；`macos-x64` job skipped 生效；三渠道：Homebrew tap 1.4.4 / pacman 出包 / winget PR #441646）**（见 [1.4.4.md](1.4.4.md)）；**1.4.5 补丁版本（生成物格式统一：`ezmk.lock` → `ezmk.lock.json`、`list.toml` → `list.json`，旧格式双读 + 首次写入自动迁移 + 原子写）—— ✅ 已发布（2026-10-02，tag `v1.4.5`；全量 1113/6491 零回归，4 跳过；i18n 412 键三向一致；三渠道：Homebrew tap `206a4e4` / pacman 出包 / winget PR #445627）——**tag 同日回退一次**（首次定稿 commit 的 man 源文件带 UTF-8 BOM，CI man 静态 lint 抓到并已修复重发，详见 [1.4.5.md](1.4.5.md) 发布小节）**（见 [1.4.5.md](1.4.5.md)）；**1.4.6 补丁版本（代码质量审计修复第三轮）—— ✅ 实现收口（未发布，见 [1.4.6.md](1.4.6.md)）**；**1.4.7 补丁版本（MSVC 工具链支持修复 + 工具链优先级调整）—— ✅ 已发布（2026-10-03）（见 [1.4.7.md](1.4.7.md)）**。
>
> 本目录为 1.4.x 全部文档的**平铺结构**（无子文件夹）：`1.4.0-dev.N.md` 为开发子版本，`1.4.0-pre.N.md` 为发布前收口（`1.4.0-pre.1.md` 已建），`1.4.0.md` 为正式版聚合（届时新建），`1.4.1.md` / `1.4.2.md` / `1.4.3.md` 为正式发布后的补丁版本（对照 1.3.x 补丁惯例；1.4.3 = man 手册、**已发布** 2026-09-19），`1.4.4.md` 为历史遗留清理补丁（**已发布** 2026-09-26），`1.4.5.md` 为生成物格式统一补丁（**已发布** 2026-10-02），`1.4.6.md` 为代码质量审计修复第三轮补丁（**已发布 2026-10-03**），`1.4.7.md` 为 MSVC 工具链支持修复 + 优先级调整补丁（**已发布 2026-10-03**）。

## 定位

1.4.0 是 1.3.x 全部补丁收口后的**首个功能 minor**。两大主线：

1. **调试配置自动化**（dev.1）：VS Code 三件套（`launch.json`/`tasks.json`/`settings.json`）一键生成，per-platform 调试器（gdb/lldb/cppvsdbg），与 `[compile.profile.*]` 联动——把 ezmk 的构建知识（include/宏/`-std`/依赖注入）复用进调试器配置，消灭手写拼参。
2. **语言标准收尾**（dev.2 ~ dev.4）：1.3.1 区间语言标准（语义 A）的后续——工具链能力表 `max_supported_std`（语义 C 铺路）→ 校验严格化开关 → 编译协商（语义 B，包按 `max(包min, 消费者标准)` 重编）→ CMake `CXX_STANDARD` 导入映射补全。

dev.5 集中收口 1.3.x 各版延后的小功能项（watch `--` 透传 / `workspace watch` / `tgz` 别名 / sha256 边车自动校验）。dev.6 对全部代码做系统质量审计（8 路并行审查），修复 8 项 P0 正确性缺陷与高价值 P1（确定性缓存失效 / `--locked` 不锁版本 / 依赖名与 repo 名路径穿越 / git branch 注入 / MSVC 本地化解析 / CMake 括号注释 / Lua 根路径），并修复 4 个恒真/无断言测试。**dev.7 为用户确认的插队功能子版本**：`ezmk workspace scan`——一键采纳现有目录树为 workspace（棕地场景，补齐 `project new`/`import` 之外的采纳路径）。**公共 API 无破坏性变更**（纯增量；破坏性变更仍仅归 2.0.0）。

## 版本规划（dev / pre 拆分）

> 1.4.0 正式版按 dev → pre 两阶段推进：dev 落地功能，pre 做文档/门槛收口，最后聚合发布。

| 子版本 | 主题 | 关键交付 | 状态 |
|--------|------|----------|------|
| [1.4.0-dev.1](1.4.0-dev.1.md) | 调试配置生成 | `project export vscode` 三件套（launch/tasks/settings）+ per-platform 调试器 + profile 联动 + JSON 序列化/覆盖保护 | ✅ 已完成（2026-08-27，dev.1：全量 931/5396 零回归，+20 用例/+94 断言） |
| [1.4.0-dev.2](1.4.0-dev.2.md) | 工具链能力表 + 校验严格化 | `max_supported_std(family, version)`（gcc/clang/msvc 分段）+ `[pkg] strict_std_check` 开关（warn→error） | ✅ 已完成（2026-08-27，dev.2：全量 939/5442 零回归，+8 用例/+46 断言） |
| [1.4.0-dev.3](1.4.0-dev.3.md) | 编译协商（语义 B） | 包按 `max(包min, 消费者min)` 重编（cap 到能力表与包 max）+ 缓存签名自动失效 + 与 1.3.1 warn 共存 | ✅ 已完成（2026-08-27，dev.3：全量 948/5472 零回归，+9 用例/+30 断言） |
| [1.4.0-dev.4](1.4.0-dev.4.md) | CMake 互操作补全 | `import` 读 `CXX_STANDARD` → 区间 language（`">=CPP<N>"`）；`export` 超能力注释 | ✅ 已完成（2026-08-27，dev.4：全量 959/5492 零回归，+11 用例/+20 断言） |
| [1.4.0-dev.5](1.4.0-dev.5.md) | 功能收口 | watch `--` 透传 + `workspace watch` + `tgz` 别名 + sha256 边车自动校验（1.3.x 延后项） | ✅ 已完成（2026-08-27，dev.5：全量 968/5588 零回归，+9 用例/+96 断言） |
| [1.4.0-dev.6](1.4.0-dev.6.md) | 代码质量审计 | 8 路并行审查全量代码；修复 8 P0 + 高价值 P1（缓存签名/`--locked` 锁版本/依赖名与 repo 名校验/git branch 注入/MSVC 本地化/CMake 括号注释/Lua 根路径）+ 4 恒真测试 | ✅ 已完成（2026-08-29，dev.6：全量 968/5612 零回归，+24 断言） |
| [1.4.0-dev.7](1.4.0-dev.7.md) | workspace scan | `ezmk workspace scan [<dir>] [--dry-run] [-y]`：递归扫描目录树收集成员、生成/合并 `ezmk-workspace.toml`（文本级拼接保留注释与 options）、`ws` 简写、跳过规则（隐藏/嵌套根/逃逸） | ✅ 已完成（2026-08-30，dev.7：全量 988/5770 零回归，+20 用例/+158 断言） |
| [1.4.0-pre.1](1.4.0-pre.1.md) | 发布前收口 | 用户触达打磨（别名总表 / `--help` / README 速览）+ 全量文档检查（docs/README/tutorial/skill + i18n 三向）+ 缺陷收集与未实现项补全（dev 已知限制聚合裁定表）+ 1.4.0 聚合 changelog + API 稳定性承诺扩展 + 发布门槛预核对 | ✅ 收口完成（2026-08-30：全量 1003/5835 零回归，+15 用例/+65 断言；门槛 ①②③ 满足） |
| [1.4.1](1.4.1.md) | pkg install 支持 git 仓库 URL | `pkg install` 识别 git URL（`git@`/`git://`/`file://`/`.git`）→ 克隆 → ref 定位（`#<ref>`/`--branch`，分支/标签浅克隆、commit 全量）→ 复用目录安装链路 → lockfile 记录 `source="git"` + `commit` + `--locked` 校验 | ✅ 已发布（2026-09-03，tag `v1.4.1`；全量 1020/5970 零回归；三渠道：winget PR #428792 / homebrew tap / pacman PKGBUILD） |
| [1.4.2](1.4.2.md) | 代码质量审计修复（第二轮） | 六路并行审计 + 独立核查的修复落地（P0~P4 共 36 项）：Lua 报错对象 UB/权限/执行预算、`workspace watch` 线程模型、MSVC 依赖跟踪（stdout 流/本地化前缀）、`--locked` 哈希语义分离、watcher 死亡感知与目录补挂、Windows 窄 API/参数装配、repo 路径约束、import/CLI 语义修正、workspace 文件层健壮性等 | ✅ 已发布（2026-09-13，tag `v1.4.2`；阶段一~八逐 commit：全量 **1099/6342 零回归** vs 基线 1020/5970；首方代码零告警；i18n 405 键三向一致；F-37 低危项择优随附）；三渠道分发完成：Homebrew tap 1.4.2 / pacman 产物 / winget PR #434086 |
| [1.4.3](1.4.3.md) | man 手册 | 新增 man 4 页（`man/ezmk.1`、`man/ezmk-lua.1`、`man/ezmk.toml.5`、`man/ezmk-workspace.toml.5`，手写 roff，仅英文）；`scripts/check_man_sync.py` 防漂移闸门（man ↔ `src/cli.cpp` 双向比对 + 提取基线断言）+ groff 渲染 lint；三渠道分发（`install.sh` 加 `EZMK_NO_MAN` / PKGBUILD / Release 资产 + Homebrew `man1.install`/`man5.install`）；`ezmk help` 末尾 See also（i18n 405→406） | ✅ 已发布（2026-09-19，tag `v1.4.3`；阶段一~八逐 commit：全量 **1099/6348 零回归**（基线 6342 + 新键 6 条）；`check_man_sync.py` OK + groff 4 页零告警；i18n 406 键三向一致；三渠道分发完成：Release 7 资产含 `man/` 4 页（真实 digest 回填 homebrew/winget）/ pacman 本机 `makepkg -fd` 出包 / winget PR #437604 **已合并上线**） |
| [1.4.4](1.4.4.md) | 历史遗留清理 | ① `install.ps1 -DryRun` 缺陷修复（预览契约：退出码 0 + 零副作用 + 零网络）+ windows CI 冒烟 ② `test/` 7 条编译告警清零（"首方代码零告警"覆盖 `test/`）③ `.gitignore` 过期条目/去重/分组 ④ 过期 TODO（`cli.cpp:1144-1146`）改为不挂版本的已知限制 ⑤ `release.yml` 的 `macos-x64` job 默认跳过（`if: vars.ENABLE_MACOS_X64`）⑥ `scripts/check_docs_sync.sh` 接入 CI | ✅ 已发布（2026-09-26，tag `v1.4.4`；全量 **1099/6349 零回归**，4 跳过；首方代码零告警；i18n 406 键三向一致；`macos-x64` job skipped；三渠道：Homebrew / pacman / winget #441646）；设计见 [1.4.4.md](1.4.4.md)，阶段见根 [`plan.md`](../../plan.md) |
| [1.4.5](1.4.5.md) | 生成物格式统一（lockfile / 仓库注册表 JSON 化） | ① `ezmk.lock` → **`ezmk.lock.json`**：`nlohmann::json`（`dump(2)`）+ **原子写**；`list.toml` → **`list.json`**（全局/用户/项目三作用域）② **双读 + 自动迁移**：旧 TOML 继续可读，首次成功写入后写新删旧 ③ 缓存签名（`build.cpp` / `cache.cpp` / `pkg.cpp` 三处）改用 `lockfile::active_path()`——迁移当天恰一次全量重编 ④ `util::atomic_write_text()` 上提为公共 helper ⑤ API 纯增量（`lockfile::*` 签名不变；`repo_list_path()` / `legacy_repo_list_path()` 新增，`list_toml_path()` 保留）⑥ i18n **6 个迁移键**（406 → **412**）+ man 2 页 + docs **7 文件** + tutorial + README 中英 + skills **5 文件** ⑦ zsh 补全双分支（静态脚本不随升级更新）⑧ REMOVALS 登记 R-03/R-04 | ✅ 已发布（**2026-10-02，tag `v1.4.5`**；阶段一~六逐 commit + 阶段七发布；全量 **1113/6491 零失败**（基线 1099/6349，+14 用例/+142 断言，4 跳过）、`check_man_sync.py` OK + groff 零告警、i18n 412 键三向一致、docs-sync 通过；Release run `36993548272` success + 7 资产 digest 逐一核对、`macos-x64` skipped；三渠道：Homebrew tap `206a4e4` / pacman `makepkg -fd` 出包 / winget PR #445627；**同日回退过 tag**以修掉 man 源文件里的 UTF-8 BOM，digest 均已按重建后的产物重取）；设计见 [1.4.5.md](1.4.5.md)，阶段见根 [`plan.md`](../../plan.md) |
| [1.4.6](1.4.6.md) | 代码质量审计修复（第三轮） | Q-01~Q-32：内存/数据完整性（zip 二次 `fclose` / pkg 目录-git 安装事务回滚 / `atomic_rename` 删源）、编译正确性与确定性（响应文件转义 / 目录遍历排序 / 并行合并 / link-only profile）、归档-下载健壮性（tar 八进制-溢出-负状态 / `curl --fail` + URL 文件名净化 / gzip 头边界 / zip 原子写）、pkg-lockfile（取消三态 / verify 盲点 / `.new` 误删 / stoul 越界 / 写入可见性 / 名字校验）、watcher-workspace（macOS fd 双 close / `stop_on_error` / 写回边界）、导出-CLI-跨平台（`--precompiled` / `--report` 注入 / `detect_toolchain` 线程安全 / Windows 引用 / POSIX 重定向引号）；七阶段逐 commit + 低危随附 | ✅ 实现收口（未发布；见根 [`plan.md`](../../plan.md)） |
| [1.4.7](1.4.7.md) | MSVC 工具链支持修复 + 工具链优先级调整 | M-01~M-06：vswhere `-products *` + BuildTools 回退；vcvars `cmd /c call` 引用修复；`msvc_env()`/`apply_msvc_env()` 注入 compile/link/ar/test/pkg；优先级改为 g++/clang++ 优先、MSVC 回退；新增 `EZMK_TOOLCHAIN` 显式覆盖；工具链切换缓存失效 | ✅ 已发布（2026-10-03）（见根 [`plan.md`](../../plan.md)） |

### 依赖关系

```
1.3.6 (收口) ──→ 1.4.0-dev.1 (调试配置)
              ├──→ 1.4.0-dev.2 (能力表) ──→ 1.4.0-dev.3 (编译协商)
              │                                  └──→ 1.4.0-dev.4 (CMake 补全)
              ├──→ 1.4.0-dev.5 (功能收口, 独立)
              ├──→ 1.4.0-dev.6 (代码质量审计, 独立)
              └──→ 1.4.0-dev.7 (workspace scan, 独立)
1.4.0-pre.1 (收口) ──→ 1.4.0 (正式发布, 2026-08-30) ──→ 1.4.1 (git URL 安装, 已发布 2026-09-03) ──→ 1.4.2 (代码质量审计修复第二轮, 已发布 2026-09-13 / tag `v1.4.2`) ──→ 1.4.3 (man 手册 4 页, 已发布 2026-09-19 / tag `v1.4.3`) ──→ 1.4.4 (历史遗留清理, 已发布 2026-09-26 / tag `v1.4.4`) ──→ 1.4.5 (生成物格式统一, 已发布 2026-10-02 / tag `v1.4.5`) ──→ 1.4.6 (代码质量审计修复第三轮, 已发布 2026-10-03) ──→ 1.4.7 (MSVC 工具链支持修复 + 优先级, 已发布 2026-10-03)
```

- dev.1 / dev.5 / dev.6 / dev.7 与语言标准主线完全独立，可并行。
- dev.2 → dev.3 顺序依赖（协商需能力表 cap）；dev.4 仅导出确认依赖 dev.2（未就绪时跳过）。
- dev.7 依赖 dev.6 收口后重新打开 dev 阶段（用户确认）；**pre.1 依赖全部 dev 完成**（已完成，接 1.4.0 正式版聚合发布）。
- **1.4.1 为 1.4.0 发布后的补丁版本**（对照 1.3.x 补丁惯例），复用 1.4.0 的 `install_from_directory` 目录安装链路与 repo 子系统 git helper；与 1.4.0 各 dev 无顺序依赖。dev 阶段 2026-09-01 完成（全量 1020/5970 零回归），**2026-09-03 正式发布（tag `v1.4.1`）**。
- **1.4.2 为 1.4.1 发布后的代码质量审计补丁版本**（对照 dev.6 / 1.3.6 审计先例），修复面跨 Lua/workspace/build/pkg/repo/CLI/Windows 六域（见 [1.4.2.md](1.4.2.md) §2）；零功能新增、公共 API 无破坏性变更；与 1.4.1 无顺序依赖（独立补丁）。**✅ 已发布（2026-09-13，tag `v1.4.2`）**：阶段一~八逐 commit 落地 + 每阶段全量回归，最终 **1099 用例 / 6342 断言零失败**（基线 1020/5970）；三渠道分发完成：Homebrew tap `3667808244/homebrew-eazymake` 已更新至 1.4.2、pacman `publish/arch/PKGBUILD` 本机 `makepkg -fd` 出包验证、winget `microsoft/winget-pkgs#434086`（**已合并上线**）。
- **1.4.3 为 1.4.2 发布后的文档/分发补丁版本**：新增 man 手册 4 页（`ezmk(1)` / `ezmk-lua(1)` / `ezmk.toml(5)` / `ezmk-workspace.toml(5)`，手写 roff、仅英文）+ 构建期防漂移校验（`scripts/check_man_sync.py` + groff 渲染 lint，纳入 CI）+ 三渠道分发（`install.sh` 加 `EZMK_NO_MAN`、PKGBUILD、Release 资产 + Homebrew formula）；零 CLI 行为变更（唯一改动为 `ezmk help` 末尾 See also 一行，i18n 405→406）；与 1.4.2 无顺序依赖。**✅ 已发布（2026-09-19，tag `v1.4.3`）**：阶段一~八逐 commit 落地，最终 **1099 用例 / 6348 断言零失败**（基线 6342 + 新 i18n 键 6 条）。
- **1.4.4 为 1.4.3 发布后的历史遗留清理补丁**：范围来自全仓遗留扫描中"不需要新功能即可收口"的项——用户可见缺陷（`install.ps1 -DryRun`）、质量口径（`test/` 7 条编译告警）、仓库卫生（`.gitignore`）、过期承诺（`cli.cpp` TODO）、流程僵尸与未接线检查（`macos-x64` job、`check_docs_sync` 入 CI）；**零功能新增、零 CLI/配置语义变更、不放宽也不移除任何弃用面**（弃用面归 [2.0.x/REMOVALS.md](../2.0.x/REMOVALS.md)）；与 1.4.3 无顺序依赖。**✅ 已发布（2026-09-26，tag `v1.4.4`）**：阶段一~六逐 commit 落地，全量 **1099/6349 零回归**（4 跳过，环境相关条件断言），首方代码零告警、i18n 406 键三向一致；三渠道分发完成：Homebrew tap 1.4.4（commit `1128c8b`）/ pacman `makepkg -fd` 出包 / winget PR `microsoft/winget-pkgs#441646`（CI/版主审批为发布后跟进项）。
- **1.4.5 为 1.4.4 发布后的生成物格式统一补丁**（**✅ 已发布（2026-10-02，tag `v1.4.5`）**）：把两个「ezmk 全权生成、用户不该手改」的文件从 TOML 改为 JSON——`ezmk.lock` → `ezmk.lock.json`、`list.toml` → `list.json`（全局/用户/项目三作用域），并顺带升级为**原子写**（原本是仓储里少数非原子写的生成物）。**旧格式继续可读**（`ezmk.lock.json` 优先 → 旧 `ezmk.lock` TOML 回退 → 首次成功写入后写新删旧），因此升级无感；唯一真实破坏面是**降级**（≤1.4.4 读不到 `*.json`：`deterministic = true` 致命），已在设计 §5、`CHANGES.md` 与文档里写死。**公共 API 纯增量**（`lockfile::*` 签名不变；`repo::repo_list_path()` / `legacy_repo_list_path()` 新增，`list_toml_path()` 保留为别名）；不碰任何既有弃用面与 lockfile 字段语义（`sha256` 别名继续双写、`version` 仍 1）。本版**新增**两条兼容回退，已在 [`2.0.x/REMOVALS.md`](../2.0.x/REMOVALS.md) 登记 R-03/R-04 供 2.0.0 消费。与 1.4.4 无顺序依赖。**发布实测**：阶段一~六逐 commit + 版本定稿 `e29ae82` + **BOM 修复 `65c7af3`（tag 同日回退后重建，tag 对象 `68e214d` 指向 `65c7af3`）**，全量 **1113 用例 / 6491 断言零失败**（基线 1099/6349 → +14 用例 / +142 断言，4 跳过），`check_man_sync.py` 通过 + groff 4 页零告警、i18n **412** 键三向一致、docs-sync 通过；关键用例为**双格式对拍**（同值 TOML/JSON 逐字段相等）与**确定性签名跟随 `active_path()`**。Release run `36997151873` success（`macos-x64` skipped），7 资产 digest 与 `assets[].digest` 逐一一致（tar 内 man 无 BOM、渲染首行即标题）；三渠道（回退后重做）：Homebrew tap `206a4e4` / pacman `makepkg -fd` 出包 `eazymake-1.4.5-1-x86_64.pkg.tar.zst`（sha256 `fb4980eb…`）/ winget PR [#445627](https://github.com/microsoft/winget-pkgs/pull/445627)（`InstallerSha256` 已更新；长跑 check 与版主审批为发布后跟进项）。设计见 [1.4.5.md](1.4.5.md)，阶段见根 [`plan.md`](../../plan.md)。

- **1.4.6 为 1.4.5 发布后的代码质量审计修复补丁（第三轮）**（✅ 已发布 2026-10-03）：承接 1.4.0-dev.6（第一轮）与 1.4.2（第二轮）的同一主题，修错误路径与不可信输入边界；零功能新增、公共 API 无破坏性变更；与 1.4.5 无顺序依赖。设计见 [1.4.6.md](1.4.6.md)，阶段见根 [`plan.md`](../../plan.md)。

- **1.4.7 为 MSVC 工具链支持修复 + 优先级调整补丁**（✅ 已发布（2026-10-03））：承接 1.4.6 Q-20 的真实 MSVC 验证——修 Build Tools 探测、vcvars cmd 引用、MSVC 环境注入，并把优先级改为 g++/clang++ 优先、MSVC 回退；新增 `EZMK_TOOLCHAIN` 显式覆盖用于验证。设计见 [1.4.7.md](1.4.7.md)，阶段见根 [`plan.md`](../../plan.md)。

## 跨版本关注点

- **1.3.1 语义 A 为基线**：dev.2/dev.3 扩展而非替换区间语法语义；上界/元数据语义不变。
- **1.3.6 重构收益**：`run_executable`（watch 透传通道）、`run_member`（workspace watch 模型）、`TestRunContext`（测试基础设施）为本系列复用。
- **1.3.6 延后重构项随主线穿插**：build.cpp/pkg.cpp 全面重构、Catch2 结构化解析（报告语义化）——不单列 dev，随相关 dev 一并评估。
- **cli.cpp 命令组拆文件**（`parse_*` 1272 行单文件）：2.0.0 前评估。
- **回归基线**：全量 1020 用例 / 5970 断言（1.4.1 发布态实测）；1.4.2 开发完成后为 1099/6342，新增功能不得引入回归；**1.4.3 实测 1099 用例 / 6348 断言**（基线 6342 + 新键 6 条；纯文档/分发，不新增用例数），附加门槛为 `check_man_sync.py` + groff 渲染 lint 通过、i18n **406** 键三向一致；**1.4.4 实测 1099 用例 / 6349 断言零回归**（文档基线 6348；把 `test/` 回退到 1.4.3 状态在本机复跑同为 6349，±1 来自运行环境的条件断言，本版用例/断言数均未改变），附加门槛为 docs-sync CI 步 + `check_man_sync.py` + groff 渲染 lint 通过、i18n **406** 键三向一致；**1.4.5 基线 = 1099/6349**，本版**新增用例**（双格式对拍 / 迁移 / 缓存"仅重命名恰一次全量重编" / F-24 双路径 / 路径断言，约 +15 用例），**只增不减**，实测数字在发布时回填；**1.4.6 基线 = 1.4.5 发布态 1113/6491**，新增用例只增不减，发布时回填。
- **i18n**：各 dev 新增 key 三向一致 + `check_i18n.py` 通过；1.4.4 键数保持 **406** 不变；**1.4.5 实测 406 → 412**（6 个迁移键：`lock_legacy_detected` / `lock_migrated` / `lock_legacy_stale` / `repo_list_legacy_detected` / `repo_list_migrated` / `repo_list_legacy_stale`；`zh-TW` 变体只译差异键，其**既有** lock_* 消息同步改名；§3.8 的两个可选 i18n 化键本版不做）。
- **与 2.0.0 解耦**：本系列纯增量，不依赖任何 deprecation 到期；2.0.0 保持破坏性变更窗口——**1.4.x（含 1.4.4）不放宽也不移除任何弃用面**，移除清单见 [2.0.x/REMOVALS.md](../2.0.x/REMOVALS.md)。**1.4.5 的方向相反**：它**新增**两条兼容回退（旧 lockfile / 旧注册表格式读取、`list_toml_path()` 别名），并已在同一清单登记 **R-03/R-04** 供 2.0.0 移除。
