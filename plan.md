# EazyMake 1.5.0-dev.9 执行计划

> **状态：📝 待执行（2026-11-07 立项）** —— 详细设计见 [plans/1.5.x/1.5.0-dev.9.md](plans/1.5.x/1.5.0-dev.9.md)。dev.8 已收口（1224/7046；CI run 42347726309 全绿）；本版完成后与 dev.8 一起进入 pre.2。
>
> **目标**：新增 `[settings].compiler_output`，控制成功编译时编译器诊断的显示方式：`always`（总是）/ `warning`（有警告时）/ `error`（有错误时，默认保持旧行为）。

## 1 背景

当前 `compile_one_source()` 只在编译失败时把 stderr/stdout 放进 `error_msg`；成功编译的警告被丢弃。用户无法在正常构建中看到 `-Wall` 警告，也无法选择只看警告或全部诊断。

## 2 执行阶段

### 阶段一：配置与数据（P0）
- [ ] `SettingsConfig.compiler_output` 默认 `"error"`
- [ ] 解析并校验 `always` / `warning` / `error`，非法值加载期报错
- [ ] `SingleCompileResult` 增加 `compiler_output`
- [ ] `CompileResult` 增加成功编译的输出集合
- [ ] `compile_one_source()` 成功/失败都保存 stderr+stdout
- [ ] 单测：默认/合法值/非法值、warning 检测

### 阶段二：输出策略（P0）
- [ ] `build.cpp` 串行与并行路径按策略输出成功诊断
- [ ] `warning` 识别 GCC/Clang `warning:` 与 MSVC `warning C`
- [ ] `always` 输出全部；`error` 保持旧行为；失败诊断始终输出
- [ ] `util::compiler_output()` 原样写 stderr，不加 `[ezmk]` 前缀
- [ ] 缓存命中不产生编译器输出
- [ ] 手工验证三种策略 + 单元回归

### 阶段三：文档 / 闸门 / pre.2（P0）
- [ ] user_config、man、CHANGES、索引更新
- [ ] `check_config_docs` / man / docs / i18n / groff
- [ ] 全量 `test-all` 零失败（基线 1224/7046）
- [ ] CI 绿色
- [ ] 与 dev.8 一起进入 pre.2

## 3 关键设计

1. **默认 error**：保持 1.5.0-dev.8 的行为，成功警告仍不显示；只有显式配置才改变。
2. **编译阶段**：本键只控制编译诊断；链接失败仍走现有固定错误输出。
3. **并行安全**：worker 只收集输出，主线程在 futures join 后按顺序打印，避免交错。
4. **不缓存输出**：诊断只在本次编译产生；缓存命中不会重放旧警告。
5. **与 log_level 独立**：`compiler_output=always` 时即使 `log_level=off` 也显示编译器诊断。

## 4 兼容性

| 变更 | 影响 | 说明 |
|------|------|------|
| 新增 `compiler_output` | 纯 opt-in | 默认 error；未配置时行为与 dev.8 一致 |
| 成功编译警告 | 仅显式开启时输出 | 不改变编译结果、缓存或退出码 |
| 公共 API | 无破坏性变更 | 只新增配置键与内部结果字段 |

## 5 后续

- 本版与 dev.8 一起完成 pre.2，不单独形成发布检查点。
- 正式版聚合、版本号、tag 与三渠道分发待 dev.8 + dev.9 + pre.2 完成后执行。
