# EazyMake 1.1.0-dev.5 执行计划

> 详细设计：[`plans/release/1.1.0-dev.5.md`](plans/release/1.1.0-dev.5.md)

---

## 1 背景

当前 EazyMake 存在三个待解决问题：

1. **缺少官方 utils 包体系**：`ezmk-cc` 以硬编码内置方式分发，与"默认 util 包"体系不一致；缺少链接管理、构建包生成等常用 util
2. **跨项目源文件共享不便**：无标准化的链接机制，多项目共享源文件需手动管理相对路径
3. **watch 模式 bug**：Linux 和 Windows 上对默认项目运行 `ezmk project watch` 时报错 `filesystem error: cannot make absolute path: Invalid argument []`

**解决方案**：创建 `ezmk-official-utils` 包（含 `link`/`cc`/`gen-build-package` 三个工具），引入 `.ezmk/links.json` 链接机制与 `@link:` 路径语法，修复 watch 空路径 bug。

---

## 2 目标

| # | 目标 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | **Watch 空路径修复** | P0 | 定位 `fs::absolute("")` 的根因，添加防御性检查 + 测试 |
| 2 | **`@link:` 链接机制** | P0 | `resolve_link_path()` 实现、循环检测、深度限制、`ezmk.toml` 解析集成 |
| 3 | **`ezmk-official-utils` 包** | P0 | 创建包结构 + `ezmk.toml` + 注册到 `ezmk-repo` |
| 4 | **`link` 工具** | P0 | `link.lua` 实现 add/remove/list/show |
| 5 | **`cc` 迁移** | P0 | `cc.lua` 从内置移到 `ezmk-official-utils`，移除硬编码加载 |
| 6 | **`gen-build-package` 工具** | P1 | `gen-build-package.lua` 生成自包含构建包 |
| 7 | **安装脚本预装** | P0 | `install.sh` + `install.ps1` 末尾预装 `ezmk-official-utils` |

---

## 3 执行阶段

### 阶段一：Watch 空路径修复（P0，独立 bugfix）

**文件**：`src/file_watcher.cpp` + `src/project.cpp`（或 `src/build.cpp`）+ `test/test_file_watcher.cpp`

- [ ] 在 watch 目录收集逻辑中添加空路径断言/日志，复现并定位空字符串来源
- [ ] 定位根因：`src_dirs`/`include_dirs` 默认值 `["src"]` 解析路径时某代码路径拼接出错产生空字符串
- [ ] 修复：确保所有传入 `fs::absolute()` 的路径非空；在路径拼接处添加 `fs::path` 有效性检查
- [ ] 加固：`FileWatcher::add_directory()` 入口处防御性检查，空路径直接跳过 + warning
- [ ] 测试：`test_file_watcher.cpp` 新增空路径传入不崩溃用例；创建默认项目 → `ezmk project watch` 手动验证

### 阶段二：`@link:` 链接机制（P0，核心 C++ 功能）

**文件**：`include/ezmk/config.hpp` + `src/config.cpp` + `src/util.cpp`

- [ ] 在 `config.hpp` 中声明 `resolve_link_path(name, sub_path, links_map)` 函数：
  - 参数：链接名称、子路径（可为空）、`.ezmk/links.json` 解析结果
  - 返回：解析后的绝对/相对路径
- [ ] 在 `config.cpp` 中实现 `resolve_link_path()`：
  - 查找 `.ezmk/links.json` 中对应条目
  - 条目不存在 → 报错 `"link '{name}' not found in .ezmk/links.json"`
  - `@link:<name>` 无子路径 → 直接替换为目标路径
  - `@link:<name>/sub/path` → 目标路径 + `/sub/path`
  - 链接链解析：允许 A→B→C 链式解析，深度限制 10 层，超过报错
  - 循环链接检测：A 引用 B，B 引用 A → 解析时报错
- [ ] 在 `util.cpp` 中实现 `parse_link_syntax()` 解析 `@link:<name>/...` 格式
- [ ] 修改 `config.cpp` 的 TOML 解析，在读取 `src_dirs`/`include_dirs`/`link_dirs` 后自动展开 `@link:` 引用
- [ ] 应用场景覆盖：`[compile]` 的 `src_dirs`、`include_dirs` 和 `[link]` 的 `link_dirs`

