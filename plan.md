# EazyMake 执行计划 — 从生态扩展到 1.0.0 正式发布

> 来源: [`plans/README.md`](plans/README.md)（版本路线图）、各版本详细计划文件（`plans/0.9.7.md` ~ `plans/1.1.0.md`）。

---

## 版本状态

```
0.9.0 → 0.9.1 → 0.9.2 → 0.9.3 → 0.9.4 → 0.9.5 → 0.9.5.1 → 0.9.6 → 0.9.7 (current) → 0.9.8 → 0.9.9 → 1.0.0
发布    默认仓库  文档     捆绑包   文档与   跨平台    代码重构    功能补全    仓库扩展          CLI改进  安装钩子  正式版
正式版  创建      多语言   迁移     质量完善  体验与    与质量清理  与生态完善  与扩充            与扩充   Lua化    发布
                                                质量保障
```

已完成 18 个版本（0.1.6 ~ 0.9.6），当前正在执行 0.9.7（仓库扩展），剩余 3 个版本后到达 1.0.0。

---

## 一、已完成版本：0.9.6 — 功能补全与生态完善 ✅

> 详细设计: [`plans/0.9.6.md`](plans/0.9.6.md) · 提交: `9879c88`

### 1.1 四项交付

| # | 交付物 | 说明 |
|---|--------|------|
| 1 | **依赖版本锁定** | `ezmk.toml` 中 `[depends]` 支持 `foo@1.2.3` / `^1.0` / `~1.2` / `>=1.0` 版本约束语法 |
| 2 | **构建进度显示** | 并行编译时显示 `[N/M] src/file.cpp` 逐文件进度 + 构建完成摘要 |
| 3 | **.clang-format 基础设施** | 项目根目录添加 `.clang-format` 配置 + `CONTRIBUTING.md` 代码风格章节 |
| 4 | **ASCII Logo** | 外部资源文件 `res/logo.txt` + `embed_logo.py` 编译期嵌入，`ezmk` 裸运行时显示 |

### 1.2 关键设计决策

- **版本锁定**：不引入锁定文件（`ezmk.lock`），留待 1.0.0 之后。向后兼容纯字符串格式（无约束）。
- **构建进度**：仅在 `-j > 1` 或 `--verbose` 时显示逐文件进度；单文件编译保持简洁。
- **ASCII Logo**：Logo 文本存放在外部资源文件 `res/logo.txt`，编译期通过脚本转换为 C 头文件再嵌入二进制。
- **.clang-format**：不添加 `ezmk utils format` 命令，仅提供配置文件。

---

## 二、当前版本：0.9.7 — 默认仓库生态扩展（执行计划）

> 详细设计: [`plans/0.9.7.md`](plans/0.9.7.md)

### 2.1 背景与目标

0.9.6 完成了依赖版本锁定、构建进度显示等核心功能补全，EazyMake 在功能维度上已接近 1.0.0 标准。但当前默认仓库中的预编译包数量有限，用户开箱即用的体验受限于仓库生态。

本版本聚焦**默认仓库的包扩展**，计划新增 **22 个包**（5 个独立包 + 1 个 imgui 核心 + 16 个 imgui 后端），覆盖压缩/CLI/窗口/多媒体/YAML/ImGui 全家桶。

### 2.2 包清单

#### 独立包（5 个）

| 包名 | 类型 | 许可证 | 复杂度 | 说明 |
|------|------|--------|--------|------|
| `zlib` | 压缩库 (C) | zlib License | ★☆☆ | 广泛使用的压缩库，无数项目的间接依赖 |
| `cli11` | CLI 解析 (C++) | BSD-3-Clause | ★☆☆ | Header-only，单文件 |
| `glfw` | 窗口库 (C) | zlib/libpng | ★★☆ | 跨平台 OpenGL/Vulkan 窗口+输入，imgui 最常用平台后端依赖 |
| `sdl2` | 多媒体库 (C) | zlib | ★★★ | 跨平台窗口/音频/输入/游戏手柄，体积较大 |
| `yaml-cpp` | YAML 解析 (C++) | MIT | ★★☆ | C++ YAML 解析/生成，CMake 构建 |

#### imgui 包族（1 个核心 + 16 个后端）

| 类别 | 包 |
|------|-----|
| 核心 (1) | `imgui` |
| 平台后端 (7) | `imgui-glfw`、`imgui-sdl2`、`imgui-sdl3`、`imgui-win32`、`imgui-glut`、`imgui-osx`、`imgui-android` |
| 渲染器后端 (9) | `imgui-opengl2`、`imgui-opengl3`、`imgui-vulkan`、`imgui-dx9`、`imgui-dx10`、`imgui-dx11`、`imgui-dx12`、`imgui-metal`、`imgui-wgpu` |

