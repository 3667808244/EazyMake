# EazyMake 1.4.11 执行计划

> **状态：🚀 发布中（2026-10-17）** —— 版本已定稿并打 tag `v1.4.11`；全量 **1169 用例 / 6788 断言 / 4 跳过 / 0 失败**；pacman 已出包验证，GitHub Release 与 Homebrew/winget 回填进行中。本文档为可执行清单；详细设计见 [plans/1.4.x/1.4.11.md](plans/1.4.x/1.4.11.md)，系列索引见 [plans/1.4.x/README.md](plans/1.4.x/README.md)。
>
> **版本主题**：代码质量审计修复（第四轮）—— 47 项（P1 4 / P2 20 / P3 23），零功能新增、公共 API 无破坏性变更。

## 1 背景

第四轮审计在 1.4.10 发布态上展开，覆盖 util / build+cache / pkg+repo / lua+i18n / cli+main+project / config+lockfile / toolchain+export+import / watcher+workspace+version 八个域。缺陷集中在错误路径、不可信输入、跨平台分支、跨模块契约与静默失败；Linux 侧验证另发现并修复了 4 个可移植性问题（重载歧义、i18n 静态初始化顺序、import 平台断言、含空格路径的 `cd` 引号）。

## 2 目标

| 组 | 覆盖 | 状态 |
|----|------|------|
| 命令执行安全 | Q4-01~06 | ✅ |
| 数据完整性与错误路径 | Q4-07~10 | ✅ |
| 增量缓存与构建正确性 | Q4-11~14 | ✅ |
| 包 / 仓库 / 锁文件 | Q4-15~20 | ✅ |
| Lua 与 i18n | Q4-21~26 | ✅ |
| 配置解析 | Q4-27~30 | ✅ |
| import / export / workspace / project / CLI | Q4-31~43 | ✅（Q4-40 文档化、Q4-42 低危延后） |
| 归档 / 下载 / 工具函数 | Q4-44~47 | ✅ |

## 3 执行阶段

### 阶段一：命令执行与进程安全（Q4-01~06）
- [x] POSIX `run_command` 预构造 envp / `execve`
- [x] Lua run 权限逐段校验（高级 shell 构造拒绝）
- [x] 测试运行器命令平台正确转义
- [x] `project new` 的 `git init` Windows 引号
- [x] `build_editor_command` Windows 引号
- [x] `$CXX`/`$CC` 探测路径引号化
- [x] 回归：全量零失败

### 阶段二：数据完整性与错误路径（Q4-07~10）
- [x] 回退 copy 失败保留临时文件并报错（复用 `atomic_rename`）
- [x] `record.json` / `compile_commands.json` 回退后清理 `.tmp`
- [x] `ThreadPool` spawn 失败可恢复
- [x] `repo remove` 先持久化注册表
- [x] 回归：全量零失败

### 阶段三：增量缓存与构建正确性（Q4-11~14）
- [x] test 缓存随编译器版本 / deterministic 失效
- [x] executable 入口接受 `main.cc`/`main.cxx`
- [x] 单源失败保留其余缓存更新
- [x] `collect_test_sources` 按路径去重
- [x] 回归：全量零失败

### 阶段四：包 / 仓库 / 锁文件（Q4-15~20）
- [x] `pkg remove` 剪除 lockfile
- [x] 跨仓库版本选择用 `compare_version_precedence`
- [x] precompiled 包 lockfile 内容固定
- [x] `compare_version` 分量饱和
- [x] `direct_dep_specs` 与 `parse_depends_entry` 互逆
- [x] 未找到 / 未知命令非零退出
- [x] 回归：全量零失败

### 阶段五：Lua 与 i18n（Q4-21~26）
- [x] `lua_pushlstring` 保留内嵌 NUL
- [x] `json_to_lua` 深度上限
- [x] 权限 glob 允许项锚定
- [x] `lua_setupvalue` 返回值检查
- [x] `ezmk_lua_main` 异常处理
- [x] i18n 变体独立数据 + overlay 不半写
- [x] 回归：全量零失败

### 阶段六：配置解析（Q4-27~30）
- [x] 语言范围界序号比较
- [x] `resolve_link_path` 链式 / 循环
- [x] 已知键类型错误 fail-fast
- [x] `normalized_lang` 含默认版本
- [x] 回归：全量零失败

