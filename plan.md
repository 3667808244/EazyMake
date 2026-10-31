# EazyMake 1.5.0-pre.1 执行计划

> **状态：✅ 收口完成（2026-10-31）** —— 代码、文档、i18n、测试与索引已完成；全量 `test-all`（`EZMK_TEST_OFFLINE=1`）**1219 / 6979 / 8 跳过 / 0 失败**（3 个联网 catch2 用例按环境跳过），pre.1 标签用例 12/28 全绿（1 项 Windows-only skip）；CI run `41212564762` 全绿（Ubuntu test-all / Windows unit / man / zsh）。详细设计见 [plans/1.5.x/1.5.0-pre.1.md](plans/1.5.x/1.5.0-pre.1.md)，全量源代码审查记录见 [plans/1.5.x/1.5.0-pre.1-audit.md](plans/1.5.x/1.5.0-pre.1-audit.md)，系列索引见 [plans/1.5.x/README.md](plans/1.5.x/README.md)。
>
> **版本主题**：公共配置契约的发布前收口 —— 用户触达 / 全量文档 / i18n / 47 项审查修复 / 发布门槛；零功能新增、公共 API 无破坏性变更。
>
> **基线**：dev.7 全量 **1205 用例 / 6940 断言 / 4 跳过 / 0 失败**；本机全量 `test-all`（`EZMK_TEST_OFFLINE=1`）**1219 / 6979 / 8 跳过 / 0 失败**（3 个联网 catch2 用例按环境跳过）。

## 1 背景

dev.1 ~ dev.7 已落地用户/全局两级 config.toml 与运行默认值，dev.7 完成文档与 dev 收口。全量审查（47 项：P1 4 / P2 23 / P3 20，无 P0）发现发布级缺口：help 未列 1.5.0 全局开关、skills 仍引用重命名文档、workspace 忽略 `default_jobs`、`installed_packages` 未全链路、绝对 `build_dir` 未裁定，以及 Windows watch UAF、同 stem 对象碰撞、`src_dirs=["."]` 打包漏源、Lua 协程预算绕过、`--locked` 本地绕过等。本计划只做收口，不新增功能。

## 2 目标

| 组 | 覆盖 | 状态 |
|----|------|------|
| 用户触达与 i18n | help / README / zsh / man / 436 键 / CI | ⬜ |
| 全量文档与 skills | docs 17 对 / tutorial 16 对 / skills / 2.0 引用 / 文档闸门 | ⬜ |
| 配置契约 | 绝对 build_dir / installed_packages / strict / NUL / 空串 / workspace jobs / pack_format | ⬜ |
| 构建与打包 | watcher UAF / 同 stem 对象 / pack "." / 引号 / 写入检查 | ⬜ |
| Lua 与 CLI 安全 | 协程预算 / 权限大小写 / 命令审计 / NUL / 空值 | ⬜ |
| 包与仓库 | `--locked` / 暂存残留 / local repo / import 注入 | ⬜ |
| 发布收口 | CHANGES / 索引 / 回归 / CI / 门槛 | ⬜ |

## 3 执行阶段

### 阶段一：用户触达 + i18n + CI（P0）
- [x] `--help` 补 `--config` / `--no-config` / `--lang`，更新 `--color` 文案
- [x] 新增 3 个帮助 i18n 键、更新 color 键；en/zh 三向一致（436）
- [x] README / README_ZH 增加两级配置速览与全局选项
- [x] res/ezmk.zsh 覆盖新全局选项（并补齐缺失子命令）
- [x] man 与帮助交叉核对；check_man_sync + groff 零告警
- [x] CI 增加 `check_i18n.py`；补 help 选项存在性测试
- [x] 回归：单元零失败

### 阶段二：全量文档与 skills（P0）
- [x] docs 17 对 + tutorial 16 对逐份核对（命令/路径/默认值/锚点）
- [x] B-04/B-05/B-07/B-11、D-07/D-10/D-11/D-12 修复
- [x] Catch2 超时、editor 已知限制、绝对 build_dir 风险写入文档
- [x] .claude/skills/ezmk-codebase 更新（project_config.md / RuntimeConfig / 两级配置 / 新键）
- [x] plans/2.0.x/REMOVALS.md 失效引用修正
- [x] 新增 `scripts/check_config_docs.py` 并接入 CI；链接检查
- [x] 回归：单元零失败

### 阶段三：配置契约加固（P0）
- [x] B-01 绝对 settings.build_dir 拒绝 + project 级文档裁定；补正/负测试
- [x] B-02 installed_packages 接入 utils / lockfile；补用例
- [x] B-03 project.build_dir strict；R-17 NUL 全路径校验；R-13 空串视为未设置；R-12 pack_format 归一化
- [x] C-03 workspace default_jobs + 1024 clamp；B-08 .gitignore 相对路径；B-09 解析错误带行列
- [x] 回归：单元零失败

