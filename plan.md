# EazyMake 1.1.0-pre.1 执行计划

> 详细设计：[`plans/release/1.1.0-pre.1.md`](plans/release/1.1.0-pre.1.md)
>
> **状态：已完成。** 全量测试 545 用例 / 2613 断言，零回归。
>
> 代码实现（`ced2e05` ~ `2049f33`）在 plan.md 编写前已完成；本次执行补齐了剩余项（zsh 补全、CLAUDE.md 更新）并验证了全量测试回归。

---

## 1 背景

当前 `1.1.0-dev.7` 已将所有核心功能实现完毕。在合入 `1.1.0` 正式版之前，需要改善用户首次接触 EazyMake 的体验：

1. **命令冗长**：所有命令都是 `ezmk project <action>` 形式，新用户记住压力大
2. **`--help` 信息密度低**：分组不直观，日常命令和高级功能混在一起
3. **README 信息过载**：技术栈、依赖表等开发者细节占位过多，普通用户一目不了然
4. **安装方式单一**：仅有 `curl | bash` 一种方式，缺少包管理器支持
5. **API 稳定性未承诺**：用户担心未来破坏性变更

---

## 2 执行状态总览

| # | 目标 | 优先级 | 状态 |
|---|------|--------|------|
| 1 | 顶层命令别名（7 个） | P0 | ✅ 已实现（`0fe217a`） |
| 2 | `--help` 输出重组 | P1 | ✅ 已实现（`0fe217a`） |
| 3 | README 精简重写 | P1 | ✅ 已实现（`031de13`） |
| 4 | API 冻结承诺 | P1 | ✅ 已实现（`031de13`） |
| 5 | zsh 补全更新 | P1 | ✅ 本次补齐 |
| 6 | 包管理器分发 | P2 | ✅ 已完成 |
| 7 | 编译 + 回归验证 | P0 | ✅ 545 用例 / 2613 断言，零回归 |

---

## 3 本次执行（补齐项）

### 3.1 zsh 补全更新（`completions/_ezmk`）

- [x] 顶层别名补全：`build`/`run`/`clean`/`watch`/`install`/`test`/`pack`
- [x] `project install` + `project pack` + `project test` 子命令补全
- [x] `pkg install` 新增 `--locked` / `--no-lock` 标志
- [x] 不包含单双字母简写（`pb`/`pr`/`ki`/…）
- [x] 主 dispatch 支持顶层别名直接路由到对应补全函数

### 3.2 CLAUDE.md 更新

- [x] Quick reference 改为顶层别名优先（`ezmk build` / `ezmk test`）

### 3.3 编译与回归验证

- [x] `bash build.sh` 编译通过
- [x] `bash build.sh test` 全量测试通过：545 用例 / 2613 断言，零回归

---

## 4 延后项

（无 — 全部已完成或本次补齐）

---

## 5 涉及文件变更摘要

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `src/cli.cpp` | 修改 | 顶层别名展开 + `print_usage()` 重写（`0fe217a`） |
| `include/ezmk/i18n_keys.def` | 修改 | 新增 6 个 pre.1 key（`0fe217a`） |
| `locale/en.json` | 修改 | 英文翻译更新（`0fe217a`） |
| `locale/zh.json` | 修改 | 中文翻译更新（`0fe217a`） |
| `README.md` | 重写 | 精简为用户面向（`031de13`） |
| `README_ZH.md` | 重写 | 中文同步（`031de13`） |
| `docs/en/technical.md` | **新建** | 技术栈/依赖表迁移（`031de13`） |
| `CHANGES.md` | 修改 | API Stability 章节（`031de13`） |
| `completions/_ezmk` | 修改 | 顶层别名 + project install/pack/test + --locked/--no-lock（**本次**） |
| `CLAUDE.md` | 修改 | Quick reference 顶层别名（**本次**） |
| `plan.md` | 重写 | pre.1 执行计划（**本次**） |

---

## 6 版本路线图

```
1.0.0 (正式版) ──→ 1.1.0-dev.1~7 (包编译与开发体验) ✅
                 → 1.1.0-pre.1 (改善用户触达) ✅
                 → 1.1.0-pre.2 (文档检查)
                 → 1.1.0-pre.3 (CI / 包管理器分发)
                 → 1.1.0 (正式版发布)
```
