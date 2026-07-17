# EazyMake 0.9.5 执行计划 — 跨平台体验与质量保障

> 来源: [`plans/0.9.5.md`](plans/0.9.5.md)（详细设计）、[`plans/README.md`](plans/README.md)（版本路线图）。

---

## 版本状态

```
0.9.0 (done) → 0.9.1 (done) → 0.9.2 (done) → 0.9.3 (done) → 0.9.4 (done) → 0.9.5 (current)
  发布正式版      默认仓库创建      文档多语言          捆绑包迁移          文档与质量完善          跨平台体验与质量保障
  → 0.9.6 (next) → ... → 1.0.0
    功能补全              正式发布
```

---

## 一、目标

0.9.4 完成了文档和质量打磨。本版本聚焦两个关键缺口：**Windows 原生安装体验**（当前仅有 `install.sh`，裸 Windows 用户需手动下载预编译二进制）和**缺乏端到端集成测试**（17 个测试文件全部为单元测试，从未真正验证完整的 build pipeline）。

作为 1.0.0 之前的质量保障版本，三平台冒烟测试将确保 Linux / macOS / Windows 上的行为一致性。

三项交付物：

| # | 交付物 | 说明 |
|---|--------|------|
| 1 | PowerShell 安装脚本 | 新建 `install.ps1`，对标 `install.sh`，提供 Windows 原生一键安装体验 |
| 2 | 集成测试 | 至少 1 个端到端测试场景：`project new` → `pkg install` → `project build` → `project run` |
| 3 | 三平台冒烟测试 | Linux + macOS + Windows/MSYS2 + Windows/MSVC 上验证核心流程 |

---

## 二、当前状态分析

### 2.1 安装方式现状

```
install.sh          ✅ Bash 脚本（Linux / macOS / MSYS2）
install.ps1         ❌ 不存在 — 裸 Windows（无 MSYS2）用户无原生安装途径
```

`install.sh` 功能清单（对标基准）：
- 平台检测（Linux / macOS / MSYS2）
- `git clone` → `bash build.sh` 源码编译安装
- 原子化安装（temp copy → mv）
- 内置 `ezmk-cc` 工具安装
- 官方仓库预注册（`ezmk repo add -u`）
- zsh 命令补全安装
- PATH 检查和提示

### 2.2 测试现状

```
test/
├── test_main.cpp           ✅ Catch2 入口（17 个测试文件）
├── test_build.cpp          ✅ 单元测试
├── test_cache.cpp          ✅ 单元测试
├── test_cli.cpp            ✅ 单元测试
├── test_config.cpp         ✅ 单元测试
├── test_crypto.cpp         ✅ 单元测试
├── test_file_watcher.cpp   ✅ 单元测试
├── test_hooks.cpp          ✅ 单元测试
├── test_i18n.cpp           ✅ 单元测试
├── test_lua.cpp            ✅ 单元测试
├── test_pkg.cpp            ✅ 单元测试
├── test_project.cpp        ✅ 单元测试
├── test_repo.cpp           ✅ 单元测试
├── test_thread_pool.cpp    ✅ 单元测试
├── test_toolchain.cpp      ✅ 单元测试
├── test_util.cpp           ✅ 单元测试
├── test_utils_perms.cpp    ✅ 单元测试
└── test_integration.cpp    ❌ 不存在 — 无端到端集成测试
```

**关键缺失**：所有测试都是隔离的模块级单元测试，从未验证过完整链路 "创建项目 → 安装依赖 → 编译 → 运行"。

### 2.3 工具函数准备

`util::run_command()` 已存在于 `include/ezmk/util.hpp:115`，可用于集成测试中调用 `ezmk` 二进制。返回 `ProcResult { int exit_code; std::string stdout; std::string stderr; }`。

---

## 三、详细执行步骤

### 阶段一：PowerShell 安装脚本 (`install.ps1`)

**目标文件**: `install.ps1`（仓库根目录）

#### 3.1.1 功能对标

`install.ps1` 需要覆盖 `install.sh` 的核心功能，但适配 Windows 原生环境（无 bash / git / g++ 假定）：

