# EazyMake 0.9.4 执行计划 — 文档与质量完善

> 来源: [`plans/0.9.4.md`](plans/0.9.4.md)（详细设计）、[`plans/README.md`](plans/README.md)（版本路线图）。

---

## 版本状态

```
0.9.0 (done) → 0.9.1 (done) → 0.9.2 (done) → 0.9.3 (done) → 0.9.4 (current)
  发布正式版      默认仓库创建      文档多语言          捆绑包迁移          文档与质量完善
  → 0.9.5 (next) → 0.9.6
    跨平台与测试      功能补全
```

---

## 一、目标

0.9.3 完成了捆绑包迁移，项目功能层面已基本稳定。本版本聚焦**文档补全**与**代码质量打磨**，不新增核心功能。

五项交付物：

| # | 交付物 | 说明 |
|---|--------|------|
| 1 | FAQ / 故障排除文档 | `docs/en/faq.md` + `docs/zh/faq.md`，覆盖安装/构建/包管理/配置/跨平台常见问题 |
| 2 | CHANGES.md 补全 | 从 0.9.0 补到 0.9.4，补全所有中间版本的变更记录 |
| 3 | 错误信息打磨 | 修复空异常消息、增加 "did you mean" 模糊匹配建议、审计裸 `std::runtime_error` |
| 4 | Lua API 版本化 | 添加 `EZMK_LUA_API_VERSION` 宏 + `ezmk.api_version` Lua 字段 + 向后兼容策略文档 |
| 5 | 离线场景文档 | 在 FAQ 或 pkg.md 中补充完全离线 / 手动下载 / 预置镜像三种方案 |

---

## 二、当前状态分析

### 2.1 已有文档

```
docs/
├── en/
│   ├── cli.md          ✅ CLI 参考
│   ├── config_file.md  ✅ 配置文件规范
│   ├── pkg.md          ✅ 包管理
│   ├── repo.md         ✅ 仓库管理
│   ├── utils.md        ✅ Lua 插件 API
│   ├── @cache.md       ✅ 缓存机制
│   ├── @safety.md      ✅ 安全模型
│   └── tutorial/       ✅ 上手教程
├── zh/
│   └── ...             ✅ 对应中文版
└── glossary.md         ✅ 术语表
```

**缺失**：`faq.md`（集中排错入口）、离线使用指南。

### 2.2 CHANGES.md 现状

当前仅覆盖到 **0.2.6**，缺失 0.9.0 ~ 0.9.4 共 5 个版本的条目。

### 2.3 代码质量问题

| 位置 | 问题 | 严重程度 |
|------|------|----------|
| `src/cli.cpp:65` | `throw std::invalid_argument("")` — 空消息 | 中 |
| `src/cli.cpp` profile 解析 | 未知 profile 无 "did you mean" 建议 | 低 |
| `src/cli.cpp` 命令解析 | 未知子命令无模糊匹配建议 | 低 |
| `src/` 多处 | 裸 `std::runtime_error` 未 i18n 化 | 低 |

---

## 三、详细执行步骤

### 阶段一：FAQ / 故障排除文档

**目标文件**: `docs/en/faq.md` + `docs/zh/faq.md`，各 ≥20 条 FAQ。

#### 内容结构

```
# FAQ / Troubleshooting

## 安装问题 (Installation)
  - MSYS2 环境配置（Windows）
  - 权限错误（Permission denied）
  - 网络问题（install.sh 下载失败）

## 构建问题 (Build)
  - "fatal error: xxx.h: No such file or directory"
  - "undefined reference to ..."（链接错误）
  - 缓存损坏 → 清理 .ezmk/cache/
  - "ezmk.toml not found" → 检查工作目录
  - 编译标志不生效 → 检查 profile 覆盖规则

## 包管理问题 (Package Management)
  - "package not found" → 检查 repo 列表 → 检查网络 → 手动安装
  - SHA-256 校验失败 → 清理缓存重试
  - 依赖链断裂 → 手动安装缺失依赖
  - 全局安装权限不足 → 使用 -u 作用域

## 配置问题 (Configuration)
  - TOML 语法错误 → 检查字段名拼写、引号、表嵌套
  - include_dirs / src_dirs 路径格式（相对路径 vs 绝对路径）
  - profile 不生效 → 确认 --profile 参数是否正确传递

## 跨平台问题 (Cross-platform)
  - Windows 路径分隔符（反斜杠 vs 正斜杠）
  - MSVC vs GCC 编译标志差异
  - Linux/macOS 下找不到 libwinhttp → 去掉 -lwinhttp
```

