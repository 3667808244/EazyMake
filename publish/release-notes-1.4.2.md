# EazyMake 1.4.2 — 代码质量审计修复（第二轮）

1.4.1 发布后对 v1.4.1 全量代码做**六路并行逐行审计 + 独立核查**的修复落地（对照 1.4.0-dev.6 / 1.3.6 审计先例）。审计结论：无 zip-slip / SHA 绕过 / 可利用内存破坏（除一处 Lua UB），但发现 5 项 P0（崩溃 / 缓存正确性失效）、10 项 P1、7 项 P2、8 项 P3、6 项 P4 与一批低危健壮性问题——**本版全部落地**（低危项择优随附，余者明确延后至后续版本）。**零功能新增、公共 API 无破坏性变更**（纯缺陷修复 + 内部签名调整）。

## 崩溃 / 正确性（P0）

- **Lua 报错对象 UB**：`error({...})` / `assert(false, {...})` / `error(42)` 等非字符串错误对象此前直接 `std::string(lua_tostring(L,-1))`（返回 `NULL`）→ CLI 崩溃；11 处报错路径统一走安全提取（数字保留文案，其余降级 `unknown Lua error`）。另修 Lua 栈预扩：json ↔ Lua 递归期间栈重分配会堆损坏（≥36 层嵌套必崩，基线预存缺陷）
- **`workspace watch` 成员饿死**：改「每选中成员一个线程」（原先线程池大小 = `-j`，成员数 > jobs 时后续成员永不启动，Ctrl+C 后还会从队列拉起新 watcher）
- **MSVC 依赖跟踪失效**：`/showIncludes` 注解改从 stdout 解析（原读 stderr → 依赖集恒空、改头文件永不重编）+ 前缀本地化无关（en / zh-CN 半/全角 + 冒号后路径形态兜底）。**修复后 MSVC 首次全量重编一次属预期**
- **`--locked` 哈希语义冲突**：拆分 `archive_sha256`（安装源归档，`--locked` 重装校验）与 `lib_sha256`（产物哈希，verify 用）；编译型包的 `--locked` 此前**必然失败**。旧 lockfile 兼容（version 保持 1）
- **watcher 静默死亡**：新增 worker 错误通道，`ezmk watch` 观测到监视线程死亡即打印 `file watcher stopped unexpectedly; leaving watch mode` 并以 **exit 1** 退出（此前界面看似在监视、实际无任何监视）

## 语义 / CLI / 导入（P1/P2）

- **`[test].framework` 缺省**：缺省值未归一化 → 省略该键的 `ezmk test` 必然 fatal；现与分发处统一归一化
- **`import` 修正**：`add_library` 关键字感知（收集源码、无关键字默认 static、`INTERFACE` → header-only）；未求值 `if`/`else` 不再把未知分支当条件为真而采纳
- **`--disable-cache` 真正全停**：不写缓存条目且保存空 record（此前会静默写回缓存，反而为下一次构建重新武装缓存）
- **C++17 标准性**：`TestRunContext` 去 C++20 designated initializers（`-std=c++17 -Wpedantic` 下不再依赖 GCC 扩展）
- **`ezmk test` 链接补全**：纳入依赖包归档 + link flags/dirs/system targets（原先静默缺链接）
- **子目录调用安全**：`link_dirs` 与 `-I`/`-L`/`-include`/`-isystem`（MSVC `/I`、`/LIBPATH:`）按项目根绝对化；项目根调用命令行保持字节不变
- **`clean -w` / 位置参数 / `ezmk example` 解析**：`clean -w` 首轮 spec 对齐工作区（专用拒绝分支复活）；`project install/pack/test` 拒绝尾随参数；`example` 改为 `-h`/`-o <dir>`/名称/`list` 规范解析（`output_dir` 缺省 `.`）
- **Windows 参数装配 + 非 ASCII 路径**：新增 MSVCRT 规则 `quote_windows_arg`（`--report` 值不再用 POSIX 转义）；统一 UTF-8 ↔ UTF-16 层，`CreateProcessW` / `GetModuleFileNameW` / watcher / zip 全链路走宽 API
- **MSVC 标准映射与探测**：不再输出非法的 `/std:c++98`、`/std:c++11`（`c++23`/`c++26`/`c++2a` → `latest`，`gnu*` 剥前缀 + 告警）；`cl` 探测改 `cl /Bv`（裸 cl 无输入按 D8003 非零退出，已装 MSVC 会被误判为不可用）

## pkg / repo / lockfile（P3）