| 功能 | `install.sh` | `install.ps1` |
|---|---|---|
| 平台检测 | Linux/macOS/MSYS2 (uname -s) | Windows (PowerShell 5.1+) |
| 获取方式 | `git clone` + 源码编译 | 从 GitHub Release 下载预编译 `ezmk.exe` |
| 下载工具 | `curl` | `Invoke-WebRequest` (TLS 1.2+) |
| 安装路径 | `$PREFIX/bin`（默认 `~/.local/bin`） | `$env:LOCALAPPDATA\ezmk\bin` |
| PATH 配置 | shell rc 文件追加 | 用户 PATH 环境变量（`[Environment]::SetEnvironmentVariable`） |
| 仓库预注册 | `ezmk repo add -u` 官方仓库 | 同（调用已安装的 ezmk.exe） |
| 权限检查 | `sudo` 检测 | 管理员权限检测（写入系统 PATH 时需要） |
| SHA-256 校验 | 从 GitHub Release 获取 checksum | 同 |
| 幂等性 | 重复运行安全（覆盖升级） | 同 |
| 版本选择 | `EZMK_REF` 环境变量（git ref） | `-Version` 参数（GitHub Release tag） |

**关键差异**: `install.sh` 是源码编译安装，`install.ps1` 是下载预编译二进制安装（因为裸 Windows 用户没有 g++）。这决定了 `install.ps1` 不需要 git/g++ 依赖。

#### 3.1.2 脚本结构

```powershell
# install.ps1 — EazyMake Windows installer
param(
    [string]$Version = "latest",          # 目标版本，默认最新
    [string]$InstallDir = "",             # 安装根目录，默认 $env:LOCALAPPDATA\ezmk
    [switch]$NoPath = $false,             # 跳过 PATH 配置
    [switch]$DryRun = $false              # 仅打印操作，不实际执行
)

# === 1. 前置检查 ===
# - PowerShell 版本 ≥ 5.1
# - TLS 1.2 可用
# - 网络连通性检查（GitHub API 可达）

# === 2. 版本解析 ===
# - 若 $Version -eq "latest"，查询 GitHub Release API 获取最新 tag
# - 否则校验指定版本是否存在

# === 3. 下载 ===
# - 构造下载 URL: https://github.com/3667808244/EazyMake/releases/download/<tag>/ezmk.exe
# - Invoke-WebRequest 下载到临时目录
# - 下载 SHA-256 checksum 文件并校验

# === 4. 安装 ===
# - 默认 InstallDir = "$env:LOCALAPPDATA\ezmk"（无需管理员权限）
# - 创建 $InstallDir\bin\ 目录
# - 原子化写入：先写临时文件，校验成功后 move 到目标路径
# - 安装内置 ezmk-cc（内置于 ezmk.exe 的 Lua 工具）

# === 5. PATH 配置 ===
# - 使用 [Environment]::SetEnvironmentVariable 写入用户级 PATH
# - 若已存在则跳过（幂等）
# - -NoPath 跳过此步骤

# === 6. 官方仓库预注册 ===
# - 调用 ezmk repo add -u https://github.com/3667808244/ezmk-repo.git --name official
# - 调用 ezmk repo update -u official
# - 失败不阻塞安装（可能无网络）

# === 7. 验证 ===
# - ezmk --version
# - ezmk --help
```

#### 3.1.3 安装方式设计

```powershell
# 方式一：下载脚本后审查运行（推荐）
# 1. 浏览器打开 https://raw.githubusercontent.com/.../install.ps1
# 2. 审查脚本内容
# 3. .\install.ps1

# 方式二：一键远程执行（便利）
irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex

# 参数化使用
.\install.ps1 -Version "0.9.5" -InstallDir "D:\tools\ezmk"
```

#### 3.1.4 安全考量

- 推荐方式一（审查后运行），`README.md` 中优先展示
- 方式二仅为便利提供，附带 "请审查后再运行" 提示
- 强制使用 TLS 1.2+：`[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12`
- 下载文件 SHA-256 校验（校验失败 → 拒绝安装）
- 安装到用户目录（`%LOCALAPPDATA%`），无需管理员权限（除非用户指定系统级路径）

#### 3.1.5 执行清单

- [ ] 创建 `install.ps1`，实现完整脚本结构（7 个步骤）
- [ ] 实现 GitHub Release API 调用（版本解析 + 下载 URL 构造）
- [ ] 实现 `Invoke-WebRequest` 下载 + SHA-256 校验
- [ ] 实现原子化安装（临时文件 → 校验 → 移动）
- [ ] 实现用户级 PATH 配置（`[Environment]::SetEnvironmentVariable`）
- [ ] 实现官方仓库预注册（调用已安装的 ezmk.exe）
- [ ] 实现 `-DryRun` 模式
- [ ] 添加完整的 comment-based help（`Get-Help .\install.ps1`）
- [ ] 在 Windows 10 裸机（无 MSYS2）上测试安装流程
- [ ] 在 Windows 11 裸机（无 MSYS2）上测试安装流程
- [ ] 测试幂等性（重复运行不报错）
- [ ] 测试 `-NoPath` 和 `-Version` 参数
- [ ] 更新 `README.md` 的 Windows 安装说明（增加 `install.ps1` 方式）
- [ ] 更新 `README_ZH.md` 对应内容
- [ ] 更新 `docs/en/cli.md` 安装章节
- [ ] 更新 `docs/zh/cli.md` 安装章节

