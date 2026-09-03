# EazyMake 1.4.2 执行计划

> **状态：规划中（未开始实现）**。1.4.1 发布后的补丁版本，主题：**代码质量审计修复（第二轮）**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.2.md**](plans/1.4.x/1.4.2.md)。本计划为 v1.4.1 全量代码六路并行逐行审计 + 独立核查的修复落地（对照 1.4.0-dev.6 / 1.3.6 审计收口先例），P0~P4 共 36 项修复 + 低危随附项。
>
> **范围边界**：只修缺陷与健壮性，**零功能新增**。Linux/macOS 文件监视真递归**明确不做**（设计 §3.9）。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 **1020 用例 / 5970 断言**，1.4.1 发布态）。
>
> **版本决策**：dev 阶段二进制版本号保持 1.4.1（1.2.x/1.3.x/1.4.1 补丁先例不提前 bump），正式发布 commit 按 workflow §3 置 1.4.2。

---

## 1 背景

- 1.4.0-dev.6 完成第一轮审计（8 P0/P1）；本版第二轮审计发现的问题集中在**子系统接口层与跨平台路径/编码**：Lua 报错对象 UB 崩溃、`workspace watch` 成员饿死/多次 Ctrl+C、文件监视器 worker 静默死亡、MSVC 依赖跟踪失效（stdout/stderr 流错位 + 前缀英文硬编码）、`--locked` 产物哈希与归档哈希混用、`[test].framework` 缺省大小写不一致、CMake 导入器 `add_library` 丢源/未知条件 `else` 反转、Windows 窄 API 非 ASCII 乱码、repo 路径信任边界、watcher/workspace 文件层多处（详见设计文档 §1/§2）。

## 2 目标

| 组 | 优先级 | 覆盖 |
|----|--------|------|
| 崩溃/缓存正确性（F-01~F-05） | P0 | Lua UB 崩溃、watch 饿死、MSVC 头文件失效、`--locked` 哈希、watcher 假死 |
| 语义/CLI/import（F-06~F-15） | P1 | test.framework 缺省、import ×2、`--disable-cache`、C++17 标准性、测试链接、相对路径、clean -w、位置参数、Windows 参数装配 |
| Windows/路径（F-16~F-22） | P2 | example 解析、watch 文案、Lua 权限/预算、窄 API 非 ASCII、MSVC 标准映射、cl 探测 |
| pkg/repo/锁（F-23~F-30） | P3 | repo 路径约束/名字校验、preinstall cwd、platform 字段、预发布 tie-break、verify 盲点、update 流程 |
| watcher/workspace 文件（F-31~F-36） | P4 | 目录补挂、存在性过滤、members 拼接、符号链接环、校验不动点、kevent 槽 |
| 低危随附（F-37） | P5 | iso_time/双 close/进程组/get_home_dir/record version/scaffold 等择优随附 |

## 3 执行阶段（每阶段一个 commit，阶段间 `bash build.sh test-all` 全量回归）

### 阶段一：Lua 运行时健壮性（F-01/F-18/F-19，对应设计 §3.1/4.1）

- [ ] `lua_tostring` NULL 判空（`luaL_tolstring`），10 处报错路径（lua_api.cpp 1106/1124/1150/1183/1294/1306/1327/1456/1464/1491）；单测 `error()`/`error({...})`/`assert(false,{})` 不崩
- [ ] `file_exists`/`list_sources` 走 `check_read_permission`（deny → false/空，不弹 ask）；`run_script` 重注册基准对齐 `run_lua_script_with_ctx`
- [ ] 沙箱执行预算：`lua_sethook` 指令上限（约 1e8）+ `ezmk.run`/`run_capture` 超时 + `lua_to_json` 深度上限（200）；钩子用完 restore

### 阶段二：workspace watch 与文件监视（F-02/F-05/F-31/F-32/F-36，对应设计 §3.2/4.2）

- [ ] `run_watch` 改为每选中成员一个 `std::thread`（层序仅定启动次序）；一次 Ctrl+C 全部退出
- [ ] FileWatcher worker 错误通道（`worker_error_` + 消息）：run() 观测到 worker 死亡即告警清理返回，不假死
- [ ] 目录运行中消失 → 周期重试补挂（Windows 重开句柄重挂、Linux 清理 wd 残留 + 重 add_watch、macOS 重开 fd）
- [ ] flush 前 `fs::exists` 存在性过滤 + 忽略前缀/后缀 API；main.cpp ProjectWatch 注册忽略 `build/`、`.ezmk/`、`.o/.d/.tmp`
- [ ] macOS kevent `changes/events` 按监视数动态分配（去 32 槽硬编码）

### 阶段三：构建缓存正确性（F-03/F-04/F-09/F-10/F-11/F-12，对应设计 §3.3/4.3）

- [ ] MSVC `/showIncludes` 依赖注解改解析 `res.out`；`parse_show_includes` 前缀本地化无关（en/zh 前缀 + `:` 后路径形态兜底）
- [ ] lockfile 哈希语义分离：`archive_sha256`（重装校验下载归档）vs `lib_sha256`（仅 verify）；URL/本地归档安装补记 source/archive hash；旧 lockfile 兼容
- [ ] `--disable-cache` 全停：跳缓存拷贝、record 合并与 save
- [ ] build.cpp:2569 TestRunContext 去 C++20 designated initializers（改成员赋值）
- [ ] `run_tests` 链接纳入 `[depends].lib` 包归档与 system targets（复用 prepare_build_state 解析）
- [ ] 相对 `link_dirs`（及 `-L/-I` 族 flags）按 `proj_root` 绝对化，子目录调用安全

