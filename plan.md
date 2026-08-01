# EazyMake 1.1.0-dev.7 执行计划

> 详细设计：[`plans/release/1.1.0-dev.7.md`](plans/release/1.1.0-dev.7.md)
>
> **状态：阶段四~七已完成。** 全量测试 555 用例 / 2661 断言（含集成测试），零回归。

---

## 1 背景

当前 EazyMake 的包处理存在两个改进方向：

1. **包生态不足**：官方仓库现有 42 个包，但缺少网络通信（HTTP/gRPC）、数据库连接（SQLite/PostgreSQL/Redis）、序列化（Protobuf/msgpack）、科学计算（Eigen）、测试框架（Google Test）等领域的常用库。
2. **缺失依赖的处理粗糙**：
   - 构建时硬依赖未安装 → 无前置检查，直接被 g++/ld 原始错误捕获，用户得不到语义化提示
   - `pkg install` 时可选依赖缺失 → 静默跳过，用户不知道有哪些可选增强
   - 仓库安装时硬依赖缺失 → 直接报错退出，需手动逐个安装

本版本聚焦包生态拓充（+22 个新包 + 已有包版本更新）和包处理改善（构建时前置检查 + 自动安装 + 交互式可选依赖）。

---

## 2 目标

| # | 目标 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | **12 个新包入库** | P0 | 覆盖加密/HTTP/RPC/数据库/序列化/科学计算/测试框架（§2.1） |
| 2 | **10 个 Boost header-only 子库** | P0 | Asio/Beast/Filesystem/System/SmartPtr/Token/UUID/Random/Math/Functional |
| 3 | **已有包版本更新** | P0 | 检查 42 个已有包的上游版本，升级过时包（预计 20~25 个） |
| 4 | **构建时硬依赖前置检查** | P0 | `prepare_build_state()` 中检查 `depends.lib`，缺失时输出语义化错误 + 安装提示 |
| 5 | **仓库安装时硬依赖自动安装** | P1 | `lib` 缺失 → 递归 `install_from_repo()`，无需用户手动干预 |
| 6 | **仓库安装时 want 交互式询问** | P1 | 四选项（Y/N/A/D）+ 递归穿透 + `-y` 兼容 |
| 7 | **编译与全量回归** | P0 | `bash build.sh` 编译通过 + 全量测试零回归 |

---

## 3 执行阶段

### 阶段一：包生态拓充 — 12 个新包（P0）

**仓库**：`ezmk-repo`

**打包策略**：简单库从源码构建，编译复杂的库使用预编译包（`precompiled = true`，复用 0.9.7 机制）。用户 `pkg install` 时直接下载预编译产物，无需本地编译。

**Header-only（源码分发，零编译）**：

- [ ] `tomlplusplus` — header-only，仅复制 include/
- [ ] `eigen` — header-only，仅复制 Eigen/ 目录
- [ ] `cpp-httplib` — header-only（基础模式），可选 openssl
- [ ] `msgpack-c` — header-only 模式

**预编译包（`precompiled = true`，编译复杂，一劳永逸）**：

- [ ] `openssl` — 预编译 → `libssl.a` + `libcrypto.a`（`./Configure` + `make`，构建耗时长）
- [ ] `libcurl` — 预编译 → `libcurl.a`（依赖 openssl + zlib，`./configure` + `make`）
- [ ] `protobuf` — 预编译 → `libprotobuf.a` + `protoc` 工具（CMake，构建耗时中高）
- [ ] `gRPC` — 预编译 → `libgrpc++.a` + `libgrpc.a`（依赖 protobuf + openssl；首版最小构建不含 abseil）
- [ ] `googletest` — 预编译 → `libgtest.a` + `libgmock.a`（CMake，用户不应为测试框架付出编译成本）

**源码编译（轻量，无复杂依赖链）**：

- [ ] `hiredis` — CMake 编译 → `libhiredis.a`（零外部依赖，编译快）
- [ ] `sqlitecpp` — CMake 编译 → `libSQLiteCpp.a`（依赖 sqlite3，编译快）
- [ ] `libpqxx` — CMake 编译 → `libpqxx.a`（依赖 libpq；首版 Linux/macOS，Windows 后续补充）

### 阶段二：包生态拓充 — 10 个 Boost header-only 子库（P0）

