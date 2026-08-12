# 1.2.0 版本系列

1.2.0 为 1.1.0（经 1.1.1 补丁）之后的次版本，主题：**工具链互操作 + 开箱工程化**。开发子版本并行推进，最终合并为 `1.2.0` 正式发布。

> **前置补丁**：dev.1 的**核心重构**（`build_compile_args()` + `compile_db` 模块 + 拦截 `ezmk utils cc` + `[compile].compile_commands` 自动生成）已移入 **[1.1.1](../1.1.x/1.1.1.md)**（1.1.x 稳定线补丁）。dev.1 本身保留，内容调整为仅覆盖 `ezmk project cc` 命令交付 + `ezmk utils cc` 弃用。

## 版本列表

| 版本 | 主题 | 关键交付 | 状态 |
|------|------|----------|------|
| [1.2.0-dev.1](1.2.0-dev.1.md) | `ezmk project cc` 命令 | 新增内置命令（基于 1.1.1 的 `build_compile_args()`/`compile_db`）；`ezmk utils cc` 弃用 | **已完成**（2026-08-11） |
| [1.2.0-dev.2](1.2.0-dev.2.md) | CMakeLists.txt 导出 | `ezmk project export cmake` 命令；project/compile/link/deps 全映射；默认拒绝覆盖；`--resolve` 具体路径模式 | **已完成**（2026-08-11） |
| [1.2.0-dev.3](1.2.0-dev.3.md) | 默认模板内建 Debug/Release Profile | `ezmk project new` 模板内置 `[compile.profile.debug/release]`；基准收敛为警告-only | **已完成**（2026-08-12） |
| [1.2.0-dev.4](1.2.0-dev.4.md) | CMake 项目导入（实验性） | `ezmk project import --from cmake`；标准命令映射 + `find_package` best-effort + 非标准写法拒绝 | 待实现 |
| [1.2.0-dev.5](1.2.0-dev.5.md) | catch2 v3 测试主程序兼容 | `ezmk test` test_main 生成改 v3 兼容（显式 `main` + `Catch::Session().run()`）；v2 vendor 路径不回归 | 待实现 |
| [1.2.0-dev.6](1.2.0-dev.6.md) | 各源文件构建耗时统计 | `ezmk build` 并行编译路径 per-file 耗时明细（`-v` 全量 / 慢构建自动 top-N）；零配置、不新增 flag | 待实现 |
| [1.2.0](1.2.0.md) | 正式发布 | 聚合 dev.1 ~ dev.6；前置 1.1.1；发布门槛 + 全量回归 | 待发布 |

> **执行计划**：根 `plan.md` 为当前执行计划，镜像正在推进的子版本设计文档（当前为 [1.2.0-dev.3](1.2.0-dev.3.md)，dev.1/dev.2 已完成）。

## 依赖关系

```
1.1.0 ──→ 1.1.1 (拦截 ezmk utils cc) ──┐
                                        ├──→ 1.2.0 (正式发布)
1.2.0-dev.1 (`ezmk project cc` 命令) ───┤
1.2.0-dev.2 (CMakeLists.txt 导出) ──────┤
1.2.0-dev.3 (默认模板内建 Profile) ──────┤
1.2.0-dev.4 (CMake 项目导入, 实验性) ────┤
1.2.0-dev.5 (catch2 v3 测试主程序兼容) ──┐
1.2.0-dev.6 (各源文件构建耗时统计) ────────┘
```

- **1.1.1 先行**：dev.1 的 `ezmk project cc` 命令依赖 1.1.1 的 `build_compile_args()`/`compile_db`/拦截/自动生成基础。
- dev.1（命令层）/ dev.2 / dev.3 / dev.4 与 1.1.1、以及彼此**相互独立**，可并行。
- dev.2 与 dev.4 互为**反向互补**（导出/导入，单向、非可逆往返），共享包名别名表。
- dev.3 与 dev.1、dev.2 存在**协同**：默认模板内建 profile 后，`ezmk project cc --profile release` 与 `ezmk project export cmake --profile release` 开箱即可对照验证。
- `1.2.0` 正式版依赖 dev.1 ~ dev.4 + 前置 1.1.1 全部完成 + 发布门槛（实现完整 / API 兼容 / 全量测试零回归）。

## 跨子版本共享的关注点

- **命令构造单一事实源**（1.1.1 起）：重构后编译命令拼装收敛到 `cache.cpp` 一处，任何新增编译标志自动进入 compile_commands.json。
- **纯增量兼容**：均为新增命令与可选配置字段，不破坏 1.1.0 稳定 API；`ezmk utils cc` 仅弃用、不删除（2.0.0 移除）。
- **i18n**：新增 key 遵循 X-macro（`i18n_keys.def` + 两份 JSON），`check_i18n.py` 三向一致。
- **回归基线**：当前全量测试 565 用例 / 2727 断言（1.1.1 后基线），重构与新增命令均不得引入回归。
