# EazyMake 1.1.0-dev.3 执行计划

> 详细设计：[`plans/release/1.1.0-dev.3.md`](plans/release/1.1.0-dev.3.md)

---

## 1 背景

AI 编程助手（Claude Code、GitHub Copilot、Cursor 等）最有效的集成方式是项目提供符合 **Agent Skills 开放标准** 的 skill 文件。EazyMake 目前仅有 `CLAUDE.md`（~160 行），混合了构建命令、架构说明、i18n 机制、包管理、配置系统等所有信息，存在以下问题：

1. **无法按需加载**：agent 修改测试文件时不需要知道包管理细节，但 `CLAUDE.md` 全量注入
2. **不符合 Agent Skills 开放标准**：`CLAUDE.md` 仅对 Claude Code 有效，Copilot、Cursor 各有自己的格式
3. **难以维护**：随项目增长越来越长，新开发者难以快速定位所需信息
4. **无法触发式加载**：理想情况下 agent 在进入特定目录或操作特定文件时自动加载对应 skill

**解决方案**：将 `CLAUDE.md` 拆分为多个符合 Agent Skills 开放标准的 skill 文件，放在 `.claude/skills/` 目录下，每个 skill 聚焦一个领域，agent 按需加载。

---

## 2 目标

| #   | 目标                        | 优先级 | 说明                                                                                   |
| --- | --------------------------- | ------ | -------------------------------------------------------------------------------------- |
| 1   | **EazyMake Build Skill**    | P0     | 教 agent 如何编译 EazyMake：`bash build.sh`、手动 g++ 命令、MSYS2 vs Linux 差异        |
| 2   | **EazyMake Test Skill**     | P0     | 教 agent 如何运行测试：Catch2 框架、测试编译命令、测试文件组织                         |
| 3   | **EazyMake Codebase Skill** | P0     | 教 agent 理解项目架构：源码布局、关键模块职责、CLI/配置/缓存/Lua/包管理等子系统        |
| 4   | **EazyMake i18n Skill**     | P1     | 教 agent 如何添加/修改翻译：X-macro `i18n_keys.def` 机制、`en.json`/`zh.json` 双语维护 |
| 5   | **EazyMake Planning Skill** | P1     | 教 agent 如何参与版本规划：`plans/` 目录结构、plan 文档格式、`plan.md` 执行计划        |
| 6   | **EazyMake Repo Skill**     | P2     | 教 agent 如何操作官方仓库：包制作流程、`index.toml` 格式、SHA-256 校验                 |
| 7   | **CLAUDE.md 精简**          | P0     | 将当前 `CLAUDE.md` 精简为入口文件（~30 行），核心内容迁移到各 skill 中                 |

此外还包括面向 **EazyMake 用户** 的 Agent Skills（用户侧 skills，§7），覆盖用户项目中的编译、测试、配置和包管理工作流。

---

## 3 执行阶段

### 阶段一：Build Skill + Test Skill（P0，成本最低、最高频）

- [x] 编写 `.claude/skills/ezmk-build.md`：
  - 编译命令（`bash build.sh` + 手动 g++ MSYS2 + 手动 g++ Linux）
  - 构建产物说明（`build/ezmk`、生成的头文件）
  - 关键 flag 解释（`-DLUA_COMPAT_5_3`、`-lwinhttp`、`-static`）
  - 平台差异
  - 常见编译问题
- [x] 编写 `.claude/skills/ezmk-test.md`：
  - 测试命令（`bash build.sh test` + 手动 g++ 命令）
  - Catch2 v3 框架简介（header-only：`include/vendor/catch2.hpp` + `src/vendor/catch2_impl.cpp`）
  - 测试文件组织（`test/test_<module>.cpp`）
  - 当前基线数据（538 用例 / ~2440 断言）
  - 新增测试指南（文件名约定、`TEST_CASE` 宏、fixtures、`[integration]` tag）

### 阶段二：Codebase Skill（P0，agent 理解项目必备）

- [x] 编写 `.claude/skills/ezmk-codebase.md`：
  - 完整目录树（`src/`、`include/ezmk/`、`test/`、`docs/`、`plans/`、`locale/`、`scripts/`）
  - 模块职责（每个模块 2-3 句话：`main.cpp`、`cli.cpp`、`build.cpp`、`cache.cpp`、`config.cpp`、`pkg.cpp`、`repo.cpp`、`toolchain.cpp`、`project.cpp`、`i18n.cpp`、`lua_api.cpp`、`file_watcher.cpp`、`crypto.cpp`、`util.cpp`、`version.cpp`）
  - 数据流：CLI → config → build → cache → toolchain
  - 关键设计模式：X-macro（i18n）、RAII、atomic write（temp → rename）
  - 不修改的文件（`src/vendor/**` 是第三方代码）
  - CLI flags not in README、配置系统、包管理、仓库管理等实现细节（从旧 `CLAUDE.md` 迁移）

### 阶段三：i18n Skill + Planning Skill（P1）

- [x] 编写 `.claude/skills/ezmk-i18n.md`：
  - X-macro 机制：`include/ezmk/i18n_keys.def` → `I18nKey` 枚举 + `key_name()` 映射
  - 添加一个 key 的完整步骤：`.def` → `locale/en.json` → `locale/zh.json` → 重建（`build.sh`）
  - Debug 构建的 `audit_missing_keys()` 检查
- [x] 编写 `.claude/skills/ezmk-planning.md`：
  - `plans/` 目录结构：`dev/`（早期开发版本）、`release/`（发布版本及 dev 子版本）、`README.md`（索引 + 路线图）、`plan.md`（当前执行计划）
  - Plan 文档格式约定（版本号标题 → 背景 → 目标 → 详细设计 → 执行步骤 → 兼容性矩阵 → 跨版本关注点）
  - 如何新增/更新版本计划

