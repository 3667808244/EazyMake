# 1.5.x — 功能版本系列

> 1.5.x 系列 = **1.5.0 功能次版本**。dev.1~dev.10 已收口，dev.11 已实现；完成后需追加 pre.2；正式版聚合计划 `1.5.0` 待 pre.2 收口后补充。

## 版本规划

| 子版本 | 主题 | 关键交付 | 依赖 | 状态 |
|--------|------|----------|------|------|
| [1.5.0-dev.1](1.5.0-dev.1.md) | 配置骨架与加载器 | 两个环境变量 / `config.toml` schema / 合并 / 校验 / `RuntimeConfig` / `--config` / `--no-config` | 无 | ✅ 实现收口（1177/6833） |
| [1.5.0-dev.2](1.5.0-dev.2.md) | 路径与工具定位 | `[base_dir]` + `build_dir` + `git_path` + `editor` | dev.1 | ✅ 实现收口（1183/6851） |
| [1.5.0-dev.3](1.5.0-dev.3.md) | 项目创建与行为默认值 | 语言 / 颜色 / 主题 + `project new` 默认（git / gitignore / readme / 初始提交 / 类型 / 语言 / 版本 / flags）+ `auto_pack` / `gen_agent_md` | dev.1 | ✅ 实现收口（1189/6876） |
| [1.5.0-dev.4](1.5.0-dev.4.md) | 日志子系统 | `log_level` 过滤 + `[base_dir].logs` 文件日志 + 失败降级 | dev.1、dev.2 | ✅ 实现收口（1194/6895） |
| [1.5.0-dev.5](1.5.0-dev.5.md) | 构建/测试/监视/打包/包管理运行默认值 | `default_jobs` / `test_timeout` / `test_report_dir` / `watch_debounce_ms` / `watch_ignore` / `pack_*` / `default_pkg_scope` / `auto_install_deps` / `scan_skip_dirs` | dev.1、dev.2 | ✅ 实现收口（1198/6922） |
| [1.5.0-dev.6](1.5.0-dev.6.md) | 代码审查（增量） | 审查 dev.1~dev.5 的配置 / 路径 / 工具 / 项目创建 / 日志 / 运行默认；P0/P1 修复或收口 | dev.1~dev.5 | ✅ 修复收口（1205/6940） |
| [1.5.0-dev.7](1.5.0-dev.7.md) | 文档、i18n 与 dev 收口 | i18n / README / docs / tutorial / man / CHANGES / 索引 / 门槛 | dev.1~dev.6 | ✅ dev 收口（1205/6940） |
| [1.5.0-pre.1](1.5.0-pre.1.md) | 发布前收口 | 用户触达 / 全量文档 / i18n / 47 项审查修复 / 发布门槛 | dev.1~dev.7 | ✅ 收口完成（test-all 1219/6979；CI run 41212564762 全绿） |
| [1.5.0-dev.8](1.5.0-dev.8.md) | 项目级 agent skills 分发 | `[settings].gen_agent_skills`（bool/string）+ 构建期嵌入 4 个 `ezmk-user-*` 技能 + 分发到 `.claude/skills` / `.github/skills` / `.agents/skills` + AGENTS.md 技能描述 + 文档/测试 | dev.1、dev.3 | ✅ 实现收口（1224/7046；CI 42347726309 绿；待 pre.2） |
| [1.5.0-dev.9](1.5.0-dev.9.md) | 编译器/连接器输出策略 | `[settings].compiler_output` + `[settings].linker_output`（always / warning / error）+ 串行/并行诊断输出 | dev.8 | ✅ 实现收口（1226/7058；CI 42363070935 绿；待 pre.2） |
| [1.5.0-dev.10](1.5.0-dev.10.md) | 工具链/缓存/网络/安装前缀配置 | `[settings].cxx` / `.cc` / `.linker` / `.use_cache` / `.network_timeout` / `.retries` / `.proxy` / `.install_prefix` | dev.9 | ✅ 实现收口（1227/7077；CI 42447385973 绿；待 pre.2） |
| [1.5.0-dev.11](1.5.0-dev.11.md) | 首次无配置生成默认模板 | 无配置首次运行生成全注释 `config.toml`；不覆盖、不阻断 | dev.10 | ✅ 实现收口（1228/7095；CI 42449910408 绿；待 pre.2） |

