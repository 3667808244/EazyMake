# EazyMake 1.4.3 执行计划

> **状态：📝 计划就绪（未开工）**——本文档把设计文档 §4 转成可勾选的八阶段清单；索引（[`plans/1.4.x/README.md`](plans/1.4.x/README.md)、[`plans/README.md`](plans/README.md)）已同步就位（2026-09-19）。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.3.md**](plans/1.4.x/1.4.3.md)。为 `ezmk` 提供离线、符合 Unix 惯例的 man 手册（`ezmk(1)` + `ezmk.toml(5)`），打通三渠道分发，并用**构建期防漂移校验**保证 man 不与 `src/cli.cpp` 脱节。
>
> **范围边界**：**零 CLI 行为变更、零功能新增**（唯一 CLI 相关改动是 `ezmk help` 末尾追加一行 See also 文案）。仅英文 man（不做 man i18n）。不做 Windows 手册、`ezmk man` 子命令、mdoc 迁移、把 docs 全文搬进 man（设计 §3.11）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（本版无 API 变更）；③ 全量测试零回归（基线 **1099 用例 / 6342 断言**，1.4.2 发布态）；④ **附加门槛**：`python scripts/check_man_sync.py` 通过、`groff`/`mandoc` 渲染 lint 零告警、i18n 三向一致（405 → **406** 键）。
>
> **版本决策**：dev 阶段二进制版本号**保持 1.4.2**（1.2.x/1.3.x/1.4.1/1.4.2 补丁先例均不提前 bump）；正式发布 commit 按 workflow §3 置 1.4.3（`build.sh` fallback + `include/ezmk/version.hpp`），tag `v1.4.3`。

---

## 1 背景

- 离线参考目前只有 `ezmk --help`（须运行二进制、单页、受 `EZMK_LANG` 影响）与仓库/在线 docs；Unix 用户预期的 `man ezmk` 完全落空——仓库**无任何 man 基础设施**。
- 三渠道安装补全 `res/ezmk.zsh` 的链路已成熟（`install.sh` / `publish/arch/PKGBUILD` / `publish/homebrew/ezmk.rb`），man 可复用；但三者构建方式不同：`install.sh` 与 PKGBUILD **从源码 tag 构建**（可取仓库内 `man/`），Homebrew **从 Release 资产安装**（man 必须进 tar.gz，且会改变 digest），Windows 无 `man`。
- 1.4.2 的文档对齐修正暴露了本仓 CLI 文档反复漂移的事实（`--pug`/`--type`/`--no-data`/`pack --output`/`workspace watch -j` 等）——因此**防漂移闸门与 man 正文同为 P0 交付物**（详见设计 §1/§3.5）。

## 2 目标

| 组 | 优先级 | 覆盖 |
|----|--------|------|
| man 内容（M-01/M-02） | P0 | `man/ezmk.1`（12 节）、`man/ezmk.toml.5`（9 节字段表） |
| 防漂移（M-03/M-04） | P0/P1 | `scripts/check_man_sync.py` 双向比对 + 提取基线断言；groff/mandoc 渲染 lint + `\-` 转义检查 |
| 分发（M-05~M-07） | P0 | `install.sh`（`$PREFIX/share/man/man1\|man5` + `EZMK_NO_MAN`）、PKGBUILD（`/usr/share/man/...`）、Release 资产（linux/macOS）+ Homebrew `man1.install` |
| 触达（M-08） | P1 | `ezmk help` 末尾 See also（`help_see_also_man`，405→406 键） |
| CI（M-09） | P0 | 渲染 lint + 漂移闸门 + 分发断言（对照既有 `zsh-completions` job 的"断言 + 局部模拟"策略） |
| 文档（M-10） | P1 | README×2 / `docs/{en,zh}/cli.md` / `docs/{en,zh}/technical.md` / CONTRIBUTING / CHANGES.md 1.4.3 |
| 随附（M-11） | P2 | `man/ezmk-workspace.toml.5`、`man/ezmk-lua.1`（择优随附，余者延后） |

## 3 执行阶段（每阶段一个 commit，阶段间 `bash build.sh test-all` 全量回归）

### 阶段一：`man/` 与 `ezmk(1)`（M-01，对应设计 §3.1~§3.3）

