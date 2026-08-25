# EazyMake 1.3.4 执行计划

> **状态：执行中**。1.3.x 系列路线图见 [`plans/1.3.x/README.md`](plans/1.3.x/README.md)。
>
> 详细设计：[**1.3.4.md**](plans/1.3.x/1.3.4.md)。本计划为 1.3.0 发布后的**补丁版本**：`ezmk watch --run` / `-r` —— 每次**成功**重建后**阻塞运行**产物，程序退出后 watch 继续监听（"改代码自动重跑"）。watcher 线程阻塞 = 程序运行期间天然暂停变更检测，无需进程管理。
>
> **范围边界**：仅 `executable` 项目（其余类型 + `--run` → 启动 fatal）；构建失败不运行（只在成功分支）；程序非零退出 → 警告**不退出 watch**；**无 `--run` 时默认行为完全不变**。拒绝形态（kill-重启 / detached 启动 / 缓存命中才运行）为定死边界。`--` 参数透传与 `workspace watch` 归 1.4.0。**公共 API 无破坏性变更**（纯新增 flag + i18n key）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 899 用例 / 5203 断言，1.3.3 后实测；原基线 863/5007）。

---

## 1 背景

- `ezmk watch`（`src/main.cpp:300-408`）是无限重建循环，但**从不运行产物**；"改代码自动重跑"只能手动 `ezmk run` 或脆弱的外壳循环。
- 基础设施已具备：`build_project()` 返回 exe 路径（`project run` 在用，`main.cpp:198`）；`util::run_command` 阻塞（watch 回调在 watcher 线程，阻塞 = 运行期间暂停检测）；`-r` 短 flag 未被占用。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | `ezmk watch --run` / `-r`：每次成功重建后阻塞运行产物；程序退出后 watch 继续监听 | P0 |
| 2 | 默认行为不变：无 `--run` 时与现状完全一致 | P0 |
| 3 | 仅 `executable`：`static`/`shared`/`utils` + `--run` → 启动时 fatal | P0 |
| 4 | 构建失败 → 不运行（现有 catch 分支），继续 watch | P0 |
| 5 | 程序非零退出 → 警告，不退出 watch | P0 |
| 6 | 与 `--no-build-on-start` 正交（首次运行发生在第一次变更后） | P1 |
| 7 | 文档：cli.md（en/zh）+ CHANGES.md 1.3.4 条目；集成测试 | P1 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：flag 解析

- [ ] **1.1 watch spec 加 `{'r', "run", false}`**（`src/cli.cpp:280-282`）+ `CliArgs.watch_run`（`include/ezmk/cli.hpp`，`watch_no_build_on_start` 旁）+ `p.has("run")` 读取
- [ ] **1.2 i18n key**：`cli_err_run_needs_executable` / `watch_run_exit_nonzero`（三向一致 + `check_i18n.py` 通过 + 重新生成 `locale_data.cpp`）

### 阶段二：执行逻辑

- [ ] **2.1 类型门禁**：watch 启动时（初始构建前）`cfg.project.type != "executable"` && `watch_run` → fatal（`cli_err_run_needs_executable`）
- [ ] **2.2 共享运行 helper**：watch case 内抽取（复用 `running` key / `run_command` / stdout/stderr 回显）；非零退出 → `watch_run_exit_nonzero` 警告（不退出）
- [ ] **2.3 两条回调路径接入**：config 变更（`:352-363`）与源/头变更（`:366-376`）的成功分支捕获 `build_project()` 返回值 → `--run` 时阻塞运行；catch 分支不运行（坑 4）

### 阶段三：集成测试

- [ ] **3.1 用例**：① executable + `--run`：改源 → 轮询断言标记输出出现（后台进程 + poll 模式）② 构建失败不运行、watch 存活 ③ 非 executable + `--run` → 启动 fatal ④ 无 `--run` → 只有构建输出、无程序输出
- [ ] **3.2 全量回归**（基线 899/5203）

### 阶段四：文档 + 收口

- [ ] **4.1 cli.md（en/zh）**：watch 节补 `--run` 语义 + 生命周期说明（阻塞运行/非零退出警告/Ctrl+C 同进程组/长驻程序暂停检测）
- [ ] **4.2 CHANGES.md**：1.3.4 条目
- [ ] **4.3 收口**：plan.md 勾选；`plans/1.3.x/README.md` 状态更新；发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 运行时机 = 每次成功重建后 | 简单可预测；"缓存命中也要跑"（看到最新结果） |
| 生命周期 = 阻塞运行 | watcher 线程阻塞 = 运行期间暂停检测，退出后恢复；免费的正确性，零进程管理 |
| 非零退出 = 警告不退出 | watch 是循环，非一次性 run |
| Ctrl+C 同进程组一起终止 | 用户意图"全停"；SIGINT 处理不受影响 |
| 拒绝 kill-重启 | 自动杀用户进程危险，违反"小而直" |
| 拒绝 detached 启动 | 不管理生命周期 → 堆积进程 |
| 拒绝仅缓存未命中时运行 | UX 怪异 |
| 首次运行发生在第一次变更后 | 与 `--no-build-on-start` 正交；初始构建不运行 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `ezmk watch --run` / `-r` | 纯新增 flag | 无 `--run` 时行为与现状完全一致 |
| `build_project()` 返回值复用 | 无行为变化 | 返回值本已存在（run 在用），watch 不再丢弃 |
| 非 `executable` + `--run` | 新增启动错误 | 仅显式传 `--run` 时触发 |
| 公共 API | 无破坏性变更 | 纯增量（新 flag + 新 i18n key） |

## 6 延后项（明确收口）

- **`--run` 的 `--` 参数透传**（watch 现 `reject_positionals`）：归 1.4.0 或后续评估。
- **`workspace watch` 命令组**（`-w` 只重定向 build/test/clean）：归 1.4.0 或后续评估。
- **kill-重启 / detached 启动 / 仅缓存未命中时运行**：§3.2 决策记录，定死边界不做。
- **2.0.0**：保持破坏性变更窗口，与本版解耦。
