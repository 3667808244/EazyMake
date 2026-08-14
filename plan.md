# EazyMake 1.2.0-dev.6 执行计划

> **状态：进行中**。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[`1.2.0-dev.6.md`](plans/1.2.0/1.2.0-dev.6.md)。本计划为 1.2.0 系列第六个开发子版本：**各源文件构建耗时统计**——为 `ezmk build` 并行编译路径补上 per-file 编译耗时明细（`-v` 全量 / 慢构建自动 top-N），零配置、不新增 flag、不改变构建语义。
>
> **范围边界**：仅 `src/build.cpp` `compile_phase()` 并行分支计时 + 完成汇总块扩展；串行路径（`compile_sources`）仅总耗时、不输出明细。纯诊断增强，不触碰 cache 模块、公共 API 与 CLI flag。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更（纯新增诊断输出）；③ 全量测试零回归（Gate 定义见 [1.1.0-pre.3](plans/1.1.x/1.1.0-pre.3.md#⛔-发布门槛release-gate)）。

---

## 1 背景

`ezmk build` 已显示总耗时（0.9.6+，`build_elapsed_time`，并行模式），但用户无法知道**哪个源文件最慢**。本计划为构建补上 per-file 编译耗时明细，让"慢在哪一步"一目了然——纯诊断增强。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 并行编译路径记录每个源文件的实际编译耗时（单次 `compile_one_source()` 粒度） | P0 |
| 2 | `-v/--verbose` 时始终输出按耗时降序的完整明细（仅本次实际编译的非缓存文件） | P0 |
| 3 | 默认构建总耗时 > 阈值（5s）时自动输出最慢 top-N（10）+ 汇总行；不刷屏 | P0 |
| 4 | 总耗时输出不变；串行路径仅总耗时、不输出明细 | P1 |
| 5 | 不新增 CLI flag、不新增配置字段（复用 `-v` + 阈值常量） | P0 |
| 6 | 单测/集成覆盖输出触发与格式；全量测试零回归 | P0 |
| 7 | i18n 新 key（en/zh）+ 文档（cli.md build 说明 / CHANGES.md） | P1 |

## 3 执行阶段

### 阶段一：计时 + 汇总

- [ ] **1.1 计时**（4.1）：`src/build.cpp` `compile_phase()` 并行任务 lambda 内 wrap `compile_one_source()`，本地收集 `compile_times[i]`（毫秒，按源文件索引，与 `single_results` 对应）
- [ ] **1.2 汇总**（4.2）：过滤 cache_hit（只计非缓存实际编译），按耗时降序排序
- [ ] **1.3 输出**（4.3）：扩展完成汇总块——`-v` 全量明细 / 总耗时 > `BUILD_TIME_SLOW_THRESHOLD`(5.0s) 时 top-`BUILD_TIME_TOP_N`(10) + 汇总行；`build_elapsed_time` 输出不变

### 阶段二：i18n

- [ ] **2.1 i18n**（4.4）：`include/ezmk/i18n_keys.def` + `locale/en.json` + `locale/zh.json` 新增 `build_time_header` / `build_time_entry` / `build_time_truncated`；`scripts/check_i18n.py` 三向一致

### 阶段三：测试

- [ ] **3.1 集成测试**（4.5）：`test/test_integration.cpp` 新增用例——`-v` 构建输出明细行（不校验具体耗时——非确定性）；小项目默认构建不刷屏明细；`test/test_i18n.cpp` 新增 key 非空断言
- [ ] **3.2 全量回归**（4.6）：`bash build.sh test-all` 零回归

### 阶段四：文档收口

- [ ] **4.1 文档**：`docs/en/cli.md` / `docs/zh/cli.md` 补充 build 耗时明细触发规则（`-v` 全量 / 慢构建自动 top-N）；`CHANGES.md` dev.6 条目
- [ ] **4.2 收口**：本计划勾选 `[x]`；`plans/1.2.0/README.md` dev.6 状态「待实现 → 已完成」

> 门槛未满足即停止，禁止带着未收口项进入下一子版本。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 不新增 flag / 配置字段 | 复用 `-v` + 命名常量，零配置面，符合"易用优先" |
| 阈值 5s / top-10 为常量 | `BUILD_TIME_SLOW_THRESHOLD` / `BUILD_TIME_TOP_N`，不做成配置字段 |
| 只统计非缓存文件 | 明细反映**实际编译**成本，避免把缓存命中误报为"编译快" |
| 串行路径不细化 | 避免侵入 cache 模块重构，改动最小 |
| 计时在 build.cpp | 不改 `cache.hpp` / `SingleCompileResult`；`compile_times` 按源索引并发写（各写不同元素，安全） |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增耗时明细输出 | 纯新增诊断输出 | 仅 `-v` 或慢构建（>5s）出现；默认总耗时行为不变 |
| 无新 CLI flag / 无新配置字段 | 无 | 复用 `-v` + 常量阈值 |
| 新增 i18n key | 纯新增 | `.def` 三行 + 两份 JSON，`check_i18n.py` 校验 |
| 串行路径无明细 | 小项目仅总耗时 | 范围边界，文档说明 |
