# EazyMake 1.2.0-dev.10 执行计划

> **状态：已完成**（2026-08-15，全量 747 用例 / 3442 断言零回归）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.10.md`](plans/1.2.0/1.2.0-dev.10.md)。本计划为 1.2.0 系列第十个开发子版本：**平台标识符扩展（工具链/ABI）**——把预编译包命名从 `lib<name>.<os>-<arch>.<ext>` 扩展为 `os-arch[-compiler][-abi]`（`gcc13`/`clang18`/`msvc143` + 默认 `abi11`），`select_precompiled_archive()` 按 **ABI 安全的 4 级匹配优先级**选择，降级匹配（可能跨工具链）时显式警告，可选 `[project].precompiled_strict = true` 改为 fail-fast。承接 `package_authoring.md` §3.3「多平台共包」——现有 os-arch 命名对 C 成立、对 **C++ ABI 不成立**（GCC/Clang/MSVC 互不兼容）。
>
> **范围边界**：只动预编译包的 `lib/` 选择路径（`select_precompiled_archive`）+ 新增 `toolchain::compiler_tag()` + `ProjectSection::precompiled_strict` 可选字段 + `pkg info` 增显；**不碰** `src_dirs`/`include_dirs`（dev.9）、export cmake（dev.2/dev.8）、repo index `os_arch_toolchain` 三元组（源码包分发维度，归 2.0.0）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（新增纯函数 + 可选字段，`select_precompiled_archive` **签名不变**、两处调用点零改动）；③ 全量测试零回归（基线 727 用例 / 3361 断言，dev.12 后；Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

- **C++ ABI 无法用 `os-arch` 区分**：预编译归档的 ABI 由「编译器族 + 工具链版本 + 标准库 ABI」共同决定。三类真实故障：GCC 编译的 `.a` 给 Clang 链接（libstdc++/libc++ 符号不匹配）、GCC 13 产物给 GCC 11 链接（新 libstdc++ 符号）、`msvc143` 产物给 `msvc142` 链接（STL 布局差异）。
- **现状只做精确 os-arch + 裸名回退**（`src/pkg.cpp:301-348` `select_precompiled_archive`）：没有工具链维度，跨工具链降级无任何提示——用户拿到错误 ABI 的库，链接期才炸。
- 目标：包作者可按工具链/ABI 细分 `lib/` 产物；消费端按 ABI 安全优先级选择；降级时显式警告。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 新增 `toolchain::compiler_tag()`：从 `Toolchain` 生成编译器标签（`gcc13`/`clang18`/`msvc143`），纯函数、无子进程（`detect_toolchain` 已缓存 `tc.version`） | P0 |
| 2 | 平台标识符扩展 `os-arch[-compiler][-abi]`，ABI 标签按工具链默认值生成（GCC/Clang+libstdc++ → `abi11`；libc++/MSVC → 无），零配置 | P0 |
| 3 | `select_precompiled_archive()` 4 级匹配：full(4) → compiler(3) → os-arch(2) → bare(1)；签名不变、两处调用点零改动 | P0 |
| 4 | 降级匹配（消费端带工具链标签却落 os-arch/裸名）→ 显式 ABI 兼容性警告（i18n） | P0 |
| 5 | 可选严格模式 `[project].precompiled_strict = true`：L2/L1 降级改 fail-fast 报错 | P1 |
| 6 | `pkg info` 对 precompiled 包增显 `lib/` 可用产物标签列表（含裸名） | P1 |
| 7 | 兼容性：既有 os-arch 标签包、裸名包、`detect_platform_tag()` 其余两处调用（repo.cpp:300 / build.cpp:1419）行为逐字节不变 | P0 |
| 8 | 测试（单测 + 集成）+ i18n 三向一致 + 文档（package_authoring / pkg / config_file / CHANGES，中文基准）；全量测试零回归 | P0 |

## 3 执行阶段

### 阶段一：`compiler_tag()`（4.1）

- [x] **1.1 实现**（4.1）：`include/ezmk/toolchain.hpp` + `src/toolchain.cpp` 新增 `compiler_tag(const Toolchain&)`——GCC/Clang 解析 `tc.version` 首个数字段拼 `gcc<major>`/`clang<major>`；MSVC 解析 cl 版本 `19.<minor>…` → `_MSC_VER` 等价数（`1900+minor`）→ **查表**（1900→msvc140 / 1910-1919→msvc141 / 1920-1929→msvc142 / ≥1930→msvc143，不用算术避免 1943→144 错算）
- [x] **1.2 单测**（4.1）：`test_toolchain.cpp` `compiler_tag`——GCC/Clang/MSVC 版本串解析 + 工具集映射表边界（1900/1910/1930/1943）

### 阶段二：匹配重写 + 降级警告/严格模式（4.2 + 4.3）

- [x] **2.1 匹配重写**（4.2）：`select_precompiled_archive()` 改分段解析（os/arch 固定前缀表 + `gcc\d+`/`clang\d+`/`msvc14\d` compiler 段 + `abi\d+` abi 段，未知段不识别） + 4 级评分（L4 full=4 / L3 compiler 同且 abi 缺失=3 / L2 os-arch=2 / L1 裸名=1）；**特例**：compiler 同但 abi 段存在且不等 → 视为 ABI 不兼容跳过（仅此候选则报错）；同分取 filename 字典序最小；签名不变，`src/pkg.cpp:375` 与 `src/build.cpp:655` 两处调用点零改动
- [x] **2.2 降级警告**（4.3）：选中 L2/L1（消费端带 compiler 标签）→ i18n 警告（`precompiled_toolchain_fallback_warn`），指明当前工具链标签与 available 列表；无任何匹配时错误信息补当前完整标签
- [x] **2.3 严格模式**（4.3）：`ProjectSection` 增可选 `precompiled_strict`（默认 `false`，`src/config.cpp`/`config.hpp`）；开启后 L2/L1 降级 → `precompiled_strict_mismatch` fatal

### 阶段三：`pkg info` 增显 + i18n（4.4 + 4.5）

- [x] **3.1 `pkg info` 增显**（4.4）：precompiled 包输出 `lib/ 产物:` 行（列出全部识别标签含裸名，镜像 include_dirs 输出块）
- [x] **3.2 i18n**（4.5）：`precompiled_toolchain_fallback_warn` / `precompiled_strict_mismatch` / `pkg_info_precompiled_variants` 三向一致（`.def` + en/zh），`check_i18n.py` 通过；`bash build.sh` 编译通过

### 阶段四：测试与全量回归（4.6）

- [x] **4.1 单测矩阵**（4.6）：`test_pkg.cpp` `select_precompiled_archive`——L4/L3/L2/L1、abi 不匹配跳过、未知段忽略、字典序确定性、降级警告、strict 报错
- [x] **4.2 集成**（4.6）：端到端——当前工具链 full-tag 产物被选中并链接；仅有跨工具链产物时降级 + 警告；strict 模式报错
- [x] **4.3 全量回归**（4.6）：`bash build.sh test-all` 零回归（基线 727 用例 / 3361 断言，dev.12 后；既有 os-arch/裸名包路径必须零变化）

### 阶段五：文档收口（4.7）

- [x] **5.1 文档**（4.7）：`docs/en|zh/package_authoring.md`（命名约定 `os-arch[-compiler][-abi]`、标签表、4 级优先级、ABI 警告、Apple Clang/clang-cl/旧 ABI 覆盖局限）、`pkg.md`、`config_file.md`（`precompiled_strict`）、`CHANGES.md`——**中文基准，先 `docs/zh/` 再同步 `docs/en/`**

### 阶段六：收口（4.8）

- [x] **6.1 收口**（4.8）：本计划勾选 `[x]`；`plans/1.2.0/README.md` dev.10 状态「待实现 → 已完成」；发布门槛复核（签名不变 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| `compiler_tag()` 从 `tc.version` 解析 | `detect_toolchain()` 已缓存版本串，纯函数零子进程；GCC/Clang 取 major，MSVC 查工具集映射表 |
| MSVC 工具集查表而非算术 | `_MSC_VER=1943` 按算术会错算 `144`；VS2022 各 17.x 保持 v143 二进制兼容，`≥1930` 一律收束 `143` |
| ABI 标签按工具链默认值 | GCC/Clang+libstdc++ → `abi11`（CXX11 ABI），libc++/MSVC → 无；零配置；`-D_GLIBCXX_USE_CXX11_ABI=0` 覆盖 P2 延后 |
| 4 级评分匹配 | L4 full > L3 compiler(abi 缺失) > L2 os-arch > L1 裸名；compiler 同但 abi 不等 → 不兼容跳过；同分字典序（确定性） |
| 降级警告仅当消费端带工具链标签落 L2/L1 | 既有 os-arch/裸名包（消费端匹配 L2）不产生新噪音；严格模式默认关 |
| 签名不变 | `select_precompiled_archive` 内部调 `detect_toolchain()`（已缓存），两处调用点零改动 |
| `detect_platform_tag()` 不改 | repo index 过滤与 pack 元数据属源码包分发维度，不涉及 C++ ABI |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 平台标识符扩展 `os-arch[-compiler][-abi]` | 纯新增维度 | 既有 os-arch 标签文件仍命中 L2，行为不变 |
| 匹配优先级 4 级 | 新增 L4/L3 精确匹配优先 | 高优先级先试；同分字典序确定 |
| 降级到 os-arch/裸名时警告 | 既有包无工具链标签 → 首次见警告 | 纯新增诊断输出，不影响匹配结果；strict 默认关 |
| `[project].precompiled_strict` | 新增可选字段（默认 false） | 旧配置无此字段 → 行为不变 |
| `select_precompiled_archive` 签名 | 无 | 内部 `detect_toolchain()`（已缓存），两处调用点零改动 |
| `detect_platform_tag()` | 无 | 不改；repo.cpp/build.cpp 调用点不受影响 |
| `pkg info` 增显 variants 行 | 纯新增输出 | 新增 i18n key |
| 裸名 `lib<name>.a` 回退 | 无 | L1 保留，向后兼容单平台旧包 |

## 6 延后项

- **`_GLIBCXX_USE_CXX11_ABI=0` 覆盖**（P2）：消费端显式旧 ABI 构建不自动探测；包作者可为该场景单独命名 `abi8` 产物，本版默认按 `abi11` 匹配（该文件仅能经降级路径选中并触发警告）。
- **repo index `os_arch_toolchain` 三元组**：现有机制只区分编译器族（源码包分发过滤），与 `lib/` 文件级 ABI 匹配是两套系统；版本化工具链三元组合并归 2.0.0 窗口。
- **Apple Clang / clang-cl 局限**：Apple Clang 版本号与 LLVM 不对齐（文档注明，精确 full 匹配仍可用）；clang-cl 不生成 `msvc1xx` 标签（需 MSVC ABI 用真 MSVC 构建）。
- **pre.2 联动**：本版命名约定与 ABI 警告是 pre.2「加强预编译包跨工具链兼容性警告 + 最佳实践」的实现前提。
- **回归基线**：全量测试零回归（dev.12 后基线 727 用例 / 3361 断言），作为硬门槛。
