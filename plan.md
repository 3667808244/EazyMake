# EazyMake 执行计划

> 来源: `plans/README.md`（版本规划与路线图）。
> 仓库已克隆到 `E:\claude_workspace\ezmk-repo`，Gitee 镜像: `https://gitee.com/egglzh/ezmk-repo.git`。

---

## 路线图总览

```
0.9.0 (done) → 0.9.1 (done) → 0.9.2 (current) → 0.9.3 (next) → 1.0
  发布正式版      默认仓库创建      文档多语言          捆绑包迁移        正式发布
```

| 版本 | 状态 | 主题 | 依赖 |
|------|------|------|------|
| 0.9.0 | ✅ 已完成 | 准备发布正式版 | — |
| 0.9.1 | ✅ 已完成 | 默认仓库创建 | 0.9.0 |
| 0.9.2 | 🟡 执行中 | 文档多语言 | 0.9.0（文档基础） |
| 0.9.3 | ⬜ 待执行 | 捆绑包迁移 | 0.9.1（官方仓库就绪） |

---

## 一、0.9.2 — 文档多语言（当前）

> 详细设计: [`plans/0.9.2.md`](plans/0.9.2.md)

**目标**: 将 `docs/` 和 `tutorial/` 拆分为 `en/` + `zh/` 子目录，补齐英文翻译，建立术语表与维护规则。

**背景**: EazyMake 从 0.1.7 起建立了中英双语的 i18n 体系（CLI 字符串），0.9.0 做了 README 双语互链。但项目文档和教程目前仍以中文为主，英文用户无法阅读设计文档和入门教程。对于 1.0 正式发布，英文文档是硬需求。

### 目录结构设计

**重组后**：
```
docs/
  en/                         # 英文文档（权威链接目标）
    cli.md, config_file.md, pkg.md, repo.md, utils.md
    @cache.md, @safety.md, glossary.md
  zh/                         # 中文文档（权威语言，source of truth）
    cli.md, config_file.md, pkg.md, repo.md, utils.md
    @cache.md, @safety.md, glossary.md
tutorial/
  en/                         # 英文教程
  zh/                         # 中文教程
```

**不翻译**: `plans/*.md`（内部版本计划）、源码注释（已英文为主）、`locale/en.json`/`locale/zh.json`（i18n 体系，不在本版本范围）。

### 翻译策略

- **中文为权威语言**（source of truth）：现有文档均为中文，质量经多轮审阅；英文从中文译出
- **技术准确性优先于文学性**：API 名、字段名、命令名、错误信息原文保留
- **术语一致性**：中→英映射统一，参考 `docs/en/glossary.md`
- **代码块不翻译**：TOML 示例、shell 命令、Lua 代码保持原样

### 执行步骤

**阶段一：目录重组**

- [ ] 创建 `docs/en/` 和 `docs/zh/`，将现有 `docs/*.md` 移入 `docs/zh/`
- [ ] 创建 `tutorial/en/` 和 `tutorial/zh/`，将现有教程文件移入 `tutorial/zh/`
- [ ] 更新 `CLAUDE.md` 中的路径引用（`docs/` → `docs/en/`）

**阶段二：docs 翻译**（中文为权威语言，英文从中文译出）

- [ ] `docs/en/cli.md` — CLI 命令参考表 + 详细说明（命令名/flag 保留原样；示例输出更新为英文 locale 下实际输出）
- [ ] `docs/en/config_file.md` — `ezmk.toml` 完整格式规范（字段名保留；类型/默认值/说明翻译）
- [ ] `docs/en/pkg.md` — 包管理设计文档（概念翻译 + 命令示例保留）
- [ ] `docs/en/repo.md` — 仓库系统设计文档（`index.toml` 结构说明翻译；字段名保留）
- [ ] `docs/en/utils.md` — Lua API 参考 22 个函数（函数签名保留；参数/返回值说明翻译）
- [ ] `docs/en/@cache.md` — 缓存机制（算法描述翻译；文件路径保留）
- [ ] `docs/en/@safety.md` — 安全模型（安全策略翻译；命令保留）
- [ ] `docs/en/glossary.md` — 术语表（新写）
- [ ] `docs/zh/glossary.md` — 术语表（中文版，新写）

**阶段三：tutorial 翻译**

- [ ] 确认 0.9.0 教程的文件清单
- [ ] `tutorial/en/` 全部文件翻译完成（对应 `tutorial/zh/`）
- [ ] shell 命令保留；命令预期输出切换为英文 locale（`EZMK_LANG=en`）

**阶段四：引用更新**

- [ ] `README.md` 文档链接指向 `docs/en/`
- [ ] `README_ZH.md` 文档链接指向 `docs/zh/`
- [ ] 源码注释中文档路径更新（如有 `@see docs/...` 注释）
- [ ] `plans/README.md` 路径引用更新