#### 每条 FAQ 格式

```markdown
### Q: 构建时报 "fatal error: xxx.h: No such file or directory"

**原因**: 编译器在 include_dirs 指定的路径中找不到头文件。

**解决**:
1. 检查 `ezmk.toml` 中 `[compile]` → `include_dirs` 是否包含头文件所在目录
2. 检查头文件路径拼写是否正确（区分大小写）
3. 如果头文件来自依赖包，确认已运行 `ezmk pkg install <name>`
4. 检查 `.ezmk/cache/` 是否损坏，尝试 `rm -rf .ezmk/cache/` 后重新构建
```

#### 执行清单

- [ ] 从现有文档（`@cache.md`、`cli.md`、`pkg.md`）中收集已有排错提示
- [ ] 从 `temp.md` 用户期待分析中提取高频痛点
- [ ] 编写 `docs/en/faq.md`（≥20 条 FAQ）
- [ ] 翻译 + 本地化为 `docs/zh/faq.md`
- [ ] 在 `README.md` / `README_ZH.md` 中增加 FAQ 链接
- [ ] 在 `docs/en/faq.md` 中添加离线场景章节（见阶段五）

---

### 阶段二：CHANGES.md 补全

**目标文件**: `CHANGES.md`（仓库根目录）

#### 待补全条目

| 版本 | 关键变更 | 来源 |
|------|----------|------|
| **0.9.0** | 一键安装脚本、文档整理（`docs/cli.md`、安全性集中化、README 双语互链）、上手教程 | `plans/0.9.0.md` |
| **0.9.1** | 默认仓库创建（`ezmk-repo`）、预注册策略（`install.sh` 自动注册）、初始包与贡献流程 | `plans/0.9.1.md` |
| **0.9.2** | `docs/` + `tutorial/` 拆分为 `en/` + `zh/`、术语表（`glossary.md`）、CI 文件对应检查 | `plans/0.9.2.md` |
| **0.9.3** | 7 个捆绑预编译包迁移至官方仓库（catch2/fmt/lua/nlohmann_json/spdlog/sqlite3/tinyxml2）、清理 `install.sh` 捆绑包拷贝逻辑 | `plans/0.9.3.md` |
| **0.9.4** | FAQ/故障排除文档、CHANGES.md 补全、错误信息打磨（模糊匹配建议 + i18n 审计）、Lua API 版本化（`EZMK_LUA_API_VERSION` + `ezmk.api_version`）、离线场景文档 | `plans/0.9.4.md` |

#### 执行清单

- [ ] 阅读 `plans/0.9.0.md` ~ `plans/0.9.4.md` 的"目标"和"关键交付"章节
- [ ] 阅读各版本 git log，补充 plans 中未记录的细节
- [ ] 按版本倒序（最新在前）写入 `CHANGES.md`
- [ ] 保持与已有 0.2.6 及之前条目的格式一致

---

### 阶段三：错误信息打磨

#### 3.1 修复空异常消息

**文件**: `src/cli.cpp:65`

当前代码（预期）：
```cpp
throw std::invalid_argument("");  // 空消息，用户看不到任何有用信息
```

修改为 i18n 化错误：
```cpp
throw std::invalid_argument(i18n::get(I18N_KEY_CLI_INVALID_ARGUMENT));
```

需要在 `i18n_keys.def` 中新增 key，并在 `locale/en.json` + `locale/zh.json` 中添加对应字符串。

#### 3.2 实现 `closest_match()` 模糊匹配

**文件**: `include/ezmk/util.hpp` + `src/util.cpp`

```cpp
// 返回 candidates 中与 input 编辑距离 ≤ max_distance 的候选项列表
// 按编辑距离升序排列；无匹配时返回空 vector
std::vector<std::string> closest_match(
    const std::string& input,
    const std::vector<std::string>& candidates,
    int max_distance = 2
);
```

