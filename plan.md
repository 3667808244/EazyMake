# EazyMake 1.4.10 执行计划

> **状态：🚧 已立项（2026-10-13，语法 A 冻结），未开始实现** —— 本文档把设计文档 §4 转成可勾选的分阶段清单。1.4.x 系列路线图见 [plans/1.4.x/README.md](plans/1.4.x/README.md)；2.0.0 的移除清单见 [plans/2.0.x/REMOVALS.md](plans/2.0.x/REMOVALS.md)。
>
> 详细设计：[**1.4.10.md**](plans/1.4.x/1.4.10.md)。主题：**可选递归源文件查找（语法 A：条目后缀 `/**`）**。
>
> **范围边界**：只新增一个**可选**的目录级递归源发现语法（`[compile].src_dirs` / `[test].dirs` 条目以 `/**` 结尾即递归），**默认行为零变化**；不引入 glob 表达式语言 / 构建图；Linux/macOS 文件监视真递归不在本版。
>
> **⛔ 发布门槛**：① 阶段清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试**无新增失败**（**立项基线待 `bash build.sh test-all` 实测回填**；参考值：1.4.9 发布态 1137 用例 / 6622 断言 / 4 跳过 / 0 失败；用例 / 断言数只增不减）；④ 附加门槛：`python scripts/check_man_sync.py` 通过、`groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警、i18n 三向一致、`bash scripts/check_docs_sync.sh` 通过；⑤ `ci.yml` 在 push 上全绿。

---

## 1 背景

`util::list_files()`（`src/util.cpp:359-377`）用 `fs::directory_iterator`（`:363`）——只遍历目录自身，所以 `[compile].src_dirs` / `[test].dirs` 的子目录源文件从不参与编译，除非把子目录逐个列出。而 `project export cmake`（`src/export.cpp:156` / `:175`）与 `project pack`（`src/build.cpp:1800-1823`）**本来就递归**，导出 / 打包与 `ezmk build` 的源集合长期分叉；`collect_sources` 按文件名去重（`src/build.cpp:163-168`）又让「逐目录列出」在嵌套同名文件上踩坑。用户裁定：新增一个**可选**的目录级递归语法（**候选 A**），默认行为零变化。详见设计 §1。

## 2 目标

| 组 | # | 覆盖 | 优先级 |
|----|---|------|--------|
| 文档 | R-01 | 用户文档写明「非递归」（config_file / FAQ / glossary / tutorial，zh+en）+ man —— 已完成 | P0 |
| 发现 | R-02 | `util::list_files` 增递归变体：跳过规则、符号链接不跟随、结果排序 | P0 |
| 配置 | R-03 | 条目尾部 `/**` 标记的解析与剥离；非法形态按字面目录名处理 | P0 |
| 构建 | R-04 | `collect_sources` 支持递归条目；递归模式按项目根相对路径去重 | P0 |
| 对齐 | R-05 | test / pkg / utils 门控 / Lua `list_sources` / workspace 探测同源 | P0 |
| 互操作 | R-06 | export 的 `GLOB` / `GLOB_RECURSE` 按标记分派；pack 与 watch 的差异写明 | P1 |
| 测试 | R-07 | 单元（发现 / 跳过 / 排序 / 去重）+ 集成（嵌套项目 build / test / 包 / `compile_commands`） | P0 |
| 收口 | R-08 | 配置文档 + FAQ + `CHANGES.md` + i18n 三向 + 索引状态 | P0 |

## 3 执行阶段

### 阶段零：文档澄清 + 立项（R-01）

- [x] 用户文档写明「非递归」：`docs/{zh,en}/config_file.md` 的 `src_dirs` + `[test].dirs`、`docs/{zh,en}/faq.md`、`docs/{zh,en}/glossary.md`、`tutorial/{zh,en}/basic/03-config.md` + `tutorial/{zh,en}/dev/03-test.md`、`man/ezmk.toml.5`
- [x] 语法拍板（2026-10-13）：**候选 A（条目后缀 `/**`）**；范围 = `[compile].src_dirs` + `[test].dirs`
- [x] 立项：根 `plan.md` 生成 + 索引登记（`plans/1.4.x/README.md` / `plans/README.md`）
- [ ] 记录立项基线：`bash build.sh test-all`（纯文档 + 规划阶段，预期与 1.4.9 发布态一致；实测后回填用例 / 断言数）

### 阶段一：发现原语（R-02）

- [ ] `util::list_files` 递归变体（**旧签名不变**）：跳过 `.` 前缀目录与 `build/`、不跟随目录符号链接、结果按路径排序
- [ ] 单测：嵌套文件被收录 / 隐藏与 `build` 被跳过 / 排序稳定 / 符号链接不成环 / 缺失目录 warn
- [ ] 回归：全量零失败

### 阶段二：配置解析 + 项目构建（R-03 / R-04）

- [ ] `[compile].src_dirs` 识别并剥离尾部 `/**`；`[test].dirs` 同步；非 glob，非法形态按字面目录名处理
- [ ] `collect_sources` 按条目标记分派；递归模式按项目根相对路径去重（flat 模式语义不变）
- [ ] 单测：解析（递归 / flat / 混合 / 边界形态）、递归收集、嵌套同名文件都参与编译、flat 行为不变
- [ ] 回归：全量零失败

### 阶段三：全路径对齐（R-05）

- [ ] test / 包编译 / utils 门控 / Lua `ezmk.list_sources()` / workspace 测试探测统一走递归感知 helper
- [ ] 集成：嵌套源的 `build` + `test` + `compile_commands`；嵌套源的包安装
- [ ] 回归：全量零失败

### 阶段四：互操作（R-06）

- [ ] export：flat 条目 `GLOB`、递归条目 `GLOB_RECURSE`（`--glob` / `--no-glob` 两条分支）
- [ ] 文档写明 pack「总是递归收录」与 watch 的 POSIX 非递归限制
- [ ] 集成：`export cmake` 的文件集合与 `ezmk build` 一致
- [ ] 回归：全量零失败

### 阶段五：测试 / 文档 / 收口（R-07 / R-08）

- [ ] 配置文档（递归语法 + 跳过规则 + 与 flat 差异）、FAQ、`CHANGES.md`、man（如涉及）
- [ ] i18n 三向一致（若有新键）
- [ ] 索引：`plans/1.4.x/README.md` / `plans/README.md` / 本文件
- [ ] 门槛复核：全量零回归 + `check_man_sync.py` + groff 零告警 + i18n 三向 + `check_docs_sync.sh`

### 发布阶段：1.4.10（正式发布时）

- [ ] 版本号置为 `1.4.10`（`build.sh` 的 `EZMK_VERSION` fallback；`include/ezmk/version.hpp` 由构建生成）
- [ ] `git tag v1.4.10` → Release；核对 7 资产（`macos-x64` skipped）
- [ ] 三渠道（Homebrew / pacman / winget）随发布跟进（加载 `ezmk-publish` skill）

## 4 关键设计决策

1. **语法 A 冻结（2026-10-13）**：条目以 `/**` 结尾 = 递归该目录整棵子树；其余条目保持 flat；同一列表可混用；`[compile].src_dirs` 与 `[test].dirs` 同一语法。
2. **不是 glob**：`/**` 只是目录级递归开关；`src/**/*.cpp` 不解析，按字面目录名处理（缺失时 warn），避免引入 glob 引擎与排除语法。单层 `/*` 不在本版。
3. **默认零变化**：不写 `/**` 的项目，扫描集合与 flat 去重（按文件名）逐字节不变。
4. **递归模式按相对路径去重**：嵌套同名文件（`src/a/util.cpp` 与 `src/b/util.cpp`）都参与编译——这是递归的意义；与 flat 的可观察差异写入文档。
5. **跳过规则固定**：`.` 前缀目录 + `build/`；`recursive_directory_iterator` 不跟随目录符号链接（Windows junction 实现阶段实测确认）。
6. **确定性**：递归结果按路径排序（承接 1.4.6 Q-19 对 `directory_iterator` 无序的收口）。
7. **单一 helper**：build / test / package / utils 门控 / Lua `list_sources` / workspace 探测 / `compile_commands` 同源，避免再次分叉。
8. **export 按标记分派** `GLOB` / `GLOB_RECURSE`，消除导出与原生构建的源集合分叉。
9. **POSIX watch 不在本版**：Linux/macOS 只监视目录自身（`include/ezmk/file_watcher.hpp` 已文档化），递归目录新增的嵌套文件不触发重建——文档注明，不承诺修复。
10. **契约不破**：`util::list_files` 旧签名保留；`collect_sources` 默认路径语义不变；无弃用面放宽或移除。

## 5 兼容性矩阵

| 变更 | 影响 | 说明 |
|------|------|------|
| 默认行为 | 零变化 | 不写 `/**` 时扫描与去重语义与今天一致 |
| `/**` 递归标记 | 纯新增（配置面） | 旧版本（≤1.4.9）把 `"src/**"` 当字面目录名 → 缺失 warn，可能 `no_source_files` fatal。**降级不兼容**，文档写明 |
| 递归模式去重 | 新模式内差异 | 嵌套同名文件都参与编译（flat 仍只编第一个） |
| export 的 `GLOB_RECURSE` | 行为修正 | 过去无条件递归；分派后与 `ezmk build` 一致（既有导出的 CMake 可能少编译以前「多编」的文件） |
| `util::list_files` / `collect_sources` | 无破坏性变更 | 旧签名保留；默认路径语义不变 |
| POSIX watch | 已知限制（不变） | 递归目录新增的嵌套文件在 Linux/macOS 不触发重建 |
| 弃用面 | 无变化 | 归 2.0.0，见 [2.0.x/REMOVALS.md](plans/2.0.x/REMOVALS.md) |

## 6 延后项

- `/*` 单层递归、glob 表达式与排除模式。
- 全局便捷开关 `[compile].recursive_sources`（候选 B，视需求另立）。
- Linux/macOS 文件监视真递归（沿用 1.4.2 §3.9 / 1.4.7 §3.6 裁定，另立版本）。
- `project import` 用一条 `src/**` 折叠长 `src_dirs` 列表。
- 大树递归发现的性能优化（如需要）。
