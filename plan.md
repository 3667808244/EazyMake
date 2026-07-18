# EazyMake 执行计划 — 从代码重构到 1.0.0 正式发布

> 来源: [`plans/README.md`](plans/README.md)（版本路线图）、各版本详细计划文件（`plans/0.9.5.1.md` ~ `plans/1.0.0.md`）。

---

## 版本状态

```
0.9.0 → 0.9.1 → 0.9.2 → 0.9.3 → 0.9.4 → 0.9.5 → 0.9.5.1 → 0.9.6 (current) → 0.9.7 → 0.9.8 → 0.9.9 → 1.0.0
发布    默认仓库  文档     捆绑包   文档与   跨平台    代码重构    功能补全与        仓库扩展  CLI改进  安装钩子  正式版
正式版  创建      多语言   迁移     质量完善  体验与    与质量清理  生态完善          与扩充   Lua化    发布
                                                质量保障
```

已完成 17 个版本（0.1.6 ~ 0.9.5.1），当前正在执行 0.9.6（功能补全与生态完善），剩余 4 个版本后到达 1.0.0。

---

## 一、已完成版本：0.9.5.1 — 代码重构与质量清理 ✅

> 详细设计: [`plans/0.9.5.1.md`](plans/0.9.5.1.md) · 提交: `2958019`

### 1.1 完成内容

0.9.5.1 完成了四维度代码质量清理，消除了约 600+ 行重复代码，修复了资源管理泄漏风险，补全了测试盲区。

| 优先级 | 类别 | 完成内容 |
|--------|------|----------|
| P0 | 代码重复消除 | `link_phase()` 提取为 `execute_link()`；`compile_sources()` 调用 `compile_one_source()`；debounce 统一为 `check_and_flush()`；main.cpp 仓库自动更新去重；config.cpp 配置名校验去重 |
| P0 | 资源管理修复 | `g_cached_config` → `unique_ptr`；`OVERLAPPED*` → 内嵌值对象 |
| P1 | 测试盲区补全 | `compare_version()` 15+ 场景；`extract_archive()` 基础测试；依赖解析边界测试；共享 fixtures 提取到 `test_helpers.hpp` |
| P2 | 魔术字符串常量化 | 引入 `ezmk::path::*` 内联常量，7 个源文件全部迁移 |
| P2 | 死代码移除 | `ParsedOptions::count()`、`native_path()` 移除 |
| P3 | 头文件与 i18n 清理 | `file_watcher.hpp` 复用 `util.hpp` 平台宏；toolchain/cache 硬编码字符串 → i18n key |

---

## 二、当前版本：0.9.6 — 功能补全与生态完善（执行计划）

> 详细设计: [`plans/0.9.6.md`](plans/0.9.6.md)

### 2.1 背景与目标

0.9.4 和 0.9.5 分别补全了文档质量与跨平台体验，0.9.5.1 完成了代码质量清理（含魔术字符串常量化）。本版本聚焦 temp.md 分析中识别出的**最后一个核心功能缺口——依赖版本锁定**，以及若干提升开发体验的功能：构建进度显示、代码格式化基础设施、启动 Logo。

完成本版本后，EazyMake 在**功能、文档、质量、跨平台**四个维度上达到 1.0.0 标准。

### 2.2 四项交付

| # | 交付物 | 优先级 | 说明 |
|---|--------|--------|------|
| 1 | **依赖版本锁定** | P0 | `ezmk.toml` 中 `[depends]` 支持 `foo@1.2.3` / `^1.0` / `~1.2` / `>=1.0` 版本约束语法 |
| 2 | **构建进度显示** | P0 | 并行编译时显示 `[N/M] src/file.cpp` 逐文件进度 + 构建完成摘要 |
| 3 | **.clang-format 基础设施** | P1 | 项目根目录添加 `.clang-format` 配置 + `CONTRIBUTING.md` 代码风格章节 |
| 4 | **ASCII Logo** | P1 | 外部资源文件 `res/logo.txt` + `embed_logo.py` 编译期嵌入，`ezmk` 裸运行时显示（彩色、与 `--color` 联动） |

