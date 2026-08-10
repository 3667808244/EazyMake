# EazyMake 1.1.3 执行计划

> 详细设计：[`1.1.3.md`](plans/1.1.x/1.1.3.md)。本计划为 1.1.x 稳定线补丁（**安全收敛二轮 + 健壮性收口**），承接 1.1.2 发布后执行；完成后恢复 1.2.0 执行计划。
>
> **范围边界**：只修 bug、安全与低风险重构；不新增命令、不弃用、不触碰 1.1.0 稳定 API。1.2.0 功能计划（`plans/1.2.0/`）不受影响。
>
> **发布口径**：对外（CHANGES.md）表述为「安全收敛（二轮）+ 健壮性收口」；命令名/配置格式/CLI 参数不变。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口（含 S1b 决策）；② 公共 API 无破坏性变更；③ 全量测试零回归（Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#-发布门槛release-gate)）。

---

## 1 背景

2026-08-09 对 1.1.2 代码库做了第二轮多模块安全与质量审计（3 个并行 reviewer，覆盖 pkg/repo/lockfile/crypto、lua/util/build/toolchain、main/cli/config/cache/watcher），并逐条人工核验高风险结论。1.1.2 已修复第一轮 4 处漏洞，本轮发现第二层问题：

- **安全（5 处）**：钩子沙箱仍裸 `_G`（S1）、包名路径穿越（S2）、URL 直装无完整性（S3）、POSIX flags 命令注入（S4）、编辑器命令注入（S5）。
- **健壮性（5 处）**：`SOURCE_DATE_EPOCH` 非数字崩 worker 线程（C1）、`~` 前缀误截断（C2）、OVERLAPPED 全局池悬垂（C3）、`recursive_` 死代码（C4）、watcher/缓存/CLI 边缘（C5）。
- **代码质量（3 处）**：手写 JSON 解析器（Q1）、大函数（Q2）、cli 重复代码（Q3）。

本版为 1.1.x 稳定线纯修复补丁，逐项对应设计文档 §3。

---

## 2 目标

| 类别 | 条目 | 优先级 |
|------|------|--------|
| 安全 | S1 钩子沙箱收敛：`run_lua_script_with_ctx` 改用受限全局表 + 注释诚实化 | P0 |
| 安全 | S2 包名校验：`validate_pkg` 拒绝路径穿越字符 | P0 |
| 安全 | S3 URL 安装完整性：无 sha256 需确认 + 默认拒 `http://` 明文 | P1 |
| 安全 | S4 POSIX 编译/链接 flags 元字符引号补全 | P1 |
| 安全 | S5 编辑器命令转义（`open_in_editor`）+ vcvars 路径转义 | P2 |
| 健壮性 | C1 `SOURCE_DATE_EPOCH` 安全解析（修 worker 崩线程） | P0 |
| 健壮性 | C2 `~` 前缀边界检查 | P1 |
| 健壮性 | C3 file_watcher OVERLAPPED 池实例化（修悬垂风险） | P1 |
| 健壮性 | C4 `recursive_` 死代码收口（删除 + 文档注明） | P2 |
| 健壮性 | C5 边缘批处理（`name.back()` UB / `.o` 魔法串 / CLI NUL） | P2 |
| 质量 | Q1 手写 JSON 解析器改 `nlohmann/json` | P1 |
| 质量 | Q2 `parse_config`/`compile_one_source` 大函数拆分 | P2 |
| 质量 | Q3 `cli.cpp` 重复代码收敛 | P2 |
| 测试 | T1 测试永真断言清理（`file_watcher >= 0`） | P1 |
| 测试 | T2 `argparse`/`main` 单测补齐 | P2 |
| 通用 | 每个修复配套单元/集成测试；全量测试零回归 | P0 |

---

## 3 执行阶段

### 阶段一：安全收敛（S1–S5）

