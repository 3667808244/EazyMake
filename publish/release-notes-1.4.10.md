# EazyMake 1.4.10 — 可选递归源文件查找（`src_dirs` / `[test].dirs` 的 `/**`）

本版新增**可选的目录级递归源发现**：条目以 `/**` 结尾即递归收录整棵子树。**默认行为零变化、公共 API 无破坏性变更。**

### 新功能

- **`[compile].src_dirs` / `[test].dirs` 支持 `/**` 后缀**：`src_dirs = ["src/**", "lib"]` → `src` 递归收录整棵子树，`lib` 仍只扫描顶层。逐条目生效、可混用。
  - 不是 glob：`src/**/*.cpp`、`src/**/` 之类按字面目录名处理（缺失时仅告警）。
  - 递归模式按**项目根相对路径**去重（不同子目录的同名文件都会编译）；非递归模式沿用按文件名去重 + 重复告警。
  - 递归扫描跳过 `.` 前缀目录与任意深度的 `build/`，不跟随目录符号链接，结果按路径排序。
- **全路径统一走递归感知收集**：项目 build、`ezmk test`、包编译、`utils` 包门控、Lua `ezmk.list_sources()`、workspace 测试探测、`compile_commands.json`。
- **`project export cmake` 对齐**：flat 条目改用 `GLOB`、`/**` 条目用 `GLOB_RECURSE`（混合时 `GLOB` + `GLOB_RECURSE` + `list(APPEND ...)`），消除「导出的 CMake 编译 `ezmk build` 不编译的子目录文件」的长期分叉。

### 已知限制

- **Linux/macOS 的 `ezmk watch` 仍只监视目录自身**：递归目录里新增的嵌套文件不会触发重建（Windows 走子树监视，正常）。
- **降级不兼容**：≤1.4.9 读到 `"src/**"` 会把它当字面目录名（缺失时 warn，最终可能 `no_source_files` fatal）。

### 后续（2.0.0）

- 默认创建（`ezmk project new`）与默认扫描在 1.x 保持 `src` **扁平**；**2.0.0 起改为 `src/**`**（`plans/2.0.x/REMOVALS.md` 决策项 D-10，方向已定）。

**测试**：全量 **1156 用例 / 6675 断言 / 5 跳过 / 0 失败**；`check_i18n.py`（418 键）、`check_man_sync.py`、`check_docs_sync.sh` 通过，groff 4 页零告警。

**安装**：`install.sh`（Linux/macOS）、`install.ps1`（Windows）、`winget install EazyMake.EazyMake`、`brew install 3667808244/eazymake/ezmk`、`publish/arch/PKGBUILD`（Arch/MSYS2）。

**完整变更**：见 `CHANGES.md` 的 1.4.10 节；设计与执行计划见 `plans/1.4.x/1.4.10.md`。