**仓库**：`ezmk-repo`，沿用 0.9.8 的 Boost 打包模式

- [ ] `boost-asio` — 异步 I/O 网络编程（header-only 模式）
- [ ] `boost-beast` — HTTP/WebSocket 库（基于 Asio）
- [ ] `boost-filesystem` — 跨平台文件系统操作（header-only 模式）
- [ ] `boost-system` — 错误码基础设施（Asio/Beast/Filesystem 依赖）
- [ ] `boost-smart-ptr` — 智能指针
- [ ] `boost-tokenizer` — 字符串分词
- [ ] `boost-uuid` — 通用唯一标识符
- [ ] `boost-random` — 随机数生成
- [ ] `boost-math` — 数学特殊函数（header-only 部分）
- [ ] `boost-functional` — 函数对象适配器

### 阶段三：包生态拓充 — 已有包版本更新（P0）

**仓库**：`ezmk-repo`

- [ ] Boost×10 统一升级至 1.88.0
- [ ] `catch2` 3.6.0 → 检查 3.8.x API 变更
- [ ] `fmt` 10.2.1 → 检查 11.x 破坏性变更
- [ ] `spdlog` 1.14.1 → 1.15.x patch 升级
- [ ] `cli11` 2.5.0 → 2.5.x patch 升级
- [ ] `imgui`×17 1.91.9 → patch 升级
- [ ] `zlib` 1.3.1 → patch 升级
- [ ] `sqlite3` 3.46.0 → 3.49.x 年度升级
- [ ] `nlohmann_json` 3.11.3 → patch 升级
- [ ] `stb`×10 → 基于 commit hash 判定是否更新
- [ ] 更新 `index.toml`：注册所有新包 + 更新已有包版本
- [ ] 各包编写 `README.md` 说明用途和基本用法

### 阶段四：构建时硬依赖前置检查（P0）

**文件**：`src/build.cpp` + `include/ezmk/pkg.hpp` + `src/pkg.cpp`

- [x] `include/ezmk/pkg.hpp` 新增 `package_available(name)` 接口声明
- [x] `src/pkg.cpp` 实现 `package_available()` — 遍历已注册仓库搜索包名
- [x] `src/build.cpp` `prepare_build_state()`：在 `want.lib` 处理之后、版本验证之前，遍历 `cfg.depends.libs` 检查缺失
- [x] 缺失时输出语义化错误：
  - 列出所有缺失依赖名称
  - 检查仓库中是否可安装（仅直接依赖）
  - 可安装 → 提示 `ezmk pkg install <names>`
  - 不可安装 → 提示添加仓库或手动安装
- [x] `i18n_keys.def` 新增 `missing_dep_at_build` + `locale/en.json` + `locale/zh.json` 翻译

### 阶段五：仓库安装时硬依赖自动安装（P1）

**文件**：`src/pkg.cpp`

- [x] `install_from_repo()` 依赖解析循环：`lib` 缺失且未在 seen set → 递归 `install_from_repo()`
- [x] 自动安装失败时输出清晰错误链（显示哪个依赖的哪个子依赖失败）
- [x] 复用现有 seen set 机制防止循环依赖无限递归

### 阶段六：仓库安装时 want 交互式询问（P1）

**文件**：`src/pkg.cpp`

- [x] 在 `lib` 依赖解析完成后，遍历当前包的 `want` 列表
- [x] 实现四选项交互逻辑：
  - `Y`：安装当前 want（不递归其子 want）
  - `N`：跳过当前 want
  - `A`：安装当前及之后所有 want（递归穿透子 want）
  - `D`：拒绝当前及之后所有 want（递归穿透子 want）
- [x] 非交互模式（`-y`）：跳过所有 want（等效 `D`），保持 CI 兼容
- [x] 交互逻辑抽取为可注入的函数对象（或 `std::istream&` 参数），便于单元测试
- [x] `i18n_keys.def` 新增 `want_prompt_title`/`want_prompt_options` + locale JSON 翻译

### 阶段七：编译与回归验证（P0）