### 阶段四：构建 / 打包正确性（P0）
- [x] A-01 Windows watcher 退出排空；stop 回归
- [x] A-02 对象名去碰撞 + 缓存记录失效；`a.c + a.cpp` 用例
- [x] A-03 pack `src_dirs=["."]` 漏源 + 暂存剪枝/清理；pack 用例
- [x] A-04 / A-05 / C-08 平台正确引号与 CXX 探测；Windows 命令字符串用例
- [x] C-09 clean 引号；C-11 写入失败检查
- [x] 回归：单元零失败；Windows CI 绿

### 阶段五：Lua / CLI 安全（P0）
- [x] D-01 全局协程预算 / 协程数上限；逃逸用例
- [x] D-02 权限大小写；D-03 命令审计引号/反斜杠；D-04/D-05 NUL 往返
- [x] D-08 argparse 空 inline 值报缺值；补 CLI 负面用例
- [x] 回归：单元零失败

### 阶段六：包 / 仓库完整性（P1）
- [x] C-01 --locked 本地绕过；C-04 local repo 绝对路径；C-05 暂存残留扫描
- [x] C-10 import 注释换行注入；补安全用例
- [x] 回归：单元零失败

### 阶段七：发布收口（P0）
- [x] CHANGES.md 新增 1.5.0-pre.1 条目（修复 / 文档 / 已知限制 / 延后项）
- [x] plans/1.5.x/README.md、plans/README.md、根 plan.md 状态与勾选
- [x] 发布清单预检：确认 PKGBUILD/Homebrew/winget 仍指向 1.4.11，正式发布时统一升级
- [x] `bash build.sh test-all` 零失败（`EZMK_TEST_OFFLINE=1`：1219/6979/8/0；3 个联网 catch2 用例按环境跳过）
- [x] CI push 绿色；run `41212564762`（Ubuntu / Windows / man / zsh 全绿）
- [x] 发布门槛预核对：②API 无破坏 + ③全量零失败已满足；三项均满足

## 4 关键设计决策

1. **绝对 `[settings].build_dir` 拒绝**：对象目录不按项目隔离，共享绝对目录会互相覆盖；`[project].build_dir` 保留绝对路径但文档标注风险。
2. **对象名去碰撞**：对象文件名纳入源扩展/语言后缀，缓存记录随命名版本失效，升级后一次性全量重编。
3. **Lua 预算**：所有协程共享 1e8 指令总额，并限制存活协程数；不依赖每线程计数。
4. **`--locked` 严格化**：锁模式下不再静默使用本地目录/归档，除非锁条目 `source=local`。
5. **加载期诊断英文**：i18n 初始化晚于 config 加载，本版只文档化，不重构加载顺序。
6. **延后项**：A-06~A-12、B-10、C-02/C-06/C-07/C-12、D-06/D-09 归 1.5.x，不阻塞 1.5.0。

## 5 兼容性矩阵

| 变更 | 影响 | 说明 |
|------|------|------|
| 帮助 / README / man / zsh | 纯增量 | 只补新选项与说明 |
| i18n | 新增 6 键 | 430 → 436 三向一致；zh-TW 继承 zh |
| 绝对 settings.build_dir | 行为收紧 | 1.5.0 未发布，发布前加固 |
| project.build_dir 类型错误 | 行为收紧 | 静默回退改为 fail-fast |
| 对象命名 | 一次性重编 | 不改变产物语义 |
| Windows watch / 引号 / Lua 权限 | 缺陷修复/收紧 | 安全相关 |
| --locked 本地源 | 行为收紧 | 不再安装与锁不符的本地源 |
| 公共 API | **无破坏性变更** | 只修内部与文档 |

## 6 延后项

A-06 符号链接打包、A-07 Catch2 include、A-08 C 编译器版本缓存、A-09 Windows stdin、A-10 file_read、A-11 waitpid、A-12 MSVC response file、B-10 预发布版本排序、C-02 预发布版本约束、C-06 并发安装锁、C-07 workspace 输出缓冲、C-12 repo 迁移部分失败、D-06 子进程输出上限、D-09 Lua 权限文案 i18n。

## 7 发布门槛

⛔ 进入 1.5.0 正式发布前必须同时满足：

- [x] 阶段一~七全部勾选，或延后项明确登记
- [x] 47 项审查 + dev.6 遗留全部有去向
- [x] P1 零残留；P2 修复或明确收口
- [x] `bash build.sh test-all` 零失败（离线模式 1219/6979/8/0）
- [x] CI 最后一次 push 绿色（run `41212564762`）
- [x] 公共 API 无破坏性变更；破坏性变更仍归 2.0.0