### 2.3 关键设计决策

- **版本锁定**：不引入锁定文件（`ezmk.lock`），留待 1.0.0 之后。向后兼容纯字符串格式（无约束）。
- **构建进度**：仅在 `-j > 1` 或 `--verbose` 时显示逐文件进度；单文件编译保持简洁。
- **ASCII Logo**：Logo 文本存放在外部资源文件 `res/logo.txt`，编译期通过脚本转换为 C 头文件再嵌入二进制，便于修改和审阅。
- **.clang-format**：不添加 `ezmk utils format` 命令，仅提供配置文件。

---

## 三、0.9.6 详细执行步骤

### 阶段一：依赖版本锁定（P0，预计占 40% 工作量）

#### 3.1.1 数据结构定义

**文件**: `include/ezmk/config.hpp`

- [ ] **T1.1** 新增 `VersionConstraint` 结构体：
  ```cpp
  struct VersionConstraint {
      enum Op { None, Exact, Compatible, Approx, Gte, Gt };
      Op op = None;
      std::string version;  // "1.2.3"
  };
  ```
- [ ] **T1.2** 新增 `DependsEntry` 结构体（`name` + `constraint`）
- [ ] **T1.3** 更新 `DependsSection`：`libs` 和 `want` 从 `std::vector<std::string>` 改为 `std::vector<DependsEntry>`

**验证**: 编译通过（此时下游代码会有编译错误，需配合阶段 1.3 一起修复）

#### 3.1.2 解析实现

**文件**: `src/config.cpp`

- [ ] **T1.4** 实现 `parse_depends_entry(raw: string) -> DependsEntry`：
  - 检测首个运算符 `@` / `^` / `~` / `>=` / `>`
  - 拆分包名和版本号
  - 无效格式 → 明确错误信息（附带原始字符串）
- [ ] **T1.5** 处理边界情况：多余空格、缺少版本号、无效版本号、运算符出现在中间位置
- [ ] **T1.6** 更新 `extract_depends()` / `extract_string_array()` 适配新旧格式混用：
  - `"foo"` → `DependsEntry{name="foo", op=None}`
  - `"foo@1.0"` → `DependsEntry{name="foo", op=Exact, version="1.0"}`

**验证**: 单元测试覆盖所有运算符 + 边界情况

#### 3.1.3 依赖解析应用约束

**文件**: `src/pkg.cpp`

- [ ] **T1.7** 在拓扑排序后的版本选择阶段，对每个 `DependsEntry` 应用约束过滤
- [ ] **T1.8** 复用现有 `compare_version()` 实现约束比较逻辑（`src/version.cpp`）
- [ ] **T1.9** 错误信息：
  - 精确版本不存在 → `"foo@1.2.3 not found, available: 1.2.0, 1.3.0"`
  - 兼容范围无匹配 → `"no version of foo satisfies ^3.6.0, available: 2.0.0, 4.0.0"`
- [ ] **T1.10** 确保 `op == None` 时行为与 0.9.5.1 完全一致（取最高版本，向后兼容）

**验证**: 依赖解析单元测试 + 端到端测试

#### 3.1.4 测试

**文件**: `test/test_config.cpp`, `test/test_pkg.cpp`

- [ ] **T1.11** 解析测试（`test_config.cpp`）：
  - 每种运算符各 1 个正向用例
  - 无效格式 × 5（无版本号、空版本号、重复运算符、非法字符、多余空格）
  - 新旧格式混用：`["foo", "bar@1.0", "baz^2.0"]`
  - 纯旧格式兼容：`["foo", "bar", "baz"]`
- [ ] **T1.12** 依赖解析测试（`test_pkg.cpp`）：
  - 精确版本匹配 / 不匹配
  - 兼容范围 `^1.2.3` 内 / 外
  - 近似范围 `~1.2.3` 内 / 外
  - `>=` / `>` 边界
  - 多约束混合（`lib` + `want` 各有约束）
  - 错误信息完整性验证

#### 3.1.5 文档更新

