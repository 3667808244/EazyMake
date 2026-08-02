# EazyMake 1.1.0-pre.2 执行计划

> 详细设计：[`plans/release/1.1.0-pre.2.md`](plans/release/1.1.0-pre.2.md)
>
> **状态：已完成。** 全量测试 545 用例 / 2613 断言（单元）· 555 用例 / 2661 断言（含集成），零回归。
>
> 本次执行完成 1.1.0-pre.1 与 1.1.0-dev.7 之后的全量文档审计：CLI 文档顶层别名化、`config_file.md` 补全 `[install]`/`[test]`、Tutorial 别名化、pre.1 遗留项补齐（`docs/zh/technical.md` + `res/ezmk.zsh`）、`version.hpp` fallback 修正、CHANGES.md 补全。

---

## 1 背景

`1.1.0-pre.1`（顶层命令别名 / `--help` 重组 / README 精简 / API 冻结承诺）与 `1.1.0-dev.7`（包处理改善）之后，需要一次全面的文档审计，确保所有文档与代码一致：

1. **顶层别名未反映到文档**：`docs/en/cli.md`、`docs/zh/cli.md`、所有 tutorial 文件仍使用 `ezmk project build` 旧写法；`README.md` / `README_ZH.md` 遗漏 `ezmk pack` 顶层别名
2. **`config_file.md`（en + zh）遗漏配置节**：`[install]`（1.1.0 新增）和 `[test]`（1.1.0-dev.6 新增）在 API 稳定性承诺中列为稳定 API，但配置参考文档完全未记录
3. **pre.1 遗留项未完成**：`docs/zh/technical.md` 中文版缺失、`res/ezmk.zsh` 补全脚本不存在（`install.sh` 已引用 `EZMK_NO_COMPLETIONS`）
4. **`plans/README.md` 未更新**：未反映 pre.1 / pre.2 进展
5. **`include/ezmk/version.hpp` 版本号**：fallback 值仍为 `1.0.0`
6. **`CHANGES.md` 需补全**：缺少 `1.1.0-dev.7` 与 `1.1.0-pre.1` 条目
7. **i18n 完整性**：`i18n_keys.def` ↔ `en.json` ↔ `zh.json` 三方一致性
8. **`docs/zh/` 与 `docs/en/` 一致性**：逐文件同步检查

---

## 2 目标

| # | 目标 | 优先级 | 状态 |
|---|------|--------|------|
| 1 | CLI 文档更新（en + zh）— 顶层别名章节 + 命令表格别名优先 | P0 | ✅ 已实现 |
| 2 | `config_file.md` 补全（en + zh）— `[install]` + `[test]` 配置节 | P0 | ✅ 已实现 |
| 3 | Tutorial 更新（en + zh）— 所有代码示例改为顶层别名优先 | P0 | ✅ 已实现 |
| 4 | `plans/README.md` 更新 — 反映 dev.7 / pre.1 / pre.2 真实进展 | P0 | ✅ 已确认（本次执行前已就绪） |
| 5 | `docs/zh/technical.md` 中文版（pre.1 遗留） | P1 | ✅ 已实现 |
| 6 | `res/ezmk.zsh` zsh 补全创建（pre.1 遗留） | P1 | ✅ 已实现 |
| 7 | i18n 完整性校验 — def ↔ en.json ↔ zh.json 三方一致 | P0 | ✅ 已确认（273 key） |
| 8 | `CHANGES.md` 补全 — 补充 dev.7 + pre.1 条目 | P0 | ✅ 已实现 |
| 9 | `version.hpp` 更新 — fallback 版本号 `1.0.0` → `1.1.0` | P0 | ✅ 已实现 |
| 10 | `docs/zh/` 与 `docs/en/` 同步检查 — 逐文件对比 | P1 | ✅ 已确认（结构一致） |
| 11 | `docs/en/technical.md` 修正 — zsh 补全路径 + 测试数据更新 | P1 | ✅ 已实现 |
| 12 | 全量测试回归 — 编译通过，零回归 | P0 | ✅ 545/2613（单元）· 555/2661（含集成） |

