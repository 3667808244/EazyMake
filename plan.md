# EazyMake 1.4.0-dev.7 执行计划

> **状态：执行中**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.0-dev.7.md**](plans/1.4.x/1.4.0-dev.7.md)。本计划为 1.4.0 第七个开发子版本，主题为 **workspace scan（现有项目一键采纳）**——`ezmk workspace scan [<dir>] [--dry-run] [-y]` 递归扫描目录树收集成员，生成 / 合并更新 `ezmk-workspace.toml`。经用户确认**插队**：dev.6 收口后重新打开 dev 阶段，唯一新增功能项。
>
> **范围边界**：`scan --prune`（移除已消失成员）、成员依赖自动推断、非 workspace 场景零改动。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 968 用例 / 5612 断言，1.4.0-dev.6 后实测）。

---

## 1 背景

- workspace（1.3.0）解决「一个目录下若干独立项目的批量管理」，但创建 workspace 仍靠手写 `ezmk-workspace.toml`——对已有目录树（棕地：克隆仓库 / CMake 转来 / 手写 ezmk 项目）逐个列 members 是最大上手障碍。
- `project new`（绿地）与 `project import --from cmake`（单项目采纳）已覆盖；workspace 侧缺少「采纳现有目录树」命令。本版本补上 `ezmk workspace scan`。
- 三个已确认决策：命令名 `workspace scan`；版本槽位 1.4.0-dev.7；已有文件走**合并更新**（保留 name/options/已有成员/注释，追加缺失成员）。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `ezmk workspace scan [<dir>]`：递归扫描收集成员，生成 `ezmk-workspace.toml` | P0 |
| 2 | 已有文件合并更新（toml++ 往返保留注释 / name / options） | P0 |
| 3 | 跳过规则：隐藏目录 / 嵌套 workspace 根 / 符号链接逃逸；扫描根自身不是成员 | P0 |
| 4 | `--dry-run` 预览 + `-y` 跳过确认 + `ws` 简写 | P1 |
| 5 | 单元 + 集成测试、文档、全量零回归 | P0 |

## 3 执行阶段

### 阶段一：设计文档与计划索引

- [ ] **1.1** `plans/1.4.x/1.4.0-dev.7.md` 设计文档
- [ ] **1.2** `plans/1.4.x/README.md`：dev.7 行 + 系列说明 + 依赖关系
- [ ] **1.3** `plans/README.md`：当前执行 + 汇总表 + 依赖图
- [ ] **1.4** `plan.md` 重写为 dev.7 执行计划（本文件）

### 阶段二：扫描 / 合并 / 写盘核心（workspace.hpp/cpp）

- [ ] **2.1** `scan_projects(root)`：递归 + 跳过规则（隐藏 / 嵌套根 / 逃逸）+ `/` 分隔 + 排序
- [ ] **2.2** `merge_members(existing, discovered)`：规范化（`\`→`/`、去尾 `/`）+ 去重 + 保留现有顺序 + 追加缺失
- [ ] **2.3** 写盘：新文件生成 + toml++ 往返合并（`toml_formatter`）+ 临时文件 + `util::atomic_rename`

### 阶段三：CLI 与分发（cli.hpp / cli.cpp / main.cpp / workspace_build.hpp/cpp）

- [ ] **3.1** `Command::WorkspaceScan` + `WorkspaceScanOptions`（dir / dry_run / assume_yes）+ 解析（positionals ≤ 1）
- [ ] **3.2** `ws` 简写（kAliases）+ `help_workspace_scan` 帮助行
- [ ] **3.3** `run_scan()`：定位扫描根（向上命中优先）→ 确认（`-y`/交互）→ 汇总分派（created / updated / no_change / dry-run / aborted / none）

### 阶段四：i18n（i18n_keys.def + locale/en.json + locale/zh.json）

- [ ] **4.1** 新增 13 键：`workspace_scan_*` × 11 + `workspace_err_scan_dir` + `help_workspace_scan`，三向一致

### 阶段五：单元测试（test_workspace.cpp + test_cli.cpp）

- [ ] **5.1** 扫描：多深度 / 隐藏 / 嵌套根（含子树排除）/ 根自身 / 排序 / `/` 斜杠 / 逃逸（POSIX 守卫）
- [ ] **5.2** merge：去重 / 顺序 / 追加 / 规范化（反斜杠 + 尾斜杠）
- [ ] **5.3** 写盘往返：注释 / name / options 保留；parse 后 members 合并
- [ ] **5.4** CLI 解析：`workspace scan`（dir / `--dry-run` / `-y`）、`ws` 简写、>1 positional、未知 flag

### 阶段六：集成测试（test_integration_workspace.cpp）

- [ ] **6.1** 扫描生成 → `workspace list`/`build` 可用（2 成员，含依赖注入）
- [ ] **6.2** 合并更新（options + 注释保留）
- [ ] **6.3** `--dry-run` 不写盘 / 空目录不写 / 子目录定位向上

### 阶段七：构建 + 全量回归

- [ ] **7.1** `bash build.sh` 编译通过（含新 i18n 键审计）
- [ ] **7.2** `bash build.sh test-all` 零失败（基线 968/5612 + 新用例，无回归）

### 阶段八：文档（cli.md en/zh + README + README_ZH + 教程 05 en/zh）

- [ ] **8.1** cli.md（en/zh）：workspace 节补 `scan` 命令 + 语义（定位 / 跳过规则 / 合并 / dry-run / -y / ws 简写）
- [ ] **8.2** README / README_ZH 命令速览补 `ezmk workspace scan`
- [ ] **8.3** 教程 05（en/zh）：新增「采纳现有项目」小节

### 阶段九：收口

- [ ] **9.1** `CHANGES.md` 1.4.0-dev.7 条目（新增 / 行为变更 / 测试 / 已知限制）
- [ ] **9.2** plan.md 全勾选 + plans README 状态更新
- [ ] **9.3** 门槛复核：清单完成 + API 无破坏性变更 + 全量零回归

## 4 关键设计决策

| 决策 | 结论 | 理由 |
|------|------|------|
| 命令名 | `workspace scan`（非 scan-project） | 简洁；简写 `ws` 与 wl/wb/wt/wc/ww 简写族一致 |
| 版本槽位 | 1.4.0-dev.7（插队） | 用户确认；dev.6 后重新打开 dev，pre.1 仍收口 |
| 已有文件 | 合并更新 | 保留 name/options/已有成员/注释；重扫=同步 |
| 写盘方式 | toml++ 往返 + 原子 rename | v3.4 `toml_formatter` 保留注释；`util::atomic_rename` 防损坏 |
| 依赖推断 | 不自动（用户意图） | 扫描只收成员，`[depends] workspace` 手工声明 |

## 5 兼容性矩阵

| 变更 | 影响 | 说明 |
|------|------|------|
| 新增命令 `workspace scan` | 纯新增 | `ws` 简写无冲突；scan 不接受 `-w` |
| 新增 i18n 键 | 纯新增 | zh-TW 变体缺键回退 zh |
| workspace.hpp/cpp 新增函数 | 纯新增 | 既有路径零改动 |
| 公共 API | 无破坏性变更 | 新命令 + 新 flag |

## 6 延后项

- `scan --prune`（移除已消失成员）——后续评估。
- 成员依赖自动推断（`[depends] workspace`）——用户意图，扫描不猜测。
