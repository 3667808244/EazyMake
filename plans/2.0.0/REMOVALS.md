# EazyMake 2.0.0 — 弃用面移除清单（REMOVALS）

> **状态：📝 草拟（2026-09-19）**——本文件**不是** 2.0.0 的完整计划，而是**移除清单 + 决策台**：1.x 期间所有"已对外宣布移除 / 已退化为兼容垫片 / 已静默无效"的接口在此登记，供 2.0.0 计划逐条消费。
>
> **适用窗口**：破坏性变更仅在 `2.0.0` 引入（`CHANGES.md` §API Stability 的既有承诺）。1.4.x（含 1.4.4）**不放宽也不移除**其中任何一项。
>
> **清单可信度**：每条都带"现状证据"（文件:行）与"联动面"，均来自 1.4.3 发布后的**全量遗留扫描**（代码弃用面 / 兼容垫片 / 静默无效配置三类）；`src/vendor/`（Lua、miniz、Catch2）不在范围内（第三方，含其上游 TODO，按 `CLAUDE.md` 不改）。

## 1 单条移除的执行清单（模板）

任何一条 `R-xx` / `D-xx` 落地时，逐项确认：

| 步 | 动作 | 验证 |
|----|------|------|
| 1 | **代码**：删实现/入口/回退分支；旧写法保留**显式迁移报错**（不静默失效） | 负向用例（旧写法 → 明确文案 + 非零退出码） |
| 2 | **i18n**：删 key（`include/ezmk/i18n_keys.def` + `locale/en.json` + `locale/zh.json`；`zh-TW` 是变体、删基础键即可） | `python scripts/check_i18n.py` 三向一致（键数按实际变化） |
| 3 | **man**：删/改相关段落；同步 `scripts/check_man_sync.py` 的**页面清单 / 白名单 / BASELINE** | `python scripts/check_man_sync.py` 通过 + `groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警 |
| 4 | **docs / 教程 / skill**：`docs/{en,zh}/*`、`tutorial/{en,zh}/*`、`README*.md`、`.claude/skills/*`（尤其 `ezmk-user-*`） | `bash scripts/check_docs_sync.sh`（en↔zh 文件集合）+ 人工核对措辞 |
| 5 | **测试与夹具**：删/改用例、清仓库内夹具 | 全量回归零失败；断言基线按实际变化更新 |
| 6 | **CHANGES**：2.0.0 条目给出**逐条迁移指南**（旧 → 新）；必要时更新 §API Stability 段 | 迁移指南与 `docs/` 措辞一致 |

## 2 确定移除（P0，已对外承诺）

### R-01 `[test].flags`（配置键）

| 项 | 内容 |
|----|------|
| 现状证据 | 读取：`src/config.cpp:842`（`cfg.test.flags = extract_string_array(test->get("flags"), …)`）；使用点弃用告警：`src/build.cpp:2502-2503`（`test_flags_deprecated`）；消费：`src/build.cpp:2631`；模板注释刻意不展示：`src/config.cpp:1030-1032` |
| 承诺 | `CHANGES.md` 1.2.0：dev.12 弃用、**2.0.0 移除**；`docs/{en,zh}/config_file.md`（en:484 / zh:413）、`tutorial/{en,zh}/dev/03-test.md`、`man/ezmk.toml.5:318-319`（"Array of strings, deprecated. Use …"）均已标注 |
| 移除内容 | 字段读取 + 使用点告警 + i18n key `test_flags_deprecated`（def/en/zh；zh-TW 同步删）+ 各文档字段行 + man 字段条目 |
| 迁移写法 | `[test].default_profile` + `[compile.profile.<name>]`；测试专属 include/链接改用 `[test].include_dirs` / `[test].link_targets` |
| 联动 | `check_man_sync.py`：`config_keys` 基线 **55 → 54**；`ezmk.toml.5` 的 `[test]` 字段表删 `flags` 行 |
| 风险 | 现配置解析**不拒绝未知键**（`ezmk.toml` 侧无 unknown-key 报错路径；只有 `[workspace]` 有 `workspace_err_unknown_key`）→ 若不新增迁移报错，老项目的 `[test].flags` 会被**静默忽略**。见 **D-09** |

### R-02 `ezmk utils cc`（CLI 入口 + 官方包内工具）

| 项 | 内容 |
|----|------|
| 现状证据 | 分流：`src/main.cpp:104`（注释）、`:127-130`（弃用告警后转调 `ezmk project cc`）；i18n `utils_cc_deprecated`（def:60 / en:168 / zh:168 / zh-TW:157）；官方包内 `ezmk-official-utils/utils/cc.lua` + 其 `README.md`（1.2.0 起标 `@deprecated`） |
| 承诺 | `CHANGES.md` 1.2.0：「`ezmk utils cc` 自本版起**弃用**（保留可用并提示转用 `ezmk project cc`，**2.0.0 移除**）」；`docs/en/cli.md:455`、`docs/zh/cli.md:377`、`docs/{en,zh}/utils.md`（en:7-11,71 / zh:7,67）、`docs/{en,zh}/glossary.md:55`、`docs/{en,zh}/config_file.md`（en:133 / zh:102）、`tutorial/{en,zh}/dev/02-utils.md`、`README_ZH.md:166` 均已标注 |
| 移除内容 | `Command::Utils` 的 `cc` 分支（含转调逻辑）+ i18n key `utils_cc_deprecated`；官方包内 `cc.lua`（包版本 → 2.0.0，保留 `link.lua` / `gen-build-package.lua`） |
| 迁移写法 | `ezmk project cc [-o <path>] [--profile <name>]`（内置命令，1.2.0 起即为正式入口） |
| 联动 | **仓库内夹具** `pkg/ezmk-cc/`（被跟踪的 2 个文件：`ezmk.toml` + `utils/cc.lua`）与 `test/test_integration.cpp:256-302` 的使用例；`test/test_utils_perms.cpp:23` 仅把它当**路径字符串**（无 I/O，无需改，但可顺手更名）；`ezmk-official-utils` 打包与分发材料（若提及） |
| 风险 | 夹具删除后，`ezmk utils <name>` 的"开发回退查找"（`<cwd>/pkg/*/utils/<name>.lua`）将无仓内样例 → 若该查找逻辑仍需测试，改用新的中立夹具 |

## 3 需在 2.0.0 计划中拍板的决策项

> 每项给"选项 + 倾向"，**结论必须在 2.0.0 设计文档里落定**再开工。

| # | 项 | 现状证据 | 选项 | 倾向 |
|---|----|----------|------|------|
| D-01 | lockfile `sha256` 旧别名（`lib_sha256` 的别名，1.4.2 起双写） | `src/lockfile.cpp:68,135`；`src/pkg.cpp:1618`；`docs/{en,zh}/config_file.md:290-292 / 244-246` | a) **写出停用、读取保留**（lockfile `version` 仍 1）<br>b) 读写全删 + `version` → 2（彻底） | **a**：读取兼容成本低，且 `--locked` 场景可能仍在消费旧文件；`version` 升 2 留到"字段结构真变"时 |
| D-02 | `[compile].include_dir`（单数旧键） | 回退读取：`src/config.cpp:594`；`man/ezmk.toml.5:176`（"old singular form"） | a) 删回退（旧项目需改键名）<br>b) 保留 | **a**（与 D-09 的 fail-fast 一起，给出明确迁移报错） |
| D-03 | `-V` 兼容别名 | `src/cli.cpp:584`（"`-V` kept for backward compatibility"） | a) 删（只留 `-v` / `--version`）<br>b) 保留 | **a**：CLI 面越小越好，且 `-V` 与 `-v` 大小写易误触 |
| D-04 | `util::run_executable(int)` 旧重载 | `src/util.cpp:1364`（"int overload (backward compat) forwards to the RunOptions version"）；位于 **public header** `include/ezmk/util.hpp` | a) 删（属公共 API 破坏，正好落在 2.0.0）<br>b) 保留 | **a**：内部无调用方即删；2.0.0 是唯一合法窗口 |
| D-05 | `[utils.permissions]` 缺省 = **不限制**（legacy 模型） | `src/lua_api.cpp:55,240,265,280`（`nullopt → Allow`）；一次性弃用告警 `:577` | a) 缺省改为"**拒绝**并提示声明权限"（安全优先）<br>b) 保留缺省放行 + 告警 | **a**（但需评估存量 utils 包兼容性）；若选 b，则把告警文案升级为"2.0.0 起缺省拒绝" |
| D-06 | 旧式 **shell** 安装钩子 | `src/pkg.cpp:388`（找 `.sh`）、`:444`（打开编辑器审查） | a) 仅保留 Lua 钩子（沙箱 + 无编辑器）<br>b) 保留 shell（编辑器审查为唯一防线） | **b 或 a-变体**：倾向"保留但要求显式 opt-in（如 `[hooks].allow_shell = true`）"，把默认面收敛到 Lua |
| D-07 | `[install].sharedir`（**解析了但无消费点**） | `src/config.cpp:807-814`（解析 + 默认 `share`）；全仓无消费者；man `ezmk.toml.5` 已标"保留/未实现" | a) 实现（把 `share/` 纳入 install 动作）<br>b) 移除（字段 + man/docs 段落） | 二选一都行，但**必须在本版定性**：留着"可写不生效"是最坏状态（静默无效） |
| D-08 | repo `[platform]` 缺省 = 全平台 | `src/repo.cpp:297,327,337` | a) 保留<br>b) 改为必填 | **a**（向后兼容成本低，且仓库格式已在生态中固化） |
| D-09 | **`ezmk.toml` 未知键策略**（R-01/D-02 的迁移体验） | 现状：`ezmk.toml` 解析**不报未知键**（仅 `[workspace]` 有 `workspace_err_unknown_key`） | a) 2.0.0 起对**已移除键**做 fail-fast（专属文案 + 迁移提示）<br>b) 对**所有**未知键 fail-fast<br>c) 维持静默忽略 | **a**：与 1.4.3「把静默失效变成红灯」的取向一致，且不误伤用户自定义注释/未来键；b 可作为 2.x 演进目标 |

## 4 建议保留（登记理由，避免反复讨论）

| 项 | 证据 | 保留理由 |
|----|------|----------|
| `pack --precompiled` | `src/build.cpp:1707,1743` | 1.2.5 起默认包格式改为源码包，`--precompiled` 只是**显式**入口；删除无收益且会伤害既有脚本 |
| repo `[platform]` 缺省 | `src/repo.cpp:297,327,337` | 见 D-08 |
| 历史溯源注释（`// 1.2.0-dev.11: …`、`// 1.4.2: …`） | `src/` 多处 | 这是**变更溯源**，不是"挂版本的承诺"；与 1.4.4 清理的过期 TODO 口径不同（1.4.4 §3.4） |
| `src/vendor/`（Lua / miniz / Catch2） | 目录 | 第三方，`CLAUDE.md` 明确不改；上游 TODO 不属本仓债 |
| `locale/zh-TW.json` 的"只译差异"形态（374/406 键） | `locale/zh-TW.json` | 变体继承是既定设计（缺键回落基础语言）；若要补齐属**增强**而非移除（1.4.4 §3.8 已裁定延后） |

