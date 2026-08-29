# EazyMake 1.4.0-dev.6 执行计划

> **状态：已完成（收口）**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.0-dev.6.md**](plans/1.4.x/1.4.0-dev.6.md)。本计划为 1.4.0 第六个开发子版本，主题为**代码质量审计（全量）**——对当前使用的全部代码做系统审计并修复正确性（P0）与健壮性（P1）缺陷：8 路并行审查覆盖 ~19k 行核心代码 + include + test。
>
> **范围边界**：P2 风格/死代码/需设计决策项（跨盘符路径、zip 解压上限、lua_to_json 循环表、export C 侧能力表、import add_library 形态、CMake 引号化等）**明确延后**（记录在 design doc §6）。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 968 用例 / 5588 断言，1.4.0-dev.5 后实测）。

---

## 1 背景

- dev.5 收口后对全部代码系统审计，发现一批历史遗留的正确性/健壮性缺陷，多数**静默出错**（无报错、无测试锁定）：确定性构建缓存永久失效、非英文系统 MSVC 探测失败、`--locked` 不锁版本还改写 lockfile、git branch 命令注入、Lua 钩子根路径钉死 CWD 等。
- 测试套件存在 4 个恒真/无断言测试（回归无法被捕获）与 dev.5 新测试的脆弱点（进程泄漏/空转断言/假优先级）。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `-Wall -Wextra` 严格编译零警告 | P0 |
| 2 | 修复全部 P0 正确性缺陷（8 项） | P0 |
| 3 | 修复高价值 P1 健壮性问题（源码 9 项 + 测试 4 项） | P1 |
| 4 | 修复测试套件 P0（4 个恒真/无断言测试） | P0 |
| 5 | 全量零回归 + 记录未修 P2 延后项 | P0 |

## 3 执行阶段（全部完成）

### 阶段一：编译零警告 + 审计

- [x] **1.1 严格编译**：`CXXFLAGS="-std=c++17 -Wall -Wextra"` 全量编译零警告（含测试二进制）
- [x] **1.2 并行审计**：8 路子代理覆盖 build/cache · pkg/repo/lockfile · cli/config/toolchain · util/file_watcher/crypto · export/import/compile_db · workspace · i18n/lua/main · test
- [x] **1.3 实证复核**：MSVC cl 本地化横幅、vswhere BuildTools、CMake 括号注释、`fs::relative` 跨盘符、Catch2 SECTION 重跑语义
- [x] **1.4 分级清单**：P0×8 / P1×13 / P2×N

### 阶段二：源码 P0/P1 修复（commit ab78208 + 15df60c 源码部分）

- [x] **2.1 缓存签名对称**（cache.cpp）：deterministic 构建 lock 哈希并入校验侧
- [x] **2.2 依赖名安全**（pkg.cpp）：libs/want 两处 `validate_pkg_name`
- [x] **2.3 `--locked` 锁版本**（pkg.cpp）：Exact 约束 + 不重写 lockfile + 2 新 i18n 键
- [x] **2.4 repo 名安全**（repo.cpp）：add 处校验 + `name_from_url` 支持 `\`（npos 哨兵 bug）
- [x] **2.5 MSVC 版本解析语言无关**（toolchain.cpp）：digit.digit 扫描（跳过 x64）
- [x] **2.6 git branch 注入**（util.cpp）：branch 补引号
- [x] **2.7 `#[[...]]` 括号注释**（import.cpp）：skip_comment 复用 parse_bracket
- [x] **2.8 Lua register_api 重注册**（lua_api.cpp）：根变化即重注册
- [x] **2.9 其余 P1**：run_watch SIGINT 恢复、句柄泄漏/mkstemp 静默成功、SDE 崩溃、缓存写失败、config 非数组报错

### 阶段三：测试 P0/P1 修复（commit f084b7e + 15df60c 测试部分）

- [x] **3.1 恒真/无断言测试**：hooks ctx.output、toolchain 恒真式、crypto raw 长度、repo round-trip
- [x] **3.2 watch 测试脆弱点**：RAII 进程清理（workspace watch + watch --run 失败路径）、wait_exists 空转、sidecar 假优先级
- [x] **3.3 MSVC 解析断言更新** + zh-CN banner 新用例

### 阶段四：收口

- [x] **4.1 全量零回归**：`bash build.sh test-all`（**968 用例 / 5612 断言零失败**，+24 断言）
- [x] **4.2 文档**：design doc（plans/1.4.x/1.4.0-dev.6.md）+ CHANGES.md dev.6 条目 + plan.md + plans/README 状态更新
- [x] **4.3 门槛复核**：清单完成/收口 + API 无破坏性变更 + 全量零回归

> 门槛未满足即停止。**本版门槛全部满足，dev.6 收口。**

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 缓存签名对称补 lock 哈希 | 校验侧按 `record.deterministic` 与保存侧同规则（复用同一 `compile_options_signature` 输入） |
| `--locked` 改为 Exact 版本约束 | 从 lockfile 取该包记录版本 → `search_package(name, scopes, Exact)`；找不到即 fatal；`no_lock=true` 禁止改写 |
| MSVC 版本解析语言无关 | 扫描第一个 `<digits>.<digits>`（复用 `first_version_major` 思路），跳过 "x64"/"x86" 平台标签 |
| repo 名校验配套 name_from_url 修 `\` | npos 是 size_t 最大值，`x > npos` 恒假 → 显式 `found` 标志比较 |
| config 非数组报错 | 节点存在但类型错 → 抛 `config_err_array_field_type`；`include_dirs` 不 fallback 到旧字段 |
| Lua API 根变化即重注册 | `g_project_root != abs(api_project_root)` 时重注册（register_api 同时失效配置缓存） |
| 测试 RAII 进程清理 | 局部 killer 析构兜底，任何 REQUIRE 失败路径都 kill watch 进程树 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 确定性缓存签名含 lock 哈希 | 行为修复 | deterministic 项目首次构建重编一次，之后恢复正常增量 |
| `--locked` 锁版本 + 不重写 | 行为收紧 | 与文档契约一致；无记录/版本不可得 → 新错误信息 |
| repo 名安全校验 | 行为收紧 | 非法名被拒；Windows 本地路径名提取修复配套 |
| config 非数组字段报错 | 行为收紧 | 拼写错误不再无声失效 |
| MSVC 版本解析语言无关 | 行为修复 | 非英文系统对齐英文系统；英文结果不变 |
| 公共 API | **无破坏性变更** | 纯修复 + 3 新 i18n 键 |

## 6 延后项（记录在案，随 1.4.0 后续 dev/pre 或 1.5.x）

- 跨盘符 `fs::relative` 空路径 → cache 键碰撞 + 对象覆盖
- 测试链接缺依赖包归档/链接参数；`test --profile` 未转发内层 build
- `extract_zip` 无总大小上限；`extract_targz` 不校验 gzip footer
- watcher 事件风暴退出 + 关闭期 OVERLAPPED 竞态
- `lua_to_json` 循环表无界递归崩溃
- export C 侧能力表误报；import `add_library` 无类型形态误判
- export CMake 字面路径/名称不引号化（含空格路径）
- 死代码/重复/硬编码英文等 P2 清理（详见 design doc §6）