- [x] 新建顶层 `man/`；按设计 §3.2 约定（传统 man macros、`.TH`、`\-` 转义、纯 ASCII）写 `man/ezmk.1` 全 12 节（NAME/SYNOPSIS/DESCRIPTION/COMMANDS/OPTIONS/SCOPE FLAGS/SHORTHANDS/OPTION SYNTAX/ENVIRONMENT/EXIT STATUS/FILES/SEE ALSO；802 行、118 个 `.TP`、21 个 `.SS`）
- [x] 内容逐条对照 `docs/en/cli.md` 与 `src/cli.cpp` 的 spec/`kAliases`：命令表、选项组、scope flags 默认值、28 简写、GNU 语法、环境变量、exit status、FILES、SEE ALSO
- [x] 本地静态自检：结构 lint 0 异常（含"`.` 开头非合法请求"检查）、未转义连字符 0、非 ASCII 字节 0、`.nf`/`.fi` 与 `.ft` 配对、行尾纯 LF；**选项清单对照 `cli.cpp` 权威集：37 个长选项 0 缺失**，多出者仅白名单项（`--color` 全局 / `--flag` 占位符 / `--version` 顶层；`-s` 为文中"不传给 Catch2"的提及）
- [x] **渲染自检（本机已可执行）**：MSYS2 安装 `pacman -S --needed groff man-db`（`groff 1.23.0-3` / `man-db 2.13.1-2`）后，`groff -man -Tutf8 -z -ww man/ezmk.1` **零告警**、`man --warnings -l man/ezmk.1` 通过；渲染目检修掉两处真实排版缺陷（COMMANDS 的 `[options]` 掉进描述列 → 标签改为「命令 + 参数」单行、占位符用 `.BI` 斜体；自动断词把 "temporary" 断成 "tempo-rary" → 加 `.nh`）。已知无害项：`-Tascii`/不带 `-z` 时 grotty 1.23 会报一条 `unrecognized X command 'sgr 0'`，**系统自带 man 页同样如此**（设备级怪癖，与本文无关）
- [x] 本机工具链踩坑记录（写入设计 §3.6/§3.10）：man 源**必须是 LF**——用 PowerShell `Set-Content` 重写会引入 CRLF，troff 会对每一行报 `invalid input character code 13`

### 阶段二：`ezmk.toml(5)`（M-02，对应设计 §3.4）

- [x] 写 `man/ezmk.toml.5`：13 个小节字段表（`[project]`/`[compile]`/`[compile.macros]`/`[compile.profile.<name>]`/`[link]`/`[link.profile.<name>]`/`[depends]`/`[test]`/`[hooks]`/`[install]`/`[utils]`/`[utils.permissions]`/`[pkg]`）+ `VERSION CONSTRAINTS` + `DETERMINISTIC BUILDS` + `EXAMPLE`；517 行
- [x] 逐条对照 `docs/en/config_file.md` 与 `src/config.cpp`；标注 `system_target` 单数、`[link.profile.*]` 前置条件、`sharedir` 未实现、`[test].flags` 已弃用；并补上设计清单未列的真实键：`[utils.permissions]` 的 `read_deny`/`write_deny`/`run_deny`、`network`（声明式未强制）、legacy `include_dir`、`precompiled_strict`/`header_only`/`stdlib`
- [x] 自检：`groff -man -Tutf8 -z -ww` 零告警；**配置键覆盖对照：`config.cpp` 读取的 55 个键 0 缺失**（多出者仅为散文词）
- [x] 渲染目检并修掉四类真实缺陷（已回写设计 §3.6 检查清单与 §3.10 坑 13）：裸 `^`/`~` → `\(ha`/`\(ti`（否则 utf8 下变 ˆ/˜）；宏参数 `\\` → `\e`（否则 `%LOCALAPPDATA%\ezmk\pkg` 渲染成 `\zmkkg`）；宏参数裸 `"` → `\(dq`（否则引号被吞）；SEE ALSO 的超长 URL 触发 `cannot break line` → 改为仓库 URL + 树内路径

### 阶段三：防漂移校验脚本（M-03，对应设计 §3.5）

