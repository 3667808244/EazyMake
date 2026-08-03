# EazyMake 1.1.0-pre.3 执行计划

> 详细设计：[`plans/release/1.1.0-pre.3.md`](plans/release/1.1.0-pre.3.md)
>
> **状态：执行中 · 阶段一（测试系统缺陷修复）已完成。** 聚合 pre.2（文档检查）之后的全量已知缺陷与未实现项：测试系统缺陷（超时未实现 / Catch2 解析脆弱 / build-first 粗糙 / watch flaky）、CI 测试工作流缺失、工具/文档缺陷，以及 pre.1 / pre.2 延后的发布流水线项。
>
> 阶段一完成：`run_command()` 超时、Catch2 解析加固、`check_built` 修正、watch 集成测试去 flaky；`bash build.sh test` 546/2617 + `test-all` 556/2666 全量零回归。
>
> 启动时已就绪：**3.2.3 `install.sh` zsh 补全激活 bug 已修复**（精确 marker 匹配 + ZDOTDIR 尊重），真实 zsh 回归待验；其余 3.2.x 待执行，3.3.x 延后项可与 pre.3 并行或并入 1.1.0 收尾。

---

## 1 背景

`1.1.0-pre.2` 完成文档审计并补齐 pre.1 遗留项，但系统性检查代码与文档时发现：

1. **测试系统存在实现缺陷**：`run_tests()`（`src/build.cpp`）的超时逻辑未实现（`timed_out` 恒为 0）、Catch2 输出依赖脆弱的文本解析、`shared`/`utils` 类型的 build-first 检查粗糙；watch 集成测试存在 timing-sensitive 误报可能
2. **缺少 CI 测试工作流**：`.github/workflows/` 仅有 `release.yml`（Release 触发，从未实际跑过），没有任何在 push/PR 上运行 `bash build.sh test` 的工作流——回归全靠本地
3. **若干文档引用了不存在的工具/过时数据**：`package_authoring.md` §6.3 引用 `ezmk utils sha256`（未发布）；`ezmk-test` skill 记录的测试基线已过时（~538/2440，实际 545/2613）
4. **pre.1 / pre.2 延后项仍未做**：CI 激活、Homebrew formula、winget manifest、`plans/release/1.1.0.md`、Tutorial 09/10 章节
5. **zsh 补全从未在真实 zsh 环境验证**（Windows 无法验证）；i18n 三方一致性无自动化脚本

本版本将这些缺陷与未实现项汇总为可执行的清单，并以「⛔ 发布门槛」约束 1.1.0 正式版的发布条件。

---

## 2 目标

| # | 目标 | 类别 | 优先级 | 状态 |
|---|------|------|--------|------|
| 1 | ezmk 内置测试框架超时未实现 → 修复 | 缺陷 | P0 | ⬜ 待执行 |
| 2 | 新增 CI 测试工作流（push/PR 跑 `bash build.sh test`） | 缺陷 | P0 | ⬜ 待执行 |
| 3 | Catch2 输出解析加固（或改用结构化 reporter） | 缺陷 | P1 | ⬜ 待执行 |
| 4 | `run_tests()` build-first 检查对 shared/utils 类型修正 | 缺陷 | P1 | ⬜ 待执行 |
| 5 | watch 集成测试 flaky 处理（提高稳定性或标注） | 缺陷 | P1 | ⬜ 待执行 |
| 6 | `package_authoring.md` 修正不存在的 `ezmk utils sha256` 引用 | 缺陷 | P1 | ⬜ 待执行 |
| 7 | zsh 补全在 zsh 环境实际加载验证 | 缺陷 | P1 | 🔄 install.sh 激活 bug 已修复 · 真实 zsh 回归待验 |
| 8 | i18n 三方一致性校验脚本化 | 缺陷 | P1 | ⬜ 待执行 |
| 9 | 过时文档数据修正（ezmk-test skill / technical.md） | 缺陷 | P2 | ⬜ 待执行 |
| 10 | `release.yml` CI 激活（触发测试 + 首次真实 Release） | 未实现 | P2 | ⬜ 待执行 |
| 11 | Homebrew formula（`homebrew-eazymake/ezmk.rb`） | 未实现 | P2 | ⬜ 待执行 |
| 12 | winget manifest（`manifests/e/ezmk/1.1.0.yaml`） | 未实现 | P2 | ⬜ 待执行 |
| 13 | `plans/release/1.1.0.md` 最终发布计划 | 未实现 | P2 | ⬜ 待执行 |
| 14 | Tutorial 09-test.md + 10-top-level-aliases.md | 未实现 | P2 | ⬜ 待执行 |
| 15 | **发布门槛生效**：实现未完成或兼容性破坏不得发布 | 门槛 | P0 | ⬜ 待执行 |

---

## 3 执行阶段

### 阶段一：测试系统缺陷修复（P0/P1）✅

**文件**：`src/build.cpp` + `src/util.cpp` + 相关头文件 + `test/test_integration.cpp` + `test/test_util.cpp`

