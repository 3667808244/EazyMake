# EazyMake 1.5.0-dev.12 执行计划

> **状态：🚧 执行中；阶段零 ✅、阶段一 ✅、阶段二 ✅、阶段三（部分）🚧（2026-11-08）** —— 详细设计见 [plans/1.5.x/1.5.0-dev.12.md](plans/1.5.x/1.5.0-dev.12.md)。dev.11 已收口（1228/7095；CI run 42449910408 全绿）；本版完成后与 dev.8 ~ dev.11 一起进入 pre.2。
>
> **回归基线**：dev.11 全量 **1228 / 7095 / 8 跳过 / 0 失败**；阶段三后单元 **1117 / 6363 / 4 跳过 / 0 失败**、离线集成 **122 / 785 / 4 跳过 / 0 失败**。
>
> **目标**：修复 2026-11-08 全量代码审核发现的剩余 P1/P2/P3（31 项已随阶段零落地，24 项待修），零回归进入 pre.2。

## 1 背景

全量审核覆盖 `src/` 非 vendor 代码（约 2.1 万行）、`include/ezmk/` 头文件与 `build.sh`，使用逐文件阅读 + clang-tidy + 单元/离线集成测试 + 黑盒复现。P1 与一批低风险 P2/P3 已在阶段零修复（`f77cf5d`、`72e96fe`）；剩余项集中在 Lua 沙箱资源上限、Linux/macOS 递归 watch、网络/临时文件、工具链正确性与 i18n 清理。

## 2 执行阶段

### 阶段零：审核已落地修复（✅ 2026-11-08）

- [x] P1 六项：Windows stdin、`scan_skip`、`project.name` 逃逸、`--locked` 传递依赖、归档溯源/伪 git 源、Lua RUN 权限缓存
- [x] P2 十六项：测试对象命名、Catch2 include、`-j 0`、C89/C99、dry-run、pack 排除、tar.gz 非 ASCII、git/ps1 引用、watcher 0 字节、无效 member、LANGUAGES、compile_db 跨盘符、export 2a/2b、`-y` 与 Lua 提示、deny glob、测试包排序
- [x] P3 九项：`lua::init` try、links.json 穿越、LC_ALL 优先级、semver 预发布、build.sh stale、read/sha 错误、watcher 回调异常、`--prefix ~`、负整数宏
- [x] 回归：单元 1114/6344、离线集成 121/781；手工复现 4 项
- [x] 提交：`f77cf5d`、`72e96fe`

### 阶段一：运行时安全与完整性（P1/P2）✅

- [x] A-01 Lua 沙箱内存上限（512 MiB，tracking allocator）+ 整脚本墙钟 deadline（900s）；`ezmk.run/run_capture` 使用剩余预算
- [x] A-02 不跨 longjmp 存活 C++ 对象：`run_capture` 先释放输出缓冲再报错；JSON encode/decode 在 catch 外销毁异常/JSON 树后 `lua_error`（全局 try/catch thunk 经 MinGW 验证会截获 longjmp，已放弃并记录原因）
- [x] A-04 repo 缺 sha256：warn + 确认；`--locked` 直接失败；sha256 边车不用于 repo/URL
- [x] 测试：内存炸弹（`string.rep 600 MiB`）、缺哈希 repo 安装告警（新增集成用例）
- [x] 回归：单元 1115/6359、离线集成 122/785 零失败

### 阶段二：跨平台与并发健壮性（P1/P2）✅

- [x] A-03 Linux/macOS 递归监听：`add_directory(recursive)` 展开现有子树（跳过隐藏/build/目录符号链接），每 2s rescan 补挂新子目录；Windows 继续用 bWatchSubtree；头文件/文档同步（Linux 由 CI 验证，macOS 本机未验证）
- [x] A-05 临时路径排他创建：`util::make_temp_dir()`（POSIX `mkdtemp`/0700；Windows 不可预测名），pkg 的 stage/clone/download 全部改用
- [x] A-06 MSVC 环境块按大小写不敏感覆盖（`PATH`/`Path` 不再双份；MSVC 实机仍待验证）
- [x] A-07 POSIX `waitpid` EINTR 重试；失败不再当 exit 0
- [x] A-10 pack 遍历/stat 错误立即失败；目录符号链接不递归
- [x] A-11 tar 长名不再静默截断（超限明确报错并提示 `--format zip`；GNU LongLink 未实现）
- [x] A-12 `atomic_write_text` 唯一临时名 + `fsync`（含目录）；`create_targz`/`create_zip` 临时名唯一
- [x] A-22 git 操作默认 300s 兜底超时 + 非 TTY 设 `GIT_TERMINAL_PROMPT=0`（集成 helper 超时未改）
- [x] 测试：`make_temp_dir` 唯一性/0700 单测；pack、pkg、watcher、workspace 相关离线集成全绿