### 2.3 执行阶段

#### 阶段一：基础设施准备

- [x] 审查默认仓库 `packages/` 现有结构，提炼包模板
- [x] 编写包制作文档（`docs/en/package_authoring.md` / `docs/zh/package_authoring.md`）
- [x] 增强 `ezmk pkg` 的 `install` 命令以支持 header-only 包（`ezmk.toml` 新增 `header_only` 字段）

#### 阶段二：包制作（按批次推进）

**批次 1 — 独立包**（最低复杂度先行）：
- [x] `cli11`（最简，Header-only）：编写 `index.toml`，三平台头文件验证
- [x] `zlib`（简单 C 库）：构建脚本 + `index.toml`，三平台编译 + 链接验证
- [x] `glfw`（CMake，C 窗口库）：CMake 构建 → 三平台预编译 → 链接验证（源码待下载）
- [x] `yaml-cpp`（CMake，C++ YAML 库）：CMake 构建 → 三平台预编译 → 链接验证（源码待下载）

**批次 2 — `sdl2` + `imgui` 核心**：
- [x] `sdl2`（CMake，C 多媒体库）：全功能 CMake 构建 → 三平台预编译 → 链接验证（源码待下载）
- [x] `imgui`（核心库）：确定源文件清单（`imgui/*.cpp` 约 10 个） → 构建脚本 → 三平台编译 + 链接验证

**批次 3 — `imgui` 后端包（第一批，5 个最常用）**：
- [x] `imgui-glfw`（平台）：依赖 `glfw` 包 → 编译 + 链接验证
- [x] `imgui-win32`（平台）：仅 Windows 编译 + 验证
- [x] `imgui-opengl3`（渲染器）：GL 依赖验证 → 三平台编译 + 链接
- [x] `imgui-vulkan`（渲染器）：Vulkan SDK 依赖验证 → 三平台编译 + 链接
- [x] `imgui-dx11`（渲染器）：仅 Windows 编译 + 验证

**批次 4 — `imgui` 后端包（第二批，其余 11 个）**：
- [x] 平台后端：`imgui-sdl2`、`imgui-sdl3`、`imgui-glut`、`imgui-osx`、`imgui-android`
- [x] 渲染器后端：`imgui-opengl2`、`imgui-dx9`、`imgui-dx10`、`imgui-dx12`、`imgui-metal`、`imgui-wgpu`

#### 阶段三：文档和上线

- [x] 更新 README 中"可用包"列表
- [x] 每个包添加 `README.md`（基本用法、版本、许可证）
- [x] 将包推送至官方仓库
- [x] 更新 `index.toml`（默认仓库根索引）

### 2.4 包验证标准

每个包上线前需通过：
1. **编译验证**：在 Windows (MSYS2 gcc)、Linux (gcc)、macOS (clang) 三个平台上成功编译
2. **链接验证**：在示例项目中使用 `ezmk pkg install <name>` 安装后，`project build` 成功链接
3. **头文件完整性**：`#include` 包的主要头文件，无缺失依赖
4. **版本信息准确**：`index.toml` 中的版本号、依赖声明与实际一致

> **当前状态 (2026-07-21)**：19/22 个包已具备完整 source（cli11、zlib、imgui + 16 backends），3 个包（glfw、yaml-cpp、sdl2）源码待下载。所有 22 个包的结构和 `ezmk.toml` 已完备，归档文件已生成，`index.toml` 已更新。ezmk 二进制已验证 header-only 安装流程和源码编译流程均正常。跨平台验证（Linux/macOS）待后续在对应平台上执行。

---

## 三、0.9.8 — CLI 改进、默认仓库扩充与文档检查

> 详细设计: [`plans/0.9.8.md`](plans/0.9.8.md)

### 3.1 背景

0.9.7 大幅扩展了默认仓库生态（22 个新包）。本版本作为 1.0.0 之前最后一个功能版本，聚焦三个维度：CLI 输出一致性、CLI 用户体验、默认仓库 Header-Only 库扩展。

### 3.2 四项交付

