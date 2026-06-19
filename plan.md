# EazyMake 0.1.4 更新计划

> 0.1.3 已完成 pkg/repo 子命令，0.1.4 聚焦于构建健壮性、包编译链路完善和安装安全性。

---

## 1. 编译产物原子写入

### 背景

当前 `record.json` 已通过"写 `.tmp` 再 `rename`"实现原子写入（`cache.cpp:227-239`），但 `.o` / `.obj` 文件和 `.a` 归档文件由 GCC/ar 直接写入目标路径，若构建中途中断（Ctrl+C、断电），残留的半截文件会在下次构建时被当作"有效缓存"命中，导致链接失败或静默生成错误二进制。

### 任务

#### 1.1 项目构建 `.o` 原子写入

**涉及文件**: `src/build.cpp`（`build_project`）、`src/cache.cpp`

- 编译时指定 `-o` 到临时文件（如 `main.cpp.tmp.o`）
- 编译成功 → `rename` 覆盖正式 `.o`
- 编译失败 → 删除临时文件，不影响已有 `.o`
- `record.json` 条目同样在编译成功后才更新（当前已在 `save_record` 中原子化，但要确保只在编译成功后写入对应条目——目前是逐文件更新 record 后统一 save，可保持但需保证 .o 原子性）

#### 1.2 包编译 `.o` 和 `.a` 原子写入

**涉及文件**: `src/pkg.cpp`（`compile_package`）

- 同 1.1，编译 `.o` 到临时文件再 rename
- `ar rcs` 创建 `.a` 时先输出到 `build/lib<name>.a.tmp`，成功后再 rename
- 包缓存记录 `.pkg_cache.json` 同理（当前已原子写入，无需改动）

#### 1.3 构建中途失败的清理策略

- 启动构建前扫描并清理 `.ezmk/temp/` 和 `build/` 下的 `*.tmp.*` 残留文件
- 避免上次崩溃的临时文件污染本次构建

> ✅ **已完成 (1.1 + 1.2 + 1.3)**: 项目构建和包编译均通过 `.tmp.o` / `.a.tmp` 实现原子写入；link 产物（.a/.dll/.so/.exe）也先写 `.tmp` 再 rename；构建前清理残留 tmp 文件。涉及文件: `build.cpp`, `pkg.cpp`。

---

## 2. 包编译选项与链接选项传播

### 背景

包的 `ezmk.toml` 已有完整的 `[compile]` 和 `[link]` 节，且 `config.cpp` 已能解析。但在项目构建链路 (`build.cpp`) 中，只收集了包的 `.a` 归档和 `include/` 路径，**未读取包的 `[link]` 配置**。这意味着包声明依赖的系统库（如 `pthread`、`m`）或自定义链接标志不会传递到最终链接命令。

### 任务

#### 2.1 收集包的链接选项

**涉及文件**: `src/build.cpp`（`build_project`）、`src/pkg.cpp`

- 新增辅助函数 `pkg::collect_link_options(const fs::path& pkg_dir)` → 返回包的 `LinkSection`
  - 或直接在 `build_project` 中遍历 `.ezmk/pkg/` 时读取每个包的 `ezmk.toml`，合并 `link.flags`、`link.link_dirs`、`link.system_targets`
- 合并策略：项目自身选项优先，包选项追加（去重可由链接器处理）
- 注意：依赖包的编译选项（`compile.flags`、`compile.include_dirs`）在包编译阶段已使用，无需重复传播

#### 2.2 依赖包的 include 路径自动收集

当前的 `extra_includes` 只收集 `.ezmk/pkg/<name>/include/`，但包可能在 `compile.include_dirs` 中声明额外路径（如 `include/internal`）。需要一并收集。

**涉及文件**: `src/build.cpp`

- 在遍历 `.ezmk/pkg/` 时，解析每个包的 `ezmk.toml`，收集 `compile.include_dirs` 中相对于包根目录的路径