---

## 3 执行阶段

### 阶段一：CLI 文档更新（P0）✅

**文件**：`docs/en/cli.md` + `docs/zh/cli.md` + `README.md` / `README_ZH.md`

- [x] 在 `## project` 之前新增 "Top-level aliases" / "顶层别名" 章节，列出 7 个别名 ↔ 完整形式对照表，说明两种形式等价
- [x] `## project` 命令表格改为**别名优先**：`ezmk build [build-opts]` 为主行，原 `ezmk project build` 作为 "full form" 放入描述
- [x] `README.md` / `README_ZH.md` 命令速览补充 `ezmk pack` 顶层别名
- [x] 中文版 `docs/zh/cli.md` 同步翻译
- [x] `docs/en/technical.md` Shell Completion 章节路径修正：`completions/_ezmk` → `res/ezmk.zsh`，更新安装说明

### 阶段二：`config_file.md` 补全（P0）✅

**文件**：`docs/en/config_file.md` + `docs/zh/config_file.md`

- [x] 新增 `[install]` 配置节文档（`prefix` / `bindir` / `libdir` / `includedir` / `sharedir`，支持 `~` 展开）
- [x] 新增 `[test]` 配置节文档（`dirs` / `framework` / `flags`）
- [x] 插入位置：`[hooks]` 节之后、`[utils]` 节之前，格式与现有配置节一致
- [x] 中英文同步

### 阶段三：Tutorial 更新（P0）✅

**文件**：`tutorial/en/*.md` + `tutorial/zh/*.md` + `docs/en/faq.md` / `docs/zh/faq.md`

- [x] `tutorial/en/02-first-project.md` — 别名优先 + 首次出现处标注完整形式 + Shorthands 章节提及顶层别名
- [x] `tutorial/en/04-cache.md` — `ezmk project build` → `ezmk build`、`ezmk project clean` → `ezmk clean`
- [x] `tutorial/en/05-profiles-parallel.md` — 所有 `ezmk project build` / `ezmk project run` 替换
- [x] `tutorial/en/06-packages.md` — `ezmk project build` → `ezmk build`
- [x] `tutorial/en/07-watch-hooks.md` — `ezmk project watch` → `ezmk watch`
- [x] `tutorial/en/README.md` — 简写说明更新，提及顶层别名
- [x] `tutorial/zh/` 对应 6 个文件同步更新
- [x] `docs/en/faq.md` + `docs/zh/faq.md` — 命令示例改为顶层别名

### 阶段四：`plans/README.md` 更新（P0）✅

**文件**：`plans/README.md`

- [x] "当前执行" 显示 `1.1.0-pre.2`（本次执行前已就绪）
- [x] "已完成" 包含 `1.1.0-dev.7`、`1.1.0-pre.1`
- [x] "待执行" 包含 `1.1.0` 正式版发布
- [x] 版本主题概览表 `1.1.0` 行包含 pre.1 / pre.2 交付物
- [x] Mermaid 图含 `v110pre1`（done）、`v110pre2`（active）、`v110`（todo）

### 阶段五：pre.1 遗留项补齐（P1）✅

#### 5.1 `docs/zh/technical.md` 中文版（新建）

- [x] 基于 `docs/en/technical.md` 完整翻译：依赖表 / 源码编译 / 编译器支持矩阵 / MSVC 使用 / 项目结构 / Shell 补全说明
- [x] 术语一致性（header-only → 仅头文件、precompiled → 预编译）

#### 5.2 `res/ezmk.zsh` zsh 补全（新建）

> 现状说明：`completions/_ezmk` 已存在（pre.1 更新过顶层别名）。执行时将 `res/ezmk.zsh` 作为新规范位置，**迁移**而非从零创建。