### 阶段三：`ezmk-official-utils` 包基础结构（P0）

**文件**：新建 `ezmk-official-utils/` 目录（在 `ezmk-repo` 仓库中）

- [ ] 创建包目录结构：
  ```
  ezmk-official-utils/
  ├── ezmk.toml
  ├── utils/
  │   ├── link.lua
  │   ├── cc.lua
  │   └── gen-build-package.lua
  └── README.md
  ```
- [ ] 编写 `ezmk.toml`：
  ```toml
  [project]
  name = "ezmk-official-utils"
  type = "utils"
  version = "1.1.0"
  language = "C++17"

  [utils]
  tools = ["link", "cc", "gen-build-package"]
  ```
- [ ] 编写 `README.md` 说明三个工具的用途和使用方法
- [ ] 注册到 `ezmk-repo` 的 `index.toml`，作为 `type = "utils"` 包

### 阶段四：`link` 工具实现（P0）

**文件**：`ezmk-official-utils/utils/link.lua`

- [ ] 实现 CLI 命令：
  - `ezmk utils link add <name> <path>` — 添加链接
  - `ezmk utils link remove <name>` — 删除链接
  - `ezmk utils link list` — 列出所有链接
  - `ezmk utils link show <name>` — 查看链接详情
- [ ] 行为细节：
  - `add`：检查 name 合法性（字母/数字/下划线/连字符，不允许 `..` 和 `/`）；path 必须存在（文件或目录）；写入 `.ezmk/links.json`
  - `remove`：删除条目，`.ezmk/links.json` 变空则保留 `{}`
  - `list`：格式化打印所有链接（名称 → 路径，目标是否存在）
  - `show`：打印目标路径 + 目标是否存在 + 目录则列出内容摘要
- [ ] Lua API 使用：`ezmk.file_read()`/`ezmk.file_write()` 读写 JSON，`ezmk.json_decode()`/`ezmk.json_encode()` 解析，`ezmk.file_exists()` 验证，`ezmk.project_root()` 获取根目录

### 阶段五：`cc` 从内置迁移到 `ezmk-official-utils`（P0）

**文件**：`ezmk-official-utils/utils/cc.lua` + `src/lua_api.cpp`

- [ ] 将现有内置 cc 脚本内容迁移到 `ezmk-official-utils/utils/cc.lua`
- [ ] 从 `src/lua_api.cpp` 的 `find_utils_script()` 中移除 `cc` 的硬编码加载逻辑
- [ ] 确保 `ezmk utils cc` 通过标准 utils 查找链（project → user → global）发现 `cc.lua`
- [ ] 验证：安装 `ezmk-official-utils` 后 `ezmk utils cc` 正常工作

### 阶段六：`gen-build-package` 工具实现（P1）

**文件**：`ezmk-official-utils/utils/gen-build-package.lua`

- [ ] 实现 CLI：`ezmk utils gen-build-package [--output <dir>] [--name <name>]`
- [ ] 功能：为当前项目生成自包含构建包（`.tar.gz`），包含：
  - 所有源文件（`src_dirs` 下文件）
  - 头文件（`include_dirs` 下文件）
  - `ezmk.toml`
  - 生成的构建脚本（`build.sh` / `build.ps1`），使用 `ezmk project build` 编译
- [ ] 输出结构：
  ```
  <name>-build-<version>.tar.gz
  ├── <name>/
  │   ├── ezmk.toml
  │   ├── src/...
  │   ├── include/...
  │   └── build.sh
  ```
- [ ] Lua API 使用：`ezmk.project_*()` 获取项目信息，`ezmk.run_command()` 调 tar，`ezmk.file_*()` 复制文件

### 阶段七：安装脚本预装逻辑（P0）

**文件**：`install.sh` + `install.ps1`

- [ ] 在 `install.sh` 安装流程末尾添加：
  ```bash
  # 预装官方 utils 包
  ./build/ezmk repo add ezmk-repo <url> 2>/dev/null || true
  ./build/ezmk pkg install -g ezmk-official-utils -y 2>/dev/null || \
    warn "预装 ezmk-official-utils 失败（网络不可用），可稍后手动安装"
  ```
- [ ] 在 `install.ps1` 中添加等效 PowerShell 逻辑
- [ ] 使用 `-g`（全局作用域）安装，确保所有用户项目可用
- [ ] 使用 `-y` 跳过确认
- [ ] 预装失败不阻塞安装（网络问题时降级为 warning）