使用标准 Levenshtein 距离算法。仅在候选集较小时（≤50）遍历；大候选集可考虑优化但 0.9.4 暂不需要。

#### 3.3 未知 profile — "did you mean" 建议

**文件**: `src/cli.cpp`（profile 解析逻辑）

当前行为：`--profile debuug` → 报错 "unknown profile: debuug. Available: debug, release"

改进后：
```
Error: unknown profile 'debuug'. Did you mean: debug?
Available profiles: debug, release
```

实现：
1. 收集所有已定义的 profile 名称
2. `closest_match(input, candidates)` → 如果非空，追加 "Did you mean: ...?"
3. 始终列出所有可用 profile

#### 3.4 未知命令 — "did you mean" 建议

**文件**: `src/cli.cpp`（命令解析逻辑）

当前行为：`ezmk projcet build` → "unknown command: projcet"

改进后：
```
Error: unknown command 'projcet'. Did you mean: project?
Available commands: project, pkg, repo, utils, help, version
```

实现：
1. 收集所有顶级命令名（含简写展开后的规范名）
2. `closest_match(input, candidates)` → 如果非空，追加建议
3. 列出可用命令

#### 3.5 审计裸 `std::runtime_error`

**文件**: `src/` 下所有 `.cpp` 文件

执行：
- [ ] `grep -n "throw std::runtime_error" src/*.cpp` 列出所有位置
- [ ] 逐条评估：错误信息是否需要 i18n 化？（面向用户的 → 是；内部断言/编程错误 → 否）
- [ ] 对需要 i18n 的位置：新增 i18n key → 替换为 `i18n::get(KEY)`
- [ ] 同步更新 `locale/en.json` + `locale/zh.json`

#### 执行清单

- [ ] 修复 `cli.cpp:65` 空异常消息 → i18n key
- [ ] 实现 `util::closest_match()` + 单元测试
- [ ] 未知 profile → 增加 "did you mean" 建议
- [ ] 未知命令 → 增加 "did you mean" 建议
- [ ] `grep` 审计所有 `throw std::runtime_error`
- [ ] 需要 i18n 化的替换为 i18n key
- [ ] 更新 `locale/en.json` + `locale/zh.json`
- [ ] 测试：新错误消息在 en/zh 语言下正确显示

---

### 阶段四：Lua API 版本化

#### 4.1 C++ 侧

**文件**: `include/ezmk/lua_api.hpp`

```cpp
#define EZMK_LUA_API_VERSION 1
```

**文件**: `src/lua_api.cpp` — `register_api()` 函数中注册：

```cpp
lua_pushinteger(L, EZMK_LUA_API_VERSION);
lua_setfield(L, -2, "api_version");
```

使得 Lua 脚本可通过 `ezmk.api_version` 读取版本号。

#### 4.2 向后兼容策略（文档）

**文件**: `docs/en/utils.md` + `docs/zh/utils.md`

在 API 参考章节末尾新增"API 版本化"小节：

```markdown
## API Versioning

`ezmk.api_version` is an integer that increments only on **backward-incompatible** changes.

| Change type | Version bump? |
|---|---|
| Add new function | No (backward compatible) |
| Deprecate function (`@deprecated since v<N>`) | No, removed after ≥2 minor versions |
| Remove deprecated function | **Yes** |
| Change function signature | **Yes** |
| Change function behavior (semantics) | **Yes** |

Scripts can guard against version differences:
```lua
if ezmk.api_version >= 2 then
    -- use new API
else
    -- fallback
end
```
```

#### 执行清单

- [ ] 在 `include/ezmk/lua_api.hpp` 添加 `#define EZMK_LUA_API_VERSION 1`
- [ ] 在 `src/lua_api.cpp` `register_api()` 中注册 `ezmk.api_version = 1`
- [ ] 在 `docs/en/utils.md` 添加 API 版本化章节
- [ ] 在 `docs/zh/utils.md` 添加对应中文章节
- [ ] 测试：Lua 脚本中 `print(ezmk.api_version)` 输出 `1`
- [ ] 确认不修改任何现有 API 行为（本版本仅添加基础设施）