- [x] **3.1.1 超时（P0）**：`run_command()` 增加可选 timeout 参数（POSIX `fork`/`waitpid` 轮询 + `SIGKILL`，Windows `WaitForSingleObject` 超时 + `TerminateProcess` + 非阻塞 `PeekNamedPipe` 排空）；`run_tests()` ezmk 模式传 30s，超时测试记为 FAIL 并归入 `timed_out` 计数；新增 `run_command: timeout` 单元测试
- [x] **3.1.2 Catch2 解析（P1）**：移除 `-s` 逐行 `find("PASSED"/"FAILED")` 猜测，改为退出码 + 大小写级汇总行解析（`test cases:` 与 `All tests passed` 两种格式，解析失败时回退退出码）；保持 `--filter` 透传不变；端到端验证（scratch 项目全过/含失败路径）
- [x] **3.1.3 build-first（P1）**：`check_built` 对 `shared` 检查真实 `.dll`/`.so` 产物（MSVC `<name>.dll` + MinGW `lib<name>.dll`）；`utils` 类型返回 true 明确跳过构建步骤；端到端验证（删除 DLL 后 `ezmk test` 触发重建）
- [x] **3.1.4 watch flaky（P1）**：固定 sleep 改为日志轮询（启动就绪 + 重建检测，30s 超时窗口）；修复 Windows `start /B` 不继承项目 CWD 的 bug（改用 `start "" /D <dir> /B`）；`EZMK_LANG=en` 强制英文输出使日志解析与 locale 无关；断言 WARN → CHECK + INFO；连跑 3 次稳定
- [x] 编译通过 + `bash build.sh test`（546/2617）+ `test-all`（556/2666）全量零回归

### 阶段二：CI 测试工作流（P0）

**文件**：`.github/workflows/ci.yml`（新建）

- [ ] 新增 `.github/workflows/ci.yml`：push/PR 触发，Ubuntu + MSYS2 环境跑 `bash build.sh test`（单元），可选 `test-all` 含集成
- [ ] workflow 中显式安装 g++ / python / msys2（避免缺 python 走空 locale 分支）
- [ ] 失败时输出测试报告（上传 artifact 或注释 PR）
- [ ] 本地校验 workflow 语法（`actionlint` 或至少 YAML 解析）

### 阶段三：工具/文档缺陷（P1）

**文件**：`docs/en/package_authoring.md` + `docs/zh/package_authoring.md` + `scripts/check_i18n.py`（新建）+ `.claude/skills/ezmk-test.md` + `install.sh`（已改）

- [ ] **3.2.2**：决定 `ezmk utils sha256` 实现或删文档引用（`docs/en/package_authoring.md:297` + `docs/zh/package_authoring.md` §6.3）；若实现则落在 `ezmk-official-utils` 包（`ezmk-repo` 仓库），需与主仓库发布节奏同步
- [x] **3.2.3 修复**：`install.sh` 幂等性检查改为精确 marker 匹配（`grep -qF '# Added by EazyMake installer'`）+ `ZSHRC="${ZDOTDIR:-$HOME}/.zshrc"`（尊重 ZDOTDIR）——**本次执行前已应用**
- [ ] **3.2.3 回归**：在 Linux/macOS（或 MSYS2 zsh）跑一遍 `install.sh`（或复现安装步骤）后新开 zsh，验证顶层别名与子命令补全生效
- [ ] **3.2.4**：`scripts/check_i18n.py` 三向一致性脚本（读 `i18n_keys.def` + `locale/en.json` + `locale/zh.json`，比对 key 集合），纳入 CI
- [ ] **3.2.5**：更新 `ezmk-test` skill 测试基线 ~538/2440 → **545/2613**、集成 7 → 8 个场景；检查是否有其他引用过时测试基线的文档

### 阶段四：发布流水线（P2，可与 pre.3 并行或在 1.1.0 收尾）

**文件**：`.github/workflows/release.yml` + `homebrew-eazymake/ezmk.rb`（外部仓库）+ `manifests/e/ezmk/1.1.0.yaml` + `plans/release/1.1.0.md`（新建）+ `tutorial/{en,zh}/09-test.md` / `10-top-level-aliases.md`（新建）

- [ ] **3.3.1**：触发一次真实 Release 验证 `release.yml`（4 平台构建/打包步骤 + `res/ezmk.zsh` 拷贝路径）
- [ ] **3.3.2**：Homebrew formula `homebrew-eazymake/ezmk.rb`（依赖 GitHub Release 产物 URL/哈希）
- [ ] **3.3.3**：winget manifest `manifests/e/ezmk/1.1.0.yaml`（依赖 Release 存在的 `.exe` 安装包）
- [ ] **3.3.4**：`plans/release/1.1.0.md` 最终发布计划（合并 dev.1~dev.7 + pre.1~pre.3）
- [ ] **3.3.5**：Tutorial `09-test.md`（`ezmk test` 专题教程：`[test]` 配置 + 两种框架）
- [ ] **3.3.6**：Tutorial `10-top-level-aliases.md`（顶层别名快速参考）