- [ ] **T1.13** 更新 `docs/en/config_file.md`：`[depends]` 章节新增版本约束语法说明
- [ ] **T1.14** 更新 `docs/zh/config_file.md`：同步中文翻译

---

### 阶段二：构建进度显示（P0，预计占 20% 工作量）

**涉及文件**: `src/build.cpp`, `include/ezmk/build.hpp`（或 `thread_pool.hpp`）

#### 3.2.1 进度计数

- [ ] **T2.1** 在编译调度层（`build.cpp` 或 `ThreadPool`）添加：
  ```cpp
  std::atomic<int> completed{0};
  int total;  // 总编译任务数（含缓存命中的）
  ```
- [ ] **T2.2** 每个编译任务完成时 `++completed`

#### 3.2.2 格式化输出

- [ ] **T2.3** 实现进度行格式化：`[ezmk] [{}/{}] {}{}` — 进度 + 文件名 + 可选 `(cached)` 标记
- [ ] **T2.4** 缓存命中时追加 `(cached)` 标记：
  ```
  [ezmk] [2/12] src/util.cpp  (cached)
  ```
- [ ] **T2.5** 构建完成摘要：`[ezmk] Build succeeded (X cached, Y compiled, Z.Zs)`
- [ ] **T2.6** 构建失败摘要：`[ezmk] Build failed (X cached, Y compiled, N error(s))`

#### 3.2.3 显示策略

- [ ] **T2.7** 仅在 `-j > 1` 或 `--verbose` 时显示逐文件进度
- [ ] **T2.8** 单文件编译（`-j 1` 且非 verbose）保持 0.9.5.1 简洁输出格式
- [ ] **T2.9** 输出互斥：进度行使用 `\r` 原地刷新（非 verbose 模式），或逐行输出（verbose 模式）

**验证**: 手动测试多文件项目 + `-j 4`，确认进度显示正确

---

### 阶段三：.clang-format 基础设施 + ASCII Logo（P1，预计占 25% 工作量）

#### 3.3.1 .clang-format 配置

- [ ] **T3.1** 创建 `.clang-format`（项目根目录）：
  - 基础风格：LLVM
  - 缩进：4 空格
  - 列宽：120
  - 指针/引用对齐：靠左（`int* p` 而非 `int *p`）
  - 大括号：函数/类/命名空间换行（与现有代码一致）
- [ ] **T3.2** 扫描 `src/` 和 `include/` 目录，确认 `.clang-format` 配置与现有代码风格偏差 ≤ 5%

#### 3.3.2 CONTRIBUTING.md 更新

- [ ] **T3.3** 新增"代码风格"章节：
  - `.clang-format` 文件位置和用途说明
  - 推荐 VS Code / CLion / vim 的 clang-format 集成方式
  - 不强制要求格式化现有代码（仅对新贡献推荐使用）
- [ ] **T3.4** 新增"提交前检查清单"条目：建议运行 `clang-format --dry-run`

#### 3.3.3 ASCII Logo

**设计原则**：Logo 文本存放在外部资源文件，编译期嵌入，不硬编码在 C++ 源码中。便于修改、审阅和复用（未来可用于 `--version` 输出、帮助信息等处）。

**资源文件**: `res/logo.txt`

```
  ______                __  ___
 /_  __/__ ___ _  __ _/  |/  /___ _____
  / / /_  __ `/ |/_/ / /|_/ / _ `/ __ \
 /_/ /_/ /_/ /_>  </ /_/ /_/  \_,_/_/ /_/
```

**嵌入脚本**: `scripts/embed_logo.py`

- [ ] **T3.5** 创建外部资源文件 `res/logo.txt`，仅包含 ASCII Logo 纯文本（不含版本号、不含 ANSI 颜色码）
- [ ] **T3.6** 编写 `scripts/embed_logo.py`，将 `res/logo.txt` 转换为 C 头文件 `include/ezmk/logo.gen.h`：
  - 生成 `constexpr const char* EZMK_LOGO = R"(...)";` 形式的常量
  - 使用原始字符串字面量 `R"(...)"` 保留换行和特殊字符
  - 输出文件包含 include guard，遵循项目现有 `*.gen.h` 命名惯例