#### 2.3 `ezmk pkg info` 展示完整链接信息

**涉及文件**: `src/pkg.cpp`（`info` 函数）

- 在现有输出中增加：
  - `Link flags:`（包的 `link.flags`）
  - `Link dirs:`（包的 `link.link_dirs`）
  - `System targets:`（包的 `link.system_targets`）
- 当包作为依赖被项目链接时，这些信息对排查链接错误很有价值

> ✅ **已完成 (2.1 + 2.2 + 2.3)**: `build_project` 遍历 `.ezmk/pkg/` 时解析每个包的 `ezmk.toml`，收集 `link.flags`/`link_dirs`/`system_targets` 合并到项目链接命令；收集 `compile.include_dirs` 中额外路径作为 `-I`；`ezmk pkg info` 新增 `Link flags:`/`Link dirs:`/`System targets:` 输出。涉及文件: `build.cpp`, `pkg.cpp`。

---

## 3. SHA-256 校验（仓库安装安全）

### 背景

`docs/@safety.md` 规定："从 URL 下载包时，若仓库 `index.toml` 提供了 `sha256`，必须校验"。当前 `repo.cpp` 的 `read_pkg_file_from_index` 可读取 `sha256` 但忽略该字段，`pkg::install` 也未校验。这是安全合规缺口。

### 任务

#### 3.1 SHA-256 校验实现

**涉及文件**: `src/pkg.cpp`（`install`）、`src/repo.cpp`、`include/ezmk/pkg.hpp`

- 在 `pkg::install` 中，下载/获取归档后、解压前，计算文件 SHA-256
- 若归档来自仓库（通过 `search_package` 获取），从 `index.toml` 读取对应的 `sha256` 字段并比对
- 校验失败 → 报错并清理临时文件，阻止安装
- 若仓库未提供 `sha256` → 跳过校验（向后兼容）
- 需要调整 `search_package` 的返回值以携带 `sha256`（或新增重载/输出参数）

#### 3.2 用户提供的 URL/本地文件也可带校验

- `ezmk pkg install` 增加可选 `--sha256 <hash>` 参数
- 若提供，安装时校验归档哈希
- **涉及文件**: `src/cli.cpp`（`parse_pkg_args`）、`include/ezmk/cli.hpp`（`InstallOptions` 增加字段）

> ✅ **已完成 (3.1 + 3.2)**: `search_package` 返回 `PkgSearchResult{archive_path, sha256}`；`pkg::install` 在解压前校验 SHA-256；`read_pkg_from_index` 从 `index.toml` 提取 `sha256` 字段；CLI 支持 `--sha256 <hash>` 和 `-y`/`--yes` 标志。涉及文件: `repo.hpp`, `repo.cpp`, `pkg.hpp`, `pkg.cpp`, `cli.hpp`, `cli.cpp`, `main.cpp`。

---

## 4. preinstall / postinstall 钩子脚本

### 背景

包文件结构中在 `script/` 目录下增加两个可选脚本，按平台选择对应文件：

| 平台 | preinstall | postinstall | 说明 |
|------|-----------|-------------|------|
| Linux | `script/preinstall.sh` | `script/postinstall.sh` | bash 执行 |
| Windows | `script/preinstall.ps1`（优先）或 `script/preinstall.bat` | `script/postinstall.ps1`（优先）或 `script/postinstall.bat` | PowerShell / cmd 执行 |

- 包可同时提供多平台脚本（如 `.sh` + `.ps1`），工具按当前平台选择对应文件
- Windows 下 `.ps1` 优先于 `.bat`，`.ps1` 不存在时回退到 `.bat`

### 任务

#### 4.1 脚本检测与交互确认

**涉及文件**: `src/pkg.cpp`（`install`）