- [x] 实现 `scripts/check_man_sync.py`（零依赖 Python 3）：从 `src/cli.cpp` 提取 OptionSpec 长/短选项、`kAliases` 简写与顶层别名、`print_help()` 命令行，从 `src/*.cpp` 提取 `getenv`，从 `src/config.cpp` 提取配置键；与两份 man（先做 roff 反转义）双向比对
- [x] 双向差集断言 + 白名单（`--color`/`--help`/`--version`/`--flag` 占位符、`-s` 他方工具提及）+ **提取基线断言**（长选项 37 / 短选项 12 / 简写 28 / 顶层别名 7 / 命令 32 / 环境变量 14 / 配置键 55，低于基线即失败并提示更新提取器）；环境变量还做"三分法"（文档化 / `ENV_INTERNAL` 归类 / 未归类即失败），避免新增变量静默漏文档
- [x] 跑通：`python scripts/check_man_sync.py` → **OK**（0 漂移）；用法写入 CONTRIBUTING（新增「Man pages」小节 + pre-commit 清单一项）
- [x] **对抗测试（沙箱副本，6 例全部按预期失败）**：man 删选项 → 抓；cli.cpp 加选项 → 抓；man 写不存在的选项 → 抓；重命名 `kAliases` → 提取失败（exit 1）；man 删配置键 → 抓；代码改用未归类环境变量 → 双向抓（原变量"文档有代码无" + 新变量"未归类"）

### 阶段四：渲染 lint 与 CI 闸门（M-04/M-09，对应设计 §3.6）

- [x] `ci.yml` ubuntu job 增装 `groff man-db`，新增 `Man pages: static lint`（groff `-z -ww` + 6 条字符陷阱 grep：CRLF / 未转义连字符 / 裸 `^`~` / 宏参数 `\\` / `\"` 注释转义 / 标签内裸引号）与 `Man pages: CLI drift gate`（`python3 scripts/check_man_sync.py`）两步，插在依赖安装之后、构建之前（快速失败）
- [x] 新增 `man-pages` job（`man pages (1.4.3)`）：装 groff+man-db，两页 `man --warnings -l` 渲染（仅放行 grotty 设备级 `sgr 0` 一条），并断言 `NAME`/`SECTIONS` 节存在
- [x] 本地 dry-run：static lint 7 项全 OK（**并借此发现 `ezmk.toml.5` 4 处真实未转义连字符**：`ezmk-workspace.toml`、标识符正则、示例里 `-Wall`/`-g` → 已修）；`man-pages` job 渲染 dry-run 通过（538/364 行）
- [ ] **分发断言（延后到阶段五）**：`install.sh`（man 块 + `EZMK_NO_MAN`）、PKGBUILD（两条 `install -Dm644`）、`release.yml`（linux/macOS 含 man、Windows 不含）、`ezmk.rb`（`man1.install`/`man5.install`）的 grep 断言，以及 `PREFIX=$RUNNER_TEMP` 复现安装——**必须等阶段五的脚本改动落地后再加**，否则 push 即红
- [ ] push 后确认 CI 全绿（`gh run list` / `gh run watch`）

### 阶段五：分发集成（M-05/M-06/M-07，对应设计 §3.7）

- [ ] `install.sh`：man 安装块 + `EZMK_NO_MAN=1` 跳过 + 非标准 PREFIX 的 `MANPATH` 提示 + 头部注释
- [ ] `publish/arch/PKGBUILD`：两条 `install -Dm644`（Linux 与 MSYS2 两分支共用）
- [ ] `.github/workflows/release.yml`：linux/macOS 打包步骤加 `man/`；**Windows 不加**
- [ ] `publish/homebrew/ezmk.rb`：`man1.install`/`man5.install` + 头注释资产列表（`version`/`url`/`sha256` 留待发布步）
- [ ] 本机 MSYS2 复现：`makepkg -fd` 出包后 `man ezmk` 命中

### 阶段六：`ezmk help` See also 与 i18n（M-08，对应设计 §3.8）

- [ ] `i18n_keys.def` 加 `help_see_also_man` + en/zh 文案（zh-TW 按变体惯例继承）；`print_help()` 在语法说明之后（`src/cli.cpp:1426` 后）打印
- [ ] `bash build.sh` 重建（embed_locale）；`python scripts/check_i18n.py` 三向一致（**406 键**）
- [ ] 验证 `ezmk help` / `EZMK_LANG=zh ezmk help` / zh-TW 回退

### 阶段七：文档收口（M-10，对应设计 §3.9）

- [ ] `README.md`/`README_ZH.md`：安装选项表与 "Customize with …" 句子补 `EZMK_NO_MAN`
- [ ] `docs/{en,zh}/cli.md`：Installation 节说明手册页位置 + 环境变量表补 `EZMK_NO_MAN`
- [ ] `docs/{en,zh}/technical.md`：`## Shell Completion (zsh)` 旁新增 `## Man Pages`（路径 / 三渠道 / MANPATH 提示）
- [ ] `CONTRIBUTING.md`：man 改动流程（跑 `check_man_sync.py` + groff lint + 发布 commit 更新 `.TH` 日期）+ man 与 docs 的职责边界
- [ ] `CHANGES.md` 1.4.3 条目（新增 / 行为变更 / 文档 / 已知限制）
- [x] `plans/1.4.x/README.md`、`plans/README.md` 索引与根 `plan.md`（2026-09-19 随设计文档一并就位）
- [ ] 全仓死链检查零断链