---

### 阶段二：集成测试

**目标文件**: `test/test_integration.cpp`（新建）

#### 3.2.1 测试框架

在现有 Catch2 v3 框架内新增一个测试文件。`build.sh test` 自动编译 `test/test_*.cpp`，无需修改构建脚本。

集成测试调用已编译的 `ezmk` 二进制（通过 `util::run_command()` 或直接构造命令行），在独立临时目录中执行端到端场景。

**前提**：集成测试运行时需要 `build/ezmk`（或 `build/ezmk.exe`）已编译。`build.sh test` 只编译测试二进制，不编译 ezmk 本身。需在 `build.sh` 中增加逻辑或提供独立脚本。

**方案**：在 `build.sh` 的 test 模式下，先编译 `ezmk` 二进制（若不存在或过期），再编译测试。测试中用 `#ifdef` 或环境变量指定 ezmk 二进制路径。

#### 3.2.2 测试场景

所有场景使用 Catch2 `TEST_CASE` + `SECTION` 组织，标记为 `[integration]` tag，允许按需运行：

```bash
# 仅运行集成测试
./build/test_ezmk "[integration]"

# 跳过集成测试（仅单元测试）
./build/test_ezmk "~[integration]"
```

**场景一：从零创建项目 → 安装依赖 → 构建 → 运行**

```
GIVEN 一个空的临时目录
WHEN  ezmk project new test_app
THEN  目录中包含 ezmk.toml + src/main.cpp + include/

GIVEN 编辑 ezmk.toml，添加 [depends] lib = ["catch2"]
WHEN  ezmk pkg install catch2 -p -y
THEN  .ezmk/pkg/ 中存在 catch2 包

GIVEN 依赖已安装
WHEN  ezmk project build
THEN  build/ 目录生成可执行文件 (test_app.exe 或 test_app)
AND   退出码为 0

GIVEN 构建成功
WHEN  ezmk project run
THEN  stdout 包含 "Hello" 或项目默认输出
AND   退出码为 0
```

**场景二：增量构建 — 缓存命中**

```
GIVEN 项目已成功构建一次
WHEN  再次运行 ezmk project build
THEN  所有源文件均为缓存命中（cache hit）
AND   构建时间显著短于首次
AND   日志中包含 "cached" 或 "up to date"
```

**场景三：Watch 模式 — 文件变更触发重编译**

```
GIVEN 项目已成功构建
WHEN  后台启动 ezmk project watch（超时终止，如 5 秒后 kill）
AND   修改 src/main.cpp（追加一个空格或注释）
THEN  watch 模式检测到文件变更
AND   触发重新编译
```

**场景四：`ezmk utils cc` — compile_commands.json 生成**

```
GIVEN 一个完整的项目
WHEN  ezmk utils cc
THEN  生成 compile_commands.json
AND   文件包含 src/main.cpp 的编译命令
AND   文件为合法 JSON（可被解析）
```

#### 3.2.3 实施要点

- **隔离**: 每个场景使用独立临时目录 `std::filesystem::temp_directory_path() / "ezmk_test_XXXXXX"`，`TEST_CASE` 结束后清理
- **ezmk 路径**: 通过环境变量 `EZMK_TEST_BIN` 指定 ezmk 二进制路径；默认 `build/ezmk`（Windows: `build/ezmk.exe`）
- **超时保护**: Watch 模式测试需超时机制（启动 watch → sleep N 秒 → 触发变更 → sleep N 秒 → kill watch 进程）
- **跳过条件**: 如果 ezmk 二进制不存在 → `SKIP`（而非 FAIL）；无网络时 pkg install 场景 → `SKIP`
- **平台适配**: 可执行文件名后缀（`.exe` vs 无后缀）、路径分隔符在测试中处理
- **辅助函数**: 封装 `run_ezmk(args, work_dir) -> ProcResult` 简化调用

#### 3.2.4 build.sh 适配

当前 `build.sh test` 只编译测试二进制。需修改为：
1. 先编译 `ezmk` 二进制（若 `build/ezmk` 不存在）
2. 再编译测试二进制
3. 运行测试时 `EZMK_TEST_BIN` 自动指向刚编译的 ezmk

#### 3.2.5 执行清单

