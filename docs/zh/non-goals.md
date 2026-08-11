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
- **为什么不设计**：配置模型没有工作区 / 子项目概念。项目间复用走包（`[depends]` + `ezmk pkg install`），不走项目引用。
- **替代方案**：
  - 独立项目 + 共享包。
  - 相互咬合的 monorepo → 用 CMake 的多 target 模型。

## 迁移路径

"复杂构建请使用 CMake" 不是推诿：

- `ezmk project export cmake`（1.2.0）一条命令把你的 `ezmk.toml` 生成 `CMakeLists.txt`——项目变大需要离开时，是带走配置，而不是从零重写。
- `ezmk project import --from cmake`（计划中，1.2.0）反向把标准 CMake 项目迁入 EazyMake。

## 相关

- 设计理念 — [README](../../README_ZH.md)
- 配置参考 — [config_file.md](config_file.md)
- `utils` 多工具包 — [utils.md](utils.md)
- Lua hooks — [config_file.md](config_file.md)