- [x] **1.1 钩子沙箱统一**（S1a）：提取 `push_restricted_globals()` + `run_lua_script_with_ctx` 接入 + `pkg.cpp` 注释/确认文案诚实化；`test_lua.cpp` 逃逸面用例；`docs/en|zh/build_hooks.md` / `docs/en|zh/pkg.md` 补沙箱限制说明
- [x] **1.2 安装钩子权限门控**（S1b，可选）：**决策（1.1.3）：收口为仅 S1a**。S1b 语义变更最大，补丁线保守处理——安装钩子权限门控（`g_in_script_context` + 从包 `ezmk.toml` 加载权限 + legacy 兼容）延到 1.2.0，见 §6 延后项
- [x] **1.3 包名校验**（S2）：`validate_pkg_name()` + 安装/lockfile 恢复调用点接入；`test_pkg.cpp` 恶意名用例
- [x] **1.4 URL 完整性**（S3）：无 sha256 的 URL 安装确认流程 + 显式 `http://` MITM 警告；`test_pkg.cpp` 用例
- [x] **1.5 flags 元字符引号**（S4）：`join_shell_args` 元字符集补全 + `build.cpp` GCC/MSVC 链接 flags 双引号包裹；`test_compile_db.cpp` 用例
- [x] **1.6 编辑器/vcvars 转义**（S5）：`open_in_editor`/`find_editor` 转义 + 移除全库唯一 `system()`；vcvars 路径转义；`test_util.cpp` 用例
- [x] 阶段一自测：`bash build.sh test` 相关用例通过（591 用例 / 2806 断言）

### 阶段二：健壮性收口（C1–C5）

- [x] **2.1 SDE 安全解析**（C1）：`resolve_source_date_epoch()` 提取 + 非数字 env 警告按 0 处理；`test_cache.cpp` 用例
- [x] **2.2 `~` 前缀边界**（C2）：仅 `~/` 或单独 `~` 展开，`"~abc"` 字面保留；`test_config.cpp` 用例
- [x] **2.3 OVERLAPPED 池实例化**（C3）：`g_overlapped_pool` 改为 `FileWatcher` 实例成员；新增重叠实例清理测试
- [x] **2.4 `recursive_` 收口**（C4）：删死字段 + 头文件注明递归行为（Windows 递归 / Linux·macOS 非递归）；行为不变
- [ ] **2.5 边缘批处理**（C5）：`name.back()` 空串防御 + `.o`/`.obj` 魔法串常量 + CLI NUL 注释（TODO 归 1.2.0）
- [ ] 阶段二自测：`bash build.sh test` 相关用例通过

### 阶段三：代码质量收敛（Q1–Q3）

- [ ] **3.1 JSON 解析器替换**（Q1）：`load_links_json` 改 `nlohmann/json` + 畸形 JSON catch；`test_config.cpp` round-trip/畸形/Unicode 用例
- [ ] **3.2 大函数拆分**（Q2，P2）：`parse_config`/`compile_one_source` 纯提取拆分，逐步跑单测（零行为漂移）
- [ ] **3.3 cli 去重**（Q3，P2）：提取「选项规范」「positional 校验」「scope 收集」共享 helper；`test_cli.cpp` 全量通过
- [ ] 阶段三自测：`bash build.sh test` 相关用例通过（重构零行为漂移）

### 阶段四：测试质量 + 回归与发布准备

- [ ] **4.1 永真断言清理**（T1）：`test_file_watcher.cpp` 精确断言（轮询等待 + 精确路径集合）/ 显式跳过；本地 + CI 通过
- [ ] **4.2 argparse/main 单测**（T2）：新建 `test_argparse.cpp`（tokenizer 直接测）；main 顶层别名展开 + 未知命令用例
- [ ] 全量回归：`bash build.sh test` + `test-all` 零回归（基线 1.1.2 发布值 592 用例 / 2813 断言，2026-08-08）
- [ ] `CHANGES.md` 新增 1.1.3 条目（口径：安全收敛二轮 + 健壮性收口；命令/配置/CLI 不变）
- [ ] 版本号预置：`include/ezmk/version.hpp` / `build.sh` 默认版本 `1.1.3`（在 1.1.2 发布后执行）
- [ ] **发布门槛预检**：① 计划清单全部完成或明确收口（含 S1b 决策）；② 公共 API 无破坏性变更；③ 全量测试零回归
- [ ] 打 `v1.1.3` tag 触发 `release.yml`；产物核验 + 分发（沿用 1.1.2 流程）— **待用户执行**
- [ ] 收尾：`plan.md` 状态更新（1.1.2 已发布 → 1.1.3 执行 → 完成后恢复 1.2.0）；`plans/README.md` / `plans/1.1.x/README.md` 状态更新 — **发布后执行**