- [ ] **T3.7** 更新 `build.sh`：在现有 `scripts/embed_locale.py` 调用之后，新增 `embed_logo.py` 调用步骤：
  ```bash
  python3 scripts/embed_logo.py res/logo.txt include/ezmk/logo.gen.h
  ```
- [ ] **T3.8** 在 `src/main.cpp` 中 `#include "ezmk/logo.gen.h"`，使用 `EZMK_LOGO` 常量：
  - `ezmk` 裸运行（无子命令）→ 显示 Logo + 版本号 + `--help` 等效信息
  - `ezmk --version` → 仅显示版本号（不显示 Logo）
  - `ezmk <any-command>` → 不显示 Logo
- [ ] **T3.9** 彩色输出：`main.cpp` 中对 `EZMK_LOGO` 包裹 ANSI 颜色码，与 `--color` 模式联动
  - `--color=always` / `auto`（TTY）→ 彩色 Logo
  - `--color=never` / `auto`（管道）→ 纯文本 Logo

**验证**: 手动测试 `ezmk` / `ezmk --version` / `ezmk --help` / `ezmk project build` 四种场景

---

### 阶段四：校验与收尾（预计占 5% 工作量）

- [ ] **T4.1** `build.sh` 编译通过（MSYS2 + Linux 双平台）
- [ ] **T4.2** 全量测试通过（`build.sh test-all` 或等效命令）
- [ ] **T4.3** 依赖版本锁定端到端测试：
  - 新建测试项目，`ezmk.toml` 中使用 `lib = ["fmt@10.0.0"]`
  - 验证精确版本匹配 / 不匹配报错 / 无约束取最新
- [ ] **T4.4** `.clang-format` 验证：在 `src/main.cpp` 上运行 `clang-format --dry-run`，确认偏差在可接受范围
- [ ] **T4.5** 更新 `CHANGES.md`，记录 0.9.6 版本变更
- [ ] **T4.6** git tag `v0.9.6` + commit

---

## 四、0.9.6 文件变更清单

| 文件 | 阶段 | 变更类型 | 说明 |
|------|------|----------|------|
| `include/ezmk/config.hpp` | 阶段一 | 修改 | 新增 `VersionConstraint` / `DependsEntry`；更新 `DependsSection` |
| `src/config.cpp` | 阶段一 | 修改 | 新增 `parse_depends_entry()`；更新 `extract_depends()` |
| `src/pkg.cpp` | 阶段一 | 修改 | 依赖解析应用版本约束 |
| `src/version.cpp` | 阶段一 | 可能修改 | 约束比较辅助函数（复用现有 `compare_version()`） |
| `test/test_config.cpp` | 阶段一 | 修改 | 版本约束解析测试 |
| `test/test_pkg.cpp` | 阶段一 | 修改 | 依赖解析约束测试 |
| `docs/en/config_file.md` | 阶段一 | 修改 | `[depends]` 版本约束语法文档 |
| `docs/zh/config_file.md` | 阶段一 | 修改 | 中文同步 |
| `src/build.cpp` | 阶段二 | 修改 | 进度计数 + 格式化输出 |
| `include/ezmk/build.hpp` | 阶段二 | 可能修改 | 进度相关声明 |
| `.clang-format` | 阶段三 | **新增** | LLVM 风格 + 项目定制 |
| `CONTRIBUTING.md` | 阶段三 | 修改 | 代码风格章节 |
| `res/logo.txt` | 阶段三 | **新增** | ASCII Logo 外部资源文件（编译期嵌入源） |
| `scripts/embed_logo.py` | 阶段三 | **新增** | Logo 资源 → C 头文件转换脚本 |
| `include/ezmk/logo.gen.h` | 阶段三 | **生成** | 编译期由 `embed_logo.py` 生成的 Logo 常量 |
| `build.sh` | 阶段三 | 修改 | 新增 `embed_logo.py` 调用步骤 |
| `src/main.cpp` | 阶段三 | 修改 | `#include "ezmk/logo.gen.h"` + Logo 显示逻辑 |
| `CHANGES.md` | 阶段四 | 修改 | 版本变更记录 |

