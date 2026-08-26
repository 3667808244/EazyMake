# EazyMake 1.4.0-dev.2 执行计划

> **状态：执行中**。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)。
>
> 详细设计：[**1.4.0-dev.2.md**](plans/1.4.x/1.4.0-dev.2.md)。本计划为 1.4.0 第二个开发子版本，主题为**工具链能力表 + 标准校验严格化**——收口 1.3.1 的两个遗留点：① `toolchain::max_supported_std(family, version)` 能力表（语义 C 与 dev.3 编译协商的前置）；② `[pkg] strict_std_check` 可配置开关（1.3.1 兼容校验 warn → error，默认仍 warn）。**1.3.1 语义 A（编译取 min）不变**。
>
> **范围边界**：**明确不做**——CLI 全局 `--strict` flag（设计 §3.2：1.4.0 最小面，配置字段即可）；语义 C（"支持多高用多高"）启用（建表但启用归 dev.3/后续评估）；`[compile].language` 的 `max_supported_std` 配置期校验（dev.4 或后续）；`max_supported_std` 返回 C/C++ 双上限（签名无语言参数，返回 C++ 主轴规范形，C 侧同代上限在头注释注明）。**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 931 用例 / 5396 断言，1.4.0-dev.1 后实测）。

---

## 1 背景

- 1.3.1 落地了区间语言标准（语义 A：编译取 min）与安装期标准兼容校验（warn 不 fail）。
- 遗留点 1：`ver_map` 是配置侧白名单，与真实编译器能力无关——语义 C 与 dev.3 编译协商需要「当前编译器最高支持哪个 `-std=`」，先建 `max_supported_std(family, version)`。
- 遗留点 2：1.3.1 校验是「信息性不 fail」（严格 fail 会破坏现有包生态），本版给可配置开关 `[pkg] strict_std_check = true`，默认仍 warn。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `toolchain::max_supported_std(family, version)`：给定工具链（gcc/clang/msvc + 版本）返回最高可用 `-std=` 规范值（如 gcc 11 → `CPP20`） | P0 |
| 2 | 能力表来源：MSVC 走 `_MSC_VER` 分段（复用 `toolchain.cpp` 已有先例）；GCC/Clang 走版本号分段表（注释标注来源/日期） | P0 |
| 3 | 校验严格化开关：标准兼容校验 warn → error 可配（`ezmk.toml [pkg] strict_std_check`）；默认不变（warn） | P0 |
| 4 | 严格化错误沿用 1.3.1 的 `pkg_warn_std_mismatch*` 语义（改 fatal 措辞）；i18n 新 key 三向一致 | P1 |
| 5 | 测试：能力表分段断言（各 family 关键版本/未知）+ 严格化开/关两态 + 默认回归 | P0 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：能力表（4.1）

- [ ] **1.1 `max_supported_std()`**（`toolchain.cpp` + `toolchain.hpp` 声明）：`(family, version) → "CPP<n>"` 规范形；版本号取 major 比较（复用 `first_version_major`）；未知版本 → 保守下限 `CPP11`
- [ ] **1.2 GCC 分段表**：<5 → CPP11；5–6 → CPP14；7 → CPP14（保守，7 的 C++17 不全）；8–10 → CPP17；11–12 → CPP20；≥13 → CPP23——注释标注来源（gcc cxx-status）与实测日期
- [ ] **1.3 Clang 分段表**：<4 → CPP11；4 → CPP14；5–10 → CPP17；≥11 → CPP20（16+ 的 C++23 部分支持 → 保守 CPP20）
- [ ] **1.4 MSVC 分段表**：`msvc_msc_ver()` 从 cl 版本行解析 `_MSC_VER`（复用 `parse_digits`，与 `msvc_toolset_tag` 共享解析 helper）；1910–1919 → CPP17；≥1920 → CPP20（1930+ 的 C++23 视配置 → 保守 CPP20）；<1910/未知 → CPP11
- [ ] **1.5 单测**（`test_toolchain.cpp`）：各 family 关键版本（gcc 4.8/5/8/11/13、clang 3.4/4/5/11/16、msvc 19.0/19.10/19.20/19.30）+ 未知版本 → CPP11

