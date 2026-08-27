# EazyMake 1.4.0-dev.4 执行计划

> **状态：执行中**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.0-dev.4.md**](plans/1.4.x/1.4.0-dev.4.md)。本计划为 1.4.0 第四个开发子版本，主题为**CMake 互操作补全**——收口 1.3.x 预留的两处缺口：① **导入**读 `CXX_STANDARD`/`C_STANDARD`/`target_compile_features` → 映射为 ezmk **区间** language（`">=CPP<N>"`，语义 A 兼容）；② **导出**在 dev.2 能力表就绪后确认 `CXX_STANDARD` 不超目标工具链能力（超 → 注释警告而非静默改值）。
>
> **范围边界**：**明确不做**——`target_compile_features` 全量语义映射、`CXX_STANDARD_REQUIRED` 严格化映射（1.4.0 后续或 1.5.x）；`CXX_EXTENSIONS ON` 的完整扩展语义（用 GNU 前缀表达或注释注明）；导入的 LANGUAGES 缺省回退（无 `CXX_STANDARD` → 现状 `C++17` 保留）。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 948 用例 / 5472 断言，1.4.0-dev.3 后实测）。

---

## 1 背景

- CMake 互操作两处预留缺口：`import.cpp` 对 `LANGUAGES CXX` 硬编码 `C++17`（不读 `CXX_STANDARD`，导入项目语言标准丢失）；`export cmake` 的 `CXX_STANDARD` 映射（1.3.1 修复取 min）未与 dev.2 能力表联动（导出标准可能超目标工具链能力）。
- 本版补全：导入读标准并映射为区间（`">=N"`，语义 A 兼容，dev.3 协商自动惠及导入项目）；导出在能力表就绪后确认/校正映射。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `import` 解析 `CXX_STANDARD N`（及 `C_STANDARD`）→ `[project].language = ">=CPP<N>"`（区间形式，语义 A） | P0 |
| 2 | `CXX_STANDARD_REQUIRED OFF` / `CXX_EXTENSIONS ON` → 保持精确值而非区间（扩展语义不同）或注释注明 | P1 |
| 3 | 缺省回退不变：无 `CXX_STANDARD` → 现状 `C++17`（文档注明"CMake 缺省下 ezmk 取 C++17"） | P0 |
| 4 | 导出：能力表（dev.2）就绪后，`CXX_STANDARD` 映射确认不超能力（超 → 注释警告而非静默） | P1 |
| 5 | 测试：导入含/不含 `CXX_STANDARD` 两态 + 区间断言；导出能力警告 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：导入映射（4.1）

- [ ] **1.1 `set_target_properties` 扫描**（`import.cpp` 解析链新增分支）：`PROPERTIES` 后的键值对——`CXX_STANDARD`/`C_STANDARD` → N；`CXX_EXTENSIONS` ON/OFF → GNU 前缀；`CXX_STANDARD_REQUIRED`（信息性，`>=` 语义天然兼容）
- [ ] **1.2 `target_compile_features` 扫描**：`cxx_std_<N>` / `c_std_<N>`（双路径，坑 1）
- [ ] **1.3 映射应用**：解析循环后——有标准 → `language = ">=CPP<N>"` / `">=C<N>"`（`CXX_EXTENSIONS ON` → `>=GNUCPP<N>` / `>=GNUC<N>`）；无 → 现状回退 `C++17`/`C17`（LANGUAGES 决定）
- [ ] **1.4 单测**（`test_import.cpp`）：含 `CXX_STANDARD`（区间断言）、不含（`C++17` 回退）、`target_compile_features` 路径、`C_STANDARD`（C 项目）、`CXX_EXTENSIONS ON`（GNU 前缀）

### 阶段二：导出能力确认（4.2）

- [ ] **2.1 `std_capability_note()`**（`export.cpp` + `export.hpp`，纯函数）：`(std_kind, std_ver, tc)` → 超能力注释文本（`# CXX_STANDARD <n> exceeds the target toolchain capability (<max>)`）；未超/工具链版本未知 → 空（保守不警告）
- [ ] **2.2 `build_cmake_text` 接入**：`CXX_STANDARD`/`C_STANDARD` 发射前调用（`toolchain::detect_toolchain()`，静态缓存）；正常导出输出不变
- [ ] **2.3 单测**（`test_export.cpp`）：正常（gcc 13 + C++17 → 无注释）、超能力（gcc 4.8 + C++17 → 注释含 max）、未知版本（空 version → 无注释）

### 阶段三：文档 + 收口（4.3）

- [ ] **3.1 cli.md（en/zh）**：import/export 节补标准读取/能力确认说明；config_file.md 区间引用（`">=CPP<N>"` 来自 CMake 导入）
- [ ] **3.2 CHANGES.md**：1.4.0-dev.4 条目（新增 / 行为变更 / 文档 / 已知限制）
- [ ] **3.3 全量零回归**：`bash build.sh test-all`（基线 948/5472）
- [ ] **3.4 文档收口**：plan.md 勾选；`plans/1.4.x/README.md` 状态更新；发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 导入产出区间 `">=CPP<N>"` | 与 1.3.1 区间语法对齐（语义 A：编译取 min=N）；`CXX_STANDARD` 的 CMake 语义本就是"至少 N"，`>=` 是精确映射；dev.3 协商自动惠及（min=N 参与） |
| 双路径扫描 | `set_target_properties` + `target_compile_features` 都提取标准（坑 1）；漏 → 回退现状不报错 |
| `CXX_EXTENSIONS` | 显式 ON → GNU 前缀（`>=GNUCPP<N>`）；显式 OFF/缺省 → 非 GNU（CMake 缺省 ON，但 ezmk 导入取保守非 GNU，文档注明可手改） |
| 导出能力确认纯函数 | `std_capability_note()` 可单测（注入 fake tc）；`build_cmake_text` 用 `detect_toolchain()`（静态缓存）接入 |
| 未知版本不警告 | 工具链版本解析失败（`compiler_tag` 为空）→ 跳过能力确认（坑 3：能力确认误报，保守） |
| 超能力只注释不改值 | CMake 项目可自行决定（设计 §3.2：不静默改值） |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 导入产出 `>=CPP<N>` | 行为增强 | 原硬编码 `C++17` → 读标准；无 `CXX_STANDARD` 项目不变（回退现状） |
| 导出超能力注释 | 纯新增注释 | 正常导出输出不变（逐字节相同） |
| 公共 API | **无破坏性变更** | 纯增量（解析增强 + 新纯函数） |

## 6 延后项（明确收口）

- **`target_compile_features` 全量语义映射**（`cxx_std_*` 的 PUBLIC/INTERFACE 传播）：1.4.0 后续或 1.5.x。
- **`CXX_STANDARD_REQUIRED` 严格化映射**（ON → ezmk 侧 fail-fast）：1.4.0 后续或 1.5.x。
- **2.0.0**：保持破坏性变更窗口，与本版解耦。