## 5 联动清单（删任何一项都必须同步的位置）

| 面 | 文件 / 工具 | 备注 |
|----|-------------|------|
| i18n | `include/ezmk/i18n_keys.def`、`locale/en.json`、`locale/zh.json`（`zh-TW` 变体无需改） | `python scripts/check_i18n.py` 会强制三向一致 |
| man | `man/ezmk.1`、`man/ezmk.toml.5`（+ 其余两页若提及）、`scripts/check_man_sync.py` | 脚本内 `MAN_PAGES` / `EXTRA_LONG_OK` / `EXTRA_SHORT_OK` / `ENV_*` / `CONFIG_KEY_EXTRA_OK` / `WORKSPACE_KEY_EXTRA_OK` / **BASELINE** 均需按新规模更新；`groff -z -ww` 必须零告警 |
| docs | `docs/{en,zh}/{cli,config_file,utils,glossary,pkg,…}.md` | en/zh 必须同名同集合（`check_docs_sync.sh`） |
| 教程 | `tutorial/{en,zh}/**` | 同上 |
| 顶层文档 | `README.md`、`README_ZH.md`、`CHANGES.md`、`CLAUDE.md` | README 速览与 CLAUDE 的"快速参考"若列旧写法需改 |
| skills | `.claude/skills/*`（尤其 `ezmk-codebase` 的 CLI/配置面、`ezmk-user-*`） | 与 `docs/` 同步；`CLAUDE.md` 的 skill 表若受影响也改 |
| 测试 | `test/*.cpp`、仓库内夹具 `pkg/ezmk-cc/` | 断言基线随实际变化更新，负向用例必须新增 |
| 分发材料 | `publish/**`、`install.sh` / `install.ps1`（若提及旧写法）、Release notes 模板 | wings/pacman/homebrew 一般不涉及，但 release notes 需含迁移指南链接 |