### 阶段四：精简 `CLAUDE.md`

- [x] 将当前 `CLAUDE.md` 内容逐节迁移到对应 skill：
  - Build & test commands → `ezmk-build` + `ezmk-test`
  - Architecture → `ezmk-codebase`
  - Internationalization → `ezmk-i18n`
  - CLI flags not in README → `ezmk-codebase`
  - Configuration / Package management / Repository management / etc. → `ezmk-codebase`
  - Workflow rules → `ezmk-codebase`
  - Safety requirements → `ezmk-codebase`
- [x] 重写 `CLAUDE.md` 为精简入口（~30 行）：概述 + skill 索引表 + quick reference
- [x] 确保所有来自旧 `CLAUDE.md` 的信息都能在新 skill 中找到（逐节对照检查）

### 阶段五：可选交付

- [x] 编写 `.claude/skills/ezmk-repo.md`（P2）：
  - 官方仓库结构（`index.toml` + `packages/`）
  - 包制作流程（编译 → 打包 → SHA-256 → 更新索引）
  - `index.toml` 的 `[[packages]]` 条目格式
  - 预编译包 vs 源码包的区别
- [x] 编写 `.github/copilot-instructions.md`（桥接文件）：
  - 精简版指令，不重复 skill 内容，仅指向 `CLAUDE.md` + skill 目录
  - 包含 build/test 命令 + 关键目录
- [x] 用户侧 Agent Skills（§7）：面向 EazyMake 使用者的 skill 文件，覆盖：
  - **EazyMake 项目编译**（P0）：教 agent 如何用 `ezmk project build` 编译用户项目
  - **EazyMake 项目测试**（P0）：教 agent 如何用 `ezmk project test` 运行测试
  - **EazyMake 项目配置**（P1）：教 agent 理解 `ezmk.toml` 结构和配置项含义
  - **EazyMake 包管理**（P2）：教 agent 如何安装/更新第三方包
- [ ] 可选：为 `.cursor/rules/` 生成对应的 rule 文件（延后至 dev.4+）

### 阶段六：校验

- [x] 检查所有 skill 文件结构一致（frontmatter → 标题 → 内容 → 示例）
- [x] 逐 skill 验证触发条件（`trigger.glob`）是否正确覆盖了对应文件
- [x] `bash build.sh` 编译通过（`build/ezmk.exe` 生成成功；skill 文件为纯文档，不影响代码）
- [ ] 手动验证：Claude Code 打开项目 → 修改 `src/build.cpp` → 确认 Build Skill 自动加载
- [ ] 手动验证：Claude Code 打开项目 → 修改 `locale/zh.json` → 确认 i18n Skill 自动加载
- [x] 确认旧 `CLAUDE.md` 中的信息在新 skill 中无遗漏（逐节对照检查）
- [x] 更新 `CLAUDE.md`（本次自身也是交付物，已精简为 skill 索引条目）

---

## 4 关键设计决策

| 决策                       | 说明                                                                                                                                                        |
| -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Skill 文件格式             | Claude Code 原生 `.claude/skills/*.md` 格式（Markdown + YAML frontmatter + `trigger.glob`），是 Agent Skills 开放标准的实现                                |
| `CLAUDE.md` 角色转变       | 从"大而全的单文件"变为"入口索引"：agent 先加载 `CLAUDE.md`，再根据任务按需加载对应 skill                                                                    |
| P0/P1/P2 优先级分级        | P0（Build/Test/Codebase）最高频、基础依赖，优先完成；P1（i18n/Planning）中等频率；P2（Repo）最低频，可选                                                    |
| 不修改源代码               | 所有 skill 文件是纯文档，零代码风险；`bash build.sh` 编译 + 全量测试不受影响                                                                               |
| 用户侧 skills 独立         | §7 的用户侧 skill 面向 EazyMake 使用者（用户项目），与 §2~§6 的 dev 侧 skill（EazyMake 自身）格式相同、受众不同                                             |
| 与其他 agent 的兼容性      | skill 文件放在 `.claude/skills/`（Claude Code 原生），但 Markdown 正文是通用格式；可选生成 `.github/copilot-instructions.md` 和 `.cursor/rules/` 桥接文件 |
| Repo skill 作为 P2 延后    | 仓库操作频率最低，可在各阶段完成后单独处理，不阻塞主要交付                                                                                                  |

---

## 5 兼容性矩阵

| 变更                        | 影响                           | 处理                                                                               |
| --------------------------- | ------------------------------ | ---------------------------------------------------------------------------------- |
| `CLAUDE.md` 大幅精简        | agent 首次加载时获得的信息变少 | 通过 skill 按需加载补充；入口 `CLAUDE.md` 的索引表确保 agent 知道有哪些 skill 可用 |
| 新增 `.claude/skills/` 目录 | 仅 Claude Code 原生支持        | 不影响其他工具；其他 agent 通过各自的指令文件（`.cursor/rules/` 等）引用相同内容   |
| 不修改任何源代码            | 零风险                         | skill 文件是纯文档                                                                 |

---

## 6 延后项（1.1.0-dev.4+）

- 用户侧 skills 的分发实现（`ezmk project new` 模板生成 / `ezmk utils skill-gen` 工具）— 设计文档已在 §7.3 中定义，具体实现延后
- `.cursor/rules/` 同步生成（可作为独立小任务在任何阶段完成）
- Skill 内容随项目架构演进的持续维护（与 1.1.0-dev.4 编译器/语言配置增强相关）
