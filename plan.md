# EazyMake 1.3.1 执行计划

> **状态：已完成**。1.3.x 系列路线图见 [`plans/1.3.x/README.md`](plans/1.3.x/README.md)。
>
> 详细设计：[**1.3.1.md**](plans/1.3.x/1.3.1.md)。本计划为 1.3.0 发布后的**补丁版本**：`[project].language` 支持**区间语法**（`">=C++11"` 单边下界、`"C++11..C++17"` 双边区间），让库作者能声明"最低兼容标准"；安装期新增**标准兼容校验**（包 min > 消费者 min → 警告，防预编译库 ABI 断裂）；顺带修复 CMake 导出的裸数字提取 bug。
>
> **范围边界**：语义定死为 **A（元数据 + 校验）**——编译仍用单一精确标准（取 min），区间只表达最低要求（上界仅元数据）。**工具链能力表 + 编译协商（语义 B/C）预留 1.4.0**（文档待写）；本版纯增量，**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 863 用例 / 5007 断言，1.3.0-dev.5 后）。

---

## 1 背景

- `parse_language()`（`src/config.cpp:230-327`）只接受精确标准（`<语言><版本>`），直接映射唯一 `-std=`，无区间/约束语法。
- 库作者无法声明"最低兼容标准"（C++11 基础接口 + C++17 拓展接口需在头文件用 `#if __cplusplus` 表达，配置层无对应语义）。
- 包与消费者之间无标准匹配校验：`compile_package()`（`src/pkg.cpp:638`）用包自己的 `language` 独立编译，消费者用自己的 `language` 编译（`src/build.cpp:602`），预编译库 ABI 断裂风险（`std::__cxx11`，1.2.0-dev.10 已记录）在语言标准维度无门禁。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `parse_language()` 区间语法（`>=` 单边 / `..` 双边）+ `LanguageInfo.min_ver/max_ver` + `std_flag` 取 min | P0 |
| 2 | 精确值行为完全不变（无约束前缀走原路径） | P0 |
| 3 | 安装期标准兼容校验：包 min > 消费者 min → 警告（源码/预编译统一，不 fail） | P0 |
| 4 | CMake 导出修复：区间 `normalized_lang` 裸数字提取 bug（`CXX_STANDARD 1117`） | P0 |
| 5 | `EZMK_LANG` 宏定点（区间取 min 规范形） | P1 |
| 6 | 文档 + i18n（新 key 三向一致 + `check_i18n.py` 通过） | P1 |
| 7 | 工具链能力表 + 编译协商预留 1.4.0，本版收口不实现 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：解析层

- [x] **1.1 `LanguageInfo` 扩展**（`include/ezmk/config.hpp:190-196`）：加 `int min_ver = 0` / `int max_ver = 0`；`std_flag` 语义注释为"生效标志（区间取 min）"
- [x] **1.2 `parse_language()` 区间支持**（`src/config.cpp:230-327`）：`normalize_lang()` 后**约束剥离**（`>=` / `..`，拆 min/max，先行于 GNU 前缀识别 `:261`）；ver_map 校验；生成 `std_flag` = min；回填 `min_ver`/`max_ver`
- [x] **1.3 非法区间拒绝**：`C++17..C++11`（max<min）/ `>C++11` / `C++11+` → 抛错（新 i18n key `config_err_invalid_lang_range`）
- [x] **1.4 `normalized_lang` 定点**：区间时返回 min 规范形（`">=C++11"` → `"CPP11"`），`EZMK_LANG` 宏（`src/build.cpp:101-106`）不变
- [x] **1.5 单测**（`test/test_config.cpp`）：`>=C++11` / `C++11..C++17` / `>=GNUCPP11` / `>=C`（默认 11）/ 各非法形式 / 精确值回归

### 阶段二：安装期标准兼容校验

- [x] **2.1 helper**（`src/pkg.cpp`）：`int std_min_of(const std::string& language)` + `std::optional<int> consumer_std_min()`（`locate_project_root()` → `parse_config` → `project.language` 的 min；无 ezmk.toml → nullopt）
- [x] **2.2 调用点**：`compile_package()`（`pkg.cpp:638`）与 `select_precompiled_archive()`（`pkg.cpp:605`）开头校验，包 min > 消费者 min → 警告（新 key `pkg_warn_std_mismatch`；预编译措辞加强）；消费者 language 非法 → 警告并跳过
- [x] **2.3 测试**：高/低/无消费者项目三态；全量回归

