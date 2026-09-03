# 1.4.x — 功能版本系列（调试配置自动化 + 语言标准收尾）

> 1.4.x 系列 = **1.4.0 功能版本**（dev.1 调试配置生成 → dev.2 工具链能力表 + 校验严格化 → dev.3 编译协商 → dev.4 CMake 互操作补全 → dev.5 功能收口 → dev.6 代码质量审计 → **dev.7 workspace scan（用户确认插队）** → pre.1 发布前收口）—— **2026-08-30 已发布 ✅（tag `v1.4.0`）**；**1.4.1 补丁版本**（`pkg install` 支持 git 仓库 URL）—— **开发中**（2026-08-31 立项，dev 阶段完成：git URL 安装链路 + ref 定位 + lockfile commit/`--locked` + 测试文档收口）。
>
> 本目录为 1.4.x 全部文档的**平铺结构**（无子文件夹）：`1.4.0-dev.N.md` 为开发子版本，`1.4.0-pre.N.md` 为发布前收口（`1.4.0-pre.1.md` 已建），`1.4.0.md` 为正式版聚合（届时新建），`1.4.1.md` 为正式发布后的补丁版本（对照 1.3.x 补丁惯例）。

## 定位

1.4.0 是 1.3.x 全部补丁收口后的**首个功能 minor**。两大主线：

1. **调试配置自动化**（dev.1）：VS Code 三件套（`launch.json`/`tasks.json`/`settings.json`）一键生成，per-platform 调试器（gdb/lldb/cppvsdbg），与 `[compile.profile.*]` 联动——把 ezmk 的构建知识（include/宏/`-std`/依赖注入）复用进调试器配置，消灭手写拼参。
2. **语言标准收尾**（dev.2 ~ dev.4）：1.3.1 区间语言标准（语义 A）的后续——工具链能力表 `max_supported_std`（语义 C 铺路）→ 校验严格化开关 → 编译协商（语义 B，包按 `max(包min, 消费者标准)` 重编）→ CMake `CXX_STANDARD` 导入映射补全。

dev.5 集中收口 1.3.x 各版延后的小功能项（watch `--` 透传 / `workspace watch` / `tgz` 别名 / sha256 边车自动校验）。dev.6 对全部代码做系统质量审计（8 路并行审查），修复 8 项 P0 正确性缺陷与高价值 P1（确定性缓存失效 / `--locked` 不锁版本 / 依赖名与 repo 名路径穿越 / git branch 注入 / MSVC 本地化解析 / CMake 括号注释 / Lua 根路径），并修复 4 个恒真/无断言测试。**dev.7 为用户确认的插队功能子版本**：`ezmk workspace scan`——一键采纳现有目录树为 workspace（棕地场景，补齐 `project new`/`import` 之外的采纳路径）。**公共 API 无破坏性变更**（纯增量；破坏性变更仍仅归 2.0.0）。

## 版本规划（dev / pre 拆分）

> 1.4.0 正式版按 dev → pre 两阶段推进：dev 落地功能，pre 做文档/门槛收口，最后聚合发布。

