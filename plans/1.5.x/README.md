# 1.5.x — 功能版本系列

> 1.5.x 系列 = **1.5.0 功能次版本**。当前已立项 **7 个 dev 子版本**（dev.1~dev.7），把「用户/全局配置文件」拆成可独立交付的步骤；`1.5.0-pre.N` 与正式发布聚合计划按用户后续补充（**当前不创建**）。

## 版本规划

| 子版本 | 主题 | 关键交付 | 依赖 | 状态 |
|--------|------|----------|------|------|
| [1.5.0-dev.1](1.5.0-dev.1.md) | 配置骨架与加载器 | 两个环境变量 / `config.toml` schema / 合并 / 校验 / `RuntimeConfig` / `--config` / `--no-config` | 无 | ✅ 实现收口（1177/6833） |
| [1.5.0-dev.2](1.5.0-dev.2.md) | 路径与工具定位 | `[base_dir]` + `build_dir` + `git_path` + `editor` | dev.1 | ✅ 实现收口（1183/6851） |
| [1.5.0-dev.3](1.5.0-dev.3.md) | 项目创建与行为默认值 | 语言 / 颜色 / 主题 + `project new` 默认（git / gitignore / readme / 初始提交 / 类型 / 语言 / 版本 / flags）+ `auto_pack` / `gen_agent_md` | dev.1 | ✅ 实现收口（1189/6876） |
| [1.5.0-dev.4](1.5.0-dev.4.md) | 日志子系统 | `log_level` 过滤 + `[base_dir].logs` 文件日志 + 失败降级 | dev.1、dev.2 | ✅ 实现收口（1194/6895） |
| [1.5.0-dev.5](1.5.0-dev.5.md) | 构建/测试/监视/打包/包管理运行默认值 | `default_jobs` / `test_timeout` / `test_report_dir` / `watch_debounce_ms` / `watch_ignore` / `pack_*` / `default_pkg_scope` / `auto_install_deps` / `scan_skip_dirs` | dev.1、dev.2 | 📋 计划完成，待实现 |
| [1.5.0-dev.6](1.5.0-dev.6.md) | 代码审查（增量） | 审查 dev.1~dev.5 的配置 / 路径 / 工具 / 项目创建 / 日志 / 运行默认；P0/P1 修复或收口 | dev.1~dev.5 | 📋 空白计划，待 dev.5 后执行 |
| [1.5.0-dev.7](1.5.0-dev.7.md) | 文档、i18n 与 dev 收口 | i18n / README / docs / tutorial / man / CHANGES / 索引 / 门槛 | dev.1~dev.6 | 📋 计划完成，待实现 |

## 依赖关系

~~~text
dev.1 配置骨架与加载器
  ├──→ dev.2 路径与工具定位 ──→ dev.4 日志子系统
  │                          └──→ dev.5 运行默认值
  ├──→ dev.3 项目创建与行为默认值
  └──→ dev.6 代码审查（依赖 dev.1~dev.5）
         └──→ dev.7 文档、i18n 与 dev 收口（依赖 dev.1~dev.6）
~~~

- **dev.1 是所有后续版本的唯一数据源**：后续 dev 只读 `RuntimeConfig`，不各自解析 `config.toml`。
- **dev.2 必须先于 dev.4 / dev.5**：日志目录与 `build_dir` 由路径层解析。
- **dev.6 是代码审查，dev.7 是 dev 收口**：不创建 pre / 正式 release 计划；是否进入 pre 由用户决定。
- **第 10 项（git clone 深度）不纳入**（用户裁定 2026-10-24）。

## 定位

- **1.5.0 是 1.4.11 之后的第一个功能 minor**：新增「用户/全局配置层」这一公共契约，而不是继续压在 1.4.x 补丁线。
- **默认零变化**：未创建任何 `config.toml` 时，路径与行为与 1.4.11 完全一致；所有新能力 opt-in。
- **公共契约**：两个环境变量与 `[base_dir]` / `[settings]` 键名一经发布即稳定，新增键只能追加。

## 跨版本关注点

- 1.5.0 的其余内容、`pre` 与正式发布计划待补；本系列暂不影响 2.0.0 的破坏性变更窗口。
- 回归基线：1.4.11 发布态 **1169 用例 / 6788 断言 / 4 跳过 / 0 失败**。
