# EazyMake 1.2.0-dev.12 执行计划

> **状态：执行中**（2026-08-15）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.12.md`](plans/1.2.0/1.2.0-dev.12.md)。本计划为 1.2.0 系列第十二个开发子版本：**测试配置收口**——`ezmk test` 引入 profile 支持（`[test].default_profile` + `--profile` CLI，复用 `[compile.profile.*]` / `[link.profile.*]`，与 dev.3 的 build 侧完全对称），并补齐测试专属 include / 链接目标（`[test].include_dirs` / `[test].link_targets`），弃用与 `[compile].flags` 重叠的 `[test].flags`（使用点 warn，2.0.0 移除）。
>
> **范围边界**：只动 `[test]` 配置节与 `run_tests` 路径（+ `apply_profile` 共享 helper 抽取，build 路径行为不变）；不新建 test profile 表；不碰 precompiled 命名（dev.10）与代码审查（dev.11）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯新增可选字段 + CLI 选项；`[test].flags` 仅弃用不删除）；③ 全量测试零回归（基线 719 用例 / 3328 断言，dev.9 后；Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

- **`ezmk test` 无 profile 支持**：CLI 只有 `-f/--framework`、`--filter`、`-V/--verbose`（`src/cli.cpp:344-348`）；`run_tests` 编译测试用裸 `cfg.compile.*`、链接用裸 `cfg.link.*`——无法跑 Release / 调试模式的测试，与 `ezmk build --profile` / `[compile].default_profile`（dev.3）不对称。
- **`[test].flags` 与 `[compile].flags` 语义重叠**：测试编译已经带上全部 `[compile]` 标志（`build.cpp:1743-1761`），`[test].flags` 是第二个注入点，与 profile 机制重叠。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `[test].default_profile`：`ezmk test` 未传 `--profile` 时回退应用 `[compile.profile.<name>]` + `[link.profile.<name>]` | P0 |
| 2 | `ezmk test --profile <name>`：CLI 覆盖 default_profile（CLI > default_profile > 无） | P0 |
| 3 | `[test].include_dirs`：测试编译追加的 `-I` 目录（相对项目根、缺失跳过），不污染主构建 | P0 |
| 4 | `[test].link_targets`：测试 runner 链接追加的 `-l` 目标（catch2 与 ezmk 双框架一致），不污染主构建 | P0 |
| 5 | profile 解析/合并/未知 profile fatal 与 build 单一事实源（`apply_profile` 共享 helper，build 行为不变） | P0 |
| 6 | `[test].flags` 弃用：使用点 warn（i18n），行为不变，2.0.0 移除 | P1 |
| 7 | 文档 + i18n + 测试；全量测试零回归 | P0 |

## 3 执行阶段

### 阶段一：配置 + CLI（4.1）

- [ ] **1.1 配置**（4.1）：`include/ezmk/config.hpp` `TestConfig` 增 `default_profile` / `include_dirs` / `link_targets`（`flags` 注释标 DEPRECATED）；`src/config.cpp` `parse_test` 解析三个新字段（`default_profile` 镜像 dev.3 写法，`include_dirs`/`link_targets` 用 `extract_string_array`）
- [ ] **1.2 CLI**（4.1）：`include/ezmk/cli.hpp` `CliArgs` 增 `std::string test_profile;`；`src/cli.cpp` `ezmk project test` 选项表增 `--profile`（有值）；`src/main.cpp` `ProjectTest` 分支透传 `args.test_profile`

### 阶段二：run_tests 改造（4.2）

- [ ] **2.1 `apply_profile` 共享 helper**（4.2）：`src/build.cpp` 抽取 `struct AppliedProfile { CompileSection compile; LinkSection link; }` + `apply_profile(cfg, cli_profile, default_profile)`——含 compile/link profile 合并 + 未知 profile fatal（closest-match 建议）；`prepare_build_state` 内联块（497-539）替换为调用，行为逐字节不变
- [ ] **2.2 `run_tests`**（4.2）：签名增 `const std::string& test_profile`（`include/ezmk/build.hpp` 同步）；开头 `cfg.test.flags` 非空 → `util::warn(I18nKey::test_flags_deprecated)`；`base_flags` 构建（1743-1761）改用 `apply_profile(cfg, test_profile, cfg.test.default_profile)` 结果的 compile 配置，并在其后追加 `[test].include_dirs`（相对项目根、缺失跳过）与 `[test].flags`；链接（1897-1899 / 2089-2091）改用结果的 link 配置，并追加 `[test].link_targets` 的 `-l<target>`

### 阶段三：i18n（4.3）

- [ ] **3.1 i18n**（4.3）：`test_flags_deprecated` 三向一致（`i18n_keys.def` + `locale/en.json` + `locale/zh.json`），`scripts/check_i18n.py` 通过；`bash build.sh` 编译通过

### 阶段四：测试与全量回归（4.4）

- [ ] **4.1 单测**（4.4）：`test_config.cpp` `[test].default_profile` / `include_dirs` / `link_targets` 解析 + `flags` 仍解析；`test_cli.cpp` `ezmk test --profile <name>` 解析到 `args.test_profile`
- [ ] **4.2 集成**（4.4）：`test_integration.cpp` 增——`[test].default_profile` 生效（ezmk 内建框架：profile 宏出现在测试编译中、断言通过）；`ezmk test --profile <name>` 覆盖 default_profile；`[test].include_dirs` 参与测试编译；`[test].link_targets` 参与测试链接；`[test].flags` 弃用 warn 出现在输出中
- [ ] **4.3 全量回归**（4.4）：`bash build.sh test-all` 零回归（基线 719 用例 / 3328 断言，dev.9 后；新增用例/断言在其上增加）

### 阶段五：文档收口（4.5）

- [ ] **5.1 文档**（4.5）：`docs/en|zh/config_file.md` `[test]` 节（`default_profile` 字段 + `flags` 标 DEPRECATED）+ `docs/en|zh/cli.md`（`ezmk test --profile`）+ `CHANGES.md` dev.12 条目（中文基准，再同步英文）

### 阶段六：收口（4.6）

- [ ] **6.1 收口**（4.6）：本计划勾选 `[x]`；`plans/1.2.0/README.md` dev.12 状态「待实现 → 已完成」；发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 复用 `[compile.profile.*]` / `[link.profile.*]` | 不新建 `[test.profile.*]` 表；测试编译 = 项目基础 flags + 选中 profile，与 build 共享同一套命名配置，零新增配置面 |
| `--profile`（CLI）> `default_profile` > 无 | 与 `ezmk build` 完全对称；CLI 可临时覆盖默认 |
| `include_dirs` / `link_targets` 测试专属 | 默认 `[]`，不设时零变化；仅测试路径生效，主构建不受污染；镜像 `[compile].include_dirs` / `[link].system_targets` 语义 |
| `apply_profile` 共享 helper | build 与 test 共用 profile 解析/合并/未知-fatal 单一事实源；build 路径重构但行为逐字节不变（全量回归验证） |
| 弃用 warn 在使用点（run_tests）打 | 不在 parse_config 打——parse 对每个依赖包也调用，避免噪音；warn 恰好在使用弃用特性时触发 |
| `[test].flags` 弃用期行为不变 | 仍追加到测试编译标志；2.0.0 移除；替代写法 profile + default_profile / include_dirs / link_targets |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `[test].default_profile` | 纯新增可选字段 | 不设时行为与现状完全一致 |
| 新增 `[test].include_dirs` / `[test].link_targets` | 纯新增可选字段 | 不设时行为与现状完全一致；仅测试路径生效，不污染主构建 |
| 新增 `ezmk test --profile` | 纯新增 CLI 选项 | 与 build 对称 |
| `[test].flags` 弃用 | 使用点 warn，行为不变 | 2.0.0 移除；替代写法 `[compile.profile.<name>]` + `default_profile`，include/链接用新字段 |
| `apply_profile` 抽取 | build 路径重构 | 行为逐字节不变，全量回归验证 |
| 测试编译/链接改用合并后配置 | 仅当设置了 profile 时改变 | 默认无 profile → 零变化 |

## 6 延后项

- **`[test].flags` 移除**：2.0.0 破坏性变更窗口统一移除（与 `ezmk utils cc` 同批）。
- **test 专属 profile 表**（`[test.profile.*]`）：当前决策为复用 compile/link profile 表；若未来需要 test-only 标志集，2.0.0 窗口再议。
- **dev.10 / dev.11 独立**：本版不碰 precompiled 命名（dev.10）与代码审查（dev.11）。
- **回归基线**：全量测试零回归（dev.9 后基线 719 用例 / 3328 断言），作为硬门槛。
