# 1.1.2 版本系列

1.1.2 为 **1.1.x 稳定线的补丁发布**：安全加固与静默错误修复。基于 2026-08-08 多模块代码质量评审，修复 4 处安全漏洞（zip-slip 路径穿越、归档命令注入、`ezmk_file_write` 越界、Lua 沙箱逃逸）与 7 处静默产出错误结果的正确性 bug（链接假成功、缓存签名漏输入、`--locked` 误报、Windows 脚本全挂、TOML 写入不转义、非事务化安装、确定性数据竞争）。**不新增命令、不弃用、不触碰 1.1.0 稳定 API**。

## 版本列表

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.1.2](1.1.2.md) | 安全加固与静默错误修复 | `run_command` `RunOptions`（cwd/env）；解压路径包含校验 + 上限；`ar`/`lib.exe` 转义；`file_write` 边界检查；Lua 沙箱收敛；link rename 检查；缓存签名补 `stdlib`/`pic`；lockfile `direct_deps`；Windows 脚本修复；TOML 转义；安装事务化；`SOURCE_DATE_EPOCH` env 注入 | 待实施 |

## 与 1.2.0 的关系

- 1.1.2 **先行**：修复是 1.2.0 功能计划的稳定性地基（缓存签名、确定性构建、子进程模型均被 1.2.0 复用）。
- 1.2.0 承接：功能计划（`ezmk project cc` / CMake 导出 / 模板 Profile）不受影响；`run_command` `RunOptions` 成为后续子进程调用的前置资产。