- [ ] 在 `test/` 下创建 `test_integration.cpp`，搭建测试框架（临时目录、ezmk 调用封装）
- [ ] 实现场景一："从零到运行"（`project new` → `pkg install` → `project build` → `project run`）
- [ ] 实现场景二："缓存命中"（两次 build，验证缓存日志）
- [ ] 实现场景三："Watch 模式"（启动 watch → 修改文件 → 验证重编译）
- [ ] 实现场景四："compile_commands.json 生成"（`ezmk utils cc` 验证）
- [ ] 所有场景添加 `[integration]` tag
- [ ] 处理平台差异（`.exe` 后缀、路径分隔符）
- [ ] 处理环境缺失（无 ezmk 二进制 → SKIP，无网络 → SKIP）
- [ ] 修改 `build.sh`：test 模式下先编译 ezmk 二进制
- [ ] 在 MSYS2 环境运行集成测试验证通过
- [ ] 编写 `test/test_integration.cpp` 的文件头注释，说明运行方式

---

### 阶段三：三平台冒烟测试

此阶段是**手动验证**（非自动化 CI），在三平台上执行相同的核心流程，记录并修复发现的问题。

#### 3.3.1 测试矩阵

| 平台 | 环境 | 安装方式 | 编译器 |
|---|---|---|---|
| Linux | Ubuntu 22.04+ | `install.sh` | g++ |
| macOS | macOS 13+ (Intel) | `install.sh` | clang++ (Apple) |
| macOS | macOS 13+ (Apple Silicon) | `install.sh` | clang++ (Apple) |
| Windows | Windows 10+ / MSYS2 | `install.sh` (MSYS2 内) | g++ (MSYS2) |
| Windows | Windows 10+ / 裸机 | `install.ps1` | 预编译二进制 |

#### 3.3.2 每平台验证步骤

```
1. 全新安装
   - Linux/macOS/MSYS2: curl ... | bash (或下载 install.sh 后运行)
   - Windows 裸机: .\install.ps1（或 irm ... | iex）

2. 基本验证
   - ezmk --version  → 输出版本号 "0.9.5"
   - ezmk --help     → 输出帮助信息（无崩溃）

3. 核心流程
   - ezmk project new smoke_test
   - cd smoke_test
   - ezmk project build              → 编译成功，生成可执行文件
   - ezmk project run                → 输出 "Hello, World!" 或默认输出

4. 包管理
   - ezmk pkg install catch2 -p -y   → 下载 + 安装成功
   - 编辑 ezmk.toml 添加 [depends] lib = ["catch2"]
   - ezmk project build              → 链接 catch2 成功

5. 内置工具
   - ezmk utils cc                   → 生成 compile_commands.json

6. 高级功能（抽查）
   - ezmk project build -j 4         → 并行编译正常
   - ezmk pkg list -p                → 列出已安装包
   - ezmk repo list                  → 列出已注册仓库
```

#### 3.3.3 冒烟记录模板

每次冒烟测试记录以下信息：

```markdown
## 平台: [Linux / macOS / Windows]
## 环境: [操作系统版本 / 编译器版本 / 内核架构]
## 日期: YYYY-MM-DD
## 结果: [PASS / FAIL]

### 测试详情
| 步骤 | 预期 | 实际 | 状态 |
|------|------|------|------|
| install | 成功安装 | ... | ✅/❌ |
| --version | 输出版本号 | ... | ✅/❌ |
| project new | 创建项目 | ... | ✅/❌ |
| project build | 编译成功 | ... | ✅/❌ |
| project run | 输出正确 | ... | ✅/❌ |
| pkg install | 下载安装 | ... | ✅/❌ |
| build with dep | 链接成功 | ... | ✅/❌ |
| utils cc | 生成 JSON | ... | ✅/❌ |

### 发现的问题
1. [问题描述] → 修复: [commit hash / 待修复]
```

#### 3.3.4 执行清单

- [ ] **Linux 冒烟测试**（Ubuntu 22.04+ / g++）— 记录结果到 `plans/smoke_test_0.9.5_linux.md`
- [ ] **macOS Intel 冒烟测试**（macOS 13+ / clang++）— 记录结果
- [ ] **macOS Apple Silicon 冒烟测试**（macOS 13+ / clang++）— 记录结果
- [ ] **Windows/MSYS2 冒烟测试**（Windows 10+ / g++）— 记录结果
- [ ] **Windows 裸机冒烟测试**（Windows 10+ / `install.ps1`）— 记录结果
- [ ] 汇总所有平台问题 → 建立 issue 或直接修复
- [ ] 修复确认后在对应平台回归验证

---

## 四、校验清单

### 脚本校验

