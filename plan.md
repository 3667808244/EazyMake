# EazyMake 1.2.3 执行计划

> **状态：执行中**（2026-08-18 启动）。1.2.x 系列路线图见 [`plans/1.2.x/README.md`](plans/1.2.x/README.md)。
>
> 详细设计：[**1.2.3.md**](plans/1.2.x/1.2.3.md)。本计划为 1.2.x 稳定线补丁：新增**顶层命令组 `ezmk example`**（`list` / `<name>` / `-o`），内置 6 个示例（hello / greeter / with-packages / with-tests / with-hooks / cmake-interop）。示例内容以**方案B（构建期嵌入资源）**存储：`examples/` 源目录为单一事实源，`scripts/embed_examples.py` 构建期生成 `src/example_data.cpp` 嵌入二进制（对齐 embed_locale/embed_logo 机制）——装好即用、离线可用、与版本同源。生成到 `./<name>/`（同 `project new`）；with-packages 在 CI 正常联网测试（GitHub runner 有完整出站网络）。
>
> **范围边界**：纯新增命令组——`Command::Example` + cli 解析 + `src/example.cpp` + i18n key + 嵌入管线（`scripts/embed_examples.py` + build.sh + `.gitignore`）+ 测试 + CI + 文档；**公共 API 无破坏性变更**（新增枚举值/命令为纯增量，1.2.x 窗口合法）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（1.2.1/1.2.2 基线 785 用例 / 3629 断言）。

---

## 1 背景

- 用户需要「复制即用」的示例获取通道；仓库 `examples/` 目录获取成本高、与版本漂移。
- 选定内置命令 `ezmk example <name>`（装好即用、离线可用、模板随二进制同版本）+ 方案B 构建期嵌入。
- 依赖 **1.2.1 模板差异化**：`greeter` 示例对齐 1.2.1 static 库骨架；与 **1.2.2 教程分类**联动（章节尾部指引）。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 顶层命令组 `ezmk example`：`list`（无参=列表）/ `<name>`（生成到 `./<name>/`）/ `-o|--output <dir>` | P0 |
| 2 | 方案B 存储：`examples/{hello,greeter,with-packages,with-tests,with-hooks,cmake-interop}/` + `embed_examples.py` 构建期嵌入 + build.sh 接入 | P0 |
| 3 | 6 个内置示例（中文注释讲解），与教程章节对应 | P0 |
| 4 | 测试：集成（list/生成/覆盖/已存在 fatal/与源文件一致）+ 生成后逐个 build/test | P0 |
| 5 | CI 自举验证：ci.yml 对全部 6 个示例 example → build/test（with-packages 联网） | P1 |
| 6 | 文档：cli.md / README 命令速览 / 教程章节尾部指引 / CHANGES.md 1.2.3 | P1 |
| 7 | 公共 API 无破坏性变更（纯新增顶层命令组） | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：示例源文件（4.1）

- [ ] **1.1 `examples/` 6 个完整示例**（ezmk.toml + 源码 + 中文注释，与教程对应）+ `examples/README.md` 索引（每项：一句话 + 对应教程 + 运行方式）

### 阶段二：嵌入管线（4.2）

- [ ] **2.1 `scripts/embed_examples.py`**：扫描 `examples/*/` → 生成 `src/example_data.cpp`（`{name, description, files[{path, content}]}` 表，对齐 embed_locale/embed_logo）
- [ ] **2.2 build.sh 接入**（locale/logo 嵌入后调用；python 缺失时生成空表 stub）+ `src/example_data.cpp` 加入 `.gitignore`
- [ ] **2.3 嵌入表完整性单测**：嵌入表文件集合 ≡ 源目录文件集合（防漂移）

### 阶段三：命令实现（4.3）

- [ ] **3.1 `Command::Example`**（cli.hpp 枚举新增）+ cli.cpp 子命令解析（list/<name>/-o，仿 project 组）
- [ ] **3.2 `src/example.cpp`**：按嵌入表写文件到 `./<name>/`（或 `<dir>/<name>/`），目录已存在 fatal，未知示例名 fatal（列出可用项）
- [ ] **3.3 main.cpp 分发 + i18n key 三向一致**（`example_list_header` / `example_list_item` / `example_created` / `example_not_found` / `example_exists`；`check_i18n.py` 通过）

### 阶段四：测试（4.4）

- [ ] **4.1 集成测试**：`ezmk example`/`list` 输出 6 项；`example hello` 内容与源文件一致；`--output` 覆盖、已存在 fatal、未知名 fatal；每个示例生成后 `build` 通过（with-tests 另跑 `test`；with-packages 联网 `pkg install`）
- [ ] **4.2 全量回归**：`bash build.sh test-all` 零失败（基线 785/3629）

### 阶段五：CI（4.5）

- [ ] **5.1 ci.yml**：Linux job 追加「自举验证 6 示例」步骤（`ezmk example <name>` → build/test；with-packages 联网正常测试）

### 阶段六：文档（4.6）

- [ ] **6.1 cli.md**（example 命令组）/ README 命令速览 / 教程章节尾部「运行 `ezmk example <name>` 获取完整示例」指引 / CHANGES.md 1.2.3 条目

### 阶段七：收口（4.7）

- [ ] **7.1 plan.md 全勾选** + 设计文档勾选 + `plans/1.2.x/README.md` 状态更新 + 发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 方案B 构建期嵌入 | `examples/` 源目录 = 单一事实源；嵌入表构建期生成 + 集成测试断言「嵌入表 ≡ 源目录」防漂移 |
| 顶层命令组（非 project 子命令） | `ezmk example` 独立成组；无别名（全拼已够短） |
| 生成到 `./<name>/` | 与 `project new` 一致；目标目录已存在 fatal |
| with-packages 联网测试 | GitHub runner 有完整出站网络（2023-05-02 起恢复），不打 `[network]` 跳过 |
| 中文注释示例 | 中文为基准；教程引用时双语可读 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `ezmk example` 顶层命令组 | 纯新增 | 不与既有命令冲突；无别名 |
| `examples/` 源目录 | 纯新增 | 随仓库分发；构建产物被根 .gitignore 覆盖 |
| `src/example_data.cpp` 构建期生成 | 构建产物 | 加入 .gitignore（对齐 locale_data.cpp） |
| 公共 API | 无破坏性变更 | 新增枚举值/命令为纯增量；1.2.x 窗口合法 |

## 6 延后项（明确收口）

- **示例数量与内容**：本版 6 个示例；更多示例（header-only、C 语言等）归 2.0.0 或后续补丁评估。
- **AUR**：不涉及本版（1.2.4 仓库文件夹包为下一补丁）。