### 阶段三：构建/工具链正确性（P2/P3）🚧（A-20 MSVC rsp 待办）

- [x] A-08 `[settings].linker` 生效：GCC/Clang 链接与测试链接优先使用校验过的 link 驱动（空值回退 detected compiler）
- [x] A-09 C 项目缓存标识改用实际 `$CC` 驱动 + 其 `--version`（C++ 仍用 `$CXX`），换 CC 全量重编
- [x] A-18 `run_tests` 去掉静默 `catch (...)`，改为警告并保留降级行为
- [x] A-19 export 的 defs/opt_flags/link_opts 统一 `cmake_quote`（空格与 `;` 不再拆参数）
- [ ] A-20 `ar` 已走 response file；MSVC `lib`/`link` 的 rsp 回退未做
- [x] A-21 `--locked` 自动安装直接从 lockfile 安装，不再被无约束“最新版”预搜索挡住
- [x] 测试：含空格宏/带引号宏的 export 用例；单元 1117/6363、离线集成 122/785 全绿

### 阶段四：配置/i18n/脚手架一致性（P2/P3）

- [ ] A-13 TOML 子表/permissions 类型严格校验
- [ ] A-14 watch 仅匹配项目根 `ezmk.toml`
- [ ] A-15 清理死 i18n key；硬编码日志迁移
- [ ] A-16 脚手架生成内容随语言设置
- [ ] A-17 `build.sh` 默认 `-O2 -Wall -Wextra`，首方代码零告警
- [ ] A-24 缺 `[project].name` 明确诊断
- [ ] 测试：错误子表类型、i18n 三向、严格告警构建

### 阶段五：回归、文档与收口

- [ ] 全量 `test-all`（`EZMK_TEST_OFFLINE=1`）零失败，不低于 1228/7095
- [ ] i18n / man / docs-sync 三套脚本全绿
- [ ] 文档对齐：name、`--locked`、dry-run、i18n 优先级、watch 范围
- [ ] A-23 lockfile 重哈希评估（优化或转 2.0.0）
- [ ] CHANGES / plans 索引 / 系列 README / 根 plan 回填
- [ ] 收口，进入 pre.2

## 3 关键设计决策

1. **阶段零不单独升版本**：31 项审核修复已随两次提交落地，本版统一记录、统一回归；CHANGES 在阶段五写入。
2. **安全项优先**：A-01/A-03 是本版仅有的两项 P1；未完成则不得进入 pre.2（或拆 dev.13 并记录）。
3. **默认行为不变**：Lua 限额、watch 递归、linker 生效、网络超时均按"未配置 = 旧行为/安全默认"设计；新增诊断不得改变成功路径输出契约。
4. **每阶段一提交**：`chore: 阶段N完成 — <简述>`；阶段内先测试后提交。
5. **回归门槛**：每阶段至少 `bash build.sh test`；阶段五跑 `test-all`（离线）并对比 1228/7095。

## 4 兼容性

| 变更 | 影响 | 缓解 |
|------|------|------|
| `project.name` 安全 basename | 非法名字由静默接受变报错 | 文档 + 明确错误（阶段零已落地） |
| Lua 内存/墙钟上限 | 失控脚本更早失败 | 阈值留足余量；错误说明预算 |
| 递归 watch | 监听 fd 增加 | 深度/数量上限 + `[watch].ignore` |
| `[settings].linker` 生效 | 配置者链接器改变 | 空值不变；日志提示 |
| C 缓存标识 | 换 `CC` 后首次全量重编 | 一次性成本 |
| `build.sh` 严格旗标 | 编译稍慢、历史告警出现 | 首方清零；`CXXFLAGS` 覆盖 |
| 公共 API | 无破坏性变更 | 仅内部行为/诊断 |

## 5 后续

- 阶段五收口后，dev.8 ~ dev.12 一起进入 pre.2；pre.2 只做发布门槛复核，不重复审核。
- A-01/A-03 若超期，拆 dev.13；A-23/A-15 若延后，登记到 2.0.0 移除清单。
- 正式版聚合、版本号、tag 与三渠道分发待 pre.2 完成后按 workflow 生成。