- 解压后、编译前检测 `script/` 目录下是否存在 preinstall 脚本：
  - **Linux**: 检测 `script/preinstall.sh`
  - **Windows**: 优先检测 `script/preinstall.ps1`，不存在则检测 `script/preinstall.bat`
  - 若存在 → 使用系统编辑器打开脚本供审查，编辑器退出后交互式询问 `Execute preinstall.<ext> for <pkg>? [y/N]`
  - 用户确认 → `cwd` 切换到安装目标目录（`dest_dir/<pkg_name>`），执行脚本：
    - Linux `.sh`: `bash <脚本路径>`
    - Windows `.ps1`: `powershell -ExecutionPolicy Bypass -File <脚本路径>`
    - Windows `.bat`: `cmd /c <脚本路径>`
  - 用户拒绝 → 跳过，继续安装流程
  - 脚本执行失败 → 询问 `preinstall script failed. Continue installation? [y/N]`
- 编译安装后检测 postinstall 脚本（`.sh` / `.ps1` / `.bat` 同理）
  - 同理交互确认，`cwd` 为已安装的目录
  - 执行失败不阻断安装完成通知，但需警告

#### 4.2 安全考量

- 脚本以当前用户权限执行，不提升权限
- 在执行前使用系统编辑器打开脚本供用户审查：
  - Windows: `notepad <脚本路径>`
  - Linux: 按顺序尝试 `vim` → `nano` → `emacs`，使用首个可用的编辑器
  - 编辑器退出后询问用户是否确认执行
- 全局安装 (`-g`) 的脚本执行需要二次确认（叠加全局安装的已有确认）
- Linux 下未找到 `bash` 时，警告并跳过，不阻断安装
- 新增 `--yes` / `-y` 标志跳过所有交互（自动执行脚本），用于 CI 场景

#### 4.3 CI/非交互支持

**涉及文件**: `src/cli.cpp`、`include/ezmk/cli.hpp`、`src/pkg.cpp`

- `InstallOptions` 增加 `bool assume_yes = false`
- `ezmk pkg install -p <pkg> -y` 自动确认所有提示（包括脚本执行和覆盖确认）

> ✅ **已完成 (4.1 + 4.2 + 4.3)**: `detect_install_script` 在 `script/` 目录下按平台检测 `.sh`/`.ps1`/`.bat` 脚本；`run_install_script` 编辑器审查 → 交互确认 → 执行；`-y`/`--yes` 全局跳过所有确认；`util::find_editor`/`open_in_editor`/`run_script` 新增工具函数。涉及文件: `pkg.cpp`, `pkg.hpp`, `main.cpp`, `cli.cpp`, `cli.hpp`, `util.hpp`, `util.cpp`。

---

## 5. 缓存记录一致性强化（补充）

### 背景

当前 `cache::check_cache` 通过头文件哈希逐项比对来判断缓存有效性，但当头文件被删除（源文件不再依赖它）时，旧记录中的依赖项仍存在但不会被检测到"增加/删除"变化。需要在编译后对比新旧依赖集合。

### 任务

#### 5.1 依赖集合变化检测

**涉及文件**: `src/cache.cpp`（`check_cache`）

- 缓存命中条件增加：当前编译产出的依赖文件路径集合必须与记录中的一致（数量和路径名）
- 如果路径集合有增减 → 缓存失效
- 实现方式：重新编译时（cache miss）在更新 record 前比较新旧 `dependencies` 的 path 集合，若仅有哈希变化则更新，若有路径变化则将集合变化也作为缓存键

> **注**: 此改动较小，可合入上面第 1 节的改动中一并完成。

> ✅ **已完成 (5.1)**: `cache::same_dependency_paths` 比较新旧依赖路径集合；`build.cpp` 和 `pkg.cpp` 在 cache miss 重编译后检测 include 结构变化并输出诊断信息。涉及文件: `cache.hpp`, `cache.cpp`, `build.cpp`, `pkg.cpp`。

---

## 6. 实现顺序建议