### 阶段四：CLI/配置/导入（F-06/F-07/F-08/F-13/F-14/F-16/F-17，对应设计 §3.4/4.4）

- [ ] `test.framework` 分发处 `normalize_lang` 归一化；`test_config.cpp:852` 断言同步为 `"CATCH2"`；集成：省略键可 `ezmk test`
- [ ] import `add_library` 收集源码 + 无关键字默认 static + `INTERFACE`→header-only；`else`/`elseif` 对 nullopt 帧保持跳过 + TODO
- [ ] `clean -w` 首轮 spec 对齐 `workspace_cmd_spec()`（复活 stop-on-error/多余参数拒绝）
- [ ] `project install/pack/test` 补 `reject_positionals`
- [ ] `ezmk example` 参数解析重写（index 2 起按 spec；`-h`/`-o <dir>`/名称 positional/`list` 拒多余参数；output_dir 缺省 "."）
- [ ] watch 失败路径改打新 i18n key `watch_watching`（en/zh/zh-TW + `check_i18n.py`；键数 402 → 403）

### 阶段五：Windows/路径/进程（F-15/F-20/F-21/F-22，对应设计 §3.5/4.5）

- [ ] Windows 参数装配分支（`quote_windows_arg`）：`run_executable`/`run_member`（`--report`、`extra_flags`）不再用 POSIX `escape_shell_arg`
- [ ] 窄↔宽转换层（CP_UTF8）：`CreateProcessW`（cwd/env）、`GetModuleFileNameW`、zip 归档名、`file_watcher` 宽字符存 `fs::path`
- [ ] MSVC 标准映射修正（98/11 去无效 `/std:` 开关、23/26→latest、`gnu++`/`c++2a` 处理）+ translate 全表单测
- [ ] MSVC 探测改 `cl /Bv`（exit 0 + banner/Version 解析），弃「裸 cl exit==0」判据

### 阶段六：pkg/repo/lockfile（F-23~F-30，对应设计 §3.6/4.6）

- [ ] repo `index.toml` `file`/`[platform]` 前缀约束在 repo_dir 内（safe_extract_path 同款校验）；`repo update` 后复查；`type="dir"` 同口径
- [ ] `repo remove/update/info` 入口 `validate_pkg_name`
- [ ] preinstall 钩子 cwd 改 `pkg_root`（全新安装可用）；shell 钩子 `-y`/非交互不 open_in_editor
- [ ] lockfile `platform` 写真实 os_arch_toolchain
- [ ] 预发布平局 tie-break（release 优先 → 字典序），消除 TOML 顺序依赖
- [ ] lockfile verify：header-only 校验 include/ 清单哈希（或文档化 install 时校验）；git 源校验 `.ezmk-git-source` marker 与 commit 一致
- [ ] `pkg update/update_all`：CLI `-y` 透传、install 三态（ok/cancelled/failed）分计、failed>0 exit 1；update_all 先快照名单；自动安装依赖失败 best-effort 回滚；`is_url` scheme/`.git` 优先判定

### 阶段七：workspace 文件层（F-33/F-34/F-35，对应设计 §3.7/4.7）

- [ ] `replace_members_in_text` 引号/注释状态机定位数组尾（防含 `]` 注释截断）
- [ ] `scan_dir` 已访问 canonical 集合（自指符号链接环终止 + skipped）
- [ ] 成员失效传播迭代至不动点；topo_layers "internal error" 改可解释成员错误

### 阶段八：低危随附 + 收口（F-37 随附项 + 设计 §4.8）

- [ ] iso_time 线程安全（localtime_r/localtime_s）、POSIX run_command 双 close 修正、超时杀子进程组、`get_home_dir` HOME 优先策略、record version 校验、scaffold 写失败检查、progress 序号等择优落地（详见设计 F-37）
- [ ] 收口：`-Wall -Wextra -Wpedantic -Wshadow -Wformat=2` 全量零告警；`bash build.sh test-all` 全量零回归（1020/5970）；i18n 三向 + `check_i18n.py`；docs 已知限制更新（watch 失败文案 / `--disable-cache` 语义 / lockfile 字段）；CHANGES.md 1.4.2 条目；`plans/1.4.x/README.md`/`plan.md`/`plans/README.md` 状态更新；发布门槛复核

---

## 4 关键设计决策

- **P0 优先序**：Lua UB（F-01）、watch 线程模型（F-02）、MSVC 依赖流（F-03）、`--locked` 哈希（F-04）、watcher 死亡感知（F-05）先落，其余按阶段推进。
- **行为修正为主**：全部为缺陷修复；错误行为修正（如 `--locked` 从必失败 → 可校验重装、watch 文案）不需要迁移步骤。
- **测试锁定修复**：每个修复配单测/集成，防止「单测锁死错误行为」重演（F-06 先例：test_config.cpp:852）。
- **每阶段全量回归**后才进入下一阶段（设计 §3.8 坑 8）。
