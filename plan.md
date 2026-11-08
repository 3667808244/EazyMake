# EazyMake 1.5.0-dev.10 执行计划

> **状态：✅ 实现完成（2026-11-08），待 CI 与 pre.2 发布复核** —— 详细设计见 [plans/1.5.x/1.5.0-dev.10.md](plans/1.5.x/1.5.0-dev.10.md)。dev.9 已收口（1226/7058；CI run 42363070935 全绿）；本版完成后与 dev.8/dev.9 一起进入 pre.2。
>
> **回归**：单元 **1106 / 6296 / 4 跳过 / 0 失败**；全量 `test-all`（`EZMK_TEST_OFFLINE=1`）**1227 / 7077 / 8 跳过 / 0 失败**。
>
> **目标**：补齐 8 个全局配置项 —— `cxx` / `cc` / `linker` / `use_cache` / `network_timeout` / `retries` / `proxy` / `install_prefix`。

## 1 背景

编译器路径只认 `CXX`/`CC`，关闭缓存只有 `--disable-cache`，网络无统一超时/重试/代理，安装前缀只能项目级配置。本版把它们变成 opt-in 的 `[settings]` 默认值。

## 2 执行阶段

### 阶段一：配置骨架（P0）
- [x] `SettingsConfig` 增加 8 个字段与默认值（空 / 空 / 空 / `true` / `0` / `0` / 空 / 空）
- [x] 类型、范围（timeout 0..3600、retries 0..10）、代理 scheme 校验
- [x] user_config / global_config / man 文档表
- [x] 单测：默认 / 合法 / 非法 / global→user 覆盖

### 阶段二：工具路径与缓存（P0）
- [x] `cxx` / `cc` / `linker` 接入工具链与链接命令；空 = 自动探测
- [x] 优先级：环境变量 > settings > 自动探测
- [x] `~` 展开、相对路径按项目根、缺失路径使用点 fatal
- [x] 缓存签名纳入实际编译器路径
- [x] `use_cache=false` 关闭构建/测试缓存；CLI `--disable-cache` 仍最高
- [x] 单测 + 回归

### 阶段三：网络与安装前缀（P0）
- [x] `network_timeout` 接入 HTTP 下载 / git clone / git pull
- [x] `retries` 只重试超时、连接失败、5xx；4xx/校验/认证失败不重试
- [x] `proxy` 注入 git 子进程环境与 WinHTTP 下载；凭据脱敏
- [x] `install_prefix` 优先级：CLI > 项目 `[install].prefix` > settings > 平台默认
- [x] 单测 + 回归

### 阶段四：收口（P0）
- [x] CHANGES / 索引 / 根 plan
- [x] 全量 `test-all` 零失败（基线 1227/7077）
- [ ] CI 绿色
- [ ] 与 dev.8/dev.9 一起进入 pre.2

## 3 关键设计

1. **全部 opt-in**：未配置时行为与 dev.9 完全一致。
2. **环境变量仍最高**：`CXX`/`CC` 可临时覆盖 settings；`linker` 不引入新环境变量。
3. **路径解析**：支持 `~` 与项目根相对路径；缺失路径不静默回退。
4. **缓存开关**：只做全局默认与 CLI 覆盖，不新增 `cache_dir`。
5. **网络重试**：只重试瞬态失败；4xx/校验/认证失败立即报错。
6. **安装前缀**：项目级仍高于用户级；CLI 最高。

## 4 兼容性

| 变更 | 影响 | 说明 |
|------|------|------|
| 新增 cxx/cc/linker | 纯 opt-in | 空 = 现有探测；环境变量优先 |
| 新增 use_cache | 纯 opt-in | 默认 true，行为不变 |
| 新增网络三键 | 纯 opt-in | 默认 0/0/空，行为不变 |
| 新增 install_prefix | 纯 opt-in | 默认空，回落到平台默认 |
| 公共 API | 无破坏性变更 | 仅新增配置键与内部解析 |

## 5 后续

- 本版与 dev.8/dev.9 一起完成 pre.2。
- 正式版聚合、版本号、tag 与三渠道分发待 dev.8 + dev.9 + dev.10 + pre.2 完成后执行。
- 暂不做 `cache_dir`、全局 include/src/macros、pack 压缩级别。
