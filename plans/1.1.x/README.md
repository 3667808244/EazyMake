# 1.1.x — 正式版 + 补丁发布系列

> 1.1.x 系列 = **1.1.0 正式版**（dev.1 ~ dev.7 + pre.1 ~ pre.3，2026-08-07 发布 ✅）+ **1.1.1 补丁**（拦截 `ezmk utils cc` + 自动生成，2026-08-08 发布 ✅）+ **1.1.2 补丁**（安全加固 + 静默错误修复，发布门槛已预检通过）。
>
> 本目录为 1.1.x 全部文档的**平铺结构**（无子文件夹）：`1.1.0*.md` 为正式版系列，`1.1.1.md` / `1.1.2.md` 为两个补丁。

## 1.1.0 — 正式版发布系列

> dev.1 ~ dev.7（包编译与开发体验）+ pre.1（改善用户触达）+ pre.2（文档检查）+ pre.3（缺陷收集与未实现项补全），合并为 1.1.0 正式版。**1.1.0 已于 2026-08-07 正式发布 ✅**（`v1.1.0` GitHub Release + Homebrew tap；macos-x64 / winget 为发布后跟进项）。

| 版本 | 阶段 | 主题 | 关键交付 | 状态 |
|------|------|------|----------|------|
| [1.1.0-dev.1](1.1.0-dev.1.md) | dev | MSVC 包编译、确定性构建与产物安装 | `precompiled` 包、`[install]`、`ezmk project install` | ✅ |
| [1.1.0-dev.2](1.1.0-dev.2.md) | dev | 多平台共包、index.toml 平台映射、`project pack` | `os_arch_toolchain` triple、`pack` 命令 | ✅ |
| [1.1.0-dev.3](1.1.0-dev.3.md) | dev | Agent Skills 支持 | 10 个 skill 文件 + CLAUDE.md 精简 + Copilot 桥接 | ✅ |
| [1.1.0-dev.4](1.1.0-dev.4.md) | dev | 编译器与语言配置增强 | `stdlib` / `lang` 泛化 / GNU 拓展 | ✅ |
| [1.1.0-dev.5](1.1.0-dev.5.md) | dev | 更多默认 util | `ezmk-official-utils`（cc/link/gen-build-package）、watch 修复 | ✅ |
| [1.1.0-dev.6](1.1.0-dev.6.md) | dev | 测试系统 | `ezmk test`（Catch2 + ezmk 内置框架）、`[test]` 配置 | ✅ |
| [1.1.0-dev.7](1.1.0-dev.7.md) | dev | 包生态拓充与包处理改善 | 硬依赖前置检查 + 自动安装 + `want` 交互询问 | ✅ |
| [1.1.0-pre.1](1.1.0-pre.1.md) | pre | 改善用户触达 | 顶层别名、`--help` 重组、README 精简、API 稳定承诺 | ✅ |
| [1.1.0-pre.2](1.1.0-pre.2.md) | pre | 文档检查 | 顶层别名文档化、`[install]`/`[test]` 配置节、zsh 补全迁移至 `res/` | ✅ |
| [1.1.0-pre.3](1.1.0-pre.3.md) | pre | 缺陷收集与未实现项补全 | 测试系统缺陷修复、CI 工作流、文档缺陷修正、发布流水线项 | ✅ |
| [1.1.0](1.1.0.md) | 正式版 | 正式版发布 | 合并 dev.1~dev.7 + pre.1~pre.3；发布流水线 3.3.x 收口 | ✅ 2026-08-07 |

**发布门槛**：⛔ 发布前必须同时满足：实现完整 + API 兼容 + 全量测试零回归。详细 Gate 定义见 [1.1.0-pre.3](1.1.0-pre.3.md#-发布门槛release-gate)。

## 1.1.1 — 补丁（拦截 ezmk utils cc + 自动生成）

1.1.1 为 **1.1.x 稳定线的补丁发布**（承接原 `1.2.0-dev.1` 的核心，范围收缩为"拦截 `ezmk utils cc` + 构建后自动生成"）。修复既有 `ezmk utils cc` 命令的 compile_commands.json 输出 drift，并新增 `[compile].compile_commands` 配置项实现构建后自动生成。**不新增命令、不弃用、不触碰 1.1.0 稳定 API**。

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.1.1](1.1.1.md) | 拦截 `ezmk utils cc` + 自动生成 | `build_compile_args()` 重构 + `compile_db` 模块；`Command::Utils` 拦截 `cc` 改由 C++ 生成器服务；`[compile].compile_commands` 构建后自动生成；输出 drift-free | 已发布（2026-08-08） |

### 与 1.2.0 的关系

- 1.1.1 **先行**：为 1.2.0 的 `ezmk project cc` 命令铺路（共享 `build_compile_args()` + `compile_db` + 自动生成基础）。
- 1.2.0 承接：`ezmk project cc` 新命令 + `ezmk utils cc` 弃用（见 `plans/1.2.0/`）；自动生成已在 1.1.1 交付，1.2.0 不重复。

## 1.1.2 — 补丁（安全加固 + 静默错误修复）

1.1.2 为 **1.1.x 稳定线的补丁发布**：安全加固与静默错误修复。基于 2026-08-08 多模块代码质量评审，修复 4 处安全漏洞（zip-slip 路径穿越、归档命令注入、`ezmk.file_write` 越界、Lua 沙箱逃逸）与 7 处静默产出错误结果的正确性 bug（链接假成功、缓存签名漏输入、`--locked` 误报、Windows 脚本全挂、TOML 写入不转义、非事务化安装、确定性数据竞争）。**不新增命令、不弃用、不触碰 1.1.0 稳定 API**。

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.1.2](1.1.2.md) | 安全加固与静默错误修复 | `run_command` `RunOptions`（cwd/env）；解压路径包含校验 + 上限；`ar`/`lib.exe` 转义；`file_write` 边界检查；Lua 沙箱收敛；link rename 检查；缓存签名补 `stdlib`/`pic`；lockfile `direct_deps`；Windows 脚本修复；TOML 转义；安装事务化；`SOURCE_DATE_EPOCH` env 注入 | 待发布（发布门槛已预检通过） |

### 与 1.2.0 的关系

- 1.1.2 **先行**：修复是 1.2.0 功能计划的稳定性地基（缓存签名、确定性构建、子进程模型均被 1.2.0 复用）。
- 1.2.0 承接：功能计划（`ezmk project cc` / CMake 导出 / 模板 Profile）不受影响；`run_command` `RunOptions` 成为后续子进程调用的前置资产。