### 阶段七：import / export / workspace / project / CLI（Q4-31~43）
- [x] import 条件链首个为真分支
- [x] import 平台常量按宿主
- [x] import 关键字集补齐
- [x] export `-std=c++2a/2b` 映射 + stoi 兜底
- [x] export CMake 路径引号化
- [x] export 检查 `file_write`
- [x] workspace test 无测试成员依赖仍构建
- [x] 重复成员拒绝
- [x] workspace test 绝对 `--report` 拒绝
- [x] workspace watch 输出——已知限制（文档化）
- [x] `project new` 名称校验
- [x] `project new` / `example` 失败回滚——低危延后
- [x] `project export --glob` 生效
- [x] 回归：全量零失败

### 阶段八：归档 / 下载 / 工具函数与收口（Q4-44~47）
- [x] `resolve_relative_path_flags` 精确前缀
- [x] Windows 下载拆分 host/port
- [x] `safe_extract_path` 接受 `.`/尾分隔符
- [x] `extract_targz` 校验 gzip CRC32/ISIZE
- [x] i18n 430 键三向一致（新增 12 个 fail-fast 键）
- [x] 全量 `bash build.sh test-all`：**1169 / 6788 / 4 / 0**
- [x] `check_man_sync.py` + groff 零告警 + `check_docs_sync.sh`
- [x] CHANGES / 计划 / 索引收口
- [x] 发布门槛复核（⛔ ①②③④⑤）

### 发布阶段：1.4.11
- [x] 版本号置为 `1.4.11`（`build.sh` fallback）
- [x] 全量回归零失败（1169 / 6788 / 4 / 0）
- [x] `git tag v1.4.11` 并推送（指向 `ef2b0dd`）
- [ ] GitHub Release published + 资产 digest 核对
- [x] pacman：`makepkg -fd` 出包 `eazymake 1.4.11-1`（sha256 `a06f8892…`，含 4 页 man + `_ezmk`，`ezmk version` → 1.4.11）
- [ ] Homebrew 公式回填（tap + 仓库副本，真实 digest）
- [ ] winget 三件套 + PR 提交

## 4 关键设计决策

1. **Q4-02**：权限激活时按 `;`/`&&`/`||`/`|`/`&` 逐段校验；含 `$`、反引号、重定向、子 shell、换行的命令直接拒绝。
2. **Q4-23**：相对 glob 允许项锚定项目根。
3. **Q4-28**：实现链式 link 解析 + 循环检测。
4. **Q4-37**：无测试成员仍构建、仅跳过测试（计为 skipped）。
5. **Q4-38**：重复成员为硬错误。
6. **Q4-40**：`workspace watch` 输出缓冲收口为已知限制。
7. **Q4-42**：`project new`/`example` 失败回滚为 P3 低危延后。
8. **行为收紧**：未知命令 / `pkg remove/update` / `repo remove/update/info` 未找到目标退出非零；损坏 gzip、超深 JSON、含 shell 构造的受限 `ezmk.run` 明确报错。

## 5 兼容性矩阵

| 变更 | 影响 | 说明 |
|------|------|------|
| 默认行为 | **零变化** | 未使用新开关 / 未触发错误路径时与 1.4.10 一致 |
| 退出码收紧 | 行为收紧 | 未找到目标 / 未知命令返回非零 |
| run 权限分段校验 | 行为收紧 | 含 shell 元字符的受限 `ezmk.run` 被拒 |
| 归档 / JSON 校验收紧 | 行为收紧 | 损坏输入明确报错 |
| i18n | 新增 12 键 | 418 → 430，三向一致 |
| 公共 API | **无破坏性变更** | 仅内部签名与错误行为修正 |

## 6 延后项

- `workspace watch` 流式输出（已知限制）。
- `project new` / `example` 失败回滚（P3）。
- 颜色深度引擎 / `ezmk config get/set`（属 1.4.12+）。
- 破坏性变更与弃用面移除（归 2.0.0，见 [plans/2.0.x/REMOVALS.md](plans/2.0.x/REMOVALS.md)）。
