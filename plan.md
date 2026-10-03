# EazyMake 1.4.7 执行计划

> **状态：✅ 实现收口（未发布）**——本文档把设计文档 §4 转成可勾选的三阶段清单。1.4.x 系列路线图见 [`plans/1.4.x/README.md`](plans/1.4.x/README.md)；2.0.0 的移除清单见 [`plans/2.0.x/REMOVALS.md`](plans/2.0.x/REMOVALS.md)。
>
> 详细设计：[**1.4.7.md**](plans/1.4.x/1.4.7.md)。主题：**MSVC 工具链支持修复 + 工具链优先级调整**——修好 Build Tools 探测、vcvars cmd 引用、MSVC 环境注入，并把**优先级改为 g++/clang++ 优先、MSVC 回退**。
>
> **范围边界**：只修工具链探测/环境注入与优先级；唯一的“新面”是验证用环境变量 `EZMK_TOOLCHAIN`；**公共 API 无破坏性变更**。
>
> **⛔ 发布门槛**：① 阶段清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（基线 **1130 用例 / 6536 断言**，1.4.6 实现收口态）且新增用例只增不减；④ 附加门槛：`check_man_sync.py` 通过、`groff -man -Tutf8 -z -ww man/*.1 man/*.5` 零告警、i18n 三向一致、`check_docs_sync.sh` 通过。
>
> **说明**：1.4.6 已实现收口（未发布）；本版在其基础上修复 MSVC 支持。

---

## 1 背景

1.4.6 Q-20 的真实 MSVC 验证发现 MSVC 支持实际上从未可用：Build Tools 不被 vswhere 找到、vcvars 探测命令被 cmd 当作字面量、`load_msvc_env()` 从未被调用。本版修复这三点，并按用户裁定把工具链优先级改为 g++/clang++ 优先、MSVC 作为回退。详见设计 §1。

## 2 目标

| 组 | # | 覆盖 | 优先级 |
|----|---|------|--------|
| 探测/引用 | M-01/M-02 | vswhere `-products *` + BuildTools 回退；`cmd /c call` 引用修复 | P0 |
| 优先级/覆盖 | M-04/M-06 | g++/clang++ 优先、MSVC 回退；新增 `EZMK_TOOLCHAIN` 显式覆盖 | P0/P1 |
| 环境 | M-03 | `msvc_env()` + `apply_msvc_env()`，注入 compile/link/ar/test/pkg | P0 |
| 缓存 | M-05 | 工具链切换使编译缓存失效 + 用例 | P1 |

## 3 执行阶段（每阶段一个 commit，阶段间全量回归）

### 阶段一：探测与引用（M-01/M-02/M-04/M-06）

- [x] M-01 `find_vcvars64`：vswhere 加 `-products *`、回退补 `2022\BuildTools`
- [x] M-02 探测与 `load_msvc_env` 的 `cmd /c call` 引用修复
- [x] M-04 优先级：g++/clang++ 优先、MSVC 回退
- [x] M-06 新增 `EZMK_TOOLCHAIN`（msvc/gcc/clang；不可用明确报错）
- [x] 回归：全量零失败

### 阶段二：MSVC 环境注入（M-03/M-05）

- [x] M-03 `msvc_env()`（缓存一次）+ `apply_msvc_env()`；注入 compile / link / ar / test / pkg
- [x] M-05 工具链切换的缓存失效确认 + 用例
- [x] 回归：全量零失败

### 阶段三：测试/文档/收口

- [x] MSVC 定向集成用例（`EZMK_TOOLCHAIN=msvc` 守卫，不可用 SKIP）+ 本机实测复现
- [x] docs 补“Windows 工具链选择 / MSVC 回退 / `EZMK_TOOLCHAIN`”；`CHANGES.md` 新增 1.4.7 条目
- [x] i18n 三向一致（如需新增键）+ 索引状态（`plans/1.4.x/README.md` / `plans/README.md` / 根 `plan.md`）
- [x] 门槛复核：全量零回归 + `check_man_sync.py` + groff 零告警 + i18n 三向 + `check_docs_sync.sh`

## 4 关键设计决策

- **优先级反转**：MSVC 从“首选”降为“回退”——避免装了 Build Tools 的机器默认改用 MSVC，也符合本机/CI 的 MinGW 工作流。
- **环境注入收敛**：用 `apply_msvc_env(RunOptions&, Toolchain)` 一处注入，避免编译/链接/归档多处复制；`load_msvc_env` 结果进程内只跑一次。
- **显式覆盖优先于猜测**：`EZMK_TOOLCHAIN` 让 MSVC 路径可被强制验证；指定但不可用时明确报错，不静默回退。
- **不改公共契约**：`detect_toolchain()` 等签名不变；仅新增 `msvc_env` / `apply_msvc_env`。

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|---|---|---|
| 工具链优先级（MinGW 优先） | **行为变更（Windows）** | 本机/CI 不变；装了 Build Tools 的机器不再默认切 MSVC |
| MSVC 真正可用 | 行为修正 | 此前装了也无法用；现作为回退可用 |
| `EZMK_TOOLCHAIN` | 纯新增 | 未设置=自动 |
| MSVC 环境注入 | 内部行为 | 仅 Windows + MSVC 分支 |
| 公共 API | **无破坏性变更** | 仅新增 helper |

## 6 延后项

- 破坏性 API 变更 / 弃用面移除（归 2.0.0）。
- Linux/macOS 文件监视真递归。
- MSVC 开发者命令行的完整文档重写（仅补一段）。