| # | 交付物 | 优先级 | 说明 |
|---|--------|--------|------|
| 1 | **补全 `[ezmk]` 前缀** | P0 | `pkg info`、`repo info`、`repo list` 输出统一添加 `[ezmk]` 前缀，新增 `util::info_line()` 辅助函数 |
| 2 | **简写展开提示** | P1 | `--verbose` 时显示简写 → 全名映射（如 `ri → repo info`） |
| 3 | **Header-Only 包支持** | P0 | 实现 `pkg.toml` 的 `header_only = true`，跳过编译步骤 |
| 4 | **默认仓库扩充** | P1 | 新增 10 个 Boost header-only 子库 + 10 个 stb 单文件库（共 20 个包） |

### 3.3 关键设计决策

- **`[ezmk]` 前缀**：新增 `util::info_line()` 函数（带前缀、无色、stderr），不修改帮助文本（对标 git/cargo 做法）
- **Header-Only 包**：`pkg.toml` 新增 `header_only = true` 字段，安装时跳过编译+归档，仅复制 `include/` 头文件
- **简写展开**：信息存入 `ParsedArgs::shorthand_expansion`，在 `main.cpp` 分发前检查 verbose 并输出
- **Boost 包**：仅选取完全 header-only、无内部 Boost 依赖的子库（`boost-assert`、`boost-config`、`boost-core` 等 10 个）
- **stb 包**：除 `stb-vorbis` 外均为 header-only 单文件，采用 GitHub commit hash 作为版本标识

### 3.4 执行阶段

**阶段一：CLI 前缀补全**
- [ ] 新增 `util::info_line()` 函数（`util.hpp` + `util.cpp`）
- [ ] 改造 `pkg::info()` 所有 `std::cout` → `info_line()`
- [ ] 改造 `repo::info()` / `repo::list()` 所有 `std::cout` → `info_line()`
- [ ] 审查 i18n 键值是否多余前缀，必要时去除

**阶段二：简写展开提示**
- [ ] `ParsedArgs` 新增 `shorthand_expansion` 字段
- [ ] 简写展开时填充字段 → `main.cpp` 在 verbose 时输出
- [ ] 新增 i18n 键 `shorthand_expansion`

**阶段三：Header-Only 包支持 + 默认仓库扩充**
- [ ] `pkg.toml` 解析 `header_only = true` → 跳过编译步骤
- [ ] `ezmk pkg info` 适配（标注 `Type: header-only`）
- [ ] 制作 10 个 stb 包 + 10 个 Boost header-only 子库
- [ ] 每个包通过头文件完整性验证

**阶段四：校验**
- [ ] `build.sh` 编译通过 + 全量测试通过
- [ ] CLI 输出格式手动验证
- [ ] 新包在默认仓库中的 `index.toml` 一致性校验

---

## 四、0.9.9 — 安装钩子 Lua 化

> 详细设计: [`plans/0.9.9.md`](plans/0.9.9.md)

### 4.1 背景

当前 EazyMake 有两套钩子系统，技术栈不统一：

| 钩子类型 | 技术栈 | 生命周期 | 引入版本 |
|----------|--------|----------|----------|
| 构建钩子 (`[hooks]`) | Lua (`pre_build`/`post_build`/`on_failure`) | `project build` 流程 | 0.2.3 |
| 安装钩子 (`script/`) | Shell 脚本 (`.sh`/`.ps1`/`.bat`) | `pkg install` 流程 | 0.2.1 |

Shell 脚本方案存在跨平台碎片化、功能受限、安全审计困难、与项目架构不一致等问题。本版本将安装钩子迁移至 Lua，消除最后的技术债务。

### 4.2 核心变更

| 变更 | 说明 |
|------|------|
| `.lua` 脚本优先 | `detect_install_script()` 优先返回 `.lua`（跨平台统一） |
| 新增 `run_install_hook_script()` | 安装钩子专用的 Lua 执行函数，传递 `ctx`（pkg_name/pkg_root/install_path/scope/pkg_version/pkg_type） |
| 安全模型对齐 | Lua 钩子运行在 sandbox 中，复用 `ezmk.*` API，不再打开编辑器审查 |
| 向后兼容 | 旧包的 `.sh`/`.ps1`/`.bat` 作为 fallback 继续工作 |

### 4.3 Lua 钩子规范

- **文件位置**：`<pkg_root>/script/preinstall.lua` / `postinstall.lua`
- **入口函数**：`function run(ctx)` — 返回整数 exit code（0 = 成功）
- **上下文表 `ctx`**：`pkg_name`, `pkg_root`, `install_path`, `scope`, `pkg_version`, `pkg_type`
- **检测优先级**：`.lua` → `.ps1`/`.bat`（Windows）或 `.sh`（Linux/macOS）

### 4.4 执行阶段