---

## 五、风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| `DependsEntry` 替代 `string` 导致下游代码遗漏更新 | 中 | 编译错误 | 类型变更后编译器会捕获所有遗漏点 |
| 版本约束解析与 npm/cargo 行为不一致 | 低 | 用户困惑 | 参考 npm semver 语法；文档明确说明差异 |
| 构建进度输出破坏 IDE 解析 | 低 | 用户体验 | 检测 `TERM` / `NO_COLOR`；`\r` 原地刷新仅在 TTY 使用 |
| `.clang-format` 配置与现有代码偏差大 | 低 | 格式化噪音 | 先 dry-run 验证，调整配置直到偏差 ≤ 5% |
| `embed_logo.py` 脚本跨平台兼容 | 低 | 构建失败 | 仅使用 Python 标准库；已在 `embed_locale.py` 中验证过同类模式 |

---

## 六、兼容性矩阵

| 变更 | 影响面 | 处理方式 |
|------|--------|----------|
| `DependsEntry` 替代 `string` | `ezmk.toml` 解析 | 纯字符串格式继续工作（`op == None`） |
| 版本约束不满足 → 报错 | 依赖解析 | 仅当用户显式添加约束后才启用过滤；无约束 = 行为不变 |
| 构建输出格式变化 | 日志输出 | 概率低；`--verbose` 保持不变；脚本解析输出应使用 `--version` / exit code |
| 新增 Logo | `ezmk` 裸运行 | 纯增量，美化；`--version` 不变 |
| `.clang-format` | 无 | 新增文件，不影响功能 |

---

## 七、后续版本路线图

### 7.1 0.9.7 — 默认仓库生态扩展

> 详细设计: [`plans/0.9.7.md`](plans/0.9.7.md)

**新增 22 个预编译包**，覆盖常见 C/C++ 开发场景：

| 批次 | 包 | 数量 |
|------|----|------|
| 独立包 | `zlib`、`cli11`、`glfw`、`sdl2`、`yaml-cpp` | 5 |
| imgui 核心 | `imgui` | 1 |
| imgui 后端 | `imgui-glfw`、`imgui-opengl3`、`imgui-vulkan`、`imgui-dx11`、`imgui-win32` 等 | 16 |

**依赖 0.9.6**（版本锁定功能就绪后，可为新包声明精确版本约束）。

### 7.2 0.9.8 — CLI 改进与默认仓库扩充

> 详细设计: [`plans/0.9.8.md`](plans/0.9.8.md)

| # | 交付物 | 说明 |
|---|--------|------|
| 1 | **补全 `[ezmk]` 前缀** | `pkg info`、`repo info`、`repo list` 输出统一添加 `[ezmk]` 前缀 |
| 2 | **简写展开提示** | `--verbose` 时显示简写 → 全名映射（如 `ri → repo info`） |
| 3 | **Header-Only 包支持** | 实现 `pkg.toml` 的 `header_only = true`，跳过编译步骤 |
| 4 | **默认仓库扩充** | 新增 10 个 Boost header-only 子库 + 10 个 stb 单文件库 |

### 7.3 0.9.9 — 安装钩子 Lua 化

> 详细设计: [`plans/0.9.9.md`](plans/0.9.9.md)

消除最后的技术债务——安装钩子从 Shell 脚本迁移至 Lua：

| 变更 | 说明 |
|------|------|
| `.lua` 脚本优先 | `detect_install_script()` 优先返回 `.lua`（跨平台统一） |
| 新增 `run_install_hook_script()` | 安装钩子专用的 Lua 执行函数，传递 `ctx`（pkg_name/pkg_root/install_path/scope） |
| 安全模型对齐 | Lua 钩子运行在 sandbox 中，复用 `ezmk.*` API |
| 向后兼容 | 旧包的 `.sh`/`.ps1`/`.bat` 作为 fallback 继续工作 |

**里程碑意义**：0.9.9 完成后，项目实现"零技术债务"——构建钩子和安装钩子统一使用 Lua + sandbox。

