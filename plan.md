# EazyMake 1.2.0-dev.11 执行计划

> **状态：已完成**（2026-08-15，全量 775 用例 / 3552 断言零回归）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.11.md`](plans/1.2.0/1.2.0-dev.11.md)。本计划为 1.2.0 系列第十一个开发子版本：**代码质量审查与改进**——基于 dev.10 完成后的代码（基线 747 用例 / 3442 断言）做全库审查（6 并行代理分模块，68 条问题全部带文件:行证据），本版落地 **P0 全部 + P1 大部**：run_tests 命令构造收口（单一事实源/注入面）、钩子/沙箱安全不对称、编码事故全局修复、配置/CLI 校验、包/导入/导出正确性、缓存/工具健壮性、死代码清理、空断言测试修复。
>
> **范围边界**：只做内部重构与修复，**公共 API 无破坏性变更**（命令/配置/`select_precompiled_archive` 等签名全部不变；新增 i18n key 为纯增量）；大规模拆分（build.cpp/util.cpp）与语义取舍（compare_version 预发布）明确收口到延后项，不混入本版。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 747 用例 / 3442 断言，dev.10 后；Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

- 1.1.2/1.1.3 已做 S1-S5/C1-C5/Q1-Q3 收口，但 1.2.0 系列新增功能后出现新质量债：**run_tests 绕过 `build_compile_args` 单一事实源**（命令注入面在 test 路径重新打开、MSVC 不翻译、对象列表恒空）、**钩子路径无权限门控**（恶意包 preinstall 可无提示读任意路径/执行任意命令）、**编码事故残留**（乱码固化进用户可见输出与测试运行时字符串）、一批校验缺失与死代码。
- 审查结论：68 条问题（high 13 / medium ~40 / low ~15）+ 30 条值得保留的设计；架构骨架扎实，问题集中在「后续功能引入的新缺口」与「静默失败」。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | run_tests 命令构造收口：复用 `build_compile_args`/`join_shell_args`/`translate_compile_flags`；`project_objs` 收集修正 | P0 |
| 2 | 钩子/沙箱安全不对称：安装钩子 perms 门控（或信任模型文档化）；`weakly_canonical` 堵 symlink/junction 逃逸；沙箱黑名单补 io/os | P0 |
| 3 | 编码事故全局修复（用户可见输出 + 测试运行时字符串 + EZMK_LANG 护栏） | P0 |
| 4 | 配置/CLI 校验收口（类型/版本/约束/default_profile/clean/-v/-V/互斥/sha256/SDE） | P0/P1 |
| 5 | 包/导入/导出正确性（import 关键字、export 版本/precompiled、互依赖护栏、约束回读、事务窗口、选择器两缺口） | P0/P1 |
| 6 | 缓存/工具健壮性（depfile 空格、stoul、targz、lockfile 确定性、macOS 家族、签名差分+SDE、cmd 转义、下载上限） | P1 |
| 7 | 死代码/一致性清理（watch/parse_catch2_xml/msvc_env 死代码、产物路径 helper、CliArgs 收敛、fmt、ThreadPool(0)、file_watcher） | P1 |
| 8 | 空断言测试修复（钩子越界 e2e、pkg install e2e 去空转、dev.10 oracle 去耦） | P0 |
| 9 | 全量回归零失败；CHANGES.md + 文档同步 | P0 |

## 3 执行阶段（每阶段一个 commit，带验收测试）

### 阶段一：run_tests 命令构造收口（4.1）

- [x] **1.1 run_tests 重构**（4.1）：编译/链接复用 `cache::build_compile_args` + `join_shell_args`（S4 黑名单）+ `toolchain::translate_compile_flags`（MSVC）；依赖包 extra_includes 注入；测试对象走缓存；`project_objs` 由 CompileInput sources 推导或递归收集；test_filter 经 `escape_shell_arg`
- [x] **1.2 验收测试**（4.1）：test 路径注入回归（`-DVALUE=$(touch ...)` POSIX）、引用项目符号的 Catch2/EZMK 集成测试、MSVC 翻译单测

### 阶段二：钩子/沙箱安全不对称（4.2）

- [x] **2.1 钩子门控**（4.2）：`run_install_hook_script`（至少）加载 perms + `g_in_script_context` 门控；或信任模型文档化 + 修 `lua_api.cpp:51-52` 注释（消除无声不对称）
- [x] **2.2 symlink/junction 逃逸**（4.2）：`path_within`/`norm_path` 检查前 `weakly_canonical`
- [x] **2.3 沙箱黑名单**（4.2）：`push_restricted_globals` 补 `io`/`os`（纵深防御）
- [x] **2.4 验收测试**（4.2）：钩子越界 e2e 真断言、symlink 逃逸单测、受限拷贝无 io/os 断言

### 阶段三：编码事故全局修复（4.3）

- [x] **3.1 编码修复**（4.3）：build.cpp:150/203 用户可见乱码（`鈥?`→`—`）+ util.cpp 注释 + test_integration.cpp 24+ 处（含 384 行运行时字符串 `鏈煡`→`未知` + 该测试补 `EZMK_LANG` 护栏）
- [x] **3.2 防回归**（4.3）：乱码字节检查脚本（可选）或全量测试通过验证

### 阶段四：配置/CLI 校验收口（4.4）

- [x] **4.1 配置校验**（4.4）：`extract_string_array` 非字符串元素报错（i18n）；版本约束格式校验 + 空名报错；`default_profile` 交叉校验（config 层）；`source_date_epoch` 负值报错
- [x] **4.2 CLI 校验**（4.4）：`project clean` 参数解析；`project test -v/-V` 统一；`--locked`+`--no-lock` 互斥；`--sha256` 格式校验
- [x] **4.3 profile 错误 i18n 化**（4.4）：build.cpp apply_profile 错误串换 i18n key
- [x] **4.4 验收测试**（4.4）：每项对应单测（config/cli）

### 阶段五：包/导入/导出正确性（4.5）

- [x] **5.1 import**（4.5）：`target_*` 关键字感知解析（无关键字老式写法 + 多分组关键字过滤）+ 测试
- [x] **5.2 export**（4.5）：`VERSION` 数字格式校验（预发布省略 + 注释）；precompiled 复用 `select_precompiled_variant` + 测试
- [x] **5.3 依赖安装**（4.5）：互依赖自动安装护栏（安装链集合）；自动安装后约束回读校验
- [x] **5.4 事务安装**（4.5）：`.new` 换位 + 回滚 ec 检查 + backup 隐藏唯一名
- [x] **5.5 dev.10 选择器**（4.5）：`lib/` 缺失友好错误；MSVC 平局偏好 `.lib`
- [x] **5.6 验收测试**（4.5）：无关键字/多关键字 import、预发布 export、含 tagged 归档 precompiled 导出、互依赖、约束不满足自动安装

### 阶段六：缓存/工具健壮性（4.6）

- [x] **6.1 缓存**（4.6）：depfile 转义空格解析 + 测试；缓存签名差分测试（签名 ⟺ build_compile_args 输出）+ SDE 入签名
- [x] **6.2 工具**（4.6）：`compiler_tag` stoul 溢出保护；targz 长路径 prefix 读出 + size 越界报错 + 往返测试；lockfile sha256 确定性选择；macOS 家族判定（`--version` 双信号）；`escape_cmd_arg`（cmd /c 转义）；下载大小上限 + ofstream 检查

### 阶段七：死代码/一致性清理（4.7）

- [x] **7.1 死代码**（4.7）：watch 内联 profile 合并、`parse_catch2_xml`/`Catch2TestResult`、`msvc_env` 死调用删除
- [x] **7.2 一致性**（4.7）：MSVC 产物路径共享 helper（install/pack 修复）；CliArgs optional 收敛 + `auto_update_repos` 死调用；i18n fmt 单遍扫描；`ThreadPool(0)` 拒绝；file_watcher Windows 错误恢复 + macOS kqueue 语义文档化
- [x] **7.3 验收**（4.7）：重构后全量回归

### 阶段八：空断言测试修复（4.8）

- [x] **8.1 空断言修复**（4.8）：钩子越界 e2e 真断言（写入 ../escaped.marker 断言拒绝）；pkg install e2e 去空转（断言退出码 0 + 产物 + list）；dev.10 预编译测试 oracle 去耦（独立构造期望标签）

### 阶段九：收口（4.9）

- [x] **9.1 收口**（4.9）：CHANGES.md dev.11 条目 + 文档同步（如有行为变化）+ plan.md 勾选 + 系列 README 状态「待实现 → 已完成」+ 发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| run_tests 复用单一事实源 | 编译/链接走 `build_compile_args`/`join_shell_args`/`translate_compile_flags`——注入面、MSVC 翻译、包 include、缓存一次解决；`project_objs` 由 CompileInput 推导避免两处「对象在哪」假设漂移 |
| 钩子信任模型二选一 | 优先「安装钩子加载 perms + 上下文门控」；若选「钩子=完全信任」必须文档化 + 修注释，消除当前无声不对称 |
| 词法路径检查升级 | `weakly_canonical` 解析后再比较，堵 symlink/junction 逃逸（Windows junction 亦生效） |
| 配置校验前移 | 类型/版本/约束在 parse_config 抛 i18n 错误，不再静默吞或拖到 build 深处 |
| 事务安装 `.new` 换位 | 拷贝窗口内旧版始终存在；回滚 ec 检查；backup 隐藏唯一名 |
| 每项修复带验收测试 | 注入回归/symlink/含空格 depfile/长路径/约束回读/互依赖护栏全部落测试 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| run_tests 命令构造重构 | 行为对齐主构建（转义/翻译/include/缓存） | 既有配置零变化；测试产物路径不变 |
| 钩子权限门控 | 恶意包钩子受限（按 perms） | 正常包钩子行为不变；文档同步 |
| 配置校验前移 | 非法配置从「静默」变「报错」 | 合法配置零影响；错误消息更早更清晰 |
| 编码修复 | 乱码→正确字符 | 纯文本修复；测试字符串断言同步 |
| 死代码删除 / helper 抽取 | 行为不变 | 全量回归验证 |
| 事务安装 `.new` 换位 | 失败窗口内旧版保留 | 安装语义不变，更强健 |
| 公共 API | 无破坏性变更 | 全部内部重构 + 新增 i18n key |

## 6 延后项（P2，明确收口）

- **大规模拆分**：`build.cpp`（test_runner/install/pack）、`util.cpp`（archive/download）、`process_installed_pkg` 拆分——结构性重构，收口到 dev.11 之后的独立子版本或 1.2.0 发布窗口。
- **语义取舍**：`compare_version` 预发布剥离（文档化记录）；lockfile platform 字段删除。
- **测试卫生**：测试夹具统一（test_helpers.hpp）、REQUIRE(true) 清理、watch 测试 PID 化、增量缓存断言收紧——可随各阶段顺带或延后。
- **macOS kqueue 逐文件语义**：FSEvents 重写为独立工作项（先文档化 + 断言修正）。
- **回归基线**：全量测试零回归（dev.10 后基线 747 用例 / 3442 断言），作为硬门槛。