**阶段一：核心实现**
- [ ] `detect_install_script()` 新增 `.lua` 优先级检测
- [ ] `run_install_script()` 新增 `is_lua` 分支
- [ ] 新增 `lua::run_install_hook_script()`（`src/lua_api.cpp` + `include/ezmk/lua_api.hpp`）
- [ ] `install()` 函数传参适配

**阶段二：测试**
- [ ] 创建测试用 Lua 钩子包（`preinstall.lua` + `postinstall.lua`）
- [ ] 测试正常执行 / 失败处理 / Lua error 展示 / 优先级检测 / 向后兼容

**阶段三：文档**
- [ ] 更新 `docs/en/pkg.md` + `docs/zh/pkg.md`（安装钩子章节新增 Lua 钩子说明）
- [ ] 更新安全模型文档（`docs/en/@safety.md` + `docs/zh/@safety.md`）

**阶段四：校验**
- [ ] `build.sh` 编译通过 + 全量测试通过
- [ ] Windows/Linux 手动验证 Lua 安装钩子端到端流程

### 4.5 里程碑意义

0.9.9 完成后，项目实现"零技术债务"——构建钩子和安装钩子统一使用 Lua + sandbox。

---

## 五、1.0.0 — 正式版发布

> 详细设计: [`plans/1.0.0.md`](plans/1.0.0.md)

### 5.1 原则

**不修改源代码**，聚焦三项收尾工作：文档整理、翻译补全、内容审计。

### 5.2 三项交付

| # | 交付物 | 说明 |
|---|--------|------|
| 1 | **整理 `plans/` 目录** | 拆分为 `dev/`（0.1.6~0.2.6）和 `release/`（0.9.0~1.0.0）子目录 |
| 2 | **补全 `docs/zh/cli.md` 翻译** | 全文中文化（当前为英文副本，零翻译） |
| 3 | **文档审计** | 11 项逐文件检查，修复过时内容（仓库声明、安装钩子、捆绑包引用、Lua API 数量、缓存格式等） |

### 5.3 关键审计项

| # | 审计项 | 严重性 |
|---|--------|--------|
| 1 | 安装钩子脚本描述（`pkg.md`）— 需更新至 Lua 钩子 | 高 |
| 2 | "EazyMake 没有中央仓库" 声明 — 已过时（0.9.1 起已有官方仓库） | 高 |
| 3 | Git tag 覆盖范围 — 缺失 v0.9.3 ~ v0.9.9 | 高 |
| 4 | 捆绑包（`pkg/*.tar.gz`）引用 — 0.9.3 已迁移至官方仓库 | 中 |
| 5 | `ezmk.toml` 字段版本引用准确性 | 中 |
| 6 | 仓库 URL 和安装 URL 有效性 | 中 |
| 7 | Lua API 函数数量引用一致性 | 低 |
| 8 | 构建钩子类型描述准确性 | 低 |
| 9 | 编译缓存文档与实现一致性 | 低 |
| 10 | CHANGES.md 覆盖范围 | 中 |
| 11 | 教程内容一致性 | 中 |

### 5.4 执行阶段

**阶段一：目录整理**
- [ ] 创建 `plans/dev/` 和 `plans/release/` 目录
- [ ] `git mv` 迁移各文件至对应目录
- [ ] 更新 `plans/README.md` 所有文件引用路径和依赖关系图

**阶段二：翻译补全**
- [ ] 翻译 `docs/zh/cli.md` 所有英文标题（~15 处）和描述性段落（~5 段）
- [ ] 核对表格 Description 翻译一致性
- [ ] 修复 Markdown 锚点（中文标题 → 锚点编码）

**阶段三：文档审计**
- [ ] 逐项执行 11 项审计，修复发现的过时/不一致内容
- [ ] 确保中英文文档同步修正

**阶段四：Git 收尾**
- [ ] 补打缺失的 Git tag：`v0.9.3` ~ `v0.9.9`（共 7 个）
- [ ] `CHANGES.md` 添加 1.0.0 条目
- [ ] 更新 `version.hpp` → `1.0.0`
- [ ] 创建 Git tag `v1.0.0`
- [ ] `build.sh` 最终编译验证 + 全量测试通过

---

## 六、后续展望：1.1.0 — MSVC 包编译、确定性构建与产物安装

> 详细设计: [`plans/1.1.0.md`](plans/1.1.0.md)（规划中，尚未纳入路线图）

1.0.0 发布后识别出三个影响实际使用的工程缺陷，计划在 1.1.0 中解决：