- [x] 创建 `res/ezmk.zsh`：顶层别名补全 + `project`/`pkg`/`repo`/`utils` 子命令补全（不包含单双字母简写）
- [x] `install.sh` 安装源 `completions/_ezmk` → `res/ezmk.zsh`
- [x] `.github/workflows/release.yml` 补全拷贝路径同步（`res/ezmk.zsh` → `dist/*/_ezmk`）
- [x] 移除旧位置 `completions/_ezmk`（单一数据源）

#### 5.3 CI 与包管理器分发

- [ ] **延后**：`.github/workflows/release.yml` 激活、Homebrew formula、winget manifest → pre.3 或 1.1.0 正式版（见 §6 延后项）

### 阶段六：i18n 完整性校验（P0）✅

**文件**：`include/ezmk/i18n_keys.def` + `locale/en.json` + `locale/zh.json`

- [x] `i18n_keys.def` 每个 key 在 `en.json` / `zh.json` 中存在
- [x] `en.json` / `zh.json` 无 def 未定义的冗余 key，两 JSON key 集合一致
- [x] **校验结果：已确认 ✅** — def 定义 273 个 key，en.json / zh.json 各 273 个，三方完全同步

### 阶段七：`CHANGES.md` 补全（P0）✅

**文件**：`CHANGES.md`

- [x] 补充 `1.1.0-dev.7` 条目（包生态拓充 12+10 包 + 硬依赖前置检查 + 自动安装 + want 交互式询问）
- [x] 补充 `1.1.0-pre.1` 条目（顶层命令别名 + `--help` 重组 + README 精简 + API 稳定性承诺）
- [x] 按版本序插入（newest-first，位于 `1.1.0` dev.1/dev.2 条目之前）

### 阶段八：`version.hpp` 更新（P0）✅

**文件**：`include/ezmk/version.hpp` + `build.sh`

- [x] `build.sh` 默认版本 `1.0.0` → `1.1.0`（`build.sh` 每次构建重写 `version.hpp`，真正的 fallback 源在 `build.sh`）
- [x] 构建后 `version.hpp` 为 `1.1.0`，`build/ezmk version` 输出 `EazyMake 1.1.0`

### 阶段九：`docs/zh/` 与 `docs/en/` 同步检查（P1）✅

- [x] 逐文件对比 12 对 en/zh 文档：章节结构一致（heading 数对比通过）
- [x] `docs/zh/technical.md` 缺失项由阶段五 5.1 补齐
- [x] 唯一差异 `package_authoring.md`（zh）§6.3 缺 `# Or:` 代码注释——已补齐

### 阶段十：编译与回归验证（P0）✅

- [x] `bash build.sh` 编译通过（MSYS2 / Windows）
- [x] `bash build.sh test` 全量测试通过：**545 用例 / 2613 断言**，零回归
- [x] `bash build.sh test-all` 含集成测试：**555 用例 / 2661 断言**，零回归

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **Tutorial 首选顶层别名** | 新用户看到的第一条命令就是 `ezmk build`；首次出现处标注完整形式 |
| **CLI 文档表格改为别名优先** | 与 `--help` 输出保持一致，日常命令以短形式为主行 |
| **保留旧写法不删除** | `ezmk project build` 在文档中作为 "full form" 交叉引用 |
| **zsh 补全不包含单双字母简写** | 与 pre.1 决策一致 |
| **`res/ezmk.zsh` 为新目标位置** | `install.sh` / `technical.md` / `release.yml` 同步指向；迁移自 `completions/_ezmk` 并移除旧文件 |
| **CI/包管理器分发延后** | `.github/workflows/release.yml` 激活、Homebrew、winget → pre.3 或 1.1.0 正式版 |
| **`version.hpp` fallback 改为 `1.1.0`** | 修正 `build.sh` 默认值（真正 fallback 源），构建时以 `EZMK_VERSION` 覆盖具体 pre-release 版本 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| CLI 文档增加顶层别名 | 用户看到新命令写法 | 旧写法保留为 "full form"，纯增量 |
| Tutorial 代码示例更新 | 新用户学到 `ezmk build` | 首次出现处标注完整形式 |
| `version.hpp` fallback 更新 | 非 `build.sh` 编译时版本号变化 | 已同步修正 `build.sh` 默认值 |
| i18n 校验 | 无用户可见变化 | 已确认三方一致，无需修改 |
| `CHANGES.md` 补全 | 文档完整性提升 | 纯增量 |
| zsh 补全位置迁移 | 安装补全路径变化 | `install.sh` / `technical.md` / `release.yml` 同步更新 |

