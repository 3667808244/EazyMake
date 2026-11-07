# EazyMake 1.5.0-dev.8 执行计划

> **状态：✅ 实现收口（2026-11-07；CI run `42347726309` 全绿），待 pre.2 发布复核** —— 详细设计见 [plans/1.5.x/1.5.0-dev.8.md](plans/1.5.x/1.5.0-dev.8.md)；pre.1 代码 CI 已绿（run 41212564762），本版完成后需追加 pre.2。
>
> **回归**：单元 **1103 / 6265 / 4 跳过 / 0 失败**；全量 `test-all`（`EZMK_TEST_OFFLINE=1`）**1224 / 7046 / 8 跳过 / 0 失败**。
>
> **目标**：新增 `[settings].gen_agent_skills`（bool | string），把 4 个 `ezmk-user-*` 技能分发到 `.claude/skills` / `.github/skills` / `.agents/skills`；同时让 `AGENTS.md` 在两者同开时列出技能名称、说明与实际目录。

## 1 背景

pre.1 已有 `gen_agent_md`，但只生成一个最小 AGENTS.md。本版把仓库 `.claude/skills/ezmk-user-*` 的 4 个技能嵌入二进制，并按常见 agent 目录分发；安装后的 `ezmk` 不依赖源码树。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 配置：bool/string、默认 false、别名/逗号/大小写、未知值 fail-fast | P0 |
| 2 | 嵌入：`embed_skills.py` 生成 4 个技能表（name/description/content） | P0 |
| 3 | 分发：`project new` 写三个 profile 目录，byte-for-byte 复制 SKILL.md | P0 |
| 4 | AGENTS.md：同开时追加技能描述与实际目录；单独 gen_agent_md 输出不变 | P0 |
| 5 | 文档 / man / CHANGES / 索引 / 测试 / 全量回归 | P0 |

## 3 执行阶段

### 阶段一：配置层（P0）
- [x] `SettingsConfig.gen_agent_skills`（归一化字符串，空 = 关闭）
- [x] bool/string 读取、`claude` / `copilot` / `agents` / `all`、别名、逗号、大小写
- [x] 未知 token / 非 bool/string 类型 fail-fast
- [x] 单测：缺省 / true / false / 空串 / 各 profile / 合并覆盖
- [x] 回归：单元零失败

### 阶段二：技能嵌入（P0）
- [x] 新增 `scripts/embed_skills.py`，只读 `ezmk-user-*`，校验 name/description
- [x] 生成 `src/skill_data.cpp` 与 `include/ezmk/skill_data.hpp`
- [x] `build.sh` 接入；无 Python 降级空表；`.gitignore` / TEST_SRC 同步
- [x] 嵌入测试：4 项、唯一、description 非空、frontmatter、确定性
- [x] 回归：单元零失败

### 阶段三：项目生成（P0）
- [x] `project.cpp` 写 `.claude/skills` / `.github/skills` / `.agents/skills`
- [x] 已存在文件跳过；写入失败 fatal；`debug` 汇总
- [x] `gen_agent_md` 同开时追加 `## Agent skills`（动态名称/说明/实际目录）
- [x] 单测：true / claude / copilot / agents / false、4 技能 byte 相等、AGENTS 组合
- [x] 集成：`--config` 驱动 `project new` 检查
- [x] 回归：单元零失败

### 阶段四：文档 / 闸门 / pre.2（P0）
- [x] user/global config、man5、CHANGES、README 可选说明
- [x] `check_i18n` / `check_man_sync` / `check_docs_sync` / `check_config_docs` / groff
- [x] 更新 `plans/1.5.x/README.md`、`plans/README.md`
- [x] 全量 `bash build.sh test-all` 零失败（基线 1219/6979）
- [x] CI 绿色（run `42347726309`）
- [ ] 发布门槛复核后追加 pre.2

## 4 关键设计决策

1. **只分发 `ezmk-user-*`**：4 个面向用户项目的技能，开发/发布技能不进入新项目。
2. **构建期嵌入**：安装后的二进制不读源码树；技能内容与源文件 byte-for-byte。
3. **bool `true` = `all`**：写三个目录；string 用于精简到单个/部分 profile。
4. **AGENTS.md 只在同开时追加**：保持 `gen_agent_md` 单独使用时的旧输出；描述来自 frontmatter。
5. **不做已有项目补发**：只作用于 `project new`，避免在用户仓库突然写文件。

## 5 兼容性

| 变更 | 影响 | 说明 |
|------|------|------|
| 新增 `[settings].gen_agent_skills` | 纯 opt-in | 默认 false；未配置时 project new 逐字节不变 |
| 分发 4 个技能 | 仅当开启 | 新项目目录原本不存在；不写用户全局目录 |
| `gen_agent_md` | 同开时追加 | 单独使用逐字节不变 |
| 公共 API | 无破坏性变更 | 只新增配置键、嵌入表与项目文件 |

## 6 后续

- 完成后追加 **1.5.0-pre.2**，重跑发布门槛；不沿用 pre.1 结论。
- 正式版聚合、版本号、tag 与三渠道分发待 dev.8 + pre.2 完成后执行。