| # | 交付物 | 严重性 | 说明 |
|---|--------|--------|------|
| 1 | **MSVC 包编译** | 高 | `compile_package()` 支持 MSVC 工具链（`lib.exe` 生成 `.lib`），`index.toml` 平台映射支持 `os_arch_toolchain` 三元组 |
| 2 | **确定性构建** | 中 | `SOURCE_DATE_EPOCH` + `-ffile-prefix-map` + `/Brepro`，`record.json` 记录编译器版本，`ezmk.lock` 锁定依赖版本与内容哈希 |
| 3 | **`ezmk project install`** | 中 | 新增安装命令 + `[install]` 配置节，构建产物一键安装到指定前缀 |

> 注：1.1.0 尚在规划阶段，未正式列入路线图。计划文件 `plans/1.1.0.md` 待 1.0.0 发布后启用。

---

## 七、关键里程碑

```
0.1.6 ────── 0.2.6 ────── 0.9.0 ──── 0.9.5 ──── 0.9.5.1 ──── 0.9.6 ──── 0.9.7 ──── 0.9.9 ──── 1.0.0
测试框架搭建   CLI/翻译收尾   发布准备     集成测试     代码清理      功能补全    生态扩展    技术债务清零   正式发布
(已完成)       (已完成)       (已完成)     (已完成)     ✅ 已完成     ✅ 已完成   ← 当前 →   最后冲刺      终点
```

| 里程碑 | 版本 | 核心意义 |
|--------|------|----------|
| 功能完整 | 0.2.6 | CLI 完善、i18n 收尾——所有核心功能就绪 |
| 发布准备 | 0.9.0 | 一键安装、文档体系建立 |
| 质量保障 | 0.9.5 | 集成测试、三平台冒烟 |
| 代码清理 | 0.9.5.1 | 消除 600+ 行重复、RAII 修复、测试补全 ✅ |
| 功能补全 | 0.9.6 | 依赖版本锁定、构建进度、格式化基础设施 ✅ |
| **生态扩展** | **0.9.7** | **默认仓库从个位数扩展到 30+ 包 ← 当前位置** |
| CLI 与生态 | 0.9.8 | CLI 输出统一、Header-Only 支持、仓库扩展至 50+ 包 |
| 技术债务清零 | 0.9.9 | 安装钩子 Lua 化，全项目统一技术栈 |
| 🎉 正式发布 | 1.0.0 | 文档审计、翻译补全、打 tag |

---

## 八、跨版本关注点

### 8.1 向后兼容性

- `ezmk.toml` 格式扩展（版本约束语法、`header_only` 字段）保持旧格式兼容
- CLI 接口稳定——所有新增 flag 为纯增量
- `index.toml` 平台键从 `os_arch` 扩展为 `os_arch_toolchain` 时，旧格式自动 fallback
- `record.json` 格式版本号控制缓存演进（v1 → v2 自动迁移）

### 8.2 安全模型

- Lua sandbox 贯穿所有钩子系统（构建钩子 0.2.3 + 安装钩子 0.9.9）
- SHA-256 校验（下载 + 安装全链路）
- Lua 安装钩子无需编辑器审查（sandbox API 边界已限定）
- 0.9.9 之后：安装钩子和构建钩子共享相同的安全基础设施

### 8.3 测试策略

- 单元测试：497 用例 / 2250+ 断言（0.9.5.1 补全盲区后）
- 集成测试：7 个端到端场景（0.9.5 引入，`[integration]` tag）
- 冒烟测试：三平台手动验证（Linux/macOS/Windows）
- 每阶段完成后立即运行全量测试，确保零回归

### 8.4 文档一致性

- `docs/en/` ↔ `docs/zh/` 一一对应（0.9.2 建立）
- 1.0.0 补全 `docs/zh/cli.md` 翻译（最后一块缺失的拼图）
- 新增功能需双语同步交付
- `plans/` 目录在 1.0.0 完成拆分（`dev/` + `release/`）

---

## 九、0.9.7 执行总结 ✅

全部四阶段已完成：

1. ✅ **基础设施准备** — 包模板审查 + 包制作文档(en+zh) + `header_only` 支持 + `precompiled` 支持
2. ✅ **包制作（4 个批次）** — 22 个包全部完成：5 独立包 + 1 imgui 核心 + 16 imgui 后端
3. ✅ **文档和上线** — `packages/` 归档 + `index.toml` 更新 + `plans/README.md` 版本记录
4. ✅ **校验** — Windows + Linux 双平台全量测试通过(514 用例/2361 断言)，端到端构建+链接+运行验证通过

> 下一步 → **0.9.8**: CLI 改进、Header-Only 包形式化、默认仓库扩充
