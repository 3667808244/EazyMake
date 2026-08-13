# 复杂构建（何时改用 CMake）

> README 开宗明义：**易用优先，功能从简。复杂构建请使用 CMake。**（[README](../../README_ZH.md)）

本文档具体解释"复杂"到底指什么，帮你判断自己的项目该用 EazyMake 还是 CMake。它是 [non-goals.md](non-goals.md) 的**用户视角**姊妹篇——后者从"我们不做哪些功能"的角度讲同一道边界。

## 一句话规则

EazyMake 面向**单产物、声明式、中小型**的项目。只要越过这三条墙中的任意一条，就是"复杂构建"。

## EazyMake 适合什么（不算复杂）

一个项目同时满足以下所有条件，就是 EazyMake 的适用对象：

- **单一产物** —— 一个 `ezmk.toml` 恰好产出一样东西：可执行文件、静态库、动态库，或 `utils` 工具包（`[project].type`）。
- **声明式构建** —— 源文件、头文件目录、宏、标志、依赖都是**列出**在 `ezmk.toml` 里，而不是**用代码算**出来。
- **标准工具链** —— 宿主机上的 GCC、Clang 或 MSVC。
- **包依赖** —— 第三方库走 `[depends]` + `ezmk pkg install`。
- **少量自定义** —— `[hooks]` 的 Lua 脚本覆盖 `pre_build` / `post_build` / `on_failure` 侧边步骤。

如果你的项目是一个库、一个 CLI 工具、或一个小服务，EazyMake 正是为此设计的。

## 什么算"复杂"——四个典型特征

以下四种形态会把构建推出 EazyMake 的适用范围。命中任意一条，就该改用 CMake。

### 1. 多构建目标

单个项目要产出多个产物——多个可执行文件/库，且它们之间有**目标间依赖图**。

- **例子**：一套代码同时产出 `server`、`client` 和一个共享的 `libcore`，其中 `server` 和 `client` 都链接 `libcore`。
- **为什么超出范围**：EazyMake 的模型是一个 `ezmk.toml` 一个产物。真正的多目标需要 `--target` 选择、per-target 配置和依赖图——正是本工具要避免的结构性复杂度。
- **替代方案**：拆成每个产物一个项目，或用 CMake 的 `add_executable` / `add_library` 图。（`utils` 包可以靠 `[utils].tools` 内置多个工具，同一产物的多形态用 `[compile.profile.*]`——这些不算多目标。）

### 2. 交叉编译

为**不同于宿主**的目标平台/架构构建。

- **例子**：在 x86 工作站上编译 ARM 固件，或在 Linux CI 上构建 Windows 目标。
- **为什么超出范围**：工具链检测仅宿主（`detect_toolchain()` 只探测宿主编译器）。没有 `--target`、没有 target triple、没有 sysroot，包也只按宿主平台安装。
- **替代方案**：在每个目标平台原生构建（按 OS/架构的 CI 矩阵），或需要真正交叉编译时用 CMake + toolchain 文件。

> `[link].system_target` 是链接**系统库**（`-lpthread`、`-lm`）。名字里带 target，但与交叉编译无关。

### 3. 可编程 / 自定义构建逻辑

构建需要自定义规则、代码生成，或把图逻辑写成代码而非声明。

- **例子**：编译前用 `protoc` 生成源码、用自定义步骤嵌入资源、或定义依赖运行时状态的构建规则。
- **为什么超出范围**：构建是声明式的（`ezmk.toml`）。`[hooks]` 的 Lua 脚本在沙箱里跑侧边步骤——它不是构建图语言，也不替代声明式模型。
- **替代方案**：常见情况用声明式配置 + `[hooks]`；真正需要可编程 → 用 CMake。

### 4. 多项目工作区 / 子项目相互引用

一个"工作区"里多个项目直接互相引用、共同构建。

- **例子**：monorepo 里 `app/` 直接 include `lib/` 的头文件，并期望 `lib/` 先构建。
- **为什么超出范围**：配置模型没有工作区/子项目概念。项目间复用走包（`[depends]` + `ezmk pkg install`），不走项目引用。
- **替代方案**：独立项目 + 共享包，或相互咬合的 monorepo 用 CMake 的多 target 模型。

## 判断：快速自查清单

从你的项目出发，按顺序回答：

| 问题 | 答案 | 方向 |
|---|---|---|
| 我只产出**一个**产物？ | 是 | EazyMake ✓ |
| | 否（多个，或互相依赖） | → CMake |
| 构建是**声明式**的（列出文件/标志/依赖）？ | 是 | EazyMake ✓ |
| | 否（需要规则/生成） | → CMake |
| 我为**宿主**平台构建？ | 是 | EazyMake ✓ |
| | 否（交叉编译） | → CMake |
| 我的项目**相互独立**（靠包共享）？ | 是 | EazyMake ✓ |
| | 否（直接引用） | → CMake |

四个"是"全通过，EazyMake 就是对的工具；命中任意一个"否"，你就进入了复杂构建的领域——CMake 是更顺的路。

## 你已经"越过"EazyMake 的征兆

下面这些症状通常出现在项目"临界"之前：

- 你开始想要 `--target` 来从多个产物里挑一个构建。
- 你想在 `ezmk.toml` 里写条件/循环来生成配置。
- 你的 `[hooks]` 脚本越来越像在复刻 `add_custom_command`。
- 你想让一个项目直接 include 另一个项目的源码、或链接它的产物。
- 你需要 toolchain 文件或 sysroot。

这些都不是 bug——是边界透出来的迹象。当它们越堆越多，就是该向 CMake 演进的时候了。

## 向 CMake 演进不是死胡同

"复杂构建请使用 CMake" 不是甩锅，而是一条被支持的路径：

- `ezmk project export cmake` 一条命令把你的 `ezmk.toml` 生成 `CMakeLists.txt`——项目变大需要离开时，是带走配置，而不是从零重写。
- `ezmk project import --from cmake` 反向把标准 CMake 项目迁入 EazyMake。

导入器支持/拒绝的写法见 [migrate-from-cmake.md](migrate-from-cmake.md)，每道边界背后的完整推理见 [non-goals.md](non-goals.md)。

## 相关

- 设计理念 — [README](../../README_ZH.md)
- 每道边界为何存在 — [non-goals.md](non-goals.md)
- EazyMake 与 CMake 之间迁移 — [migrate-from-cmake.md](migrate-from-cmake.md)
- 配置参考 — [config_file.md](config_file.md)
- `utils` 多工具包 — [utils.md](utils.md)