## 6 验收清单（2.0.0 发布前逐条勾）

- [ ] 被删符号在仓库内**零残留**（建议 grep 模式：`utils cc`、`test_flags_deprecated`、`cfg.test.flags`、`include_dir`（单数）、`run_executable(int)`、`sharedir`、`"sha256"`（lockfile 别名，视 D-01 结论））
- [ ] `python scripts/check_i18n.py` 通过（键数变化已在脚本输出中体现）
- [ ] `python scripts/check_man_sync.py` 通过（**BASELINE 已按新规模更新**，非"把脚本改松"）
- [ ] `groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警
- [ ] `bash scripts/check_docs_sync.sh` 通过（en↔zh 集合一致）
- [ ] 全量回归零失败，且**负向用例**覆盖：旧写法 → 明确报错/迁移提示（非静默失效）
- [ ] `CHANGES.md` 2.0.0 条目含**逐条迁移指南**；§API Stability 段更新（哪些承诺已兑现在本版）
- [ ] `docs/{en,zh}` 与 `tutorial/{en,zh}` 除"弃用 → 已移除"对照表外无旧写法残留
- [ ] 三渠道分发同 1.x（Homebrew digest 回填 / pacman `makepkg -fd` / winget PR），Windows 资产不含 man

## 7 与 1.4.x 的边界（硬约束）

- 1.4.x（含 1.4.4）**不放宽、不移除**任何弃用面，也不删除本节登记的垫片。
- **联动红灯是设计意图**：删 `[test].flags` / `ezmk utils cc` 会让 `check_man_sync.py` 与 `check_i18n.py` 立刻失败——2.0.0 计划必须把"同步 man / i18n / 测试夹具 / 文档"写成显式交付项，而不是事后补。
- 若 2.0.0 采用 D-09(a)（移除键 fail-fast），1.4.x 期间**不要**提前引入该报错（否则等于提前破坏 1.x 兼容）。

## 8 影响面速览（用户视角）

| 接口 | 现状（1.4.x） | 2.0.0 后 | 迁移写法 |
|------|---------------|----------|----------|
| `[test].flags` | 生效但打弃用警告 | 移除（按 D-09 报错或忽略） | `[test].default_profile` + `[compile.profile.<name>]`；include/link 用 `[test].include_dirs` / `link_targets` |
| `ezmk utils cc` | 打弃用警告后转调 | 移除 | `ezmk project cc [-o <path>] [--profile <name>]` |
| lockfile `sha256` | 双写（别名） | 按 D-01（倾向：停止写出、读取保留） | 读 `lib_sha256`（或 `archive_sha256` for `--locked`） |
| `[compile].include_dir`（单数） | 回退读取 | 按 D-02（倾向移除） | `[compile].include_dirs`（数组） |
| `-V` | 等价 `--version` | 按 D-03（倾向移除） | `--version` 或 `-v`（verbose 需注意语义） |
| `util::run_executable(int)` | 转发到 `RunOptions` 版本 | 按 D-04（倾向移除） | `run_executable(cmd, RunOptions{…})` |
| `[utils.permissions]` 缺省 | 不限制（有一次性告警） | 按 D-05（倾向改为缺省拒绝） | 显式声明 `read`/`write`/`run` |
| 旧式 shell 钩子 | 打开编辑器审查后执行 | 按 D-06（倾向显式 opt-in） | 迁移为 Lua 钩子（沙箱内 `ezmk.*` API） |
| `[install].sharedir` | 可写但**不生效** | 按 D-07：实现 或 移除 | 取决于结论 |