### 7.4 1.0.0 — 正式版发布

> 详细设计: [`plans/1.0.0.md`](plans/1.0.0.md)

**不修改源代码**，聚焦三项收尾工作：

| # | 交付物 | 说明 |
|---|--------|------|
| 1 | **整理 `plans/` 目录** | 拆分为 `dev/`（0.1.6~0.2.6）和 `release/`（0.9.0~1.0.0）子目录 |
| 2 | **补全 `docs/zh/cli.md` 翻译** | 全文中文化（当前为英文副本） |
| 3 | **文档审计** | 10 项逐文件检查，修复过时内容（仓库声明、安装钩子、捆绑包引用等） |

此外补打缺失的 git tag（v0.9.3 ~ v0.9.9）并创建 v1.0.0。

---

## 八、关键里程碑

```
0.1.6 ────── 0.2.6 ────── 0.9.0 ──── 0.9.5 ──── 0.9.5.1 ──── 0.9.6 ──── 0.9.9 ──── 1.0.0
测试框架搭建   CLI/翻译收尾   发布准备     集成测试     代码清理      功能补全    技术债务清零   正式发布
(已完成)       (已完成)       (已完成)     (已完成)     ✅ 已完成      ← 当前 →    最后冲刺      终点
```

| 里程碑 | 版本 | 核心意义 |
|--------|------|----------|
| 功能完整 | 0.2.6 | CLI 完善、i18n 收尾——所有核心功能就绪 |
| 发布准备 | 0.9.0 | 一键安装、文档体系建立 |
| 质量保障 | 0.9.5 | 集成测试、三平台冒烟 |
| 代码清理 | 0.9.5.1 | 消除 600+ 行重复、RAII 修复、测试补全 ✅ |
| **功能补全** | **0.9.6** | **依赖版本锁定、构建进度、格式化基础设施 ← 当前位置** |
| 生态扩展 | 0.9.7 ~ 0.9.8 | 默认仓库从个位数扩展到 50+ 包 |
| 技术债务清零 | 0.9.9 | 安装钩子 Lua 化，全项目统一技术栈 |
| 🎉 正式发布 | 1.0.0 | 文档审计、翻译补全、打 tag |

---

## 九、跨版本关注点

### 9.1 向后兼容性

- `ezmk.toml` 格式扩展（版本约束语法）保持旧格式兼容
- CLI 接口稳定——所有新增 flag 为纯增量
- 0.9.5.1 的重构已保持二进制等价行为 ✅

### 9.2 安全模型

- Lua sandbox 贯穿所有钩子系统（构建钩子 0.2.3 + 安装钩子 0.9.9）
- SHA-256 校验（下载 + 安装全链路）
- Lua 安装钩子无需编辑器审查（sandbox API 边界已限定）

### 9.3 测试策略

- 单元测试：491+ 用例 / 2250+ 断言（0.9.5.1 补全盲区后）
- 集成测试：4 个端到端场景（0.9.5 引入，`[integration]` tag）
- 冒烟测试：三平台手动验证（Linux/macOS/Windows）
- 每阶段完成后立即运行全量测试，确保零回归

### 9.4 文档一致性

- `docs/en/` ↔ `docs/zh/` 一一对应（0.9.2 建立）
- 1.0.0 补全 `docs/zh/cli.md` 翻译（最后一块缺失的拼图）
- 新增功能需双语同步交付
- `plans/` 目录在 1.0.0 完成拆分（`dev/` + `release/`）

---

## 十、当前任务（0.9.6 执行中）

按优先级排列的四阶段工作：

1. **依赖版本锁定**（P0）— 数据结构 → 解析 → 依赖解析应用约束 → 测试 → 文档
2. **构建进度显示**（P0）— 进度计数 → 格式化输出 → 显示策略
3. **格式化 + Logo**（P1）— `.clang-format` 配置 → `CONTRIBUTING.md` → ASCII Logo（外部资源 + 编译期嵌入）
4. **校验与收尾** — 双平台编译 → 全量测试 → 端到端验证 → CHANGES.md → tag

每阶段完成后立即运行全量测试，确保零回归。