### 阶段八：随附项与收口（M-11 + 门槛复核，对应设计 §3.11/§4.8）

- [ ] 择优随附 `man/ezmk-workspace.toml.5`、`man/ezmk-lua.1`（未落地者在设计 §3.11 明确标注延后）
- [ ] 门槛复核：① 清单全完成/收口；② 公共 API 无破坏性变更；③ 全量 `bash build.sh test-all` 零回归（**1099/6342**）；④ `check_man_sync.py` + 渲染 lint 通过、i18n 406 键三向
- [ ] 首方代码零告警（`-Wall -Wextra -Wpedantic -Wshadow -Wformat=2`）
- [ ] 发布清单加项：确认 man 进 Release 资产 → 回填 Homebrew digest（对照 1.4.2 流程）

---

## 4 关键设计决策

- **防漂移优先于正文**：`check_man_sync.py` 与 man 正文同列 P0——没有闸门的手写 CLI 文档在本仓历史上必然漂移（1.4.2 的文档对齐即证据）。
- **提取基线断言**：脚本内置"当前 CLI 规模"基线，提取数低于基线即失败并提示更新正则，避免 `cli.cpp` 重构后校验静默通过（设计 §3.5/§3.10 坑 5）。
- **手写 roff，零新工具链**：不引入 mdoc/pandoc/ronn/scdoc；CI 侧只用 `groff`（ubuntu runner 可用 `apt` 安装）+ Python（既有 `check_i18n.py` 先例）。
- **两页而非全套**：`ezmk(1)` + `ezmk.toml(5)` 覆盖绝大多数查询；`ezmk-workspace.toml(5)`/`ezmk-lua(1)` 作为随附（M-11），避免一次性维护 8 份 roff。
- **Windows 不做**：无 `man` 命令；MSYS2 用户走 `install.sh`。Windows 打包脚本也不加 `man/`（设计 §3.7/§3.10 坑 6）。
- **CLI 面最小化**：只加 `ezmk help` 末行 See also（i18n 405→406），不新增 `ezmk man`/`--man`，避免把文档需求变成 CLI 契约。
- **`EZMK_NO_MAN` 开关**：与既有 `EZMK_NO_COMPLETIONS` 对称，默认安装、可退出（设计 §3.7）。
- **每阶段全量回归**后才进入下一阶段（含附加门槛：man 校验 + 渲染 lint）。

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|---|---|---|
| 新增 `man/*.1`、`man/*.5`、`scripts/check_man_sync.py` | 无（纯新增，不参与二进制构建） | — |
| `install.sh` 加 man 安装 | 已安装用户多出两个手册页 | `EZMK_NO_MAN=1` 跳过 |
| 新环境变量 `EZMK_NO_MAN` | 新增可选项（默认安装） | README + `cli.md` 环境变量表记录 |
| PKGBUILD 加两条安装 | 包体约 +10 KB | 无 |
| Release 资产含 `man/` | **资产 digest 变化** | 发布后回填 Homebrew `sha256`（既有流程，勿漏） |
| `ezmk help` 末行新增文案 | 输出多一行；**非公共 API** | 无需迁移；i18n 405 → 406 键 |
| 公共 API / CLI 行为 / 配置语义 | **无变更** | 门槛②满足（纯文档 + 分发） |
| Windows（`install.ps1` / winget） | 无变化 | 明确不做（设计 §3.11） |
| 既有 `docs/`、`tutorial/` | 无删除、无改写 | man 为增量速查；职责边界见设计 §3.1 |

## 6 延后项

- `man/ezmk-workspace.toml.5`、`man/ezmk-lua.1` 若未随本版落地 → 延后至 1.4.4 或 2.0.0 前补丁（设计 §3.11 / 目标 M-11）。
- man 多语言（`man/zh_CN`）、`ezmk man` / `--man` 子命令、mdoc 迁移、与 zsh 补全的双向校验、`docs/` 与 man 的字段级自动同步 —— 均为后续可选演进，**本版不做**。
- **2.0.0 联动提醒**：2.0.0 移除 `[test].flags` 与 `ezmk utils cc` 时，`check_man_sync.py` 会立即失败（设计意图）；2.0.0 计划需把"同步 man 条目"列为显式交付项。