---

## 6 延后项（pre.3 / 1.1.0）

- `.github/workflows/release.yml` 激活 — CI 自动构建（依赖 Release 触发）
- Homebrew formula（`homebrew-eazymake/ezmk.rb`）
- winget manifest（`manifests/e/ezmk/1.1.0.yaml`）
- `plans/release/1.1.0.md` — 最终 1.1.0 发布计划（合并所有 dev.x + pre.x 内容）
- Tutorial 新增章节：`09-test.md`（`ezmk test` 专题教程）
- Tutorial 新增章节：`10-top-level-aliases.md`（顶层别名快速参考）

---

## 7 涉及文件变更摘要

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `docs/en/cli.md` | 修改 | 顶层别名章节 + 命令表格别名优先 |
| `docs/zh/cli.md` | 修改 | 中文同步 |
| `README.md` / `README_ZH.md` | 修改 | 命令速览补 `ezmk pack` 顶层别名 |
| `docs/en/config_file.md` | 修改 | 新增 `[install]` + `[test]` 配置节 |
| `docs/zh/config_file.md` | 修改 | 中文同步 |
| `docs/en/technical.md` | 修改 | zsh 补全路径修正 + 测试数据更新 |
| `docs/zh/technical.md` | **新建** | 中文翻译（pre.1 遗留） |
| `tutorial/en/` ×6 + `tutorial/zh/` ×6 | 修改 | 顶层别名优先 + 首用处脚注 + README 简写说明 |
| `docs/en/faq.md` / `docs/zh/faq.md` | 修改 | 命令示例改为顶层别名 |
| `docs/zh/package_authoring.md` | 修改 | §6.3 补齐 `# Or:` 代码注释（与 en 同步） |
| `CHANGES.md` | 修改 | 补充 pre.2 + dev.7 + pre.1 条目 |
| `include/ezmk/version.hpp` | 修改 | fallback `1.0.0` → `1.1.0`（构建生成） |
| `build.sh` | 修改 | 默认版本 `1.0.0` → `1.1.0` |
| `res/ezmk.zsh` | **新建** | zsh 补全（迁移自 `completions/_ezmk`） |
| `completions/_ezmk` | **删除** | 迁移至 `res/ezmk.zsh`（单一数据源） |
| `install.sh` | 修改 | 补全安装源 `completions/_ezmk` → `res/ezmk.zsh` |
| `.github/workflows/release.yml` | 修改 | 补全拷贝路径同步（延后项，路径保持有效） |
| `plan.md` | 重写 | pre.2 执行计划 + 完成状态（本次） |
| `plans/release/1.1.0-pre.2.md` | 修改 | 执行过程中 checkbox 更新 |

---

## 8 版本路线图

```
1.0.0 (正式版) ──→ 1.1.0-dev.1~7 (包编译与开发体验) ✅
                 → 1.1.0-pre.1 (改善用户触达) ✅
                 → 1.1.0-pre.2 (文档检查) ✅ 已完成
                 → 1.1.0-pre.3 (CI / 包管理器分发)
                 → 1.1.0 (正式版发布)
```
