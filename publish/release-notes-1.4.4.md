# EazyMake 1.4.4 — 历史遗留清理（安装器 / 告警 / 工具链卫生）

清理 1.4.3 发布后对仓库做全量「历史遗留」扫描中**不需要新功能即可收口**的项：Windows 安装器的 `-DryRun` 预览缺陷、`test/` 编译告警、`.gitignore` 过期条目、过期 TODO、`release.yml` 僵尸 job 与未接线的 docs-sync 检查。**零功能新增、零 CLI 行为变更、零配置语义变更、公共 API 无破坏性变更**。

### 修复

- **`install.ps1 -DryRun` 可用**：此前 README 与脚本 comment-based help 记载的预览用法会因 `$tempDir` 留空而在 `Join-Path` 上抛错中止（退出码 1，且在打印任何 `[DRY RUN] Would …` 之前）；三处「目标二进制不存在」早退还会在 dry-run 下误报「跳过」。现改为**两种模式都计算展示路径**（仅真实安装创建目录），`Test-DryRun` 分支前移到三处早退之前，并补空 `DestDir` 断言。`-DryRun`（含 `-Version <tag> -DryRun`）**退出码 0、零副作用、零网络、完整打印计划**；真实安装的路径 / 顺序 / 文案不变。
- **Windows CI 冒烟**：`ci.yml` windows job 新增 `install.ps1: -DryRun smoke`，断言退出码 0、目标目录未被创建、关键预览行存在——防止预览契约悄悄退化。

### 卫生

- **`test/` 编译告警清零**：严格旗标 `-Wall -Wextra -Wpedantic -Wshadow -Wformat=2` 下 `src/` + `include/ezmk/` + `test/` **首方代码零告警**（`BuildOptions` 聚合初始化补齐、注释续行改写、`std::string_view` 循环变量、删除未用函数），剩余告警全部来自 vendor。
- **`.gitignore`**：删除对已跟踪文件无效的 `plan.md` / `include/ezmk/version.hpp` 与已不存在的 `stdout_in_linux_vm.txt`，去重、按用途分组。
- **过期 TODO**：`src/cli.cpp` 的「完整防御归 1.2.0（TODO）」改为不挂版本的已知限制（OS 不会传入含嵌入 NUL 的 argv）。
- **`release.yml` 的 `macos-x64` 默认跳过**：`if: vars.ENABLE_MACOS_X64 == 'true'`。该 job 在 free tier 拿不到 `macos-13` runner，常驻会让整轮 Release 长期 `queued`（v1.2.x~v1.4.3 均如此）；需要 Intel 包时把仓库变量设为 `true`，资产集合默认不变。
- **docs-sync 接入 CI**：ubuntu job 新增 `Docs: en/zh file parity`（当前 `docs` 15/15、`tutorial` 16/16）。

### 文档

- `CONTRIBUTING.md`（docs-sync 由人工清单改为「CI 已接线、本地可预检」；windows `-DryRun` 冒烟写入测试说明）、`.claude/skills/ezmk-publish` 与 `.claude/skills/ezmk-workflow`、`publish/homebrew/ezmk.rb` 头部注释同步 `macos-x64` 默认跳过 / `ENABLE_MACOS_X64` 口径。

### 已知限制

- `locale/zh-TW.json` 的未译键按变体继承（既定设计）；`[install].sharedir`（解析但不生效）与 2.0.0 弃用面、兼容垫片留待后续 —— 见 `plans/2.0.x/REMOVALS.md`。
- `test_file_watcher.cpp` 的环境相关 SKIP 与 `test_workspace.cpp` 的 Catch2 运行期 `WARN` 为刻意保留。

**安装**：`install.sh`（Linux/macOS）、`install.ps1`（Windows）、`winget install EazyMake.EazyMake`、`brew install 3667808244/eazymake/ezmk`、`publish/arch/PKGBUILD`（Arch/MSYS2）。

**完整变更**：见 [`CHANGES.md`](https://github.com/3667808244/EazyMake/blob/main/CHANGES.md) 的 1.4.4 节；设计与执行计划见 [`plans/1.4.x/1.4.4.md`](https://github.com/3667808244/EazyMake/blob/main/plans/1.4.x/1.4.4.md)。
