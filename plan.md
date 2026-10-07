# EazyMake 1.4.9 执行计划

> **状态：📋 计划（未开工）** —— 本文档把设计文档 §4 转成可勾选的分阶段清单。1.4.x 系列路线图见 [plans/1.4.x/README.md](plans/1.4.x/README.md)；2.0.0 的移除清单见 [plans/2.0.x/REMOVALS.md](plans/2.0.x/REMOVALS.md)。
>
> 详细设计：[**1.4.9.md**](plans/1.4.x/1.4.9.md)。主题：**构建钩子 profile 解析修复 + 按 profile 导出对象归档**。
>
> **范围边界**：修 profile 传递链 + 新增一个 `[compile.profile.<name>]` 配置项及其归档导出行为；零命令新增、零弃用面变动；公共 API 无破坏性变更（`ProfileConfig` 仅追加字段）。
>
> **⛔ 发布门槛**：① 阶段清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试**无新增失败**（立项基线 **1133 用例 / 6541 断言 / 4 跳过 / 0 失败**（2026-10-07 本机 `bash build.sh test-all` 实测；1.4.8 发布态记录为 6525 + 9 个 `test_integration_git.cpp` 环境性失败，本机当前 git 集成全通过；用例/断言数只增不减））；④ 附加门槛：`python scripts/check_man_sync.py` 通过、`groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警、i18n 三向一致（**412** 键 + 本版新增）、`bash scripts/check_docs_sync.sh` 通过；⑤ `ci.yml` 在 push 上全绿。

---

## 1 背景

1.4.8 发布后核对钩子语义时发现：`pre_build` / `post_build` / `on_failure` 三个构建钩子虽然都收到 `ctx.profile`，但值是 **CLI 原始 `--profile`**，未回填 `[compile].default_profile` 解析结果——无 `-p` 时 `ctx.profile` 为空，而实际按 default profile 编译。同时用户要求新增“按 profile 导出全部源文件目标文件（obj）”的能力，裁定做成 profile 配置项，导出形态为**单个归档** `build/obj_files.zip` 或 `build/obj_files.tar.gz`。详见设计 §1。

## 2 目标

| 组 | # | 覆盖 | 优先级 |
|----|---|------|--------|
| 钩子 | H-01 | `active_profile` 贯穿 `apply_profile` → `BuildState` → 三处 `run_hook` | P0 |
| 钩子 | H-02 | 统一 profile 取名（CLI > default），`export.cpp` 去重 | P1 |
| 钩子 | H-03 | `apply_profile` 单测 + 钩子端到端 `ctx.profile` 断言 | P0 |
| 配置 | O-01 | `[compile.profile.<name>]` 的 `export_objs`（bool / 归档路径字符串）+ 类型与扩展名校验 | P0 |
| 导出 | O-02 | 编译后把全部源文件对象打包为 `build/obj_files.zip` 或 `.tar.gz`（路径由配置给出） | P0 |
| 导出 | O-03 | 复用 `create_zip` / `create_targz`；staging 只含本次对象、原子写；缓存 / `-j` / `--disable-cache` 下一致 | P0 |
| 验证 | O-04 | 配置单测 + 归档集成用例（zip / tar.gz / 默认 / 关闭 / 缓存命中 / 陈旧对象） | P0 |
| 文档 | O-05 | docs + `CHANGES.md` + i18n + 索引 | P0 |
| 模板 | O-06 | 默认模板（`write_default_config` / `project import`）的 release profile 默认 `export_objs = true` | P0 |

## 3 执行阶段

### 阶段零：立项基线与缺陷复现（H-00）

- [x] 记录基线（2026-10-07）：全量 **1133 用例 / 6541 断言 / 4 跳过 / 0 失败**，i18n 412 键
- [x] 复现钩子缺陷（`default_profile` + 钩子写 `ctx.profile`：无 `-p` → `[]`、`-p release` → `[release]`），证据落设计 §3.1
- [x] 提交计划与阶段零证据（`121d214` + 本阶段）

### 阶段一：钩子 profile 解析修复（H-01 / H-02 / H-03）

- [x] `AppliedProfile::active_profile`（`src/build.cpp:524`）
- [x] `BuildState::active_profile`（`src/build.cpp:478`）+ `prepare_build_state` 赋值（`src/build.cpp:727`）
- [x] 三处 `run_hook` 改用 `st.active_profile`（`:996` / `:1339` / `:1533`）
- [x] H-02 新增 `config::resolve_profile_name()`（`src/config.cpp` + `include/ezmk/config.hpp`）；`export.cpp` `:117-118` / `:546-547` 去重
- [x] H-03 单测 `resolve_profile_name`（CLI / default / 皆空）+ 集成用例（`default_profile` / `--profile` / 无 profile）
- [x] 回归：全量 **1135 用例 / 6551 断言 / 4 跳过 / 0 失败**（+2 用例 / +10 断言，无新增失败）

### 阶段二：profile 导出配置解析 + 默认模板（O-01 / O-06）

- [x] `ProfileConfig` 追加 `export_objs` / `export_objs_path`
- [x] `parse_profiles()` bool / string 分支 + 类型与扩展名校验（`.zip` / `.tar.gz` / `.tgz`）
- [x] 默认模板启用：`write_default_config()` release profile 加 `export_objs = true`；`import.cpp` 模板对齐
- [x] i18n 新键 6 个（412 → **418**）三向一致
- [x] `test/test_config.cpp` 用例（true / 路径 / 后缀 / false / 空串 / 非法扩展名 / 非法类型）+ 模板断言
- [x] 回归：全量 **1136 用例 / 6604 断言 / 4 跳过 / 0 失败**

### 阶段三：归档导出执行（O-02 / O-03 / O-04）

- [x] `build_project` 在 `compile_phase` 之后、`link_phase` 之前调用 `export_object_archive()`（`src/build.cpp`）
- [x] staging（`<temp_dir>/obj_export_stage`）只复制本次 `objects` → 按扩展名调 `create_zip` / `create_targz` → 移除 staging
- [x] 目标路径解析（默认 `build/obj_files.zip` / 相对 proj_root / 绝对原样）+ 父目录创建 + 覆盖
- [x] 日志 / 错误处理 + i18n（汇总 info、空集 warn、项目外源文件 warn、失败 fatal）
- [x] 集成用例：zip 默认 / tar.gz 自定义 / 关闭 / 陈旧对象只剩本次 / 二次构建缓存命中 / 链接产物仍在 / 默认模板 release 产出、debug 不产出（`.tgz` 别名由配置单测覆盖）
- [x] 回归：全量 **1137 用例 / 6622 断言 / 4 跳过 / 0 失败**

### 阶段四：文档与收口（O-05）

- [ ] `docs/zh/config_file.md` + `docs/en/config_file.md` profile 章节补 `export_objs`（语法、格式判定、默认路径、覆盖语义），release profile 示例含 `export_objs = true`
- [ ] 钩子 `ctx.profile` 语义说明（`docs/*/config_file.md` 钩子节）
- [ ] `man/ezmk.toml.5`（如含相关字段）+ `CHANGES.md` 1.4.9
- [ ] i18n 三向 + `check_i18n.py`；索引（`plans/1.4.x/README.md` / `plans/README.md` / 本文件）
- [ ] 门槛复核：全量零回归 + `check_man_sync.py` + groff 零告警 + i18n 三向 + `check_docs_sync.sh`

### 发布阶段：1.4.9 正式发布

- [ ] 版本号 `1.4.9`（`build.sh` 的 `EZMK_VERSION` fallback + `include/ezmk/version.hpp`）
- [ ] `git tag v1.4.9` + push 触发 Release，核对 7 资产
- [ ] 三渠道：Homebrew / winget PR / pacman `makepkg -fd`
- [ ] `plans/README.md` 移入「已完成」，`CHANGES.md` 定稿

## 4 关键设计决策

1. **profile 取值单源**：解析结果 `active_profile` 由 `apply_profile` 一次性产出并沿 `BuildState` 传递，三处 `run_hook` 与导出行为共用；不在 CLI/main 层回填 `opts.profile`（那会漏掉 watch / workspace 内部构造的 `BuildOptions`，`src/build.cpp:1814`、`:2596`）。
2. **配置项而非命令**：用户裁定；`export_objs` 接受 bool（`true` = 默认 `build/obj_files.zip`）或归档路径字符串，格式由扩展名判定。
3. **归档而非目录**：用户二次裁定；复用 `util::create_zip` / `create_targz`（1.3.5 / 1.3.6），staging 只放本次对象，因此归档天然不含已删除源文件的陈旧对象。
4. **导出时机在编译之后、链接之前**：导出物是编译产物，链接失败不丢弃。
5. **作用范围仅 build_project**：`ezmk test` 与依赖包编译不导出。
6. **默认文件名不带 profile 名**（`build/obj_files.zip`）：用户给定；多 profile 同时开启会互相覆盖，文档明示，需要并存时用字符串指定不同路径。
7. **默认模板 release 默认启用**：`write_default_config()` / `project import` 的 release profile 写 `export_objs = true`；`default_profile = "debug"` 不变，裸构建不导出，只有显式 release 构建产出归档。

## 5 兼容性矩阵

| 变更 | 影响 | 说明 |
|------|------|------|
| `ctx.profile` 返回解析后 profile | 行为修正 | 依赖 `ctx.profile == ""` 判断“未指定”的钩子需改判断；这是缺陷修复 |
| `export_objs` | 纯新增 | 旧版本忽略该键，不报错不导出 |
| `ProfileConfig` 追加字段 | 公共 API 纯增量 | 末尾追加，聚合初始化兼容 |
| `AppliedProfile` / `BuildState` 追加字段 | 内部 | 不影响公共头 |
| 默认归档 `build/obj_files.zip` | 纯新增 | `build/` 已被 `.gitignore` 覆盖；多 profile 共用同名会覆盖 |
| 默认模板 release profile 默认 `export_objs = true` | **新项目行为变更** | 仅影响 `ezmk project new` / `project import` 之后新建的项目：`--profile release` 额外产出 `build/obj_files.zip`；既有项目不受影响 |
| 弃用面 | 无变化 | 归 2.0.0，见 2.0.x/REMOVALS.md |

## 6 延后项

- CLI 侧导出入口 `project export obj` / `build --obj-file`（复用导出 helper）。
- 归档确定性（固定 mtime / 字节可复现）。
- 依赖包对象导出、`ezmk test` 对象导出。
- 目录镜像导出（已被用户裁定改为归档，如后续需要再评估）。