**阶段五：CI 与维护规则**

- [ ] 文件对应性检查脚本（`en/` 与 `zh/` 文件一一对应，`diff` 即可）
- [ ] `CONTRIBUTING.md` 写入文档维护规则（先改中文 → 再英译；术语参考 glossary.md）

### 翻译约定

- 命令名/flag/字段名/TL 示例**保留原样**
- 术语一致性：参考 `docs/en/glossary.md`
- 代码块不翻译；Markdown frontmatter 仅翻译 `title`/`description`
- `plans/*.md` 不翻译（内部版本计划）
- `locale/en.json` / `locale/zh.json` 不在本版本范围

### 核心术语表

| 中文 | 英文 | 说明 |
|------|------|------|
| 包 | package | 与 `pkg` 命令族对应 |
| 仓库 | repository (repo) | 与 `repo` 命令族对应 |
| 作用域 | scope | `-g`/`-u`/`-p` |
| 构建/编译/链接 | build / compile / link | |
| 缓存 | cache | |
| 钩子 | hook | 构建生命周期钩子 |
| 工具链 | toolchain | 编译器/链接器抽象 |
| 可选依赖 | optional dependency | `want.lib` |
| 权限 | permission | utils 白名单 |
| 沙箱 | sandbox | Lua 安全模型 |

### 测试要点

- [ ] `docs/en/` 与 `docs/zh/` 文件一一对应（脚本验证）
- [ ] `tutorial/en/` 与 `tutorial/zh/` 文件一一对应
- [ ] 每个 `docs/en/*.md` 内部链接可点击、不 404
- [ ] README 双语互链正确（en README → en docs/，zh README → zh docs/）
- [ ] 教程命令示例在英文 locale 下可实际运行

### 开放问题

1. **英文文档审阅**：当前维护者非英语母语，1.0 前是否需要 native speaker 做一轮审阅？
2. **翻译时滞容忍度**：`zh/` 更新后 `en/` 允许滞后多久？建议宽松策略：中文 PR 合并时不强制英文同步，每月集中补译一次。
3. **文档格式统一**：是否借翻译机会统一 Markdown 风格？建议不在此版本做，翻译时保持原格式。
4. **PDF/HTML 构建**：是否提供静态站点或 PDF 分发？推迟到 post-1.0。

---

## 二、0.9.3 — 捆绑包迁移（下一版本）

> 详细设计: [`plans/0.9.3.md`](plans/0.9.3.md)

**目标**: 将 7 个 `pkg/*.tar.gz` 捆绑包迁移到官方仓库，清理 `install.sh` 冗余拷贝，`ezmk-cc` 保留内置。

**背景**: 捆绑包是仓库系统成熟之前的过渡方案。0.9.1 已建立官方仓库并预注册，这些包应通过标准的 `repo → search → install` 流程使用。当前问题：捆绑包无版本号、旧 TOML 格式（`include_dir` 单数）、仓库与二进制重复、无源码可审计、`install.sh` 多余拷贝。

### 待迁移包（7 个）

| 包 | 类型 | 估计版本 | 大小 | 特殊处理 |
|----|------|----------|------|----------|
| `catch2` | `static` | v3.4.0 | 199 KB | 大部分 header-only + 少量编译单元 |
| `fmt` | `static` | 10.2.1 | 137 KB | 成熟格式化库 |
| `lua` | `static` | 5.4.7 | 225 KB | 与 ezmk 内嵌同版本，`language = "C"` |
| `nlohmann_json` | `static` | 3.11.3 | 135 KB | header-only + stub |
| `spdlog` | `static` | 1.14.1 | 204 KB | 依赖 `fmt`（`depends.lib = ["fmt"]`） |
| `sqlite3` | `static` | 3.45.0 | 2,449 KB | 单文件 amalgamation，`language = "C"` |
| `tinyxml2` | `static` | 10.0.0 | 34 KB | 轻量，仅 2 个文件 |

> 版本号需通过查阅各包的实际源码（头文件版本宏、CHANGELOG 等）确认，上表为估计值。

**依赖关系**: `spdlog` → `fmt`（其它包无互相依赖）

### 不迁移

| 名称 | 原因 |
|------|------|
| `ezmk-cc` | 内置工具，编译进 ezmk 二进制（`src/lua_api.cpp`），`pkg/ezmk-cc/` 保留为源码参考 |

### 通用修正（所有迁移包）

- **补 `version`**：查阅上游版本号，写入 `ezmk.toml`
- **TOML 格式更新**：`include_dir` → `include_dirs`，补 `src_dirs`
- **补 `language`**：C 库标记 `"C"`，其他标记 `"C++17"`
- **审查 flags**：去掉硬编码的 `-Wall -O2`（由用户项目控制）