| 子版本 | 主题 | 关键交付 | 状态 |
|--------|------|----------|------|
| [1.4.0-dev.1](1.4.0-dev.1.md) | 调试配置生成 | `project export vscode` 三件套（launch/tasks/settings）+ per-platform 调试器 + profile 联动 + JSON 序列化/覆盖保护 | ✅ 已完成（2026-08-27，dev.1：全量 931/5396 零回归，+20 用例/+94 断言） |
| [1.4.0-dev.2](1.4.0-dev.2.md) | 工具链能力表 + 校验严格化 | `max_supported_std(family, version)`（gcc/clang/msvc 分段）+ `[pkg] strict_std_check` 开关（warn→error） | ✅ 已完成（2026-08-27，dev.2：全量 939/5442 零回归，+8 用例/+46 断言） |
| [1.4.0-dev.3](1.4.0-dev.3.md) | 编译协商（语义 B） | 包按 `max(包min, 消费者min)` 重编（cap 到能力表与包 max）+ 缓存签名自动失效 + 与 1.3.1 warn 共存 | ✅ 已完成（2026-08-27，dev.3：全量 948/5472 零回归，+9 用例/+30 断言） |
| [1.4.0-dev.4](1.4.0-dev.4.md) | CMake 互操作补全 | `import` 读 `CXX_STANDARD` → 区间 language（`">=CPP<N>"`）；`export` 超能力注释 | ✅ 已完成（2026-08-27，dev.4：全量 959/5492 零回归，+11 用例/+20 断言） |
| [1.4.0-dev.5](1.4.0-dev.5.md) | 功能收口 | watch `--` 透传 + `workspace watch` + `tgz` 别名 + sha256 边车自动校验（1.3.x 延后项） | ✅ 已完成（2026-08-27，dev.5：全量 968/5588 零回归，+9 用例/+96 断言） |
| [1.4.0-dev.6](1.4.0-dev.6.md) | 代码质量审计 | 8 路并行审查全量代码；修复 8 P0 + 高价值 P1（缓存签名/`--locked` 锁版本/依赖名与 repo 名校验/git branch 注入/MSVC 本地化/CMake 括号注释/Lua 根路径）+ 4 恒真测试 | ✅ 已完成（2026-08-29，dev.6：全量 968/5612 零回归，+24 断言） |
| [1.4.0-dev.7](1.4.0-dev.7.md) | workspace scan | `ezmk workspace scan [<dir>] [--dry-run] [-y]`：递归扫描目录树收集成员、生成/合并 `ezmk-workspace.toml`（文本级拼接保留注释与 options）、`ws` 简写、跳过规则（隐藏/嵌套根/逃逸） | ✅ 已完成（2026-08-30，dev.7：全量 988/5770 零回归，+20 用例/+158 断言） |
| [1.4.0-pre.1](1.4.0-pre.1.md) | 发布前收口 | 用户触达打磨（别名总表 / `--help` / README 速览）+ 全量文档检查（docs/README/tutorial/skill + i18n 三向）+ 缺陷收集与未实现项补全（dev 已知限制聚合裁定表）+ 1.4.0 聚合 changelog + API 稳定性承诺扩展 + 发布门槛预核对 | ✅ 收口完成（2026-08-30：全量 1003/5835 零回归，+15 用例/+65 断言；门槛 ①②③ 满足） |
| [1.4.1](1.4.1.md) | pkg install 支持 git 仓库 URL | `pkg install` 识别 git URL（`git@`/`git://`/`file://`/`.git`）→ 克隆 → ref 定位（`#<ref>`/`--branch`，分支/标签浅克隆、commit 全量）→ 复用目录安装链路 → lockfile 记录 `source="git"` + `commit` + `--locked` 校验 | ✅ 开发完成（2026-09-01：git URL 安装链路 + ref 定位 + lockfile commit/`--locked` + 测试文档收口，全量 1020/5970 零回归；待 pre/正式发布） |

### 依赖关系

```
1.3.6 (收口) ──→ 1.4.0-dev.1 (调试配置)
              ├──→ 1.4.0-dev.2 (能力表) ──→ 1.4.0-dev.3 (编译协商)
              │                                  └──→ 1.4.0-dev.4 (CMake 补全)
              ├──→ 1.4.0-dev.5 (功能收口, 独立)
              ├──→ 1.4.0-dev.6 (代码质量审计, 独立)
              └──→ 1.4.0-dev.7 (workspace scan, 独立)
1.4.0-pre.1 (收口) ──→ 1.4.0 (正式发布, 2026-08-30) ──→ 1.4.1 (git URL 安装, 开发中 2026-09-01)
```

- dev.1 / dev.5 / dev.6 / dev.7 与语言标准主线完全独立，可并行。
- dev.2 → dev.3 顺序依赖（协商需能力表 cap）；dev.4 仅导出确认依赖 dev.2（未就绪时跳过）。
- dev.7 依赖 dev.6 收口后重新打开 dev 阶段（用户确认）；**pre.1 依赖全部 dev 完成**（已完成，接 1.4.0 正式版聚合发布）。
- **1.4.1 为 1.4.0 发布后的补丁版本**（对照 1.3.x 补丁惯例），复用 1.4.0 的 `install_from_directory` 目录安装链路与 repo 子系统 git helper；与 1.4.0 各 dev 无顺序依赖。dev 阶段已完成（2026-09-01，全量 1020/5970 零回归），下一步 pre 收口 → 正式发布。

## 跨版本关注点

- **1.3.1 语义 A 为基线**：dev.2/dev.3 扩展而非替换区间语法语义；上界/元数据语义不变。
- **1.3.6 重构收益**：`run_executable`（watch 透传通道）、`run_member`（workspace watch 模型）、`TestRunContext`（测试基础设施）为本系列复用。
- **1.3.6 延后重构项随主线穿插**：build.cpp/pkg.cpp 全面重构、Catch2 结构化解析（报告语义化）——不单列 dev，随相关 dev 一并评估。
- **cli.cpp 命令组拆文件**（`parse_*` 1272 行单文件）：2.0.0 前评估。
- **回归基线**：全量 968 用例 / 5612 断言（1.4.0-dev.6 后实测，基线 968/5588），新增功能不得引入回归。
- **i18n**：各 dev 新增 key 三向一致 + `check_i18n.py` 通过。
- **与 2.0.0 解耦**：本系列纯增量，不依赖任何 deprecation 到期；2.0.0 保持破坏性变更窗口。
