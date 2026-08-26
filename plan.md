# EazyMake 1.3.5 执行计划

> **状态：执行中**。1.3.x 系列路线图见 [`plans/1.3.x/README.md`](plans/1.3.x/README.md)。
>
> 详细设计：[**1.3.5.md**](plans/1.3.x/1.3.5.md)。本计划为 1.3.0 发布后的**补丁版本**（1.3.x 系列最后一个规划补丁）：`ezmk project pack --format zip|tar.gz` 产**多格式归档**（缺省 `tar.gz` 现状不变；zip 走 vendored miniz）——内容与 tar.gz **逐文件等价**（同一 stage 流程，仅归档器不同），`pkg install` 消费路径（`extract_archive`）早已支持 zip，端到端闭环。附带 **`.sha256` 边车**（两种格式统一，`<archive>.sha256`）。
>
> **范围边界**：**明确不做** `.deb` / `.rpm` 等包管理器分发格式（由 `fpm` + `ezmk project install --prefix <staging>` 配方覆盖，non-goals 方向）与发布自动化。`--format` 与 `--precompiled` 可组合。**公共 API 无破坏性变更**（纯新增 flag + util + i18n key）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 904 用例 / 5241 断言，1.3.4 后实测；原基线 863/5007）。

---

## 1 背景

- `pack_project`（`src/build.cpp`）只产出 `.tar.gz`（`util::create_targz`），而 `extract_archive` 早已同时支持 `.tar.gz` / `.tgz` / `.zip`（zip 走 vendored miniz）——**消费 zip 早已可用，产出 zip 是缺口**。
- Windows 用户免 tar 直接解压；发布生态（winget / 仓库 `file` 字段）常见 zip；`pkg install` 端到端闭环。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `ezmk project pack --format zip`：产出 `name-version.zip`，内容与 tar.gz 逐文件等价（同一 stage 流程） | P0 |
| 2 | 默认行为不变：无 `--format` → `.tar.gz`；非法格式值 → fatal | P0 |
| 3 | `.sha256` 边车：pack 成功后写 `<archive>.sha256`（tar.gz 与 zip 统一） | P1 |
| 4 | 测试：zip 端到端（`pkg install` 消费）+ zip/tar.gz 内容等价性 + 默认回归 + 非法格式 | P0 |
| 5 | 文档：cli.md（en/zh）/ README / CHANGES.md 1.3.5 条目 | P1 |
| 6 | 明确不做：`.deb` / `.rpm` / 其他包管理器格式；发布自动化 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：CLI

- [ ] **1.1 `--format` 解析**：`project pack` spec（`src/cli.cpp`，现有 `--precompiled`/`--output`）加 `{'\0', "format", true}`；`PackOptions.format`（`include/ezmk/cli.hpp`，缺省 `"tar.gz"`）；合法值 `tar.gz`/`zip`（大小写不敏感）；非法 → fatal（新 i18n key `cli_err_invalid_format`）
- [ ] **1.2 i18n**：`cli_err_invalid_format` 三向一致 + `check_i18n.py` 通过
- [ ] **1.3 单测**：合法（`zip`/`ZIP`/`tar.gz`）/ 非法（`deb`）/ 缺省（`tar.gz`）

### 阶段二：`create_zip` + 归档分发

- [ ] **2.1 `util::create_zip(source_dir, output_file)`**（`src/util.cpp` + `include/ezmk/util.hpp`）：miniz `mz_zip_writer_*`；**`/` 分隔符统一**（坑 1）+ 路径规范化（相对、无 `..`，坑 2）+ 复用 `create_targz` 的目录遍历/排序
- [ ] **2.2 `pack_project` 按格式分发**：`tar.gz` → `create_targz`（现状）；`zip` → `create_zip`；归档名 `name-version.zip`；**zip 内部结构与 tar.gz 等价**（顶层 `name-version/` 目录 + 相对路径，保证 `pkg install` 顶层目录探测一致）

### 阶段三：SHA-256 边车

- [ ] **3.1 pack 成功后写 `<archive>.sha256`**（`crypto::sha256_file` 已有）：`{hash}  {filename}` 格式，两种格式统一；纯新增文件不影响既有消费路径

### 阶段四：测试

- [ ] **4.1 用例**：① zip 端到端（`pack --format zip` → `pkg install <zip>` 成功，闭环消费）② 等价性（同项目 tar.gz/zip 解包后文件清单 + 关键内容哈希一致）③ 默认回归（无 `--format` → `.tar.gz`）④ 非法格式 fatal ⑤ 边车（两种格式 `.sha256` 与 `crypto::sha256_file` 一致）
- [ ] **4.2 全量回归**（基线 904/5241）

### 阶段五：文档 + 收口

- [ ] **5.1 文档**：cli.md（en/zh）pack 节补 `--format` + `.sha256` 边车；README 命令表；CHANGES.md 1.3.5 条目
- [ ] **5.2 收口**：plan.md 勾选；`plans/1.3.x/README.md` 状态更新（1.3.5 完成 → 1.3.x 系列全部收口）；发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 同一 stage 流程，仅归档器分发 | 内容逐文件等价；`pkg install` 顶层目录探测行为一致 |
| zip 内部 `/` 分隔符 | 与 tar.gz 打包结构一致；Windows 路径不产生反斜杠条目（坑 1） |
| 写侧路径规范化 | 只遍历 stage 内文件、相对路径无 `..`（坑 2，对齐 `safe_extract_path` 思路） |
| miniz 独立实现 | 复用 `create_targz` 的目录遍历/排序；内存缓冲写 |
| `.sha256` 边车统一 | 纯新增文件，不破坏既有产物；发布/仓库索引可直接引用 |
| 默认 `tar.gz` 不变 | 无 `--format` 时行为与现状完全一致（坑 4） |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `--format zip` | 纯新增 flag | 缺省 `tar.gz`，默认行为完全不变 |
| `.sha256` 边车 | 纯新增文件 | 不破坏既有产物与消费路径 |
| zip 产物 | 新增形态 | 内容与 tar.gz 逐文件等价；`extract_archive` 已支持消费 |
| 公共 API | 无破坏性变更 | 纯增量（新 flag + 新 util + 新 i18n key） |

## 6 延后项（明确收口）

- **`.deb` / `.rpm` 等包管理器格式**：明确不做（fpm 配方覆盖，non-goals 方向）；发布自动化不进 CLI。
- **`--format` 扩展其他归档（`tgz` 别名等）**：归 1.4.0 或后续评估。
- **sha256 边车纳入 `pkg install --sha256` 自动校验**（与 index.toml 联动）：归 1.4.0 或后续评估。
- **2.0.0**：保持破坏性变更窗口，与本版解耦。
