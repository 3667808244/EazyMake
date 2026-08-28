# EazyMake 1.4.0-dev.5 执行计划

> **状态：已完成（收口）**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.0-dev.5.md**](plans/1.4.x/1.4.0-dev.5.md)。本计划为 1.4.0 第五个开发子版本，主题为**功能收口（1.3.x 延后项）**——集中收口四项小而独立的功能：① `watch --run -- <args>` 参数透传；② `workspace watch` 命令组 + `-w` 重定向扩展；③ `--format tgz` 别名；④ `pkg install` 本地归档 sha256 边车自动校验。
>
> **范围边界**：**明确不做**——`workspace watch --run` 的 `--` 成员级透传、watch 的 profile 热切换（1.4.0 后续或 1.5.x）。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 959 用例 / 5492 断言，1.4.0-dev.4 后实测）。

---

## 1 背景

- 1.3.x 各补丁把一批"小而独立"的功能项明确延后到 1.4.0：1.3.4 的 `watch --run` 无 `--` 透传、1.3.0 的 `workspace watch` 缺失（`-w` 目前只重定向 build/test/clean）、1.3.5 的 `tgz` 别名缺失、1.3.5 的 sha256 边车未纳入自动校验。
- 本版集中收口（每项独立、风险低、互不依赖）。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `ezmk watch --run -- <args>`：透传参数给被运行产物（复用 `run_executable` 的 args 通道） | P0 |
| 2 | `ezmk workspace watch [--member ...]`：成员级 watch（复用 workspace 定位 + watch 语义；`-w` 重定向扩展到 watch） | P0 |
| 3 | `--format tgz` 别名 = `tar.gz`（大小写不敏感，归一化） | P0 |
| 4 | `pkg install <pkg>.tar.gz` 时自动读取同目录 `.sha256` 边车校验（无 `--sha256` 时）；index.toml `sha256` 字段联动（已有） | P0 |
| 5 | 测试：四类各独立用例 + 全量回归 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：watch `--` 透传（3.1）

- [x] **1.1 watch spec 改造**（`src/main.cpp`）：watch 不再 `reject_positionals`；允许 `--` 后透传参数（对齐 `project run` 的 `program_args` 通道）
- [x] **1.2 接线**：`run_watched_exe` → `run_executable(exe, watch_program_args, true)`（1.3.6 已建通道，仅接线）
- [x] **1.3 集成测试**（`test_watch.cpp`）：`watch --run -- <args>` 透传断言（`--` 终止符语义既有，单测锁定）

### 阶段二：workspace watch（3.2）

- [x] **2.1 命令组**（`src/cli.cpp`）：`workspace_cmd_spec` 补 watch（复用 `-j`/`--member` flag）；`kAliases` 加 `ww`；`-w` 重定向扩展到 watch（`ezmk watch -w` ≡ `ezmk workspace watch`）
- [x] **2.2 执行**（`src/workspace_build.cpp`）：成员级 watch——每个成员跑 `ezmk watch`（子进程，复用 `run_member` 的 cwd 模型）；聚合输出带前缀（复用 `print_prefixed`）；拓扑层内互不依赖才并行、同层依赖 watch 串行（坑 1）；`--stop-on-error` 语义沿用
- [x] **2.3 集成测试**（`test_workspace.cpp`）：workspace watch 基本流转 + `-w` 重定向 + `--member` + 前缀输出

### 阶段三：tgz 别名（3.3）

- [x] **3.1 归一化**（`src/cli.cpp`）：`--format` 合法值集合 `tar.gz` / `tgz` / `zip`；`tgz`（大小写不敏感）归一化为 `tar.gz`（归档名仍 `name-version.tar.gz`）
- [x] **3.2 单测**（`test_cli.cpp`）：`tgz` / `TGZ` / `Tgz` → `tar.gz`；非法值仍报错

### 阶段四：边车自动校验（3.4）

- [x] **4.1 读取校验**（`src/pkg.cpp`）：`install_package` 本地归档路径——`expected_sha256` 为空且 `<archive>.sha256` 存在 → 读取并校验（格式 `<hash>  <filename>`，1.3.5 产出）；边车缺失/格式非法 → 现状（跳过校验，不阻断）
- [x] **4.2 优先级**：显式 `--sha256` 优先；边车仅空 `--sha256` 时启用（坑 3）；URL 安装仍走显式 `--sha256`（不信任 URL 伴生边车）
- [x] **4.3 单测/集成测试**（`test_pkg.cpp`）：边车存在 → 校验通过/失败阻断；边车缺失/格式非法 → 跳过不阻断；显式 `--sha256` 优先于边车

### 阶段五：文档 + 收口（3.5）

- [x] **5.1 cli.md（en/zh）**：watch 节 `--` 透传、workspace 节 `watch` 命令 + `-w` 扩展、pack 节 `tgz` 别名、pkg 节边车自动校验
- [x] **5.2 CHANGES.md**：1.4.0-dev.5 条目（新增 / 行为变更 / 文档 / 已知限制）
- [x] **5.3 全量零回归**：`bash build.sh test-all`（基线 959/5492 → **968/5588**，+9 用例/+96 断言，3 跳过为既有环境限制）
- [x] **5.4 文档收口**：plan.md 勾选；`plans/1.4.x/README.md` 状态更新；发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。**本版门槛全部满足，dev.5 收口。**

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| watch 透传复用 `run_executable` args 通道 | 1.3.6 已建通道，仅接线；`--` 终止符语义既有（`parse_options` 支持） |
| workspace watch 为成员级子进程 | 复用 `run_member` cwd 模型 + `print_prefixed` 前缀聚合；与 build/test 一致 |
| 拓扑层内互不依赖才并行 | 成员共享 build 目录时并行会冲突（坑 1）——同层依赖 watch 串行，文档注明 |
| `tgz` 归一化为 `tar.gz` | 归档名不变（`name-version.tar.gz`）；大小写不敏感 |
| 边车仅空 `--sha256` 时启用 | 显式优先（坑 3）；URL 安装不信任伴生边车；边车缺失/非法 → 跳过不阻断 |
| 每阶段独立提交 | 四类功能互不依赖，逐阶段落地可独立验证 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| watch `--` 透传 | 纯新增 | 无 `--` 时不变 |
| `workspace watch` | 纯新增命令 | `-w` 重定向扩展（watch 此前无 `-w`） |
| `tgz` 别名 | 纯新增 | `tar.gz` 行为不变 |
| 边车自动校验 | 行为增强 | 仅边车存在且无显式 `--sha256` 时启用；失败不阻断 |
| 公共 API | **无破坏性变更** | 纯增量 |

## 6 延后项（明确收口）

- **`workspace watch --run` 的 `--` 成员级透传**：1.4.0 后续或 1.5.x。
- **watch 的 profile 热切换**：1.4.0 后续或 1.5.x。
- **2.0.0**：保持破坏性变更窗口，与本版解耦。