> **全量源代码审查**（pre.1 输入）：[1.5.0-pre.1-audit.md](1.5.0-pre.1-audit.md) —— 47 项（P1 4 / P2 23 / P3 20，无 P0）。

## 依赖关系

~~~text
dev.1 配置骨架与加载器
  ├──→ dev.2 路径与工具定位 ──→ dev.4 日志子系统
  │                          └──→ dev.5 运行默认值
  ├──→ dev.3 项目创建与行为默认值
  └──→ dev.6 代码审查（依赖 dev.1~dev.5）
         └──→ dev.7 文档、i18n 与 dev 收口（依赖 dev.1~dev.6）
                └──→ pre.1 发布前收口（依赖 dev.1~dev.7）
                       └──→ dev.8 项目级 agent skills（插队）
                              └──→ dev.9 编译器/连接器输出策略
                                     └──→ dev.10 工具链/缓存/网络/安装前缀配置
                                            └──→ dev.11 首次无配置生成默认模板
                                                   └──→ pre.2 发布前复核
~~~

- **dev.1 是所有后续版本的唯一数据源**：后续 dev 只读 `RuntimeConfig`，不各自解析 `config.toml`。
- **dev.2 必须先于 dev.4 / dev.5**：日志目录与 `build_dir` 由路径层解析。
- **dev.6 是代码审查，dev.7 是 dev 收口，pre.1 是发布收口**：pre.1 不新增功能，只修缺陷、补文档与闸门。
- **dev.8 是 pre.1 后的插队功能**：新增 `gen_agent_skills` 与项目模板；完成后必须 pre.2，不沿用 pre.1 的发布门槛结论。
- **dev.9 紧随 dev.8**：新增编译器/连接器诊断输出策略，默认保持旧行为。
- **dev.10 补齐工具链/缓存/网络/安装前缀**：全部 opt-in，默认保持旧行为。
- **dev.11 首次运行生成注释模板**：只写用户目录、仅当无配置且未禁用；不改变解析结果；dev.8~dev.11 一起进入 pre.2。
- **第 10 项（git clone 深度）不纳入**（用户裁定 2026-10-24）。

## 定位

- **1.5.0 是 1.4.11 之后的第一个功能 minor**：新增「用户/全局配置层」这一公共契约，而不是继续压在 1.4.x 补丁线。
- **默认零变化**：未创建任何 `config.toml` 时，路径与行为与 1.4.11 完全一致；所有新能力 opt-in。
- **公共契约**：两个环境变量与 `[base_dir]` / `[settings]` 键名一经发布即稳定，新增键只能追加。
- **pre.1 的职责是发布前加固**：47 项审查 + dev.6 遗留 R-09/R-12/R-13/R-16/R-17/R-18 全部裁定，P1 零残留后才进入正式发布。
- **dev.8 的职责是新增 opt-in 能力**：默认 false，未配置时 `project new` 逐字节不变；生成物只落在新项目内。

## 跨版本关注点

- 正式版聚合计划 `1.5.0`、CHANGES 聚合、版本号与三渠道分发待 dev.8 + dev.9 + dev.10 + dev.11 + pre.2 完成后创建/执行。
- 回归基线：pre.1 **1219 用例 / 6979 断言 / 8 跳过 / 0 失败**；dev.8/pre.2 不得低于该基线。
- 本系列暂不影响 2.0.0 的破坏性变更窗口；pre.1 不新增 deprecation。
