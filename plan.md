# EazyMake 1.1.2 执行计划

> 详细设计：[`1.1.2.md`](plans/1.1.2/1.1.2.md)。本计划为 1.1.x 稳定线补丁（安全加固 + 静默错误修复），**发布完成后恢复 1.2.0 执行计划**（`git checkout` 本提交前的 `plan.md` 版本）。
>
> **范围边界**：不新增命令、不弃用、不触碰 1.1.0 稳定 API。1.2.0 功能计划（`plans/1.2.0/`）不受影响。
>
> **⛔ 发布门槛**：实现完整 + API 兼容 + 全量测试零回归，三项同时满足才可发布（Gate 定义见 [1.1.0-pre.3](plans/1.1.0/1.1.0-pre.3.md#-发布门槛release-gate)）。

---

## 1 背景

2026-08-08 多模块代码质量评审发现两类问题：

1. **安全漏洞（4 处）**：解压 zip-slip 路径穿越、`ar`/`lib.exe` 命令注入、`ezmk_file_write` 硬限制逃逸、Lua 沙箱可逃逸。
2. **静默错误（7 处）**：链接 rename 假成功、缓存签名漏 `stdlib`/`pic`、`--locked` 误报、Windows 脚本全挂、TOML 写入不转义、非事务化安装、确定性数据竞争。

本版为纯修复补丁，逐项对应设计文档 §3。

---

## 2 执行阶段

### 阶段一：安全加固（S1–S4）

- [x] **1.1 解压安全**（S1）：`safe_extract_path()` + `extract_zip`/`extract_targz` 接入 + tar.gz 解压上限；`test_util.cpp` 恶意归档用例（另修复 miniz ABI 布局不一致：移除 `MINIZ_NO_TIME` 等宏）
- [x] **1.2 归档命令转义**（S2）：`compile_package()` 的 `ar`/`lib.exe` 路径转 `escape_shell_arg`（提取 `build_archive_command()` helper 便于单测）；`test_pkg.cpp` 含特殊字符路径用例
- [x] **1.3 file_write 边界**（S3）：硬限制三处改用既有 `norm_path()` + `path_within()`（目录边界）；`test_lua.cpp` 越界/边界用例
- [x] **1.4 Lua 沙箱收敛**（S4）：受限全局表（剔 `dofile`/`loadfile`/`load`/`require`/`debug`/`package`/`_G`）+ 沙箱 `_G` 自引用；`test_lua.cpp` 逃逸面用例；`docs/utils.md` 说明
- [x] 阶段一自测：`bash build.sh test`（561 用例/2714 断言）+ `bash build.sh test-all`（572/2772，含集成）零失败

### 阶段二：静默错误修复（C1–C7）

- [ ] **2.0 run_command RunOptions**（前置）：`RunOptions{timeout,cwd,env}` + POSIX fork 后 chdir/setenv + Windows lpCurrentDirectory/环境块；旧调用点迁移；`test_util.cpp` env/cwd 用例
- [ ] **2.1 链接假成功**（C1）：`atomic_rename_or_fail` + `execute_link` 接入
- [ ] **2.2 缓存签名**（C2）：`compile_options_signature` 加 `stdlib`/`use_pic`；`test_cache.cpp` 用例
- [ ] **2.3 lockfile 直接依赖**（C3）：`direct_deps` 元数据 + `depends_changed` 精确比较 + 生成侧写入；`test_lockfile.cpp`（新建）+ `--locked` 集成用例
- [ ] **2.4 Windows 脚本**（C4）：`run_script` 去 cd 前缀、走 `RunOptions.cwd`；`test_util.cpp` 命令串断言
- [ ] **2.5 TOML 转义**（C5）：`toml_quote()` + 两处写入器 + `repo.cpp` 收敛；round-trip 用例
- [ ] **2.6 安装事务化**（C6）：`remove_all`/`copy_recursive` 抛错 + 调用点过一遍；`pkg.cpp` swap 流程；失败保留旧版用例
- [ ] **2.7 确定性竞争**（C7）：`compile_one_source` 改 env 注入，删 `setenv`/恢复；`-j4` 确定性集成用例
- [ ] 阶段二自测：`bash build.sh test` 相关用例通过

### 阶段三：回归与发布准备

- [ ] 全量回归：`bash build.sh test` + `test-all` 零回归
- [ ] `CHANGES.md` 新增 1.1.2 条目（口径：安全加固 + 正确性修复；命令/配置/CLI 不变）
- [ ] 版本号预置：`include/ezmk/version.hpp` / `build.sh` 默认版本 `1.1.2`
- [ ] **发布门槛预检**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归
- [ ] 打 `v1.1.2` tag 触发 `release.yml`；产物核验 + 分发（沿用 1.1.1 流程）
- [ ] 收尾：`plan.md` 恢复 1.2.0；`plans/README.md` / `plans/1.1.2/README.md` 状态更新

> 门槛未满足即停止，禁止带着未收口项打 tag。

---

## 3 关键设计决策

| 决策 | 说明 |
|------|------|
| **`run_command` 加 `RunOptions{cwd, env}`（C4+C7 公共基础）** | POSIX 在 fork 后子进程内 `chdir`/`setenv`（无竞争）；Windows 走 `lpCurrentDirectory` + 环境块。旧签名保留，行为不变 |
| **解压路径包含校验（S1）** | `safe_extract_path()` 统一拒绝 `..`/绝对/盘符/UNC，经 `lexically_normal` 验证在 dest 内 |
| **沙箱受限全局表（S4）** | `__index` 从裸 `_G` 改为受限 `_G`（剔 `dofile`/`loadfile`/`load`/`require`/`debug`）；`linit.c` 不动 |
| **lockfile `direct_deps`（C3）** | 元数据记录根项目直接依赖（含约束），`depends_changed` 精确比较；旧文件缺字段视为 changed |
| **安装事务化（C6）** | swap 前不动旧包：stage 就绪 → 旧包 rename `.bak` → stage rename → 删 `.bak` |
| **确定性 env 注入（C7）** | 删 `setenv`/恢复，`SOURCE_DATE_EPOCH` 经 `RunOptions.env` 传给编译子进程 |

---

## 4 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 解压路径校验（S1） | 恶意/异常归档被拒 | 行为收敛；正常包不受影响 |
| `ar`/`lib.exe` 转义（S2） | 含特殊字符文件名的包 | 修复后正常归档 |
| `file_write` 边界（S3） | 依赖 `../` 越界写盘的工具 | 修复前是漏洞；项目内写不受影响 |
| Lua 沙箱收敛（S4） | 依赖文件加载函数的 utils 脚本 | 这些函数变 `nil`；官方 utils 无依赖；hooks 不受影响 |
| link rename 检查（C1） | 链接产物被占用（Windows） | 修复前假成功；修复后明确报失败 |
| 缓存签名加 stdlib/pic（C2） | 改 stdlib 或 static↔shared 的项目 | 触发一次重建（正确行为） |
| lockfile `direct_deps`（C3） | 旧 lockfile 无该字段 | 视为 changed → 重新生成；`--locked` 语义修正 |
| Windows 脚本（C4） | Windows 安装脚本 | 修复前不可用；修复后可用 |
| TOML 转义（C5） | 含特殊字符的项目名/包名 | 修复前写坏配置；修复后 round-trip 正常 |
| 安装事务化（C6） | 安装中途失败 | 修复前旧包被删；修复后旧版保留 |
| 确定性 env 注入（C7） | `deterministic` 并行构建 | 修复前输出不确定；修复后确定 |
| `run_command(cmd)` 旧签名 | 无 | 保留，行为/输出不变 |
| 无新命令 / 无弃用 | 无 | 纯修复补丁，不触碰 1.1.0 稳定 API |

---

## 5 延后项

- **评审其余加固项**（run_command 句柄泄漏/POSIX 双 close、宽窄字符路径、`include_dirs=[]` 静默覆盖、i18n 二次替换、手写 JSON 解析器、watcher 静默死亡/kqueue 缺口、测试套件永真断言清理）——超出本补丁范围，记入后续版本（可在 1.2.0 或独立补丁处理）。
- **`--locked` 对直接依赖版本约束的完整校验**：本版 `depends_changed` 纳入约束变化检测；更细粒度（含传递依赖 pin）由 1.2.0 的锁文件演进承接。

---

## 6 版本路线图

```
1.1.1 (已发布) ──→ 1.1.2 (安全加固 + 静默错误修复) 🔄 本计划
                 → 1.2.0 (功能计划：project cc / CMake 导出 / 模板 Profile)
                 → 2.0.0 (未来) —— 破坏性变更窗口
```