### 执行步骤

**阶段一：仓库侧 — 逐包重建源工程**

- [ ] 对每个包：解压 `.tar.gz` → 提取源文件 → 放入 `ezmk-repo/sources/<name>/`
- [ ] 确认/修正每个包的 `version`（查阅上游版本号）
- [ ] 更新 `ezmk.toml` 格式（`include_dirs`、`src_dirs`、`language`，去掉硬编码 `-O2`）
- [ ] 运行 `pack.sh` 生成新归档 + sha256 + 更新 `index.toml`
- [ ] 运行 `validate.sh` 确认全部通过

**阶段二：仓库侧 — CI 校验**

- [ ] 端到端测试：从仓库安装各包 → 编译 → 链接通过
- [ ] `spdlog` + `fmt` 依赖链验证（安装 `spdlog` 自动拉取 `fmt`）

**阶段三：主项目 — 清理**

- [ ] 删除 `pkg/*.tar.gz`（7 个归档）
- [ ] 删除归档对应的解压目录（如 `pkg/catch2/` 等）
- [ ] 更新 `install.sh`：移除捆绑包拷贝逻辑（或仅保留 `ezmk-cc/`）
- [ ] 保留 `pkg/ezmk-cc/`（内置工具源码参考）

**阶段四：文档**

- [ ] 官方仓库 `README.md` 更新包列表
- [ ] `README.md` / `docs/cli.md` 移除捆绑包相关描述（如有）
- [ ] 如有离线使用场景，在文档中给出替代方案（本地镜像、离线包等）

### 兼容性

| 场景 | 影响 | 处理 |
|------|------|------|
| 新用户 `install.sh` | `pkg/` 不再有捆绑归档 | 已预注册官方仓库，`pkg install <name>` 自动从仓库拉取，无感知 |
| 旧用户已安装的捆绑包 | 已安装到各作用域 | 不受影响，`pkg list` 仍可见；`pkg update` 会从仓库拉取新版本 |
| 离线环境 | 捆绑包删除后无网络无法安装 | 文档给出离线方案（`repo add` 本地镜像或保留离线包） |
| `ezmk utils cc` | 内置工具 | 不受影响，仍然可用 |

### 测试要点

- [ ] 逐包 `pkg install <name>` → 测试项目 `[depends]` 引用 → `project build` 通过
- [ ] `spdlog` 安装时自动拉取依赖 `fmt`（依赖链）
- [ ] `pkg info <name>` 显示版本号正确
- [ ] SHA-256 校验全部通过
- [ ] 旧版 `install.sh` 安装的用户 → 新仓库仍然可 `pkg install`（向后兼容）
- [ ] `ezmk utils cc` 在新安装下仍正常运行

### 开放问题

1. **版本号确认**：捆绑包制作时的上游版本号已无从追溯，需逐包打开头文件版本宏确认。
2. **`install.sh` 过渡策略**：建议直接移除捆绑包拷贝（已预注册仓库，用户无感知）。
3. **`ezmk-cc` 的 `ezmk.toml`**：是否需要同步更新格式以保持一致性？
4. **包的更新机制**：初始版本号设置需合理，确保 `pkg update` 可正常拉取仓库新版本。
5. **Catch2 编译单元**：是否保持含编译单元的原样，还是改为纯 header-only？建议保持原样。

---

## 跨版本关注点

### 向后兼容性
- `ezmk.toml` 格式扩展不影响已有配置
- CLI 接口保持稳定
- 0.9.3 捆绑包删除后，新用户通过仓库安装（已预注册），旧用户已安装的包不受影响

### 安全模型
- SHA-256 校验在仓库分发中强制使用
- Lua sandbox + utils 权限管理通过 `[utils.permissions]` 声明
- 预注册在用户作用域且显式可见（`repo list`），可关闭可撤销

### 文档一致性
- 0.9.2 建立的 `en/` ↔ `zh/` 对应关系需在后续版本中持续维护
- 术语表随新功能扩展更新
- CI 文件对应性检查防止文档漂移

---

## 已完成版本

- **0.9.1** — 默认仓库创建：ezmk 侧预注册集成、`install.sh` 预注册官方仓库、README 双语更新、版本号 0.9.1。详见 [`plans/0.9.1.md`](plans/0.9.1.md)
- **0.9.0** — 准备发布正式版：一键安装脚本、文档整理、基本教程（8 章 + 可运行示例）、发布前检查清单。详见 [`plans/0.9.0.md`](plans/0.9.0.md)
- **0.1.6 ~ 0.2.6** — 11 个功能版本（测试、国际化、跨平台、Lua 工具链、MSVC 支持、精细化控制、开发者体验、健壮性、生态与安全、翻译与命令行）。详见 [`plans/README.md`](plans/README.md)
