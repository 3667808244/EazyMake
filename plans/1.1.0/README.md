# 1.1.0 — 正式版发布系列

> 1.1.0 系列：dev.1 ~ dev.7（包编译与开发体验）+ pre.1（改善用户触达）+ pre.2（文档检查）+ pre.3（缺陷收集与未实现项补全），合并为 1.1.0 正式版。**dev / pre 全部完成 ✅，1.1.0 正式版当前执行中 🔄**。

## 版本列表

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
| [1.1.0](1.1.0.md) | 正式版 | 正式版发布 | 合并 dev.1~dev.7 + pre.1~pre.3；发布流水线 3.3.x 收口 | 🔄 当前执行 |

## 发布门槛

⛔ 发布前必须同时满足：实现完整 + API 兼容 + 全量测试零回归。详细 Gate 定义见 [1.1.0-pre.3](1.1.0-pre.3.md#-发布门槛release-gate)。