> 门槛未满足即停止，禁止带着未收口项打 tag。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **S1a 受限全局表统一** | 把 `run_script` 的受限全局表构建提取为共享 `push_restricted_globals()`，`run_lua_script_with_ctx` 沙箱 `__index` 不再指向裸 `_G`；`pkg.cpp:168` 注释诚实化。栈索引一律用相对值 |
| **S1b 安装钩子权限门控（可选）** | 安装钩子设 `g_in_script_context` + 从包 `ezmk.toml` 加载权限（复用 `[utils.permissions]` + 0.2.5 legacy）；**决策点**：回归风险大则收口为 S1a，S1b 延 1.2.0 |
| **S2 包名校验** | `validate_pkg_name` 拒绝空/`.`/`..`/路径分隔符/盘符/绝对与隐藏前缀，与 `safe_extract_path` 同口径 |
| **S3 URL 完整性** | 无 sha256 的 URL 安装走 `confirm()` 流程（`-y` 可跳过）；显式 `http://` 提示 MITM 风险建议 `https://`；不新增 flag |
| **S4 flags 引号** | `join_shell_args` 元字符集补全为 shell 全部相关字符（黑名单保守方向）；`build.cpp` 链接 flags 双引号包裹 + `escape_shell_arg` |
| **S5 编辑器转义** | `open_in_editor`/`find_editor` 经 `escape_shell_arg` + 双引号；移除全库唯一 `system()`（顺带） |
| **C1 SDE 安全解析** | `stoull` try/catch → `util::warn` + `sde=0`（与未设置走 fallback 一致），不再崩 worker 线程 |
| **C2 `~` 边界** | 仅 `prefix[0]=='~'` 且 `[1]` 为 `/` 或 `\` 才展开；单独 `~` → home；`"~abc"` 字面保留 |
| **C3 OVERLAPPED 池实例化** | `g_overlapped_pool` 从文件级全局改为 `FileWatcher` 实例成员（消除多实例悬垂）；不便改则注释 + 单实例断言收口 |
| **C4 `recursive_` 删除** | 字段本就未生效，删除并文档注明「仅监听目录本身，非递归」；行为不变 |
| **Q1 JSON → nlohmann** | `load_links_json` 改用项目已依赖的 `nlohmann/json`，返回结构/调用点签名不变；顺带修未 catch 传播路径 |
| **Q2/Q3 纯提取重构** | `parse_config`/`compile_one_source` 拆私有 helper、cli 三处 `parse_*` 提取共享 helper；只提取不改行为，每步跑单测 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 钩子沙箱收敛（S1a） | 依赖 `dofile`/`loadfile`/`require`/`debug` 的钩子脚本 | 这些函数变 `nil`；官方钩子无依赖；文档注明 |
| 安装钩子权限门控（S1b，若实现） | 未声明权限的安装钩子 | 保持全权限 + deprecation 警告（0.2.5 模式）；声明权限后按权限执行 |
| 包名校验（S2） | 含路径字符的包名 | 安装中止并报错；正常包名不受影响 |
| URL 安装确认（S3） | 无 sha256 的 URL 安装 | 需确认（`-y` 可跳过）；`http://` 提示 MITM 风险 |
| flags 引号（S4） | 含 `;|&` 等字符的 flags | 修复前 POSIX 可注入；修复后字面传递；Windows 行为不变 |
| 编辑器转义（S5） | `EDITOR`/`VISUAL` 含 shell 元字符 | 修复前可注入；修复后字面打开 |
| SDE 解析（C1） | `SOURCE_DATE_EPOCH` 非数字 | 修复前崩线程；修复后警告 + 视为未设置 |
| `~` 前缀（C2） | `"~abc"` 类前缀 | 修复前截断为 `"c"`；修复后按字面保留 |
| OVERLAPPED 池（C3） | 多 watcher 实例 | 修复前悬垂风险；修复后实例隔离 |
| `recursive_` 删除（C4） | 无（字段本就未生效） | 行为不变 |
| JSON 解析器（Q1） | 含 Unicode/反斜杠转义的合法 JSON | 修复前解析错/失败；修复后正确解析；畸形 JSON 报错清晰 |
| Q2/Q3 重构 | 无 | 纯提取，零行为漂移 |
| 无新命令 / 无弃用 | 无 | 1.1.3 是纯修复补丁，不触碰 1.1.0 稳定 API |

---

## 6 延后项

- **安装钩子权限门控（S1b）**：若阶段一决策点收口为仅 S1a，权限门控记入本项，归 1.2.0。
- **watcher 静默死亡 / kqueue 缺口**（1.1.2 延后项承接）：文件监控可靠性缺口，归 1.2.0 或后续补丁。
- **宽窄字符路径 / i18n 二次替换**（1.1.2 延后项承接）：归 1.2.0 或后续补丁。
- **CLI 参数嵌入 NUL**：本版仅注释声明已知限制（TODO），完整防御归 1.2.0。
- **OVERLAPPED 池若收口为注释 + 断言**：实例化重构归 1.2.0。