### 阶段五：回归与收尾

- [ ] `bash build.sh test`（单元）+ `test-all`（含集成）全量零回归
- [ ] **发布门槛核对**：① 实现清单全部完成或明确收口；② 公共 API 无破坏性变更（复核 `CHANGES.md`）；③ 全量测试零回归——三项同时满足才可进入 1.1.0 正式发布
- [ ] 更新 `plan.md`（本计划状态）+ `plans/README.md`（pre.3 进展 / 移至已完成）

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **超时参数向后兼容** | `run_command()` 加 timeout 默认参数，调用方不传即无超时；`run_tests()` ezmk 模式显式传 30s |
| **Catch2 解析加固** | 放弃逐行文本猜测，改用 junit/xml reporter（或退出码 + 汇总行）；`--filter` 用户透传保持不变 |
| **`check_built` 修正** | `shared` 检查 `.dll`/`.so`（Win 下含导入库），`utils` 直接走 `build_project()` 或跳过构建步骤 |
| **CI 首次落地** | MSYS2 Actions 上 g++/python 安装步骤显式写入 workflow，避免 `bash build.sh` 缺 python 走空 locale 分支 |
| **watch flaky 提前处理** | CI 一旦接入，timing-sensitive 测试是主要红灯来源，优先级提前至阶段一完成 |
| **`ezmk utils sha256` 归属** | 若实现则在 `ezmk-official-utils` 包（`ezmk-repo` 仓库），注意发布节奏同步；否则删文档引用 |
| **延后边界** | 3.3.x 全部是发布流水线建设，时间紧可整体并入 1.1.0 正式版收尾；3.1.x / 3.2.x（缺陷修复 + CI + 文档修正）在 pre.3 内完成 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `run_command()` 加 timeout 参数 | 函数签名变化（内部 API） | 默认参数，调用方不传即无超时，向后兼容 |
| Catch2 输出解析改为 XML/junit | `--filter` 透传方式可能变 | 保持 `--filter` 对用户不变 |
| CI 新增测试工作流 | 无用户可见变化 | 纯增量，防回归 |
| `check_built` 修正 | shared 项目测试前构建行为更正确 | 修复边缘 bug |
| `ezmk utils sha256` 新增 | 官方 utils 包新增一个工具 | 纯增量；若删文档引用则无 |
| Homebrew / winget | 新分发渠道 | 纯增量 |
| `install.sh` zsh 补全 marker | 重复安装不再误判"已配置" | 修复幂等性 bug（pre.3 已应用） |

---

## 6 延后项（1.1.0 正式版收尾）

- `.github/workflows/release.yml` 激活 + 首次真实 Release 验证
- Homebrew formula（`homebrew-eazymake/ezmk.rb`）
- winget manifest（`manifests/e/ezmk/1.1.0.yaml`）
- `plans/release/1.1.0.md` 最终发布计划
- Tutorial 新增章节：`09-test.md`、`10-top-level-aliases.md`

> 以上 3.3.x 项若 pre.3 时间紧可整体并入 1.1.0；缺陷修复（3.1.x）+ CI + 文档修正（3.2.x）建议在 pre.3 内完成。

---

## 7 涉及文件变更摘要

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `src/util.cpp` / `src/util.hpp` | 修改 | `run_command()` 增加 timeout 参数（POSIX + Windows 双路径） |
| `src/build.cpp` | 修改 | `run_tests()` 超时传递 + `timed_out` 计数、Catch2 解析加固、`check_built` 修正 |
| `test/test_integration.cpp` | 修改 | watch 集成测试稳定性处理 |
| `.github/workflows/ci.yml` | **新建** | push/PR 测试工作流 |
| `scripts/check_i18n.py` | **新建** | i18n 三向一致性校验脚本 |
| `docs/en/package_authoring.md` | 修改 | §6.3 修正 `ezmk utils sha256` 引用 |
| `docs/zh/package_authoring.md` | 修改 | 中文同步 |
| `.claude/skills/ezmk-test.md` | 修改 | 测试基线更新 545/2613 + 集成 8 场景 |
| `install.sh` | 修改 | zsh 补全 marker 精确匹配 + ZDOTDIR（**已应用**） |
| `plans/release/1.1.0-pre.3.md` | 修改 | 执行过程中 checkbox 更新 |
| `plans/release/1.1.0.md` | **新建** | 1.1.0 最终发布计划（阶段四） |
| `tutorial/{en,zh}/09-test.md` + `10-top-level-aliases.md` | **新建** | 教程新增章节（阶段四） |
| `plan.md` | 重写 | pre.3 执行计划（本次） |

---

## 8 版本路线图

```
1.0.0 (正式版) ──→ 1.1.0-dev.1~7 (包编译与开发体验) ✅
                 → 1.1.0-pre.1 (改善用户触达) ✅
                 → 1.1.0-pre.2 (文档检查) ✅
                 → 1.1.0-pre.3 (缺陷收集与未实现项补全) 🔄 执行中
                 → 1.1.0 (正式版发布)
```