- [x] `bash build.sh` 编译通过（MSYS2 / Windows）
- [x] 全量测试通过，零回归
- [x] 新增测试：
  - 硬依赖缺失报错 → 语义化消息验证
  - auto-install 流程 → 递归安装 + 错误链验证
  - want 交互式逻辑 → Y/N/A/D 四种路径 + `-y` 模式

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **复杂包优先预编译** | openssl/libcurl/protobuf/gRPC/googletest 编译耗时且依赖链复杂 → 使用 `precompiled = true` 分发预编译产物（0.9.7 已有机制）；用户下载即用，无需本地编译 |
| **轻量包保留源码编译** | hiredis/sqlitecpp/libpqxx 编译快或无复杂依赖链 → 源码分发保持灵活性（跨平台、可定制） |
| 硬依赖检查位置：`prepare_build_state()` | 在编译开始前拦截，避免进入编译器后产生难以理解的原始错误 |
| 仅提示直接依赖（不递归） | 传递依赖由 `pkg install` 自身处理；构建时只需确认顶层依赖存在 |
| `lib` 自动安装只适用于仓库安装 | `.zip`/`.tar.gz` 文件和 URL 安装保持原有行为（用户自行管理依赖） |
| want 交互式 A/D 递归穿透 | 避免在深层依赖中重复询问；用户体验上一次性决策 |
| `-y` 模式跳过所有 want | 保持 CI/自动化场景兼容，等效原有静默跳过行为 |
| `gRPC` 首版最小构建 | 先不依赖 abseil-cpp，降低预编译复杂度，后续迭代增强 |
| `libpqxx` 首版仅 Linux/macOS | Windows 需 libpq 随 MSYS2 安装或独立预编译包，后续补充 |
| 交互逻辑可注入化 | 将 `std::cin` 交互抽取为可注入的函数对象，便于单元测试用 `std::istringstream` 替代 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 12+10 个包 | 纯增量，无旧版本 | 不影响已有包 |
| 构建时硬依赖检查 | 之前静默失败的项目现在会报错 | 改进——用户得到明确指引而非编译器原始错误 |
| `lib` 自动安装 | `pkg install` 行为从报错变为自动安装 | 向后兼容——原本报错的场景现在自动解决 |
| `want` 交互式询问 | 之前静默跳过，现在询问 | 非交互模式（`-y`）保持原有行为（跳过） |
| `package_available()` 新 API | 内部接口，非公共 | 不影响第三方 |

---

## 6 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `src/build.cpp` | 修改 | 硬依赖前置检查 + 仓库搜索提示 |
| `src/pkg.cpp` | 修改 | 硬依赖自动安装 + want 交互式询问 + `package_available()` 实现 |
| `include/ezmk/pkg.hpp` | 修改 | 新增 `package_available()` 声明 |
| `include/ezmk/i18n_keys.def` | 修改 | 新增 `missing_dep_at_build`/`want_prompt_title`/`want_prompt_options` 等 key |
| `locale/en.json` | 修改 | i18n 英文翻译 |
| `locale/zh.json` | 修改 | i18n 中文翻译 |
| `test/test_build.cpp` | 修改 | 新增：硬依赖缺失报错测试 |
| `test/test_pkg.cpp` | 修改 | 新增：want 交互式测试 |

---

## 7 风险与注意事项

- **预编译包的平台矩阵**：openssl/libcurl/protobuf/gRPC/googletest 需要为每个目标平台（Windows/MSYS2、Linux、macOS）分别编译预编译包。采用 0.9.7 的 `precompiled = true` + 平台映射机制，`index.toml` 中按 `[target.{triple}]` 声明不同平台的 sha256。
- **`gRPC` 编译复杂度高**：即使作为预编译包，构建一次仍需处理 protobuf + openssl + abseil-cpp（可选）依赖链。首版发布不含 abseil 的最小构建模式，降低首次构建成本。
- **`libpqxx` 的 Windows 依赖**：`libpq` 需随 MSYS2 安装或作为独立预编译包，首版仅 Linux/macOS
- **交互式 prompt 测试**：将 prompt 逻辑抽取为可注入的函数对象（或 `std::istream&` 参数），便于测试时用 `std::istringstream` 替代
- **自动安装的递归深度**：复用现有 seen set 机制防止循环依赖无限递归

---

## 8 延后项（1.1.0-dev.8+）

- `gRPC` abseil-cpp 完整构建模式
- `libpqxx` Windows 平台支持
- 测试覆盖率报告生成（gcov/lcov 集成）
- `test.timeout` 可配置化（1.1.0-dev.6 延后项）