- [ ] `install.ps1` 语法正确（`Get-Command` 或 `powershell -NoProfile -Command "..."` 语法检查）
- [ ] `install.ps1` 在 Windows 10 PowerShell 5.1 上通过
- [ ] `install.ps1` 在 Windows 11 PowerShell 7.x 上通过
- [ ] `install.ps1` 的 SHA-256 校验在文件损坏时正确拒绝安装
- [ ] `install.ps1` 重复运行不报错（幂等性）
- [ ] `install.ps1 -DryRun` 不产生副作用

### 编译与单元测试

- [ ] `bash build.sh` 编译通过（MSYS2）
- [ ] 全量单元测试通过：`build/test_ezmk ~"[integration]"`（0 failures）
- [ ] 集成测试通过：`build/test_ezmk "[integration]"`（0 failures）

### 集成测试校验

- [ ] 场景一（从零到运行）：在 MSYS2 上通过
- [ ] 场景二（缓存命中）：验证缓存日志包含预期标记
- [ ] 场景三（Watch 模式）：验证文件变更被检测
- [ ] 场景四（compile_commands.json）：生成合法 JSON
- [ ] 环境缺失时正确 SKIP（非 FAIL）

### 文档校验

- [ ] `README.md` 中 Windows 安装说明更新（增加 `install.ps1` 方式）
- [ ] `README_ZH.md` 对应更新
- [ ] `docs/en/cli.md` 安装章节更新
- [ ] `docs/zh/cli.md` 安装章节更新
- [ ] 无 broken links

### 冒烟校验（手动）

- [ ] 至少 Linux + Windows/MSYS2 两个平台冒烟通过（最低要求）
- [ ] 发现的问题已记录或修复

---

## 五、兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `install.ps1` | 新用户获得 Windows 原生安装方式 | 纯增量 |
| 新增 `test_integration.cpp` | 仅影响 CI/开发流程 | 纯增量 |
| `build.sh` test 模式适配 | test 模式下先编译 ezmk 二进制 | 不影响已有流程 |
| 文档更新 | Windows 安装说明完善 | 纯增量 |
| 冒烟测试 | 可能发现跨平台 bug | 在 0.9.5 内修复，不推迟 |

---

## 六、开放问题

1. **`install.ps1` 是否需要管理员权限？** — 默认安装到 `%LOCALAPPDATA%` 不需要；若用户指定系统级路径则需管理员。脚本检测权限并给出清晰提示。

2. **集成测试中 `ezmk` 二进制路径** — 通过环境变量 `EZMK_TEST_BIN` 传入，默认 `build/ezmk(.exe)`。`build.sh test` 自动设置此变量。

3. **集成测试对网络依赖** — `pkg install` 场景需要网络下载包。测试应检测网络可用性，不可用时 `SKIP` 而非 `FAIL`。

4. **Watch 模式测试的稳定性** — 基于 sleep + 超时的方式在不同性能机器上可能不稳定。设定合理的超时上限（如 15 秒），超时则 `SKIP`。

5. **冒烟测试的执行者** — macOS 冒烟测试需要 Apple 硬件；如当前无法获取，可推迟到 0.9.6 或 1.0.0 之前补齐。最低要求 Linux + Windows 两个平台。

6. **`install.ps1` 的 SHA-256 checksum 来源** — 需确认 GitHub Release 是否发布 checksum 文件。若 0.9.5 尚未有 Release，可先实现逻辑并在 Release 创建后验证。

7. **集成测试是否纳入 `build.sh test` 默认运行** — 建议默认跳过（`~"[integration]"`），仅 CI 或手动指定时运行。理由：集成测试依赖编译好的 ezmk 二进制和网络。

---

## 七、跨版本关注点

- **向后兼容**: 所有变更为纯增量（新增脚本、新增测试文件），不改变已有行为。
- **依赖**: 依赖 0.9.4（文档就绪，FAQ 可供冒烟测试参考排错）。
- **0.9.6 准备**: 本版本的集成测试将为 0.9.6 的依赖版本锁定等功能提供端到端验证基础。
- **CI 建设**: 本版本的三平台冒烟测试可为后续建立自动化 CI 流水线打下基础。冒烟测试记录（`plans/smoke_test_0.9.5_*.md`）可作为 CI 脚本的参考用例。
- **文档一致性**: 0.9.2 建立的 `docs/en/` ↔ `docs/zh/` 一一对应关系需持续维护；新增 Windows 安装说明必须双语同步交付。
- **`install.ps1` 长期维护**: 与 `install.sh` 一样，未来版本发布时需同步更新两个安装脚本（如新增仓库、新增默认工具等）。
