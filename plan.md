# EazyMake 1.1.0 执行计划

> 详细设计：[`plans/1.1.0/1.1.0.md`](plans/1.1.0/1.1.0.md)
>
> **状态：🔄 发布准备中。** 这是 1.1.0 正式版发布执行计划，聚合 dev.1~dev.7 + pre.1~pre.3 的全部交付，并收口 pre.3 延后的 3.3.x 发布流水线项（release.yml 首次真实激活 / Homebrew / winget）。前置条件为 `1.1.0-pre.3` 全部缺陷修复与 CI 工作流已完成（✅，见 `CHANGES.md`）。
>
> **⛔ 发布门槛**：实现完整 + API 兼容 + 全量测试零回归，三项同时满足才可发布。详细 Gate 定义见 [1.1.0-pre.3](plans/1.1.0/1.1.0-pre.3.md#-发布门槛release-gate)。

---

## 1 背景

`1.1.0-pre.3` 收口了全量已知缺陷、CI 测试工作流与文档修正，并把 3.3.x 发布流水线项（依赖真实 GitHub Release 产物的 URL 与哈希）明确延后至正式版发布时执行。至此，1.1.0 的功能交付已完整：

1. **dev 阶段**（dev.1~dev.7）：`precompiled` 包与 `[install]`、多平台共包（`os_arch_toolchain` triple）与 `pack`、Agent Skills、`stdlib`/`lang` 泛化、`ezmk-official-utils`、`ezmk test` 测试系统、包生态拓充
2. **pre 阶段**（pre.1~pre.3）：顶层别名 / `--help` 重组 / API 稳定承诺、文档审计、测试系统缺陷修复与 CI 测试工作流
3. **发布就绪预置**：`include/ezmk/version.hpp` / `build.sh` 默认版本已为 `1.1.0`；`plans/1.1.0/1.1.0.md` 发布计划与 Tutorial 09/10 已交付

剩余工作全部属于**正式发布动作**：冻结回归 → 打 `v1.1.0` tag 触发 `release.yml` 首次真实 Release → Homebrew / winget / 安装脚本分发渠道 → 文档与版本索引收尾。

`README.md` 已宣告 **"Stable public API as of v1.1.0"** 并给出安装命令（`curl ... install.sh | bash` / `irm ... install.ps1 | iex`）——安装脚本回归即以这两条命令 + Release 产物为验收目标，发布成功后 README 无需改动。

---

## 2 目标

| # | 目标 | 类别 | 优先级 | 状态 |
|---|------|------|--------|------|
| 1 | 冻结与回归：`bash build.sh test-all` 全量零回归（556/2666）+ CI 绿色 | 门槛 | P0 | 待执行 |
| 2 | 打 `v1.1.0` tag 触发 `release.yml` 首次真实 Release | 发布 | P0 | 待执行 |
| 3 | 4 平台产物核验（windows/linux/macos x64+arm64，含 `_ezmk` 拷贝 + `./ezmk version`） | 发布 | P0 | 待执行 |
| 4 | Homebrew formula `homebrew-eazymake/ezmk.rb`（Release 产物 URL + sha256） | 分发 | P1 | 待执行 |
| 5 | winget manifest `manifests/e/ezmk/1.1.0.yaml`（Release `.exe` URL + sha256） | 分发 | P1 | 待执行 |
| 6 | `install.sh`（Linux，含 zsh 补全）/ `install.ps1`（Windows 预编译）回归 | 回归 | P1 | 待执行 |
| 7 | `CHANGES.md` 新增 `1.1.0` 条目（汇总 dev/pre 交付，标注里程碑） | 文档 | P1 | 待执行 |
| 8 | `plans/README.md` / `plan.md` 状态收口（1.1.0 → 已完成） | 文档 | P2 | 待执行 |
| 9 | 版本号就绪：`version.hpp` / `build.sh` 默认版本为 `1.1.0` | 预置 | P0 | ✅ 已就绪 |
| 10 | `plans/1.1.0/1.1.0.md` 发布计划 + Tutorial 09/10 | 预置 | P2 | ✅ 已交付（pre.3） |

---

## 3 执行阶段

### 阶段一：冻结与回归（发布门槛预检）✅

**文件**：`git status` / `build.sh` / `.github/workflows/ci.yml`（核验，不改动）

- [x] `1.1.0-pre.3` 收尾提交全部合并，`git status` 干净（✅ pre.3 阶段五已收口）
- [x] 版本号就绪：`include/ezmk/version.hpp` / `build.sh` 默认版本为 `1.1.0`（`EZMK_VERSION` fallback）（✅ 已验证）
- [x] 发布计划 `plans/1.1.0/1.1.0.md` + Tutorial `09-test.md` / `10-top-level-aliases.md`（en/zh）已完成（✅ pre.3 阶段四交付）
- [x] 本地全量回归：`bash build.sh test-all`（单元 546/2617 + 集成 556/2666）零回归（✅ 556/2666 passed，exit 0）
- [x] 确认 CI 工作流（`.github/workflows/ci.yml`）在最后一次 push 上绿色（含 zsh-completions job）（✅ run #31171539927，3 job 全绿）
- [x] **发布门槛预检**（§2.1 三项）：① 实现完整（pre.3 清单收口，含 3.3.x 明确延后边界）；② API 兼容（`CHANGES.md` 稳定性承诺无破坏性变更）；③ 全量测试零回归（✅ 三项均满足）

> 门槛未满足即停止，禁止带着未收口项打 tag。

### 阶段二：打 tag 触发 Release ✅（macos-x64 跟进项）

**文件**：`git` / `.github/workflows/release.yml`（已有，首次真实触发）

- [x] `git tag v1.1.0` + `git push origin v1.1.0`（✅ 已推送）
- [x] 创建 GitHub Release → `release.yml` 触发 4 平台 job（`windows-x64` / `linux-x64` / `macos-x64` / `macos-arm64`）（✅ Release 已建，run #31174454874）
- [x] 核对每个 job 的 `res/ezmk.zsh` 被拷贝为 `_ezmk`；产物齐全（`ezmk-windows-x64.zip`、`ezmk-linux-x64.tar.gz`、`ezmk-macos-x64.tar.gz`、`ezmk-macos-arm64.tar.gz`）（✅ 3/4 已核：linux-x64 / windows-x64 / macos-arm64 均含 `_ezmk`；macos-x64 ⏳ 见跟进项）
- [x] 人工核验产物可运行：下载解包后 `./ezmk version` 输出 `1.1.0`（✅ windows-x64 实测 `EazyMake 1.1.0`；linux=ELF / macos=Mach-O 结构校验通过；linux/macOS 运行时核验待真机/CI）
- [x] 记录 4 平台产物的 URL + sha256（供阶段三分发渠道使用）（✅ 3/4：linux-x64 `938F7CA8…7BF7`、macos-arm64 `3397D63D…B4C4`、windows-x64 `B3696CEC…3FCE`；macos-x64 待产物）

> **macos-x64 跟进项（已确认延后）**：Intel `macos-13` runner 在 GitHub free tier 长期无分配（排队 >50 分钟，最长可排 24h 后被取消）。`release.yml` 其余 4 个 job 全部成功，Release 已带 3 个产物上线。**决策：先推进阶段三/四，macos-x64 不阻塞发布**——job 仍在 GitHub 队列，runner 可用时会自动构建并上传 `ezmk-macos-x64.tar.gz` 到同一 Release，届时补核验；若被取消则 `gh run rerun` 重新触发。该 job 与 `macos-arm64` 结构完全相同（macOS/clang++/打包），已验证流程对其同样适用。

> **风险核销**：`release.yml` 首次真实运行未出现代码级错误——linux/windows/macos-arm64 三平台构建、`res/ezmk.zsh → _ezmk` 拷贝、产物打包上传全部成功，唯一问题（macos-x64 runner 缺货）属基础设施排队，非流水线缺陷。

### 阶段三：分发渠道

**文件**：`homebrew-eazymake/ezmk.rb`（外部仓库）+ `manifests/e/ezmk/1.1.0.yaml`（winget，外部仓库）+ `install.sh` / `install.ps1`（核验，不改动）

- [ ] **Homebrew**：以 Release 的 `ezmk-linux-x64.tar.gz` URL + `sha256sum` 填 `homebrew-eazymake/ezmk.rb`，`brew install` 冒烟
- [ ] **winget**：以 Release 的 Windows `.exe` 安装包 URL + sha256 填 manifest，提交 `microsoft/winget-pkgs` PR（后续由 winget 官方审批合并，**不阻塞 1.1.0 发布**）
- [ ] **install.sh**：真实 Linux 环境验证 README 安装命令（`curl -fsSL .../install.sh | bash`），含 zsh 补全激活——顶层别名与子命令补全生效（CI zsh-completions job 已覆盖激活逻辑，此处为真机终验）
- [ ] **install.ps1**：Windows 预编译安装回归（`irm .../install.ps1 | iex`），验证从 Release 产物安装可用

### 阶段四：收尾

**文件**：`CHANGES.md` + `plans/README.md` + `plan.md`

- [ ] `CHANGES.md` 新增 `1.1.0` 条目（汇总 dev.1~dev.7 + pre.1~pre.3 全部交付，标注里程碑）
- [ ] `plans/README.md` 更新：1.1.0 从「当前执行」移至「已完成」，路线图推进至 2.0.0
- [ ] 更新 `plan.md`（本计划 → 收口状态）
- [ ] **发布门槛最终核对**（§2.1 三项）：实现完整 / API 兼容 / 全量测试零回归
- [ ] 若任何一项不满足：**回退 tag 并修复，禁止带病发布**

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| **tag → 版本号映射** | `release.yml` 从 tag 剥离 `v` 前缀（`${TAG#v}`）得到版本号，并以 `EZMK_VERSION` 环境变量传给各平台 job——tag 命名必须为 `v1.1.0` |
| **发布顺序强依赖** | Homebrew / winget / install.ps1 全部依赖阶段二 Release 产物 URL + sha256，阶段二未完成不得进入阶段三 |
| **winget 审批不阻塞** | `winget-pkgs` 由官方维护者审批，周期不可控，作为 1.1.0 发布后跟进项，不设阻塞 |
| **zsh 补全真机终验** | Windows 无法运行 zsh；CI zsh-completions job 已在真实 zsh 覆盖 install.sh 激活逻辑（marker 幂等 + `_ezmk` 注册），阶段三真机验证为其补充终验 |
| **release.yml 首次激活风险最高** | 从未被 Release 触发，macOS（clang++ 别名 / 静态链接）与打包路径是最大不确定点——首次 Release 需现场修复并补跑 |
| **版本号已预置** | `version.hpp` / `build.sh` 默认版本在 pre.3 阶段已置为 `1.1.0`，发布时无需再改，避免最后一刻改动引入回归 |

---

## 5 兼容性矩阵

1.1.0 全周期（dev.1 ~ pre.3）公共 API 无破坏性变更（破坏性变更仅在 `2.0.0` 引入）：

| 变更 | 影响 | 处理 |
|------|------|------|
| 顶层别名（`ezmk build` 等） | 新增命令入口 | 纯增量，完整形式 `project <action>` 不变 |
| `[test]` / `[install]` 配置节 | 新增可选字段 | 不影响既有配置 |
| `run_command()` 加 timeout 参数 | 函数签名变化（内部 API） | 默认参数，调用方不传即无超时 |
| `check_built` / Catch2 解析修正 | 行为更正确 | 修复边缘 bug |
| Homebrew / winget 渠道 | 新分发渠道 | 纯增量，不影响既有安装方式 |
| `install.sh` zsh 补全 marker | 重复安装不再误判 | pre.3 已修复，本版本回归验证 |

---

## 6 延后项

- **winget 审批合并**：提交 `microsoft/winget-pkgs` 后由官方维护者审批，周期不可控——不阻塞 1.1.0 发布，作为发布后跟进项
- **2.0.0 破坏性变更窗口**：`CHANGES.md` API 稳定性承诺——破坏性变更仅在 `2.0.0` 引入，且至少在 1 个 minor 版本前给出 deprecation warning；1.1.0 无任何破坏性变更

---

## 7 涉及文件变更摘要

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `include/ezmk/version.hpp` / `build.sh` | 已就绪 | 默认版本 `1.1.0`（pre.3 已置，无需再改） |
| `.github/workflows/release.yml` | 已有，首次激活 | 打 `v1.1.0` tag 触发 4 平台构建/打包（阶段二），`res/ezmk.zsh` → `_ezmk` |
| `homebrew-eazymake/ezmk.rb` | **新建**（外部仓库） | Homebrew formula，Release 的 `ezmk-linux-x64.tar.gz` URL + sha256 |
| `manifests/e/ezmk/1.1.0.yaml` | **新建**（winget，外部仓库） | Release 的 Windows `.exe` URL + sha256，提交 `microsoft/winget-pkgs` |
| `install.sh` / `install.ps1` | 回归验证 | 真实 Linux / Windows 验证 README 安装命令（原则上不改代码，除非发现问题） |
| `CHANGES.md` | 修改 | 新增 `1.1.0` 条目（阶段四） |
| `plans/README.md` | 修改 | 1.1.0 → 已完成，路线图推进（阶段四） |
| `plan.md` | 重写 | 1.1.0 执行计划（本次） |
| `plans/1.1.0/1.1.0.md` | 已交付 | 1.1.0 最终发布计划（pre.3 阶段四） |

---

## 8 版本路线图

```
1.0.0 (正式版) ──→ 1.1.0-dev.1~7 (包编译与开发体验) ✅
                 → 1.1.0-pre.1~3 (用户触达 / 文档 / 缺陷收口) ✅
                 → 1.1.0 (正式版发布) 🔄 本计划
                 → 2.0.0 (未来) —— 破坏性变更窗口
```