| 优先级 | 条目 | 理由 |
|--------|------|------|
| P0 | 3. SHA-256 校验 ✅ | 安全合规缺口，`@safety.md` 已要求（已完成） |
| P0 | 1. 编译产物原子写入 ✅ | 构建健壮性基础（已完成） |
| P1 | 2. 包链接选项传播 ✅ | 功能完整性，影响多包项目（已完成） |
| P1 | 4. preinstall/postinstall 钩子 ✅ | 新功能，提升包管理能力（已完成） |
| P2 | 5. 缓存依赖集合检测 ✅ | 边界情况修正，影响面小（已完成） |

---

## 涉及文件总览

| 文件 | 改动类型 |
|------|----------|
| `src/build.cpp` | 原子写入、链接选项收集、include 路径收集 |
| `src/pkg.cpp` | 原子写入、SHA-256 校验、钩子脚本、info 增强 |
| `src/cache.cpp` | 依赖集合变化检测 |
| `src/cli.cpp` | `--sha256` / `-y` 参数解析 |
| `src/repo.cpp` | `search_package` 返回 sha256 |
| `include/ezmk/cli.hpp` | `InstallOptions` 字段扩展 |
| `include/ezmk/pkg.hpp` | 新增辅助函数声明 |

---

# EazyMake 0.1.5 代码质量修复

> 基于 0.1.4 代码审查，按严重程度修复所有发现的问题。

## Critical

### C1. SHA-256 位长度编码 UB ✅ [`crypto.cpp`]

**修复**: 使用 `uint64_t` 存储位计数，添加 `<cstdint>`。

### C2. curl 命令注入 ✅ [`util.cpp`]

**修复**: 单引号转义为 `'\''` 模式。

### C3. CRLF 转换破坏数据 ✅ [`cache.cpp`]

**修复**: 仅跳过 `\r\n` 对中的 `\r`，保留独立 `\r`。

### C4. 临时目录名碰撞 ✅ [`pkg.cpp`]

**修复**: 使用 `high_resolution_clock` + `atomic<uint64_t>` 计数器生成唯一名称。

### C5. SHA-256 全量加载 ✅ [`crypto.cpp`]

**修复**: 新增 `Sha256Stream` 类支持流式哈希；`sha256_file` 改为 64KiB 分块读取。

---

## Medium

### M1. 编译循环 DRY 重构 [`build.cpp`, `pkg.cpp`]

**问题**: `build_project` 和 `compile_package` 编译循环约 80% 重复。

**修复**: 提取共享编译逻辑到 `cache::compile_source()`。

### M2. 文件时间转换 bug ✅ [`pkg.cpp`]

**修复**: C++20 使用 `clock_cast`；C++17 回退到 `from_time_t(to_time_t(...))` 正确转换。

### M3. Linux 下 stdout/stderr 合并 ✅ [`util.cpp`]

**修复**: 使用 `mkstemp` 临时文件分别捕获 stdout 和 stderr，用完后 `unlink`。

### M4. `std::stoul` 异常 ✅ [`repo.cpp`]

**修复**: 用 `try-catch` 包裹 `stoul`，非数字段视为 0。

### M5. `file_write` 静默失败 ✅ [`util.hpp`, `util.cpp`]

**修复**: 返回 `bool`，增加写入成功后的 stream 状态检查。

### M6. TOML 序列化转义 ✅ [`repo.cpp`]

**修复**: 对 `"`、`\`、`\n` 进行转义。

---

## Low

### L1. 缺少 `.gitignore` 生成 ✅ [`project.cpp`]

### L2. 编译器名硬编码 → 推迟到 0.1.6（需支持 clang 等）

### L3. `.d` 文件路径含冒号 → 极边缘情况，暂不修复

### L4. `rename` 失败静默降级日志 ✅ [`build.cpp`]

### L5. 缺少 C++98/C++03/C26 标准版本 ✅ [`config.cpp`]

### L6. shell 注入风险（Linux 路径）→ M3 已解决 run_command 层面

### L7. 变量名遮蔽 → 低优先，推迟

### M1. 编译循环 DRY 重构 → 推迟到 0.1.6（较大重构）