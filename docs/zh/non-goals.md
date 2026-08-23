# 不会设计的功能（Non-Goals）

> EazyMake 的设计理念：**易用优先，功能从简。复杂构建请使用 CMake。**（[README](../../README_ZH.md)）

本文档列出 EazyMake **刻意不做**的功能。每项说明：是什么、为什么不设计、以及替代方案——这样你不会为一个永远不会来的特性等待。

## 一个功能何时成为非目标

当一个功能会导致以下任何一种情况，就不做：

1. **结构性复杂度**——目标依赖图、平台矩阵、或可编程构建图。
2. **已有其他工具做得更好**——尤其是 CMake，设计理念明确把它作为复杂构建的出口。
3. **成本不服务小项目**——配置保持平铺的单一 `ezmk.toml`；每个功能都必须让"小而直"仍是默认路径。

一旦碰到这类需求，推荐向 CMake 演进。EazyMake 提供单向导出，迁移是一个命令而非重写（见[迁移路径](#迁移路径)）。

## 不会设计的功能

### 多构建目标

- **是什么**：从单个 `ezmk.toml` 产出多个产物（可执行 / 库，以及目标间依赖图）。
- **为什么不设计**：每个项目恰好一个产物——`[project].type` 只能是 `executable` / `static` / `shared` / `utils`。真正的多目标支持会迫使引入 target 选择（`--target`）、per-target 配置与依赖图：正是本工具要避免的结构性复杂度；还会波及所有消费构建模型的地方（`build` / `watch` / `project cc` / `export cmake`）。
- **替代方案**：
  - 拆成多个独立项目（每个 `ezmk.toml` 一个产物）。
  - `utils` 包用 `[utils].tools` 内置多个可执行工具。
  - 同一产物的多种构建形态用 `[compile.profile.*]`（Debug/Release 等）。

### 交叉编译

- **是什么**：为与宿主不同的目标平台/架构构建。
- **为什么不设计**：工具链检测仅宿主——`detect_toolchain()` 探测宿主编译器（`$CXX`/`$CC` 只是替换宿主编译器）。没有 `--target`、没有 target triple、没有 sysroot。平台键（`win-x64`、`windows_x86_64_msvc`）由编译期从宿主自身架构推导，包也只按宿主平台安装——整条流水线没有"为异平台产出/获取产物"的路径。
- **替代方案**：
  - 直接在每个目标平台构建（如按 OS/架构的 CI 矩阵）。
  - 需要从单一宿主全量交叉编译 → 用 CMake + toolchain 文件。

> 注意：`[link].system_target` 是**链接系统库**（`-lpthread`、`-lm`）——名字里带 target，但与交叉编译无关。

### 完全脚本化 / 编程式构建

- **是什么**：用脚本把整个构建过程表达成程序（自定义规则、构建图逻辑写在代码里）。
- **为什么不设计**：构建是**声明式**的（`ezmk.toml`）。`[hooks]` 的 Lua 只做 `pre_build` / `post_build` / `on_failure` 沙箱脚本，不是构建图语言，也不用来替代声明式构建。可编程构建恰恰是"复杂构建"。
- **替代方案**：
  - 声明式 `ezmk.toml` + `[hooks]` 覆盖绝大多数小项目。
  - 真正需要可编程 → 用 CMake。

### 多项目工作区 / 子项目相互引用

- **是什么**：一个"工作区"里多个项目互相引用、共同构建。
- **1.3.0 起支持（最小形态）**：`ezmk-workspace.toml` 定义成员集合（`[workspace] members`），`ezmk workspace build / test / clean` 批量管理；成员间可声明**单向非循环依赖**（成员 `ezmk.toml` 的 `[depends] workspace = [...]`），构建时拓扑排序 + 静态库产物复用（`-I <ws>/<m>/include` + `-L <ws>/<m>/build -l<m>` 自动注入）——覆盖最常见的 monorepo 形态：共享基础库 + 多个可执行文件。
- **仍然不做（non-goal 边界）**：
  - 依赖**环**（A→B→A、自环）——配置校验期拒绝；成员依赖图必须**单向非循环**。
  - 被依赖成员类型非 `static`（`executable` / `shared` 不能被依赖；shared 运行时依赖归延后）。
  - 依赖**版本约束 / 平台矩阵 / 可编程构建图**——版本与快照语义走包（`[depends]` + `ezmk pkg install`），工作区只做开发中源码即改即用。
  - 单项目内多构建目标（见「多构建目标」条款）——成员依赖图**不**打开单项目多 target 的门；每个成员仍恰好一个产物。
- **替代方案**：
  - 成员间无依赖 → 平行批量（纯 workspace，所有成员在第 0 层并行）。
  - 需要版本化 / 可分发复用 → 独立项目 + 共享包。
  - 相互咬合、需要完整构建图（环 / 版本 / 平台矩阵 / 可编程）→ 用 CMake 的多 target 模型。

### 预编译头（PCH）

- **是什么**：把一组稳定的重型头文件（STL / 系统头 / 大型第三方头如 Boost、nlohmann_json）预编译成 `.gch` / `.pch` 产物，所有 TU 编译时直接复用，省去逐 TU 重复解析头文件的开销。
- **为什么不设计**：PCH 同时触碰三条非目标标准：
  - **结构性复杂度**——当前编译管线是平铺 + 并行 + 细粒度缓存（per-TU 内容哈希 + `-MMD` depfile，增量粒度 = 单个源文件）。PCH 需要：编译**屏障**（PCH 必须先于所有 TU 构建，与 `-j` 并行模型冲突，要加预阶段）；缓存失效**新维度**（PCH 哈希进每个 TU 的缓存条目，PCH 内任一头文件变更 → 全量重建）；三编译器族语义不一致（GCC/Clang 独立 `.gch` + `-include`；MSVC `/Yc` 在首个使用 TU 时副作用式创建 + `/Yu`/`/Fp`）；PCH 自身的依赖跟踪与 watch 模式触发；`[compile]` 新配置字段。
  - **已有其他工具做得更好**——CMake 的 `target_precompile_headers()` 是成熟方案（CMake 自己也把 PCH 定位为 best-effort、按配置启用）。
  - **成本不服务小项目**——PCH 的收益只在冷构建（首次/全量）出现；小项目 TU 少，摊销不起。增量场景反而变差：改 PCH 内的头文件 → PCH 重建 + **全部 TU 重编**；而现有缓存模型下「缓存命中的 TU 直接跳过编译」已经避免了重复解析头文件——PCH 想省的那部分，在增量路径上本来就不发生。需要 PCH 是项目已长大到「小而直」模型之外的信号（与多目标、平台矩阵同源）。
- **替代方案**：
  - 保持小而直：`src/` + `include/` 平铺 + `-j` 并行 + 增量缓存，小项目已足够。
  - 项目大到 PCH 能回本（几十个 TU + 重型稳定头、冷构建分钟级）→ 用 CMake 的 `target_precompile_headers()`；`ezmk project export cmake` 一条命令带走配置。

### 原生单元测试仪表盘

- **是什么**：内置的测试结果仪表盘——终端实时 UI（TUI，逐用例状态刷新、进度条）或 Web/HTML 报告页（历史、图表、抖动分析）。
- **为什么不设计**：
  - **结构性复杂度**——`ezmk test` 的执行模型是子进程 + 控制台文本解析（`src/build.cpp` 的 `run_tests()`：解析 Catch2 摘要行，格式一换就只剩退出码兜底）。仪表盘需要**结构化事件流**（每用例开始/结束、断言级事件）+ **全新渲染层**（终端重绘循环 / HTTP + 内嵌 HTML/JS 资产 + 历史持久化）——两个新子系统，不是对现有代码的增量。
  - **已有其他工具做得更好**——CI 平台（GitHub Actions / GitLab / Jenkins）原生渲染机器可读测试报告；内嵌 Catch2 v3 自带 `-r junit/xml/json` reporter。自建仪表盘 = 重复做 CI 已完成的事；且 1.2.0-dev.11 已删除 XML 报告摄入死代码（`src/build.cpp`「removed dead code — parse_catch2_xml」）——"自己消费机器可读报告"这条路被明确试过并放弃。
  - **成本不服务小项目**——仪表盘的价值在数百用例、跨运行对比（历史/抖动）时才显现；小项目跑完看摘要行 + 退出码就是正确工具。给"小而直"默认路径加 UI 旋钮，违反第三条标准。
- **替代方案**：
  - **机器可读报告出口**（`ezmk test --report junit`，1.3.2）：EazyMake 只产数据（JUnit XML 写文件），交给已有仪表盘（CI）渲染——不自建 UI，与「已有其他工具做得更好」对齐。
  - 直接跑测试二进制的 Catch2 reporter 参数（`-r junit::out=<file>`）。

### 编译耗时火焰图

- **是什么**：把每个源文件的编译耗时可视化为火焰图 / 堆叠时间线（交互式 HTML/SVG，悬浮、缩放）。
- **为什么不设计**：
  - **结构性复杂度**——EazyMake 已有**数据**：per-file 编译耗时（1.2.0-dev.6：`-v` 全量排序明细 / 慢构建自动 top-N，`src/build.cpp:991,1103-1120`）。但火焰图需要**结构化时序导出 + 交互式渲染层**（HTML/JS/SVG 资产 + 悬浮/缩放交互）——后者是全新子系统，不是对现有统计的增量。
  - **已有其他工具做得更好**——火焰图是性能剖析生态的标准产物：`perf` / `ninja -t trace`（Chrome trace 格式）+ speedscope / `flamegraph.pl` 现成可渲染；自建 = 重复生态工具已做完的事。
  - **成本不服务小项目**——慢构建自动 top-N 文本已覆盖小项目；火焰图是项目长大、构建分钟级之后的诉求。
- **替代方案**：
  - 现有 per-file top-N 文本输出（`-v` 全量 / 慢构建自动）。
  - 深入剖析 → `perf record` + `flamegraph.pl` / speedscope，或 `ninja -t trace` + `chrome://tracing`。

### 最终二进制尺寸构成图

- **是什么**：可视化最终产物（可执行 / 库）的尺寸构成——按符号 / 目标文件 / 节区的饼图 / 柱状图（交互式）。
- **为什么不设计**：
  - **结构性复杂度**——需要解析链接产物的符号 / 节区大小（`nm` / `size` / `objdump` 输出）→ 新增二进制解析子模块 + 渲染层；现有链接管线（`src/build.cpp` 链接阶段）不产出符号级数据。
  - **已有其他工具做得更好**——`size` / `nm -S`（文本）；Google `bloaty`（符号级尺寸剖析器，直接输出 JSON / HTML 报告）；`-Wl,-Map`（链接映射文件）——全是现成工具，自建属于重复造轮子。
  - **成本不服务小项目**——小项目二进制尺寸可读性无问题；按符号/节区优化尺寸是嵌入式与大项目的需求。
- **替代方案**：
  - `size <binary>` / `nm -S <binary>` 文本速查。
  - 详细剖析 → `bloaty -d symbols <binary>`（HTML/JSON 直接出报告）。
  - 链接时加 `-Wl,-Map=<file>` 看完整映射。

## 迁移路径

"复杂构建请使用 CMake" 不是推诿：

- `ezmk project export cmake`（1.2.0）一条命令把你的 `ezmk.toml` 生成 `CMakeLists.txt`——项目变大需要离开时，是带走配置，而不是从零重写。
- `ezmk project import --from cmake`（1.2.0-dev.4）反向把标准 CMake 项目迁入 EazyMake。

## 相关

- 设计理念 — [README](../../README_ZH.md)
- 什么算"复杂"（用户视角）— [complex-builds.md](complex-builds.md)
- 配置参考 — [config_file.md](config_file.md)
- `utils` 多工具包 — [utils.md](utils.md)
- Lua hooks — [config_file.md](config_file.md)