### 阶段二：严格化开关（4.2）

- [ ] **2.1 `[pkg]` 配置节**（`config.hpp` `PkgSection` + `config.cpp` 解析）：`strict_std_check` 布尔默认 `false`（纯新增字段）
- [ ] **2.2 `check_std_compat` 两态**（`pkg.cpp`）：`consumer_std_min()` 重构为 `consumer_std_ctx()`（min + strict + consumer 声明语言一次解析）；不匹配时 strict → `util::fatal`（新 key），非 strict → 现状 warn
- [ ] **2.3 i18n**：新增 `pkg_fatal_std_mismatch`（en/zh/zh-TW 三向一致，`check_i18n.py` 通过）；`pkg_warn_std_mismatch*` 保留
- [ ] **2.4 单测**（`test_pkg.cpp`）：`ConsumerProject` 支持 strict 参数——strict on + 包超消费者 → fatal（源码/预编译两路径）；strict off（默认）→ warn 不变；包在消费者标准内 → 静默

### 阶段三：文档 + 收口（4.3）

- [ ] **3.1 config_file.md（en/zh）**：`[pkg]` 节（`strict_std_check` 字段 + 「仅 CI/严格要求时开启」说明 + 与 1.3.1 校验的关联）
- [ ] **3.2 CHANGES.md**：1.4.0-dev.2 条目（新增 / 行为变更 / 文档 / 已知限制）
- [ ] **3.3 全量零回归**：`bash build.sh test-all`（基线 931/5396）
- [ ] **3.4 文档收口**：plan.md 勾选；`plans/1.4.x/README.md` 状态更新；发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 返回 C++ 主轴规范形 | 签名无语言参数 → 返回 `"CPP<n>"`（dev.3 协商按声明语言取数）；C 侧同代上限（C17/C11）在头注释注明，避免双返回值的 API 膨胀 |
| 保守默认 | 未知版本 → CPP11；分段边界取保守侧（gcc 7 → CPP14、clang 16+ → CPP20、MSVC 1930+ → CPP20）——能力表低估比高估安全（高估会产出编不过的协商结果） |
| `[pkg]` 新配置节 | 纯新增可选节；默认 off 行为与 1.3.1 完全一致（零回归） |
| 消费端读取 strict | strict 标志来自**消费者** `ezmk.toml`（安装动作发生在消费者项目内），与 `consumer_std_min()` 同源一次解析 |
| 复用既有解析 | `first_version_major`/`parse_digits` 与 `msvc_toolset_tag` 的 cl 版本行解析抽共享 helper，不复制逻辑 |
| 能力表来源可追溯 | 每 family 分段表注释标注来源（gcc/clang cxx-status、MSVC 版本表）与实测日期 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `max_supported_std()` | 纯新增 | 内部工具函数；dev.3 消费 |
| `[pkg] strict_std_check` | 纯新增配置节/字段 | 默认 `false`，行为不变（warn） |
| 严格化 fatal | 仅显式开启时 | 默认路径零影响 |
| `consumer_std_min` → `consumer_std_ctx` | 内部重构 | 仅 `check_std_compat` 唯一调用点同步 |
| 公共 API | **无破坏性变更** | 纯增量 |

## 6 延后项（明确收口）

- **CLI `--strict`**：不引入（设计 §3.2 最小面，配置字段即可）。
- **语义 C 启用**（"支持多高用多高"）：本版只建表，启用归 dev.3/后续评估。
- **`[compile].language` 超能力校验**（配置期拒绝）：dev.4 或后续。
- **2.0.0**：保持破坏性变更窗口，与本版解耦。
