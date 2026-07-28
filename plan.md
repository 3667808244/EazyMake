# EazyMake 1.1.0-dev.2 执行计划

> 详细设计：[`plans/release/1.1.0-dev.2.md`](plans/release/1.1.0-dev.2.md)

---

## 1 背景

1.1.0-dev.1 完成了 MSVC 包编译、确定性构建和产物安装。0.9.7 实现的 `precompiled` 字段支持了单平台预编译包（`lib/` 下放单个 `.a`），但缺少：

- **多平台共包分发**：同一个 tar.gz 里放 Win/Lin/mac 三平台的 `.a`，安装时自动选择
- **`index.toml` 平台映射**：`[[packages]]` 条目级平台过滤，仓库搜索时自动匹配当前平台
- **打包命令**：尚无 `ezmk project pack` 子命令，打包流程仍为手动

---

## 2 目标

1. **多平台共包支持**（第二层）— `lib/` 下按 `lib<name>.<os>-<arch>.a` 命名约定放多平台产物，安装时自动匹配当前平台
2. **`index.toml` 平台映射**（第三层）— `[[packages]]` 新增 `platform` 字段（`os-arch` 二元组），`read_pkg_from_index()` 搜索时按平台过滤
3. **`ezmk project pack`** — 新增打包子命令，将 `static` 项目一键打包为标准 EazyMake 包格式

---

## 3 执行阶段

### 阶段一：`util::detect_platform_tag()` — 平台检测基础

> 为多平台共包和 index.toml 平台映射提供统一的平台标签生成能力。

- [x] **`include/ezmk/util.hpp`**：声明 `std::string detect_platform_tag()` — 返回简化版平台标签（如 `win-x64`、`linux-x64`、`mac-arm64`）
- [x] **`src/util.cpp`**：实现 `detect_platform_tag()`：
  - OS 检测：`EZMK_WIN` → `"win"`，`EZMK_MACOS` → `"mac"`，`EZMK_LINUX` → `"linux"`
  - Arch 检测：`__x86_64__` / `__amd64__` / `_M_X64` / `_M_AMD64` → `"x64"`，`__aarch64__` / `__arm64__` / `_M_ARM64` → `"arm64"`，`__i386__` / `__i686__` / `_M_IX86` → `"x86"`
  - 格式：`"<os>-<arch>"`（与 `repo.cpp` 的 `build_platform_key()` 格式不同 — 该函数使用 `_` 分隔符 + 三元组含 toolchain，供 `[platform]` section 使用；本函数用于文件名匹配和 `[[packages]].platform` 字段）
- [x] **测试**：`test/test_util.cpp` 新增 `detect_platform_tag` 测试用例（验证返回非空、格式正确 `os-arch`、在当前平台能匹配自身）

### 阶段二：多平台共包支持（第二层）

> 修改 `compile_package()` 和 `validate_pkg()` 中的预编译包选择逻辑，按平台标签匹配 `lib/` 下的 `.a`/`.lib`。

- [x] **`src/pkg.cpp` `compile_package()`**（~L272-283）：当前 precompiled 路径选取第一个 `.a`/`.lib` → 改为：
  1. 调用 `util::detect_platform_tag()` 获取当前平台标签（如 `win-x64`）
  2. 遍历 `lib/` 文件，匹配命名约定 `lib<name>.<tag>.a` / `lib<name>.<tag>.lib`
  3. 精确匹配当前平台 → 直接返回；无精确匹配 → fallback 到无后缀 `lib<name>.a` / `lib<name>.lib`（向后兼容第一层）
  4. 仍无匹配 → 报错并列出 `lib/` 下所有可用平台文件
- [x] **`src/pkg.cpp` `validate_pkg()`**（~L240-253）：precompiled 包校验放宽 — `lib/` 下只需至少一个 `lib<name>.<tag>.a` 或 `lib<name>.<tag>.lib` 或无后缀 `lib<name>.a`（不再简单遍历 `.a`/`.lib` 后缀）
- [x] **`src/build.cpp`**（~L631）：`collect_sources()` 中收集 precompiled 归档的逻辑同步更新 — 按平台标签选择正确的 `.a`/`.lib` 参与链接
- [x] **i18n**：`i18n_keys.def` 新增 key（`pkg_precompiled_no_platform`、`pkg_precompiled_available_platforms`），`en.json` / `zh.json` 添加翻译

### 阶段三：`index.toml` 平台映射（第三层）

> 在 `[[packages]]` 条目中新增可选 `platform` 字段，`read_pkg_from_index()` 搜索时自动过滤。

- [x] **`src/repo.cpp` `read_pkg_from_index()`**（~L295-353）：
  1. 解析 `[[packages]]` 条目时读取可选的 `platform` 字段（`std::optional<std::string>`）
  2. 获取当前平台标签（`util::detect_platform_tag()`）
  3. 过滤逻辑：`platform` 为空（旧格式，向后兼容）或 `platform == detect_platform_tag()` → 候选；否则跳过
  4. 在过滤后的候选中选最高版本
  5. 无匹配 → 返回空 `PkgSearchResult`（调用方已有 "package not found" 报错逻辑，需调整错误消息注明平台信息）
- [x] **`include/ezmk/repo.hpp`**：`PkgSearchResult` 可选扩展 `platform` 字段（用于调试/日志输出）
- [x] **向后兼容验证**：`platform` 字段缺失的旧条目继续参与搜索（视为全平台可用）
- [x] **i18n**：`i18n_keys.def` 新增 key（`repo_no_platform_match`），`en.json` / `zh.json` 添加翻译
- [x] **文档**：更新 `docs/en/repo.md` + `docs/zh/repo.md` — `index.toml` 格式新增 `platform` 字段说明

