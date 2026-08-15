# EazyMake 1.2.0-dev.9 执行计划

> **状态：执行中**（2026-08-15）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.9.md`](plans/1.2.0/1.2.0-dev.9.md)。本计划为 1.2.0 系列第九个开发子版本：**包构建配置收敛（dev.7 延伸）**——让包的 `[compile].src_dirs` / `[compile].include_dirs` 真正生效，收口包配置与项目配置的语义差距（`src_dirs` 被硬编码忽略、`include_dirs` 有重复 `-I` 边界缺口、包 `type` 语义未对齐）。
>
> **范围边界**：只动包侧（`src/pkg.cpp` / `src/cache.cpp` 自编译 `-I` 构造 / `collect_sources` 参数化），不碰 `export cmake` 与 `ezmk-lua`（dev.8 范围）；包 `src_dirs`/`include_dirs` 不在 export 范围（dev.2 边界）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（`collect_sources` 新增默认参数，项目路径零变化；纯新增 i18n key 与 `pkg info` 输出行）；③ 全量测试零回归（基线 709 用例 / 3296 断言，dev.8 后；Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

- **`src_dirs` 被静默忽略**：`compile_package()`（`src/pkg.cpp`）硬编码 `util::list_files(pkg_dir / "src")`，包作者写 `src_dirs = ["src", "generated"]` 看着有效实则无效，与项目侧 `build::collect_sources()` 多目录收集不一致。
- **`include_dirs` 基本生效但未固化**：包自编译 `-I` 已生效；消费者侧逐个加 `<pkg>/<include_dir>` 已生效；但 `include_dirs` 含默认 `"include"` 时与 `def_inc`（`proj_root/include`）重复 `-I`，且无测试固化。
- **包 `type` 语义未对齐**：包文档默认 `type = "executable"` 却编译成静态库，直接复用 `collect_sources(project_type)` 会把默认包误判为应用、误要求 `main.cpp`。
- **设计补充（validate_pkg）**：`validate_pkg()`（`src/pkg.cpp:259-263`）同样硬编码 `src/` 存在性校验——若不同步改为 src_dirs 感知，自定义 `src_dirs` 的包会在安装校验阶段即被拒绝，`compile_package` 的改造无法生效。归入阶段二一并落地。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 包源收集改用 `[compile].src_dirs`（默认 `["src"]`），复用 `build::collect_sources()`：多目录、缺失目录 warn+跳过、文件名去重 | P0 |
| 2 | 包编译不受 `[project].type` 的 main.cpp 校验影响（包总是静态库）；`collect_sources` 增加 `require_main` 参数，项目路径零变化 | P0 |
| 3 | 包 `include_dirs` 行为固化：相对包根解析、与默认 `include/` 保序去重、缺失目录跳过、`@link:` 解析——自编译与消费者两侧一致 | P0 |
| 4 | utils 包「有源码才编译」门控改 src_dirs 感知（对齐）；`pkg info` 增显 `src_dirs` | P1 |
| 5 | header_only / precompiled / utils 无源短路行为**完全不变**（回归基线） | P0 |
| 6 | 测试（单测 + 集成）+ 文档（pkg.md / package_authoring.md / config_file.md / CHANGES.md）+ i18n；全量测试零回归 | P0 |

## 3 执行阶段

### 阶段一：`collect_sources` 参数化（4.1）

- [ ] **1.1 参数化**（4.1）：`include/ezmk/build.hpp` `collect_sources` 追加可选参 `bool require_main = true`；`src/build.cpp` 实现中 main.cpp 校验改为 `if (require_main && project_type == "executable")`；项目两个调用点（`src/build.cpp:578,789`）零改动
- [ ] **1.2 单测**：`test_build.cpp` 增 `require_main=false` 用例（`"executable"` 类型 + 无 main 不抛、多目录收集）

### 阶段二：包源收集改造（4.2 + validate_pkg 补充）

- [ ] **2.1 validate_pkg src_dirs 感知**（设计补充）：`src/pkg.cpp` `validate_pkg()` 的 `src/` 硬编码校验改为对 `cfg.compile.src_dirs` 逐目录存在性检查（任一存在即通过），错误消息保留 `src/` 字样（dev.7 集成测试断言）
- [ ] **2.2 compile_package 改造**（4.2）：`compile_package()` 用 `cfg.compile.src_dirs` + `build::collect_sources(..., require_main=false)`；`header_only` 短路前移到收集前（无 src 不触发 fatal）；`precompiled` 不变；空源走 `collect_sources` 的 fatal（`no_source_files`/`src_dir_missing`，对齐项目语义 fail-fast）
- [ ] **2.3 单测**：`test_pkg.cpp` 增 `compile_package` 用例——多 `src_dirs` 收集、`header_only`/`precompiled` 短路、空源 fatal、`require_main` 不误判

### 阶段三：include_dirs 保序去重（4.3）

- [ ] **3.1 去重**（4.3）：`src/cache.cpp` 自编译 `-I` 构造处（MSVC `/I` 与 GCC `-I` 两分支）对 `[def_inc] + [include_dirs 解析结果]` 做保序去重（首次出现顺序保留，`lexically_normal` 判重）；`extra_includes` 不动
- [ ] **3.2 单测**：`test_cache.cpp` 或现有用例断言 `include_dirs` 含默认 `"include"` 时 `-I` 不重复

### 阶段四：utils 门控 + `pkg info` + i18n（4.4 + 4.5）

- [ ] **4.1 utils 门控**（4.4）：`src/pkg.cpp:818` `utils && !file_exists(dir/"src")` 改为对 `cfg.compile.src_dirs` 做「任一目录存在且有源文件」检查（轻量遍历，不触发 collect_sources 的 warning 噪音）
- [ ] **4.2 `pkg info` 增显 `src_dirs`**（4.4）：`src/pkg.cpp` `info()` 镜像 `include_dirs` 输出块
- [ ] **4.3 i18n**（4.5）：`pkg_info_src_dirs` 三向一致（`i18n_keys.def` + `locale/en.json` + `locale/zh.json`），`scripts/check_i18n.py` 通过；`bash build.sh` 编译通过

### 阶段五：测试与全量回归（4.6）

- [ ] **5.1 单测补全**：`collect_sources` `require_main`、`compile_package` 多目录/自定义 include/短路/空源 fatal、`-I` 去重断言
- [ ] **5.2 集成测试**：`test_integration.cpp` 增——依赖自定义 `src_dirs`+`include_dirs` 包的端到端编译链接（`pkg install <dir>` + `build`）；compile_commands 含包 `-I`
- [ ] **5.3 全量回归**：`bash build.sh test-all` 零回归（基线 709 用例 / 3296 断言，dev.8 后；新增用例/断言在其上增加）

### 阶段六：文档收口（4.7）

- [ ] **6.1 文档**（4.7）：`docs/en|zh/pkg.md`（`src_dirs` 对包生效、`include_dirs` 语义、空源 fatal 说明）、`docs/en|zh/package_authoring.md`、`docs/en|zh/config_file.md`（`[compile].src_dirs` 对包生效说明）、`CHANGES.md` dev.9 条目（中文基准，再同步英文）

### 阶段七：收口（4.8）

- [ ] **7.1 收口**（4.8）：本计划勾选 `[x]`；`plans/1.2.0/README.md` dev.9 状态「待实现 → 已完成」；发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| `require_main` 默认 `true` | 项目调用点零改动；包路径显式传 `false`——包文档默认 `type = "executable"` 但包永远编译成静态库，不能触发 main.cpp 校验 |
| 空源收紧为 fatal | header_only/precompiled/utils 无源短路全部前移到收集之前，能到达收集的包必属「非 header_only 却无源文件」的退化情形 → 对齐项目语义 fail-fast |
| header_only 短路前移 | 无 src/ 的 header-only 包不得触发 `src_dir_missing` fatal；短路顺序 precompiled → header_only → 收集 |
| `-I` 保序去重 | `def_inc` 先、`include_dirs` 解析结果后，首次出现顺序保留；编译器语义不变，compile_commands.json 输出更干净 |
| utils 门控轻量遍历 | 对 `src_dirs` 逐目录「存在且有源文件」检查，避免 collect_sources 对缺失目录的 warning 噪音 |
| validate_pkg 同步 src_dirs 感知 | 设计补充：否则自定义 `src_dirs` 包在安装校验阶段即被拒绝；错误消息保留 `src/` 字样兼容 dev.7 断言 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 包 `[compile].src_dirs` 从「忽略」→「生效」 | 默认 `["src"]` 与现状一致，绝大多数包零影响 | 默认值对齐；多目录/自定义目录为纯新增能力 |
| 空 src 包：warn+空库 → fatal | 仅「非 header_only/precompiled/utils 却无任何源文件」的退化包 | 行为收紧；三种短路前置不变，兼容矩阵说明 |
| `collect_sources` 新增 `require_main`（默认 `true`） | 项目路径零变化 | 默认值兼容；包路径显式传 `false` |
| 自编译 `-I` 保序去重 | 消除重复 `-I`（项目+包），编译器语义不变 | 去重保持首次出现顺序 |
| `pkg info` 增显 `src_dirs` 行 | 纯新增输出 | 新增 i18n key |
| utils 门控 src_dirs 感知 | 仅影响「utils 且 src_dirs ≠ 默认」的包 | 语义对齐，默认行为不变 |
| header_only / precompiled / utils 短路 | 无 | 短路逻辑前置，行为逐字节不变 |
| validate_pkg src_dirs 感知 | 自定义 `src_dirs` 包不再被误拒；默认包校验不变 | 错误消息保留 `src/` 字样 |

## 6 延后项

- **dev.7 联动**：`pkg install <dir>` 本地目录安装共用 `validate_pkg` + `compile_package`，`src_dirs`/`include_dirs` 修复自动惠及该路径（本版集成测试覆盖）。
- **与 export cmake（dev.2/dev.8）无关**：导出的是项目 config，不导出包配置；包 `src_dirs`/`include_dirs` 不在 export 范围（范围边界，不扩展）。
- **i18n X-macro**：新增 `pkg_info_src_dirs` 走 `i18n_keys.def` + en/zh 双 JSON；空源提示复用现有 `no_source_files`/`src_dir_missing`，不新增同义 key。
- **回归基线**：全量测试零回归（dev.8 后基线 709 用例 / 3296 断言），作为硬门槛。
