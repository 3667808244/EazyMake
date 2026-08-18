# 14. 多平台多工具链预编译共包

大多数库应以**源码分发**（`src/`）：源码包在任意平台和编译器上都能编译。但有些库无法用简单的 `gcc`/`g++` 命令编译（需要 CMake、autotools、OpenSSL 的 `Configure` 等复杂构建系统），或编译耗时极长（gRPC、Qt）——这时用 `precompiled = true` 在 `lib/` 下提供预编译产物。

> ⚠️ **最容易踩的坑**：把 GCC 11 编译的 `.a` 丢进仓库，让同事用 GCC 13 链接——链接器报一堆 `std::__cxx11` undefined reference，排查一整天。C++ 没有"C 那种纯 ABI"：编译器族、工具链版本、标准库 ABI 不匹配就会链接失败。本教程用 1.2.0-dev.10 的命名约定安全地分发多份产物。

## 最小包

```toml
[project]
name = "sdl2"
version = "2.32.10"
type = "static"
precompiled = true

# 无需 src/ 目录 — 预编译产物直接放 lib/
```

## 命名约定：`os-arch[-compiler][-abi]`

`lib<name>.<os>-<arch>[-<compiler>][-<abi>].<ext>`（1.2.0-dev.10+）

| OS | Arch | 标识 |
|----|------|------|
| Windows | x86_64 | `win-x64` |
| Windows | x86 | `win-x86` |
| Linux | x86_64 | `linux-x64` |
| Linux | aarch64 | `linux-arm64` |
| macOS | x86_64 | `mac-x64` |
| macOS | aarch64 | `mac-arm64` |

- **编译器标签**（可选）：`gcc<major>`（如 `gcc13`）、`clang<major>`（如 `clang18`）、`msvc143`（VS 工具集查表：140/141/142/143）。
- **ABI 标签**（可选）：GCC / Clang（Linux，libstdc++ 默认）→ `abi11`（CXX11 ABI）；Apple Clang（libc++ 默认）与 MSVC → 无。

一个"多平台多工具链共包"：

```
sdl2/
├── ezmk.toml
├── include/       # 头文件（跨平台共用）
└── lib/           # 预编译静态库
    ├── libSDL2.win-x64-msvc143.a
    ├── libSDL2.linux-x64-gcc13-abi11.a
    ├── libSDL2.mac-arm64-clang15.a
    └── libSDL2.win-x64.a          # 无工具链标签（旧式，可降级匹配）
```

## 选择优先级：ABI 安全的 4 级匹配

`ezmk` 安装时按当前工具链从高到低匹配：

1. **L4 完整标签**：`os-arch-compiler-abi` 全部相等（如 `linux-x64-gcc13-abi11`）
2. **L3 同编译器**：`os-arch-compiler` 相等且产物无 abi 段（同编译器 = 同默认 ABI）
3. **L2 平台**：仅 `os-arch` 相等（无工具链标签的旧式产物）
4. **L1 裸名**：无后缀文件 `lib<name>.a`（向后兼容单平台旧包）

- 同编译器但 abi 段显式不同（如 `gcc11-abi8` 对 `gcc11-abi11`）→ **ABI 不兼容，直接跳过**。
- 落到 L2/L1（可能跨工具链）→ **显式警告**，指明当前工具链标签与可用产物——不再静默拿到错误 ABI 的库到链接期才炸。
- `[project].precompiled_strict = true` → 降级改为 **fail-fast 报错**。

## 失败案例：`std::__cxx11` undefined reference

文档里确实写了"平台和架构"。但作为一个被 C++ ABI 炸过无数次的开发者，看到"平台和架构"时的本能反应是："哦，就是 Windows 编译的 `.a` 不能给 Linux 用呗"。然后依然放心地把 GCC 11 编译的 `.a` 丢进仓库，让同事用 GCC 13 去链接——链接器报了一堆 `std::__cxx11` undefined reference，排查一整天。

原因：libstdc++ 的 CXX11 ABI（`_GLIBCXX_USE_CXX11_ABI`）——`abi11`（新）/ `abi8`（旧）混用即链接失败。这就是为什么 dev.10 起产物名要带编译器与 ABI 标签，让 `ezmk` 在**安装时**（而不是链接时）就选出匹配的产物。

## 最佳实践

> **最佳实践（仅限预编译包语境）**：同一包内用 `os-arch[-compiler][-abi]` 命名并排放多份工具链/ABI 产物，由 `ezmk` 按当前工具链自动选择。**源码分发（`src/`）仍远优于预编译**——预编译只能在声明过的平台/工具链/ABI 上工作，源码包处处可编译。

## 易错点

- **MSVC 运行时**：静态库的 CRT 绑定（`/MD` vs `/MT`）需与消费者一致——文档已注明；dev.10 暂不做运行时维度标签。
- **Apple Clang / clang-cl**：Apple Clang 版本号与 LLVM 不对齐（同一 major 内也可能 ABI 变）；clang-cl 不生成 `msvc1xx` 标签——需 MSVC ABI 时用真 MSVC 构建。
- **旧 ABI 消费端**：消费端显式 `-D_GLIBCXX_USE_CXX11_ABI=0` 构建时 ezmk 不做自动探测——包作者可为该场景单独命名 `abi8` 产物，默认按 `abi11` 匹配。
