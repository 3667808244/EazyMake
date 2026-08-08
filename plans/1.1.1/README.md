# 1.1.1 版本系列

1.1.1 为 **1.1.x 稳定线的补丁发布**（承接原 `1.2.0-dev.1` 的核心，范围收缩为"拦截 `ezmk utils cc` + 构建后自动生成"）。修复既有 `ezmk utils cc` 命令的 compile_commands.json 输出 drift，并新增 `[compile].compile_commands` 配置项实现构建后自动生成。**不新增命令、不弃用、不触碰 1.1.0 稳定 API**。

## 版本列表

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.1.1](1.1.1.md) | 拦截 `ezmk utils cc` + 自动生成 | `build_compile_args()` 重构 + `compile_db` 模块；`Command::Utils` 拦截 `cc` 改由 C++ 生成器服务；`[compile].compile_commands` 构建后自动生成；输出 drift-free | 待实现 |

## 与 1.2.0 的关系

- 1.1.1 **先行**：为 1.2.0 的 `ezmk project cc` 命令铺路（共享 `build_compile_args()` + `compile_db` + 自动生成基础）。
- 1.2.0 承接：`ezmk project cc` 新命令 + `ezmk utils cc` 弃用（见 `plans/1.2.0/`）；自动生成已在 1.1.1 交付，1.2.0 不重复。
