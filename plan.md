# EazyMake 1.4.6 执行计划

> **状态：✅ 实现收口（未发布）**——七阶段全部落地，全量 **1130 用例 / 6536 断言零失败**（基线 1.4.5 发布态 1113/6491）；严格告警零告警；i18n 412 键、man/groff、docs-sync 门槛通过。本文档把设计文档 §4 转成可勾选的七阶段清单。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)；2.0.0 的移除清单见 [`plans/2.0.x/REMOVALS.md`](plans/2.0.x/REMOVALS.md)。
>
> 详细设计：[**1.4.6.md**](plans/1.4.x/1.4.6.md)。主题：**代码质量审计修复（第三轮）**——对 `src/` 全部首方模块的并行逐行审计 + 独立复核，修复错误路径、边界条件与跨平台分支上的缺陷。
>
> **范围边界**：只修缺陷与健壮性，**零功能新增**；**公共 API 无破坏性变更**（仅内部签名与错误行为修正）。明确不做项见设计 §3.9。
>
> **⛔ 发布门槛**：① 阶段清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线以立项时 `bash build.sh test-all` 实测为准；本快照 1.4.5 发布态为 **1113 用例 / 6491 断言**）且新增用例只增不减；④ 附加门槛：`python scripts/check_man_sync.py` 通过、`groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警、i18n 三向一致、`bash scripts/check_docs_sync.sh` 通过。
>
> **版本决策**：开发阶段不提前 bump（沿用补丁先例）；正式发布 commit 按 workflow §3 置 1.4.6。
>
> **版本定位**：1.4.6 为 1.4.x 系列在 1.4.5 之后的补丁版本，承接前两轮审计（1.4.0-dev.6 / 1.4.2）的同一主题。

---

## 1 背景

前两轮审计（1.4.0-dev.6 第一轮、1.4.2 第二轮）已把编译告警与静态分析清干净。本轮审计印证：**首方代码在严格告警下零告警、`clang-tidy`（clang-analyzer/bugprone）零发现**，问题集中在正常路径不经过、但触发即损坏数据或放行坏输入的分支——zip 解压失败二次 `fclose`（UB）、`pkg install` 目录/git 源成功却回滚依赖、`atomic_rename` 兜底失败仍删源、GCC 响应文件未转义、tar 尺寸字段溢出、下载失败被当成功、`--precompiled` 标记错位、`--report` 注入、macOS fd 双 close、`[workspace.options].stop_on_error` 不生效等。详见设计 §1。

## 2 目标

| 组 | # | 覆盖 | 优先级 |
|----|---|------|--------|
| 内存/数据完整性 | Q-01/Q-02/Q-04 | zip 单出口关闭；pkg 目录/git 安装事务提交点；`atomic_rename` 删源时机 | P0 |
| 编译正确性 | Q-03/Q-19/Q-30/Q-31 | 响应文件转义；目录遍历排序；并行合并 `operator[]`；link-only profile | P0/P1/P2 |
| 归档/下载 | Q-05/Q-06/Q-22/Q-24/Q-25/Q-26 | tar 八进制/溢出/负状态；下载失败可见 + `curl --fail` + URL 文件名净化；gzip 头边界；zip 原子写；失败检查 | P1/P2 |
| pkg/锁 | Q-11/Q-12/Q-13/Q-15/Q-16/Q-17/Q-18 | 取消三态；verify 盲点；`.new` 误删；stoul 越界；写入失败可见；损坏注册表；名字校验 | P1/P2 |
| watcher/workspace | Q-09/Q-10/Q-28/Q-29 | macOS fd 双 close；`stop_on_error`；写回边界；inotify/OVERLAPPED | P1/P2 |
| 跨平台/CLI | Q-07/Q-08/Q-14/Q-20/Q-21 | `--precompiled`；`--report` 注入；`detect_toolchain` 线程安全；Windows 引用；POSIX 重定向引号 | P1/P2 |
| 健壮性/收口 | Q-23/Q-27/Q-32 | `toml_quote` 控制字符；`file_write` 契约；其余低危随附 + 索引/文档/门槛 | P3/P4 |

## 3 执行阶段（每阶段一个 commit，阶段间全量回归）

### 阶段一：内存安全与数据完整性（Q-01/Q-02/Q-04）

- [x] Q-01 `extract_zip` 关闭单出口；失败路径不再二次 `fclose`
- [x] Q-02 事务提交点覆盖目录/git/仓库目录三条成功返回路径（以 `InstallOutcome::Ok` 统一提交）
- [x] Q-04 `atomic_rename` 仅 copy 成功后才删源；失败保留源
- [x] 回归：全量零失败（数字回填）

### 阶段二：编译正确性与确定性（Q-03/Q-19/Q-30/Q-31）

- [x] Q-03 响应文件按 GCC/Clang `@file` 规则转义；修正 `test_workspace_build.cpp` 错误前提用例
- [x] Q-19 包归档 / object / `.ezmk/pkg` 扫描排序
- [x] Q-30 并行路径先 `find` 后插入，对齐串行；新文件不再误报
- [x] Q-31 profile 合法性按 compile 或 link 判定
- [x] 回归：全量零失败

### 阶段三：归档与下载健壮性（Q-05/Q-06/Q-22/Q-24/Q-25/Q-26）

- [x] Q-05 tar 严格八进制 + 减法式溢出判断 + 非 DONE 负状态错误；截断 gzip 报错
- [x] Q-06 Windows 下载失败可见；`curl --fail`；URL 文件名净化；临时归档清理（含异常路径）
- [x] Q-22 `skip_gzip_header` 边界校验
- [x] Q-24 `create_zip` 原子写
- [x] Q-25 tar 解包检查 `ofstream` 成败
- [x] Q-26 `mz_deflateInit2` / `CreatePipe` 返回值检查
- [x] 回归：全量零失败

### 阶段四：pkg 事务与 lockfile 校验（Q-11/Q-12/Q-13/Q-15/Q-16/Q-17/Q-18）

- [x] Q-11 取消三态向上传播；取消不写 lockfile；`update_all` 分计
- [x] Q-12 verify 缺失产物 mismatch；header-only git 走来源校验
- [x] Q-13 暂存目录改名（与合法包名无冲突）
- [x] Q-15 `stoul` 越界捕获 helper
- [x] Q-16 写入失败可见（注册表 / lockfile）
- [x] Q-17 损坏 `list.json` 不静默覆盖
- [x] Q-18 `pkg info` / `pkg update` 补名字校验
- [x] 回归：全量零失败

### 阶段五：watcher 与 workspace（Q-09/Q-10/Q-28/Q-29）

- [x] Q-09 macOS 补挂 close 后置 `w.fd = -1`
- [x] Q-10 `stop_on_error` 配置生效（CLI 优先）
- [x] Q-28 workspace 写回边界 + `default_jobs` 上限
- [x] Q-29 Linux `EAGAIN` 重试；`win32_cleanup` 排空后再释放
- [x] 回归：全量零失败

### 阶段六：导出/CLI/跨平台（Q-07/Q-08/Q-14/Q-20/Q-21/Q-23/Q-27）

- [x] Q-07 `--precompiled` 标记落入 `[project]` 节内（三种 fixture）
- [x] Q-08 `--report` 格式串转义 / 白名单
- [x] Q-14 `detect_toolchain` 线程安全
- [x] Q-20 Windows 路径统一 `quote_windows_arg`
- [x] Q-21 POSIX `run_command` 重定向路径引号化
- [x] Q-23 `toml_quote` 全控制字符转义
- [x] Q-27 `file_write` 目录创建异常返回 false
- [x] 回归：全量零失败

### 阶段七：收口（Q-32 低危随附 + 索引/文档/门槛）

- [x] Q-32 审计其余低危项择优随附（`is_ignored` 边界、`pkg` 进度输出、硬编码消息 i18n 化等；余者记入设计 §3.9）
- [x] 新增/变更用户可见消息的 i18n 键（en/zh/zh-TW 三向一致，`check_i18n.py`）
- [x] 严格告警全量零告警（含 `test/`）；`bash build.sh test-all` 全量零回归
- [x] 附加门槛：`check_man_sync.py` 通过 + groff 零告警 + i18n 三向 + `check_docs_sync.sh` 通过
- [x] `CHANGES.md` 新增 1.4.6 条目（缺陷修复 / 行为收紧 / 测试 / 明确不做）
- [x] 索引与状态更新：`plans/1.4.x/README.md`、`plans/README.md`、根 `plan.md`
- [x] 发布门槛复核（⛔ ①②③④）

---

## 4 关键设计决策

- **以 `InstallOutcome::Ok` 为唯一事务提交触发点**：不是逐点补三个 `return`，而是把「成功」集中判定，避免未来新增返回路径再漏（Q-02）。
- **关闭/清理收敛为单出口**：资源释放只在 RAII guard / 单一 cleanup 里发生，错误分支只抛（Q-01/Q-04）。
- **不可信输入一律「严格解析 + 显式报错」**：tar 八进制严格校验、减法式溢出判断（复用 1.4.5 zip 总量先例）、gzip 非 DONE 负状态一律错误、下载失败可见（Q-05/Q-06）。
- **确定性优先**：目录遍历排序键固定为规范化相对路径，消除 `-I`/链接顺序的文件系统依赖（Q-19）。
- **配置与 CLI 遵循既有优先级**：workspace 配置值作为 CLI 未给出时的默认，CLI 优先（Q-10，对照 `default_jobs`）。
- **行为收紧需可见**：归档/下载/verify 的收紧会改变「静默通过」的旧行为，全部在 CHANGES 与文档明确，并用夹具锁定（Q-05/Q-06/Q-12）。
- **不放宽也不移除弃用面**：本版纯修复，与 2.0.0 窗口解耦（REMOVALS 清单不动）。

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|---|---|---|
| Q-01 关闭单出口 / Q-04 删源时机 | 缺陷修复（仅错误路径） | 正常路径零变化；测试锁定 |
| Q-02 目录/git 安装提交事务 | **行为修正** | 此前成功安装后依赖被误删；修复后依赖保留 |
| Q-03 响应文件转义 | 行为修正 | 含空格/引号的长命令此前失败或宏值错误；修复后正确 |
| Q-05/Q-06/Q-12 校验收紧 | **行为收紧** | 坏归档/断流/来源漂移从「静默通过」变为明确报错 |
| Q-07 `--precompiled` | 缺陷修复 | 带注释/其它首节的 `ezmk.toml` 打包后可正常安装 |
| Q-10 `stop_on_error` / Q-11 取消传播 | 行为修正 | 配置生效；取消不再计为成功、不误写 lockfile |
| Q-19 遍历排序 | 确定性 | `-I`/链接顺序不再依赖文件系统；构建结果应不变 |
| Q-20/Q-21 路径转义 | 行为修正 | ASCII 路径不变；含空格/UNC 路径变正确 |
| i18n | 新增文案 | 三向一致 + `check_i18n.py` 通过 |
| 公共 API / CLI / 配置语义 | **无破坏性变更** | 仅内部签名与错误行为修正 |

## 6 延后项

- **Linux/macOS 文件监视真递归**：沿用 1.4.2 §3.9 裁定，另立版本。
- **破坏性 API 变更 / 弃用面移除**：归 2.0.0，见 [`plans/2.0.x/REMOVALS.md`](plans/2.0.x/REMOVALS.md)。
- **build.cpp / pkg.cpp 全面重构**：工程量大且与缺陷修复正交，不在本版。
- **低危项中收益极低者**：如 Windows 扩展长度路径在 MSVC 链接器上的完整适配，记录在案、按需随附。
- **1.4.x 系列的后续补丁**：按需另立计划；已发布版本见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