### 阶段三：导出与宏一致性

- [x] **3.1 `export.cpp` 修复**（`:257-259`）：兜底改读 `lang.min_ver`，消除裸数字提取（坑 1）
- [x] **3.2 `test_export` 回归**：区间 language 导出 `CXX_STANDARD 11`（防 1117）；精确值导出不变
- [x] **3.3 `EZMK_LANG` 语义确认**（坑 2）+ 缓存签名语义确认（max 不进签名，坑 3）

### 阶段四：文档 + i18n

- [x] **4.1 文档**：`docs/en|zh/config_file.md` §language Format 区间小节；`docs/en|zh/package_authoring.md` / `docs/en|zh/pkg.md` language 字段；FAQ「invalid language format」补区间正例；`CHANGES.md` 1.3.1 条目
- [x] **4.2 i18n**：`i18n_keys.def` + `locale/en.json` + `locale/zh.json` 新增 `config_err_invalid_lang_range` / `pkg_warn_std_mismatch`，更新 `config_err_invalid_lang` 文案；重新生成 `locale_data.cpp`（`scripts/embed_locale.py`）；`check_i18n.py` 通过

### 阶段五：收口

- [x] **5.1 全量零回归**（基线 863/5007）
- [x] **5.2 文档收口**：plan.md 勾选；`plans/1.3.x/README.md` 状态更新
- [x] **5.3 发布门槛复核**：API 无破坏性变更 + 计划清单收口（工具链能力表/编译协商明确延后 1.4.0）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 语义 = A（元数据 + 校验） | 区间只表达最低要求（上界仅元数据）；编译仍用单一精确标准（取 min）——产物最大兼容、行为可预测 |
| 语法复用 `[depends]` 约束集 | `>=` 单边下界 + `..` 双边区间；不引入 `+` 后缀（语法面最小）；`>` / `+` 拒绝 |
| 约束剥离先行 | 在 GNU 前缀识别之前（`config.cpp:261`），`>=GNUCPP11` 自然支持 |
| `std_flag` = min | 生效标志取下界；min 变化 → 缓存签名变 → 自动失效（零代码改动） |
| max 不进缓存签名 | 改 max 只是元数据变更，不触发全量重建 |
| `normalized_lang` = min 规范形 | `EZMK_LANG` 宏对精确/区间写法定点不变（坑 2） |
| 校验 = warn 不 fail | 严格 fail 会破坏现有包生态（许多包默认 C++17）；严格化开关留 1.4.0（坑 4） |
| 预编译措辞加强 | 预编译 ABI 风险更高，警告复用 1.2.0-dev.10 的 ABI 警告风格 |
| 导入器不改 | CMake `CXX_STANDARD` 映射归 1.4.0，避免范围蔓延 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `parse_language` 区间语法 | 纯新增 | 无约束前缀走原路径，精确值行为完全不变 |
| `LanguageInfo` 加 `min_ver`/`max_ver` | 结构体扩展 | 新增字段有默认值；既有调用点零改动 |
| 安装期标准校验警告 | 行为增强 | 仅 `包.min > 消费者.min` 时提示；安装照常 |
| `export.cpp` 修复 | 行为修复 | 区间导出 `CXX_STANDARD` 1117 → 11；精确值不变 |
| `EZMK_LANG` 宏值 | 定点不变 | 区间配置的宏值 = min 规范形 |
| 旧二进制读区间配置 | 报 invalid language | 可接受；新语法需 ≥1.3.1 |
| 公共 API | 无破坏性变更 | 纯增量 |

## 6 延后项（明确收口）

- **工具链能力表**（`max_supported_std(family, version)`，语义 C 铺路）：**预留 1.4.0**（文档待写）。
- **编译协商（语义 B）**（包按 `max(包min, 消费者标准)` 编译，需打破包独立编译/共享缓存模型）：**预留 1.4.0**。
- **标准校验严格化开关**（warn → error 可配）：**预留 1.4.0**。
- **import.cpp `CXX_STANDARD` 映射**（`src/import.cpp:446-447` 硬编码 C++17 一并处理）：**预留 1.4.0**。
- **`+` 后缀别名 / 双边区间"验证到 max"（CI 矩阵）/ header-only 包标准声明**：1.4.0 或后续评估。
- **2.0.0**：保持破坏性变更窗口（`[test].flags` / `ezmk utils cc` 移除等），与本版解耦。