---

### 阶段五：离线场景文档

在 `docs/en/faq.md` 和 `docs/zh/faq.md` 中设置独立章节，或在 `docs/en/pkg.md` 末尾补充。

#### 三种离线方案

```markdown
## Offline / Air-gapped Usage

### 方案 1: 本地仓库镜像
ezmk repo add /path/to/local/ezmk-repo --type local

### 方案 2: 手动下载归档
1. 从 GitHub Release 下载 <pkg>-<version>.tar.gz
2. ezmk pkg install ./<pkg>-<version>.tar.gz --type file

### 方案 3: 预置镜像（USB / 内网共享）
1. 在有网机器上 git clone https://github.com/3667808244/ezmk-repo.git
2. 复制到 USB 或内网共享目录
3. 离线机器上 ezmk repo add <path> --type local
```

#### 执行清单

- [ ] 在 `docs/en/faq.md` 中添加离线场景 FAQ（≥3 条）
- [ ] 在 `docs/zh/faq.md` 中添加对应内容
- [ ] 在 `docs/en/pkg.md` 末尾补充离线安装章节（交叉引用 FAQ）
- [ ] 在 `docs/zh/pkg.md` 末尾补充对应内容

---

## 四、校验清单

### 编译与测试

- [ ] `bash build.sh` 编译通过（MSYS2）
- [ ] 全量测试通过：`build/test_ezmk`（0 failures）
- [ ] 新增 `closest_match()` 单元测试通过

### 文档校验

- [ ] FAQ 中每条排错流程在真实环境中验证（至少抽查 5 条）
- [ ] `docs/en/` ↔ `docs/zh/` 文件一一对应（延续 0.9.2 规范）
- [ ] 新增文档无 broken links（内部交叉引用有效）
- [ ] `README.md` / `README_ZH.md` 中 FAQ 链接可点击

### Lua API 校验

- [ ] `ezmk.api_version` 在 Lua 脚本中可读取（值为 `1`）
- [ ] 现有 Lua 脚本（`ezmk-cc`）不受影响

### 错误信息校验

- [ ] 英文环境下未知命令建议格式正确
- [ ] 中文环境下未知命令建议格式正确
- [ ] 未知 profile 建议格式正确
- [ ] 空异常消息已修复

---

## 五、兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `ezmk.api_version = 1` | 旧脚本未使用该字段 | 纯增量，不影响现有脚本 |
| 错误信息文本变化 | 依赖错误消息解析的脚本 | 概率极低；错误消息解析不是稳定接口 |
| `closest_match()` 建议 | 未知命令/profile 输出更友好 | 纯增量 |
| 裸 `std::runtime_error` → i18n | 错误消息语言随 locale 切换 | 向后兼容（英文 fallback） |
| FAQ 文档 | 无 | 新增 |
| CHANGES.md 补全 | 无 | 纯文档 |

---

## 六、开放问题

1. **FAQ 数量** — plans 要求 ≥20 条。需从现有文档和 `temp.md` 中提取足够素材。
2. **`closest_match()` 候选集大小** — 命令名 + profile 名合计 ≤30，O(n·m²) 的 Levenshtein 足够。
3. **裸 `std::runtime_error` 审计范围** — 先 `grep` 统计数量，再决定是否全部 i18n 化或仅修复面向用户的错误。
4. **离线文档位置** — FAQ 中一条 + pkg.md 末尾详细章节，两个位置交叉引用。

---

## 七、跨版本关注点

- **向后兼容**: 所有变更为纯增量（新增字段、新增建议），不改变已有行为。
- **i18n**: 新增错误信息需同时提供 en/zh 翻译；延续 `i18n_keys.def` → `en.json` + `zh.json` 的 X-macro 单源真值流程。
- **文档一致性**: 0.9.2 建立的 `docs/en/` ↔ `docs/zh/` 一一对应关系需持续维护；新增 faq.md 必须双语同步交付。
- **依赖**: 本版本独立，不依赖 0.9.5/0.9.6 的任何前置工作。
- **0.9.5 准备**: 本版本完成后进入 0.9.5（跨平台与测试），届时可能需要跨平台验证 FAQ 中的排错流程。