- **repo 路径信任边界**：`index.toml` 的 `file` 与 `[platform]` 前缀必须位于 repo 目录内（add 与索引读取双侧 + `repo update` 后复查）；`repo remove/update/info` 入口校验包名，`list.toml` 中被手改的危险条目直接跳过
- **preinstall 钩子 cwd**：改到存在的暂存包根（原先指向尚未创建的目录 → 全新安装必然失败）；`-y` 下不再打开编辑器
- **lockfile `platform`**：写真实 `os_arch_toolchain`（原恒写 `windows_x86_64_*`）
- **预发布平局 tie-break**：`1.0.0` 不再被 `1.0.0-rc.1` 遮蔽（消除 `index.toml` 条目顺序依赖）
- **lockfile verify 盲点**：header-only 校验 `include/` 清单哈希；git 源校验 commit 一致；无哈希条目告警而非误判
- **`pkg update` 流程**：`-y` 透传；install 三态（ok / cancelled / failed）分计，失败 exit 1；`update_all` 迭代前快照包名、自动安装依赖失败时 best-effort 回滚；`is_url` 仅在存在明确 scheme 时判定（不存在的本地路径不再被拼成 `https://`）

## watcher / workspace 文件层（P4）

- **目录消失补挂**：运行中被监视目录被删/改名后每 2 秒重试补挂（Windows 重开句柄 / Linux 清理 wd 后重挂 / macOS 重开 fd）
- **不再自触发**：flush 前 `fs::exists` 过滤 + 忽略前缀/后缀 API，`build/`、`.ezmk/`、`.o`/`.d`/`.tmp` 不触发重建
- **`members` 拼接**：改为 TOML 感知定位数组结尾（字符串/转义/注释/括号深度）——含 `]` 的注释或成员名不再截断 `ezmk-workspace.toml`
- **符号链接环**：`workspace scan` 维护已访问 canonical 集合（自指/祖先链接环终止并计入 skipped）
- **校验顺序依赖**：成员失效传播迭代至不动点；`topo_layers` 的 unresolved 分支改可解释成员错误（不再是 `internal error`）
- **macOS kevent**：注册/事件数组按监视数动态分配（去固定 32 槽截断）

## 低危随附（F-37 择优）

- 时间戳线程安全（`localtime_r`/`localtime_s`，替换 cache/repo/import/pkg 四处 `std::localtime`）
- POSIX `run_command` 双 `close` 修正（`-jN` 下第二次 close 可能关掉被复用的描述符）
- 超时命令改杀整个进程组（`sh -c "a | b"` 的孙进程不再存活并持有输出文件）
- `get_home_dir` 策略：Windows 仅接受原生 `HOME`（MSYS2 的 `HOME=/home/<user>` 不再让用户级安装落到「当前盘符:\home\…」）；POSIX 缺 `HOME` 时回退 `getpwuid`
- cache `record.json` 版本门：高于支持版本（2）的记录整份忽略并告警（降级后不再部分信任）
- `pack --precompiled` 标记拼接收紧（行尾感知、已声明时不重复注入）；打包体积 1 GiB 上限；`project new` 的 scaffold 写失败检查；并行编译进度序号改按完成取号

## 测试

全量 **1099 用例 / 6342 断言零失败**（`bash build.sh test-all`；1.4.1 发布态基线 1020/5970，**+79 用例 / +372 断言**；5 跳过为既有环境限制——符号链接不可用与 MSYS2 GCC 无法在非 ASCII 路径创建目标文件）。每阶段独立 commit + 阶段间全量回归；每个修复配单测/集成锁定。`-Wall -Wextra -Wpedantic -Wshadow -Wformat=2` 下**首方代码零告警**（剩余告警全部来自第三方 vendor）。i18n 三向一致（405 键 × en/zh + zh-TW 变体）。

## 行为修正提示（非破坏性，但用户可见）

`--locked` 从「必失败」变可校验重装；`--disable-cache` 语义收严（不再回写缓存）；MSVC 构建首次获得正确头文件失效（一次全量重编）；Windows 上 POSIX 风格 `HOME` 不再被采信；`[test].framework` 缺省从 fatal 变正常执行。

## 资产

- `ezmk-windows-x64.zip`（含 `ezmk.exe` / `ezmk-lua.exe` / `_ezmk` 补全）+ 独立 `ezmk.exe` / `ezmk-lua.exe`（含 `.sha256` 边车，供 install.ps1 校验）
- `ezmk-linux-x64.tar.gz` / `ezmk-macos-arm64.tar.gz`（含 `ezmk` / `ezmk-lua` / `_ezmk`）
- 完整变更记录：[CHANGES.md](https://github.com/3667808244/EazyMake/blob/v1.4.2/CHANGES.md)
