# EazyMake 1.2.0-dev.8 执行计划

> **状态：执行中**（2026-08-15）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.8.md`](plans/1.2.0/1.2.0-dev.8.md)。本计划为 1.2.0 系列第八个开发子版本：**CMake 导出钩子运行时（`ezmk-lua` 独立运行时）**——把 EazyMake 的 Lua 运行时抽成独立、无黑白名单的二进制 `ezmk-lua`，让导出的 CMake 构建在构建节点调用它执行 `[hooks]` 钩子，消除导出产物与 `ezmk build` 的行为漂移。**dev.2 的范围收口**：只动导出侧 + 新增一个独立运行时产物，不触碰本体安全模型。
>
> **范围边界**：`ezmk` 本体沙箱 + 黑白名单**零改动**（`run_script` / `push_restricted_globals` / `check_*_permission` 一行不动）；仅 `export cmake` 的 hooks 段从「注释 + WARNING」改为「`find_program` + `add_custom_command`」；`on_failure` 保持不导出（CMake 无原生失败钩子，与 dev.2 一致）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯新增二进制产物 + 导出文本变化，本体零改动）；③ 全量测试零回归（本体沙箱路径必须零变化，作为硬门槛；Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

1. **导出钩子丢失**：`ezmk.toml` 的 `[hooks]`（`pre_build`/`post_build`/`on_failure`，Lua 脚本）在 `ezmk build` 里由沙箱 Lua 运行时执行，CMake 无等价运行时，因此 dev.2 导出时只能「不映射」（`export.cpp:387-402` 仅注释 + `message(WARNING)`）。结果是导出的 CMake 构建**不跑钩子后处理**，与 `ezmk build` 行为漂移。
2. **本计划补上钩子映射**：新增独立、无黑白名单的二进制 `ezmk-lua`，由导出的 CMake 在构建节点调用它执行钩子。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 新增独立运行时二进制 `ezmk-lua`：复用 Lua VM + `register_api` bindings，入口无沙箱/无 permissions；`ctx`（project_root/profile/output）由 CLI 注入 | P0 |
| 2 | `ezmk` 本体行为/沙箱/黑白名单**完全不变**（回归基线证明零变化） | P0 |
| 3 | `export cmake` 对 `[hooks]`：`pre_build`/`post_build` 生成 `add_custom_command` 调用 `ezmk-lua`；`find_program` 找不到 → 回退 `message(WARNING)`；`on_failure` 保持不导出 | P0 |
| 4 | 现有钩子脚本（沙箱 `ezmk.*` API）在 `ezmk-lua` 下运行结果一致（超集兼容） | P0 |
| 5 | 单测/集成：`ezmk-lua` 跑样例钩子 + export 产物含钩子调用；全量测试零回归 | P0 |
| 6 | i18n + 文档（export 钩子说明、`ezmk-lua` 用法、CHANGES.md） | P1 |
| 7 | 分发：`ezmk-lua` 随 `ezmk` 进入所有渠道（release 资产 / install.sh / install.ps1 / winget / Homebrew / pacman） | P1 |

## 3 执行阶段

### 阶段一：运行时抽取（4.1 + 4.2 + 4.3）

- [x] **1.1 无沙箱运行函数**（4.1）：`src/lua_api.cpp` 新增 `run_script_unrestricted()`（复用 `register_api`，**不建沙箱 env / 不 push_restricted_globals / 不加载 permissions**，直接全量 `_G` 执行 + 补开 `io`/`os` 库）；ctx（project_root/profile/output）由参数构建 `run(ctx)`；沙箱版 `run_script` / `run_lua_script_with_ctx` 零改动
- [x] **1.2 `ezmk-lua` 入口**（4.2）：新建 `src/ezmk_lua_main.cpp`——手工解析 CLI（位置参数脚本路径 + `--project-root`/`--profile`/`--output`），`register_api(state, project_root)` 注入全局（`g_project_root` + 配置缓存失效），再调 `run_script_unrestricted`，退出码透传；`--help` 支持；无 `--project-root` 时读配置类 `ezmk.*` 降级（返回空/warn）；CLI 错误/帮助走 i18n
- [x] **1.3 build.sh**（4.3）：新增 `ezmk-lua` 产物（`COMMON_SRC` 排除两个 main，`main.cpp` ↔ `ezmk_lua_main.cpp` 各自装配）；Windows 产 `ezmk-lua.exe`；测试/正常构建分支均产双二进制

### 阶段二：export 钩子生成（4.4 + 4.5）

- [x] **2.1 export 钩子段**（4.4）：`src/export.cpp` `build_cmake_text()` 把 hooks 段从「注释 + WARNING」改为 §3.4 的 `find_program(EZMK_LUA ezmk-lua)` + `add_custom_command`（`pre_build` → `PRE_BUILD`、`post_build` → `POST_BUILD`，`--project-root ${CMAKE_CURRENT_SOURCE_DIR}`、`--output $<TARGET_FILE:<name>>`、`--profile` 内联）；找不到 `ezmk-lua` → 回退 `message(WARNING)`；`on_failure` 保持注释（范围边界，与 dev.2 一致）；`export_cmake` 打印 `export_hook_note` 提示
- [x] **2.2 i18n**（4.5）：新增 `export_hook_note` + 5 个 `ezmk_lua_*` key（`.def` + en/zh JSON），`scripts/check_i18n.py` 三向一致（301 keys）；`bash build.sh` 编译通过

### 阶段三：测试（4.6）

- [x] **3.1 单测**：`test_export.cpp` hooks 用例更新——`find_program` + `add_custom_command`（PRE/POST）+ 回退 warning + `on_failure` 注释 + `--profile` 内联；无 hooks 仍无 hooks 段
- [x] **3.2 单测 + 集成**：`run_script_unrestricted` 单测 8 个（ctx 注入/返回码/`os`·`io` 超集/配置注入/缺失 run()/Lua error，开头 `init()` 抗测试顺序）；`test_integration.cpp` 新增 3 个——`ezmk-lua` 跑样例钩子（ctx + 返回码）、`ezmk build` 钩子沙箱路径零变化、`export cmake` 产物含钩子调用 + 导出提示；`test_i18n.cpp` dev.8 key 断言
- [x] **3.3 全量回归**：`bash build.sh test-all` 零回归（**709 用例 / 3296 断言**，基线 695 / 3234，+14 用例 +62 断言）

### 阶段四：文档收口（4.7 + 4.8）

- [x] **4.1 文档**（4.7）：`docs/en|zh/cli.md`（`ezmk-lua` 用法 + 导出钩子说明）、`docs/en|zh/config_file.md`（hooks 导出小节）、`CHANGES.md` dev.8 条目
- [x] **4.2 分发**（4.8）：`release.yml`（4 平台 job 拷贝 `build/ezmk-lua` + Windows standalone `ezmk-lua.exe` + `.sha256`）、`install.sh` / `install.ps1`（安装/下载/校验 `ezmk-lua`）、Homebrew formula（`homebrew-eazymake/ezmk.rb` `bin.install "ezmk-lua"`）
- [x] **4.3 收口**：本计划勾选 `[x]`；`plans/1.2.0/README.md` dev.8 状态「待实现 → 已完成」；发布门槛复核（本体沙箱零变化 + 全量零回归）

### 收口项（明确收口到发布流水线）

- **winget / pacman 渠道**：`publish/winget/`（`installer.yaml` 的 `NestedInstallerFiles` 增 `ezmk-lua.exe` + `PortableCommandAlias: ezmk-lua`）与 `publish/arch/PKGBUILD`（`install -Dm755 build/ezmk-lua "$pkgdir/usr/bin/ezmk-lua"`）**文件尚不存在**——`publish/` 目录由 pre.1（pacman 分发）创建，winget 清单提交为发布后跟进项；dev.8 只定义改法，随 pre.1 / 1.2.0 发布收口一并落地。
- **Homebrew 版本/哈希**：`homebrew-eazymake/ezmk.rb` 的 `install` 块已加 `bin.install "ezmk-lua"`，但公式当前仍指向 v1.1.3（tarball 不含 ezmk-lua）——**版本号与双处 sha256 必须随 1.2.0 发布重新生成后再发布公式**，否则 `bin.install "ezmk-lua"` 会因文件缺失而失败。

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 独立二进制而非 flag | `ezmk-lua` 是信任边界**之外**的开发者工具，永不接入包安装钩子/utils 脚本路径；边界体现在「独立二进制」上而非开关 |
| 共享编译单元 | Lua VM + `register_api` 与 `ezmk` 本体共用，避免 `ezmk.*` 双维护漂移；改 bindings 只改一处 |
| 入口无沙箱 = 沙箱超集 | 不建 restricted globals、不查 `[utils.permissions]`，全量 `_G`；文档约定导出钩子只用 `ezmk.*` 子集，保证两处行为一致 |
| `register_api(state, project_root)` 注入全局 | 复用既有公开 API 设置 `g_project_root` + 配置缓存失效，零新 setter；无 `--project-root` 时读配置类函数降级（返回空/warn） |
| `find_program` + 回退 warning | best-effort，不硬依赖 ezmk 已安装；找不到 `ezmk-lua` 时 CMake 回退「跳过钩子」并提示 |
| `on_failure` 不导出 | CMake 无原生「构建失败」钩子，保持注释 + 说明（范围边界，与 dev.2 一致） |
| 分发多渠道联动 | release 资产 / install 脚本 / winget / Homebrew / pacman 任一渠道漏配 → 该渠道导出钩子回退跳过；各渠道提交收口到发布流水线阶段 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `ezmk-lua` 二进制 | 纯新增产物 | 不影响 `ezmk` 本体 |
| `ezmk` 本体沙箱/黑白名单 | 无 | `run_script`/`push_restricted_globals`/`check_*_permission` 零改动 |
| `export cmake` hooks 段 | 从「注释 + warning」改为「find_program + add_custom_command」 | 找不到 `ezmk-lua` 回退 warning（best-effort） |
| `on_failure` | 仍不导出 | CMake 无原生失败钩子，范围边界（同 dev.2） |
| 能力面超集 | `ezmk-lua` 下可用沙箱外能力 | 文档约定钩子只用 `ezmk.*` 子集 |
| 新增 `ezmk-lua` 于各分发渠道 | release 资产/安装脚本/清单体积略增 | 纯新增，向后兼容；旧版安装脚本仍只装 `ezmk` |

## 6 延后项

- winget PR / Homebrew tap 更新 / AUR 提交属发布流水线阶段，与 pre.1、1.2.0 发布收口一并执行；pacman 渠道暂不提交 AUR（AUR 账户未开通），以「仓库内 `publish/arch/PKGBUILD` 自取 + `makepkg -si`」为主。
- 若未来想调整 `ezmk-lua` 的 CLI/语义，集中 2.0.0 窗口（本版为纯新增产物，无破坏性）。