### 阶段八：编译与回归验证

- [ ] `bash build.sh` 编译通过（MSYS2 / Windows）
- [ ] 全量测试通过，零回归
- [ ] 手动验证：创建默认项目 → `ezmk project watch` → 确认不再报错
- [ ] 手动验证：`ezmk utils link add/list/remove/show` 完整流程
- [ ] 手动验证：`ezmk utils cc` 正常工作（通过 utils 查找链发现）
- [ ] 手动验证：`ezmk utils gen-build-package` 生成构建包 + 解包验证
- [ ] 手动验证：`ezmk.toml` 中 `@link:` 语法解析正确
- [ ] 检查 `install.sh` / `install.ps1` 预装逻辑语法正确

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| `@link:` 解析在 `config.cpp` | 在 `ezmk.toml` 解析阶段自动展开，所有下游模块无需感知链接语法 |
| `resolve_link_path()` 支持链式解析 | 允许 A→B→C 链式引用（深度限制 10 层），方便组织分层链接；循环检测在解析时进行 |
| 链接值仅支持相对路径 | 不支持绝对路径，保持项目可移植性（`.ezmk/links.json` 可提交到版本控制） |
| `cc` 迁移走标准 utils 查找链 | project → user → global 三作用域查找，与其他 util 行为一致；预装到 global 作用域确保新用户无感知 |
| 预装失败不阻塞安装 | 网络不可用时不强制安装官方 utils，用户可稍后手动 `ezmk pkg install -g ezmk-official-utils` |
| 空路径防御在 `FileWatcher::add_directory()` 入口 | 不依赖上游调用方保证路径非空，防御性编程；空路径 skip + warning |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `ezmk-official-utils` 为新包 | 无旧版本 | 纯增量 |
| `ezmk-cc` 从内置迁移 | 现有 `ezmk utils cc` 用户 | 预装 `ezmk-official-utils`，无感知 |
| `.ezmk/links.json` | 旧项目无此文件 | 文件缺失时 `@link:` 报错提示创建链接 |
| watch 修复 | 仅修复 bug | 行为不变 |
| `link.lua` / `gen-build-package.lua` | 全新工具 | 纯增量 |

---

## 6 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `src/file_watcher.cpp` | 修改 | 空路径防御 + 根因修复 |
| `src/project.cpp`（或 `src/build.cpp`） | 修改 | watch 路径收集逻辑修复 |
| `test/test_file_watcher.cpp` | 修改 | 新增：空路径传入不崩溃 |
| `include/ezmk/config.hpp` | 修改 | `resolve_link_path()` 声明 |
| `src/config.cpp` | 修改 | `resolve_link_path()` 实现 + 循环检测 + TOML 解析集成 |
| `src/util.cpp` | 修改 | `parse_link_syntax()` 解析 `@link:<name>/...` |
| `src/lua_api.cpp` | 修改 | 移除 `cc` 的硬编码 `find_utils_script()` 逻辑 |
| `ezmk-official-utils/ezmk.toml` | **新建** | 包清单 |
| `ezmk-official-utils/utils/link.lua` | **新建** | 链接管理工具 |
| `ezmk-official-utils/utils/cc.lua` | **迁移** | 从内置迁移到 utils 包 |
| `ezmk-official-utils/utils/gen-build-package.lua` | **新建** | 构建包生成工具 |
| `ezmk-official-utils/README.md` | **新建** | 包文档 |
| `install.sh` | 修改 | 末尾预装 `ezmk-official-utils` |
| `install.ps1` | 修改 | 末尾预装 `ezmk-official-utils` |
| `ezmk-repo/index.toml` | 修改 | 注册 `ezmk-official-utils` 包 |

---

## 7 延后项（1.1.0-dev.6+）

- `ezmk-cc` 的编译期嵌入脚本机制废弃清理（`scripts/embed_cc.py` 或在 `scripts/embed_locale.py` 中的 cc 部分）— 确认无其他依赖后可移除
- `@link:` 机制的用户文档更新（`docs/zh/config_file.md` + `docs/en/config_file.md`）
- 跨平台 CI 验证 `@link:` 在 Linux/macOS/Windows 三平台的路径解析一致性