### 阶段四：`ezmk project pack`

> 新增打包子命令，将 `static` 项目打包为可分发的 `.tar.gz`。

- [x] **`include/ezmk/cli.hpp`**：
  - `Command` 枚举新增 `ProjectPack`
  - 新增 `ProjectPackOptions` 结构体（`output_dir`、`verbose`）
  - `CliArgs` 新增 `project_pack_opts` 字段
- [x] **`src/cli.cpp` `parse_project_args()`**：
  - 新增 `action == "pack"` 分支
  - 解析 `--output <dir>` flag（默认 `"."` 当前目录）
  - 解析 `-v` / `--verbose` flag
  - 更新简写映射：`"pp"` → `{"project", "pack"}`
- [x] **新增 `util::create_targz()`**（`src/util.cpp` + `include/ezmk/util.hpp`）：
  - 当前代码库仅有 `extract_archive()`（解压），无打包函数 — 本项目使用 miniz 进行 gzip 压缩，需新建
  - 签名：`void create_targz(const fs::path& source_dir, const fs::path& output_file)`
  - 实现：遍历 `source_dir` → 构造 tar header（512-byte blocks）→ gzip 压缩（miniz `tdefl_*` API）→ 写入 `output_file`
  - 对齐现有 `extract_targz()` 所使用的 miniz 库（避免引入新依赖）
- [x] **新建 `src/pack.cpp` + `include/ezmk/pack.hpp`**（或在 `build.cpp` 中新增 `pack_project()`）：
  - **流程**：
    1. 读取并解析 `ezmk.toml`
    2. 校验 `type == "static"`（非 static → 报错退出）
    3. 若 `build/lib<name>.a`（或 `.lib`）不存在 → 先调用 `build_project()` 编译
    4. 创建临时目录 `<name>-<version>/`
    5. 收集文件：`include/` → `<tmp>/include/`（递归复制）+ `build/lib<name>.a` → `<tmp>/lib/lib<name>.a` + `ezmk.toml` → `<tmp>/ezmk.toml`
    6. 调用 `util::create_targz()` 打包为 `<name>-<version>.tar.gz`
    7. 计算并打印 SHA-256
    8. 可选：在 `-v` 模式下打印 `index.toml` 条目模板
  - **输出目录**：`--output <dir>` 指定，默认当前工作目录
  - **清理**：打包成功后删除临时目录
- [x] **`src/main.cpp`**：分发 `Command::ProjectPack` → 调用 `pack_project()`
- [x] **i18n**：`i18n_keys.def` 新增 key（`pack_not_static`、`pack_collecting`、`pack_creating`、`pack_sha256`、`pack_success` 等），`en.json` / `zh.json` 添加翻译

### 阶段五：集成测试与校验

- [x] 创建 `detect_platform_tag()` 单元测试（跨平台标签格式验证）
- [x] 创建多平台共包测试：模拟 `lib/` 下有多个平台文件，验证正确选择当前平台
- [x] 创建多平台共包 fallback 测试：仅有 `lib<name>.a` 时向后兼容
- [x] 创建 `index.toml` `platform` 字段过滤测试
- [x] 创建 `ezmk project pack` 端到端测试（static 项目 → pack → 解压验证内容 → 校验 SHA-256）
- [x] `build.sh` 编译通过 + 全量测试通过（`./build/test_ezmk`）
- [x] 检查编译无新增警告
- [x] 更新 `CHANGES.md`

---

## 4 关键设计决策

| 决策                       | 说明                                                                                                                                                                                                  |
| -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 平台标签格式               | `util::detect_platform_tag()` 使用 `os-arch` 简化格式（`win-x64`），不含 toolchain；`repo.cpp` 的 `build_platform_key()` 使用 `os_arch_toolchain` 三元组（`windows_x86_64_msvc`），两者服务于不同层面 |
| 文件名约定 vs 索引字段     | `lib/` 层面按文件名匹配平台（`lib<name>.<tag>.a`），`index.toml` 的 `platform` 字段用于仓库搜索过滤 — 两层各司其职                                                                                    |
| 向后兼容                   | `platform` 字段可选；`lib/` 下单 `.a` 文件继续有效（第一层行为不变）                                                                                                                                  |
| 归档格式                   | `ezmk project pack` 输出 `.tar.gz`（与现有包分发格式一致）                                                                                                                                            |
| `project pack` 限制 static | 仅 `static` 库项目可打包（`executable` / `shared` / `utils` 不适用此流程）                                                                                                                            |

---

## 5 兼容性矩阵

| 变更                                             | 影响                             | 处理                                                    |
| ------------------------------------------------ | -------------------------------- | ------------------------------------------------------- |
| `compile_package()` precompiled 选择逻辑变更     | 仅影响 `precompiled = true` 的包 | 精确平台匹配优先，fallback 到无后缀文件（旧包行为不变） |
| `index.toml` `[[packages]]` 新增 `platform` 字段 | 新字段可选                       | 缺失时视为全平台可用（旧索引文件无需修改）              |
| 新增 `ezmk project pack`                         | 纯增量                           | 不影响现有命令                                          |
| `Command` 枚举新增 `ProjectPack`                 | 编译期 enum class                | 不影响序列化/持久化                                     |

---

## 6 延后项（1.1.0-dev.3+）

- `ezmk project pack` 的多平台交叉打包（当前仅打包本机平台）