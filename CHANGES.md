# Changelog

## API Stability

As of v1.1.0, the following public APIs are **permanently stable**:

**Commands:** `build`, `run`, `clean`, `watch`, `install`, `test`, `pack` (top-level) and their `project <action>` equivalents; `pkg install/remove/search/info/list/update`; `repo add/remove/update/list/info`.

**Configuration:** `[project]`, `[compile]`, `[link]`, `[depends]`, `[test]`, `[install]` core fields in `ezmk.toml`.

Breaking changes are introduced only in `2.0.0`, preceded by deprecation warnings in at least one minor version (`1.x.0`).

---

## 1.2.0-dev.1 (2026-08-11) — `ezmk project cc` 命令

1.2.0 系列首个开发子版本：基于 1.1.1 的编译命令单一事实源（`build_compile_args()` + `compile_db`）新增**正式命令** `ezmk project cc`，并把 `ezmk utils cc` 从「拦截」过渡为「弃用提示」。**不破坏任何公共 API**（见文首 API Stability）。

### 新增

- **`ezmk project cc`**：内置 CLI 入口生成 `compile_commands.json`（`arguments` 数组，clangd 推荐格式），零外部包依赖、不再需要 `ezmk-official-utils`。支持 `-o/--output <path>`（默认 `<proj_root>/compile_commands.json`）与 `--profile <name>`；与 1.1.1 的 `utils cc` 拦截 / `[compile].compile_commands` 自动生成共用 `generate_compile_db()`，三个入口输出永远一致

### 弃用

- **`ezmk utils cc`**：自 1.2.0 起正式弃用（1.1.1 仅拦截、未声明）。运行输出 `[ezmk warn] ... use \`ezmk project cc\` instead` 提示并转调新命令，工具保留可用；`ezmk-official-utils` 包内 `cc.lua`/README 标注 `@deprecated since 1.2.0`（包版本 1.1.0 → 1.2.0）；**2.0.0 移除**

---

## 1.1.3 (2026-08-10) — 补丁发布

1.1.x 稳定线补丁。基于第二轮多模块安全与质量审计，修复 **5 处安全缺口**与 **5 处健壮性问题**，并收敛代码质量与测试质量问题。**不新增命令、不弃用任何接口**，公共 API 保持不变（见文首 API Stability）。

### 安全收敛（二轮）

- **钩子沙箱统一**：构建钩子/安装钩子共用的沙箱 `__index` 从裸 `_G` 改为受限全局表（`dofile`/`loadfile`/`load`/`require`/`debug`/`package`/`collectgarbage` 解析为 `nil`），与 utils 脚本一致——第三方钩子脚本无法再读盘上文件或从磁盘加载代码
- **包名校验**：新增 `util::validate_pkg_name`，安装/`pkg remove`/`search`/lockfile 恢复统一拒绝含路径分隔符、盘符、`..` 等的恶意包名——不再可能经包名路径穿越越界写盘
- **URL 安装完整性**：无 sha256 的 URL 安装下载前要求确认（`-y` 可跳过）；显式 `http://` 明文下载提示 MITM 风险并建议 `https://`——不再静默下载不可校验的包
- **flags 命令注入修复**：`join_shell_args` 引号元字符集补全为 POSIX `/bin/sh` 解析涉及的全部字符，GCC/MSVC 链接 flags 改双引号包裹——恶意 `link.flags` 不再被 `sh -c` 执行
- **编辑器命令转义**：`open_in_editor` 的 `EDITOR`/`VISUAL` 派生命令经 `escape_shell_arg` + 双引号包裹，`find_editor` 移除全库唯一 `system()`，vcvars 路径转义——`EDITOR="vim; evil"` 不再注入

### 健壮性收口

- **`SOURCE_DATE_EPOCH` 安全解析**：非数字值不再在 `-jN` worker 线程抛异常崩线程，改为警告并视为未设置（提取为可单测的 `resolve_source_date_epoch`）
- **`~` 前缀边界**：install prefix 仅 `~/`（或 `~\`）与单独 `~` 展开，`"~abc"` 不再被误截断为 `"c"`
- **watcher OVERLAPPED 池实例化**：`g_overlapped_pool` 从文件级全局改为 `FileWatcher` 实例成员——多实例时不再互相悬垂
- **`recursive_` 死代码收口**：删除从未被读取的字段，头文件注明实际递归行为（Windows 递归 / Linux·macOS 非递归）
- **边缘批处理**：`name.back()` 空串防御（根目录 `filename()` 为空）、`.o`/`.obj` 后缀提为常量、CLI argv 嵌入 NUL 已知限制注释

### 代码质量

- **JSON 解析器替换**：手写 `load_links_json` 解析器改 `nlohmann/json`——支持标准转义与 Unicode，畸形 JSON 报错清晰
- **大函数拆分**：`parse_config` 按 TOML 节拆为 9 个私有 helper；`compile_one_source` 拆出记录条目/依赖解析 helper——纯提取零行为漂移
- **cli 重复代码收敛**：positional 数量校验提取 `require_positional`/`optional_positional`/`reject_positionals` 共享 helper

### 测试

- **永真断言清理**：`test_file_watcher` 三个 `call_count >= 0` 永真断言改为轮询等待 + 精确路径集合断言，事件无法送达的环境显式 SKIP
- **argparse 单测**：新增 `test_argparse.cpp` 直接测 tokenizer（长短选项/分组/`--` 透传/报错路径）
- 全量测试零回归：630 用例 / 2918 断言（含集成）。

---

## 1.1.2 (2026-08-08) — 补丁发布

1.1.x 稳定线补丁。基于多模块代码质量评审，修复 **4 处安全漏洞**与 **7 处静默产出错误结果的正确性 bug**。**不新增命令、不弃用任何接口**，公共 API 保持不变（见文首 API Stability）。

### 安全加固

- **解压路径包含校验（zip-slip）**：归档条目名统一按 `/` 与 `\` 切分，拒绝 `..` 分量、绝对路径、盘符与 UNC 前缀，再经目录边界兜底；tar.gz 解压输出封顶 1 GiB（防 zip-bomb）——恶意包无法再写到暂存目录之外
- **归档命令转义**：`ar`/`lib.exe` 归档命令的所有路径经 `escape_shell_arg` 转义——对象路径源自归档内源文件名，可能含 `$`/反引号/空格，裸引号在 POSIX `sh -c` 下可命令注入
- **`ezmk.file_write` 硬限制收紧**：项目根外禁写检查改用 `lexically_normal` + 目录边界判断，堵住 `../` 越界与 `<root>X` 前缀边界误判
- **Lua 沙箱收敛**：utils 脚本沙箱改为受限全局表，`dofile`/`loadfile`/`load`/`require`/`debug`/`package` 解析为 `nil`，`_G` 指向沙箱自身——脚本不能读盘上文件、不能从磁盘加载代码，`[utils.permissions]` 成为真正的执行边界

### 正确性修复

- **链接假成功**：产物 temp→rename 失败（Windows 下被运行中 exe/杀软占用）不再打印 `build_success`，改为报错并给出路径与原因
- **缓存签名补全**：`compile_options_signature` 纳入 `stdlib` 与 `-fPIC`——改 `[project].stdlib` 或 `type` static↔shared 不再复用陈旧对象
- **`--locked` 误报修复**：lockfile 记录根项目直接依赖（`direct_deps`），`depends_changed` 改为直接依赖精确比较（含版本约束）——有传递依赖时 `--locked` 不再恒误报
- **Windows 安装脚本修复**：`run_script` 不再用 `cd /d ... && ...` 前缀（`cd` 是 cmd 内建、`CreateProcessA` 无法启动），工作目录改经子进程 `lpCurrentDirectory`/`chdir` 注入——`.sh`/`.ps1`/`.bat` 安装脚本在 Windows 恢复可用
- **TOML 写入转义**：`write_default_config` / `ezmk.lock` / repo 列表写入统一 `toml_quote`——项目名/包名含引号或换行不再写坏配置
- **安装事务化**：安装先备份旧包、新包完全就绪后才替换，失败回滚——中途失败不再删掉既有版本；`copy_recursive`/`remove_all` 的真实失败不再被吞掉
- **确定性构建数据竞争**：`SOURCE_DATE_EPOCH` 改经子进程环境注入（POSIX fork 后 `setenv`），不再从工作线程改进程全局环境——`deterministic` + `-jN` 输出恢复确定

### 测试

新增 `test_lockfile.cpp`、解压安全、`run_command` `RunOptions`、确定性等用例。全量测试零回归：592 用例 / 2813 断言（含集成）。

---

## 1.1.1 (2026-08-08) — 补丁发布

1.1.x 稳定线补丁。优化 `compile_commands.json`（clangd 索引）的生成算法，并新增构建后自动生成配置项。**不新增命令、不弃用任何接口**，公共 API 保持不变（见文首 API Stability）。

### 优化 compile_commands.json 生成算法

- `ezmk utils cc` 输出的 compile_commands.json 现在与真实构建命令**完全一致（drift-free）**：完整包含此前缺失的 `@link:` 解析目录、依赖包 `extra_includes`、`-stdlib`、确定性标志、宏 `-D`、`--profile` 与 MSVC 翻译等
- 输出为 clangd 推荐的 `arguments` 数组（免 shell 双重转义歧义），`file` 相对项目根、`directory` 为项目根绝对路径，条目稳定排序、原子写
- 编译命令构造收敛为**单一事实源**（`build_compile_args()`），构建与索引共用同一实现——任何新增编译标志自动进入 compile_commands.json

### 新增配置项

- `[compile].compile_commands`（bool，默认 `false`）：为 `true` 时 `ezmk build` 链接成功后自动生成 compile_commands.json（对标 CMake `CMAKE_EXPORT_COMPILE_COMMANDS`），输出与本次构建逐条一致
- `ezmk build --compile-commands`：单次临时启用，无需改配置

### 测试

全量测试零回归：单元 546 用例 / 2621 断言。

### 发布后跟进项

- `macos-x64` 产物：Intel `macos-13` runner 在 GitHub free tier 长期无分配，job 仍在队列（与 1.1.0 相同情况；runner 可用时自动上传 `ezmk-macos-x64.tar.gz` 至 v1.1.1 Release）

---

## 1.1.0 (2026-08-07) — 正式版发布

合并 `1.1.0-dev.1` ~ `dev.7` 与 `1.1.0-pre.1` ~ `pre.3` 的正式版：包编译与开发体验（dev）+ 用户触达改善（pre.1）+ 文档检查（pre.2）+ 缺陷收集与 CI（pre.3）。**公共 API 自此永久稳定**（见文首 API Stability）。

### 里程碑

从 1.0.0 到 1.1.0 的关键交付：

- **包编译与产物安装**：`precompiled` 包、`[install]` 配置节、`ezmk project install`
- **多平台共包与分发**：`os_arch_toolchain` triple、`index.toml` 平台映射、`ezmk project pack`
- **测试系统**：`ezmk test`（Catch2 + ezmk 内置框架）、`[test]` 配置节、30s 超时
- **包生态**：硬依赖前置检查 + 自动安装、`want` 可选依赖交互询问
- **开发体验**：顶层别名、`--help` 重组、Agent Skills、`stdlib`/`lang` 泛化、GNU 扩展、`ezmk-official-utils`
- **质量与 CI**：`.github/workflows/ci.yml`（push/PR）、测试系统缺陷修复、zsh 补全修正、`check_i18n.py` 三向校验
- **发布与分发**：`v1.1.0` GitHub Release（`release.yml` 首次真实运行：linux-x64 / windows-x64 / macos-arm64 + 独立 `ezmk.exe`）、Homebrew formula（`homebrew-eazymake`）、README 安装脚本回归（修复 release.yml 缺独立 exe 依赖）

### 发布后跟进项

- `macos-x64` 产物：Intel `macos-13` runner 在 GitHub free tier 长期无分配，job 仍在队列（runner 可用时自动上传至同一 Release）
- winget manifest：Release 产出便携二进制而非 winget 安装器，且 `winget-pkgs` PR 需 fork 大仓——作为发布后跟进项
- `brew install` / `install.sh` Linux 真机烟测

### 测试

全量测试零回归：单元 546 用例 / 2617 断言 · 含集成 556 用例 / 2666 断言。

---

## 1.1.0-pre.3 (2026-08-04) — 缺陷收集与未实现项补全

聚合 pre.2 之后的全量已知缺陷与未实现项：测试系统缺陷修复、CI 测试工作流、工具/文档缺陷修正，以及发布流水线文档项。

### 测试系统缺陷修复

- **`run_command()` 超时支持**：新增可选 timeout 参数（POSIX `fork`/`waitpid` 轮询 + `SIGKILL`；Windows `WaitForSingleObject` 超时 + `TerminateProcess` + 非阻塞管道排空）；调用方不传即无超时，向后兼容
- **`ezmk test` 30s 超时生效**：ezmk 内置框架每个测试 30s 硬超时，超时记为 FAIL 并归入 `timed_out` 计数（原 `timed_out` 恒为 0，一个挂起的测试会无限阻塞整个套件）
- **Catch2 解析加固**：放弃逐行 `PASSED`/`FAILED` 文本猜测，改为退出码 + 汇总行解析（`test cases:` / `All tests passed` 两种格式，失败回退退出码）；`--filter` 透传行为不变
- **`check_built` 修正**：`shared` 类型检查真实 `.dll`/`.so` 产物（删除 DLL 后 `ezmk test` 正确触发重建）；`utils` 类型明确跳过 build-first
- **watch 集成测试去 flaky**：日志轮询替代固定 sleep、修复 Windows `start /B` 不继承 CWD 的 bug、`EZMK_LANG=en` 去 locale 依赖、断言 WARN→CHECK

### CI 与构建脚本

- **新增 `.github/workflows/ci.yml`**：push/PR 触发——Ubuntu 跑 `test-all`（单元 + 集成）、Windows (MSYS2) 跑 `test`（单元）、zsh-completions job 回归 `install.sh` 的 zsh 补全激活逻辑
- **`build.sh` 测试退出码透传**：`test`/`test-all`/`integration` 不再吞掉测试套件失败（原 `|| true` 恒退出 0），CI 因此可作为回归 gate

### 工具/文档缺陷修正

- **`package_authoring.md`（en/zh）**：删除对不存在的 `ezmk utils sha256` 的引用（文档已有 `sha256sum` / `Get-FileHash` 替代）
- **`scripts/check_i18n.py`（新增）**：i18n 三向一致性校验（`i18n_keys.def` ↔ `locale/en.json` ↔ `locale/zh.json`），273 key 三方一致
- **过时测试基线修正**：`ezmk-test` skill（538/2440 → 546/2617、集成 7 → 8）、`copilot-instructions`（~538/2440 → 546/2617）、`technical.md`（en/zh 545 → 546）

### 发布流水线文档

- **`plans/1.1.0/1.1.0.md`（新增）**：1.1.0 最终发布计划（合并 dev.1~dev.7 + pre.1~pre.3，含发布门槛预检、打 tag 触发 `release.yml`、Homebrew/winget 分发步骤）
- **Tutorial 09-test.md / 10-top-level-aliases.md（en/zh，新增）**：`ezmk test` 专题教程 + 顶层别名快速参考

### 测试

- 全量测试通过，零回归（单元 546 用例 / 2617 断言 · 含集成 556 用例 / 2666 断言）

---

## 1.1.0-pre.2 (2026-08-03) — 文档检查

对 1.1.0-dev.7 与 1.1.0-pre.1 之后的全量文档审计，确保文档与代码一致，并补齐 pre.1 遗留项。

### CLI 文档更新

- **顶层别名章节**：`docs/en/cli.md` / `docs/zh/cli.md` 新增 "Top-level aliases" / "顶层别名" 章节，命令表格改为别名优先（完整形式标注在描述中）
- **README 补 `ezmk pack`**：`README.md` / `README_ZH.md` 命令速览补充顶层别名
- **Tutorial 别名化**：`tutorial/en` ×6 + `tutorial/zh` ×6 + `faq.md`（en/zh）代码示例改为顶层别名优先（首次出现处标注完整形式）
- **`docs/en/technical.md`**：zsh 补全路径修正 + 测试数据更新

### `config_file.md` 补全

- **`[install]` 配置节**：`prefix` / `bindir` / `libdir` / `includedir` / `sharedir`（en + zh）
- **`[test]` 配置节**：`dirs` / `framework` / `flags`（en + zh）

### pre.1 遗留项补齐

- **`docs/zh/technical.md`**（新建）：`docs/en/technical.md` 中文翻译
- **`res/ezmk.zsh`**（新建）：zsh 补全脚本迁移至 `res/`（原 `completions/_ezmk`），`install.sh` / `release.yml` / `technical.md` 路径同步

### 版本号

- **`build.sh` 默认版本 `1.0.0` → `1.1.0`**：修正 fallback 版本号（未设置 `EZMK_VERSION` 时）

### CHANGES.md 补全

- 补充 `1.1.0-dev.7`（包生态拓充与包处理改善）与 `1.1.0-pre.1`（改善用户触达）条目

### 测试

- 全量测试通过，零回归（单元 545 用例 / 2613 断言 · 含集成 555 用例 / 2661 断言）

---

## 1.1.0-pre.1 (2026-08-02) — 改善用户触达

改善新用户首次接触 EazyMake 的体验：顶层命令别名、`--help` 输出重组、README 精简、API 稳定性承诺。

### 顶层命令别名

- **7 个顶层别名**：`build` / `run` / `clean` / `watch` / `install` / `test` / `pack`，对应 `project <action>` 完整形式
- 别名与完整形式完全等价，所有标志与参数行为一致；日常使用推荐短形式

### `--help` 输出重组

- 输出按日常 / 初始化 / 高级分组（`help_section_daily` / `help_section_init` / `help_section_advanced`）
- 顶层别名为主行，完整形式标注（`help_full_form`）；新增 6 个 i18n key（中英双语）

### README 精简重写

- `README.md` / `README_ZH.md` 重写为面向普通用户（快速开始 / 命令速览 / 文档索引）
- 技术栈与依赖表迁移至 `docs/en/technical.md`

### API 稳定性承诺

- **v1.1.0 起公共 API 永久稳定**：命令与 `ezmk.toml` 核心配置节（`[project]` / `[compile]` / `[link]` / `[depends]` / `[test]` / `[install]`）不再破坏性变更
- 破坏性变更仅在 `2.0.0` 引入，并提前至少一个次版本发出弃用警告（CHANGES.md `## API Stability`）

### zsh 补全

- `completions/_ezmk` 增加顶层别名补全与 `project install` / `pack` / `test` 子命令

### 测试

- 全量测试：**545 用例 / 2613 断言**，零回归

---

## 1.1.0-dev.7 (2026-08-01) — 包生态拓充与包处理改善

扩充官方仓库包生态（12 个新包 + 10 个 Boost header-only 子库），并改善包处理：构建时硬依赖前置检查、仓库安装时自动安装缺失依赖、可选依赖交互式询问。

### 包生态拓充

- **12 个新包**：`openssl`、`libcurl`、`protobuf`、`gRPC`、`hiredis`、`sqlitecpp`、`libpqxx`、`msgpack-c`、`tomlplusplus`、`cpp-httplib`、`eigen`、`googletest`（网络通信 / 数据库 / 序列化 / 科学计算 / 测试框架）
- **10 个 Boost header-only 子库**：`boost-asio`、`boost-beast`、`boost-filesystem`、`boost-system`、`boost-smart-ptr`、`boost-tokenizer`、`boost-uuid`、`boost-random`、`boost-math`、`boost-functional`
- **已有包版本更新**：0.9.7/0.9.8 的 42 个存量包逐一核对上游新版本并升级（Boost×10 → 1.88.0 等）

### 构建时硬依赖前置检查

- **`package_available()`**：新增 `pkg::` 接口，遍历已注册仓库搜索包名
- **`prepare_build_state()`**：构建前遍历 `[depends].lib`，缺失时输出语义化错误 + 仓库可安装提示（`missing_dep_at_build`），替代原始编译器报错

### 仓库安装时自动安装缺失库

- **硬依赖自动安装**：`pkg install` 从仓库安装时，`[depends].lib` 缺失 → 自动递归安装（含传递依赖），替代原先的 `fatal`
- **可选依赖交互式询问**：`[depends].want` 缺失 → 询问 `[Y]es/[N]o/[A]ll/[D]eny-all`；`A`/`D` 递归穿透子包；非交互模式（`-y`）保持跳过

### 测试

- 全量测试通过，零回归

---

## 1.1.0 (2026-07-28) — MSVC 包编译、确定性构建与产物安装

首个次版本升级。补齐 MSVC 工具链在包管理中的完整支持，引入确定性构建与 lockfile 机制，新增 `project install` 命令。

### MSVC 包编译

- **`compile_package()` MSVC 感知**：归档阶段根据工具链选择 `lib.exe` → `.lib`（MSVC）或 `ar rcs` → `.a`（GCC/Clang），原子写入不变
- **`Toolchain::version`**：`detect_toolchain()` 捕获编译器版本字符串（`g++ --version` / `cl` 首行输出），用于缓存失效判定
- **`index.toml` `[platform]` 三元组**：平台键格式 `{os}_{arch}_{toolchain}`（如 `windows_x86_64_msvc`），解析时 fallback 到旧二元格式（映射到 GCC），向后兼容

### Header-Only 包支持

- **`pkg.toml` `header_only = true`**：0.9.8 遗留字段完成收尾 — 安装时跳过编译+归档，仅复制 `include/`；`ezmk pkg info` 显示 `Type: header-only`

### 确定性构建

- **`[compile]` 新增 `deterministic` + `source_date_epoch`**：GCC/Clang 注入 `-ffile-prefix-map` + `-frandom-seed`；MSVC 注入 `/Brepro`；自动设置 `SOURCE_DATE_EPOCH` 环境变量
- **`SOURCE_DATE_EPOCH` 自动解析**：优先级：环境变量 → `ezmk.toml` 配置 → git HEAD commit 时间戳 → `ezmk.toml` mtime（fallback）
- **`record.json` v1 → v2**：新增 `compiler` / `compiler_version` / `deterministic` 字段；编译器版本变化 → 自动清空缓存；`deterministic` 标志纳入编译选项签名

### Lockfile（`ezmk.lock`）

- **依赖版本与内容锁定**：TOML 格式，记录每个包的精确版本、`sha256`、平台、依赖图
- **API**：`load()` / `save()` / `verify()` / `depends_changed()`
- **集成点**：`ezmk pkg install` 自动生成/更新；`ezmk project build` 启动时校验完整性
- **`--locked` / `--no-lock` CLI flags**：锁模式仅使用 lockfile 安装（不一致则报错）；`--no-lock` 跳过 lockfile 生成
- **确定性构建联动**：`deterministic = true` 时 — lockfile 缺失或校验失败 → error；lockfile 内容哈希纳入编译缓存签名

### `ezmk project install`

- **`[install]` 配置节**：`prefix` / `bindir` / `libdir` / `includedir` / `sharedir`，支持 `~` 展开
- **CLI**：`ezmk project install [--prefix <path>] [--dry-run] [--no-headers] [--no-data] [-v]`，简写 `pi`
- **安装布局**：`executable` → `<bindir>/`；`static` → `<libdir>/`；`shared` → `<bindir>/`（DLL）+ `<libdir>/`（导入库）；头文件 → `<includedir>/<name>/`

### 测试

- 全量测试：**538 用例 / 2504 断言**，零回归
- `record.json` v1→v2 测试更新（version 字段 1→2）

### 多平台共包支持（dev.2）

- **`util::detect_platform_tag()`**：新增简化平台标签（`win-x64` / `linux-x64` / `mac-arm64`），用于文件名匹配和索引过滤
- **`select_precompiled_archive()`**：预编译包 `lib/` 下按 `lib<name>.<tag>.a` 自动选择当前平台产物；fallback 到无后缀文件（向后兼容）
- **`index.toml` `platform` 字段**：`[[packages]]` 条目新增可选 `platform` 字段（`os-arch` 格式），`read_pkg_from_index()` 搜索时自动按平台过滤；缺失字段的旧条目视为全平台可用

### `ezmk project pack`（dev.2）

- **`util::create_targz()`**：新增 tar.gz 创建函数（ustar tar + raw deflate gzip），基于现有 miniz 库，零新依赖
- **CLI**：`ezmk project pack [--output <dir>] [-v]`，简写 `pp`；将 `static` 项目一键打包为 `<name>-<version>.tar.gz`
- **输出**：自动构建（如未构建）→ 收集 `include/` + `lib<name>.a` + `ezmk.toml` → 打包 → 打印 SHA-256；`-v` 模式额外输出 `index.toml` 条目模板

### 其他（dev.2）

- **`build.cpp`**：依赖包预编译归档收集改为平台感知（`select_precompiled_archive()`），避免多平台文件冲突
- **i18n**：新增 5 个 key（`pack_*`），中英双语翻译

---

## 1.1.0-dev.6 (2026-08-01) — 测试系统

新增 `ezmk project test`（`pt`）命令，支持 Catch2 和 ezmk 内置框架两种测试模式。

### `[test]` 配置节

- **`test.dirs`**：测试源文件目录（`string[]`，默认 `["test"]`）
- **`test.framework`**：测试框架（`"catch2"` | `"ezmk"`，大小写不敏感，默认 `"catch2"`）
- **`test.flags`**：额外测试编译标志（`string[]`，默认 `[]`）
- 所有字段可选，旧项目无此节可正常运行（全部有默认值）

### CLI

- **`ezmk project test`**：一键编译 → 构建测试 → 运行 → 汇总
- **`ezmk pt`**：简写别名
- **`--framework` / `-f`**：临时覆盖测试框架（不修改 `ezmk.toml`）
- **`--filter`**：过滤测试名称（Catch2 传入测试名；ezmk 做文件名 glob）
- **`--verbose` / `-V`**：展示每个测试的详细输出

### Catch2 模式

- **自动检测**（优先级）：项目作用域已安装 → `include/vendor/catch2.hpp`（单头） → 用户/全局作用域已安装 → 报错提示安装
- **入口生成**：自动检测用户自定义 `main`；无则生成 `.ezmk/cache/test_main.cpp`（`CATCH_CONFIG_MAIN` + `catch_all.hpp`）
- **链接**：自动查找并链接 `libcatch2.a`（支持 project/user/global 作用域）

### ezmk 内置框架模式

- **零依赖**：每个 `.cpp` 独立编译为可执行文件，`return 0` = PASS，非 0 = FAIL
- **轻量断言宏**：`include/ezmk/test_assert.h` — `EZMK_ASSERT` / `EZMK_ASSERT_EQ` / `EZMK_ASSERT_NEQ`
- **独立运行**：每个测试以子进程运行，捕获 stdout/stderr + 退出码

### 测试

- 全量测试：**545 用例 / 2557 断言**，零回归

### 涉及文件

| 文件 | 变更 |
|------|------|
| `include/ezmk/config.hpp` | 修改：`TestConfig` + `EzConfig::test` |
| `src/config.cpp` | 修改：`[test]` 节解析 |
| `include/ezmk/cli.hpp` | 修改：`Command::ProjectTest` + test 选项 |
| `src/cli.cpp` | 修改：`project test` 命令 + `pt` 别名 + 选项解析 |
| `include/ezmk/build.hpp` | 修改：`run_tests()` 声明 |
| `src/build.cpp` | 修改：`run_tests()` 实现（~500 行） |
| `src/main.cpp` | 修改：`Command::ProjectTest` 分发 |
| `include/ezmk/test_assert.h` | **新建**：轻量断言宏 |
| `include/ezmk/i18n_keys.def` | 修改：新增 4 个 i18n key |
| `locale/en.json`、`locale/zh.json` | 修改：中英文翻译 |

---

## 1.1.0-dev.5 (2026-08-01) — 默认 util 包与链接机制

新增官方 utils 包（`ezmk-official-utils`）、`@link:` 链接机制、watch 空路径修复。

### `ezmk-official-utils` 包

- **三个官方工具**：`link`（`.ezmk/links.json` 管理）、`cc`（从内置迁移的 `compile_commands.json` 生成器）、`gen-build-package`（生成自包含构建包 `.tar.gz`）
- **包结构**：`ezmk.toml`（`type = "utils"`, `tools = ["link", "cc", "gen-build-package"]`）+ 3 个 Lua 脚本 + `README.md`
- **`cc` 迁移**：从 `pkg/ezmk-cc/` 内置工具迁移到 `ezmk-official-utils/utils/cc.lua`，通过标准 utils 查找链（project → user → global）发现；`find_utils_script()` 无需变动（已是通用扫描）
- **`install.sh` / `install.ps1`**：安装后自动预装 `ezmk-official-utils`（全局作用域），失败不阻塞安装

### `@link:` 链接机制

- **`.ezmk/links.json`**：项目级链接映射（`{"name": "relative/path"}`），支持跨目录源文件共享
- **`resolve_link_path()`**：`config.cpp` 中解析 `@link:<name>` 引用 → 目标路径；支持链式解析（A→B→C，深度限制 10 层）；循环链接检测
- **`parse_link_syntax()`**：`util.cpp` 中解析 `@link:<name>/sub/path` 格式，验证名称合法性
- **`parse_config()` 集成**：在 `src_dirs` / `include_dirs` / `link_dirs` 中自动展开 `@link:` 引用
- **`link.lua` 工具**：`ezmk utils link add/remove/list/show` — 命令行管理 `.ezmk/links.json`

### `gen-build-package` 工具

- **`ezmk utils gen-build-package`**：将项目打包为自包含 `.tar.gz` 构建包（源文件 + 头文件 + `ezmk.toml` + 生成 `build.sh`/`build.ps1`）
- **选项**：`--output <dir>`（输出目录）、`--name <name>`（包名）、`--help`

### Watch 修复

- **空路径 bug**：`main.cpp` 中 `ezmk.toml` 使用 `proj_root / "ezmk.toml"` 绝对路径，避免 `parent_path()` 返回空字符串传入 `fs::absolute()`
- **防御性检查**：`FileWatcher::add_directory()` 空路径跳过 + warning
- **测试**：`test_file_watcher.cpp` 新增空路径传入不崩溃用例

### 测试

- 全量测试：**545 用例 / 2541 断言**，零回归

---

## 1.1.0-dev.4 (2026-07-31) — 编译器与语言配置增强

增强语言标准配置的灵活性和用户友好度，支持标准库选择与编译器拓展。

### `project.stdlib` 支持

- **`[project]` 新增 `stdlib` 字段**：可选 `libstdc++`（默认）或 `libc++`；支持别名 `glibcxx`/`gnu` → `libstdc++`、`llvm` → `libc++`
- **`-stdlib=` 自动注入**：Clang + `libstdc++` → `-stdlib=libstdc++`；GCC/Clang + `libc++` → `-stdlib=libc++`；MSVC 不注入（仅使用 STL）；GCC + `libc++` 时输出 warning（支持有限）
- **`EZMK_STDLIB` 预定义宏**：编译时自动注入（`"libstdcxx"` / `"libcxx"`），`++` 替换为 `xx` 以避免 `+` 被误解析；用户可据此条件编译

### `project.language` 泛化

- **大小写不敏感 + 变体统一**：`c++17` / `CXX17` / `CPP17` → 标准化为 `CPP17`；`C++`/`CXX` → `CPP` 自动识别
- **`normalize_lang()` 泛化函数**：统一处理语言和标准库字符串（upper-case + trim），C++/CXX → CPP 在 `parse_language()` 中处理避免污染 stdlib 值
- **版本默认**：仅 `C++`（无版本号）→ 默认 `C++17`；仅 `C` → 默认 `C11`
- **`EZMK_LANG` 宏值更新**：使用标准化后的值（如 `CPP17`），而非原始输入

### 编译器拓展支持

- **GNU 前缀检测**：`GNUCPP17` → `-std=gnu++17`；`GNU11` → `-std=gnu11`；`gnuc++20`（小写）→ `-std=gnu++20`
- **`LanguageInfo::gnu_extensions` 字段**：标识是否使用 GNU 拓展
- **non-ISO 警告**：首次使用 GNU 拓展时输出 warning，提示标准的 `CPP17` 替代写法

### 测试

- 全量测试：**544 用例 / 2539 断言**，零回归
- 新增 ~13 用例 / ~69 断言：`normalize_lang()` 18 用例、stdlib 解析 8 用例、`parse_language()` 扩展 10 用例、`get_stdlib_flags()` 7 用例

### i18n

- 新增 2 个 key（`config_err_invalid_stdlib`、`config_warn_gnu_extensions`），含中英双语翻译

---

## 1.1.0-dev.3 (2026-07-30) — Agent Skills 支持

将项目 AI 编码助手指令从单文件 `CLAUDE.md` 拆分为符合 Agent Skills 开放标准的 10 个 skill 文件。

### Dev 侧 Skills（6 个）

- **Build Skill** (`.claude/skills/ezmk-build.md`)：编译命令（`build.sh` + 手动 g++ MSYS2/Linux）、关键 flag 解释、平台差异
- **Test Skill** (`.claude/skills/ezmk-test.md`)：测试运行（Catch2 v3）、测试文件组织、新增测试指南、当前基线数据
- **Codebase Skill** (`.claude/skills/ezmk-codebase.md`)：源码架构（16 个模块职责）、数据流（CLI→config→build→cache→toolchain）、关键设计模式、子系统详解
- **i18n Skill** (`.claude/skills/ezmk-i18n.md`)：X-macro 机制（`i18n_keys.def` → 枚举 + 映射）、添加翻译完整步骤
- **Planning Skill** (`.claude/skills/ezmk-planning.md`)：`plans/` 目录结构、plan 文档格式约定
- **Repo Skill** (`.claude/skills/ezmk-repo.md`)：官方仓库结构、包制作流程、`index.toml` 格式

### 用户侧 Skills（4 个）

- 面向 EazyMake 使用者的 skill 文件，覆盖项目编译、测试、配置和包管理工作流

### `CLAUDE.md` 精简

- 从 ~160 行全量注入精简为 ~30 行入口索引（skill 表 + quick reference），agent 按需加载对应 skill
- GitHub Copilot 桥接文件（`.github/copilot-instructions.md`）

### 测试

- 纯文档变更（skill 文件为 Markdown），不影响编译或测试；全量测试通过

---

## 1.0.0 (2026-07-24) — 正式版发布

首个正式版本。**不修改源代码**（版本号字符串除外），聚焦文档与元数据收尾工作。

### 文档整理

- **`plans/` 目录重组**：拆分为 `dev/`（0.1.6~0.2.6）和 `release/`（0.9.0~1.0.0）子目录，`plans/README.md` 索引更新
- **`docs/zh/cli.md` 全中文翻译**：标题、描述段落、表格全部中文化，与 `docs/en/cli.md` 结构对齐
- **核心文档重写**：`CLAUDE.md`（新增 `detect_install_script` 公开 API、统一 sandbox 框架、`api_version` 字段）、`README.md` + `README_ZH.md`（测试数据更正：538 用例 / 8 集成测试）

### 文档审计（11 项）

- 修复 `pkg.md`（en+zh）"没有中央仓库"→"官方仓库已预注册"
- 验证安装钩子文档、捆绑包引用、`ezmk.toml` 版本标记、仓库 URL、Lua API 函数数量、构建钩子、缓存文档、CHANGES.md 覆盖、教程一致性 — 9 项通过
- Git tag 覆盖：补打缺失的 `v0.1.7`、`v0.9.8`、`v0.9.9`

### 里程碑

从 0.9.0 到 1.0.0 的关键交付：一键安装（`install.sh` / `install.ps1`）→ 官方默认仓库（50+ 包）→ 文档多语言（en+zh）→ 捆绑包迁移 → FAQ/故障排除 → 集成测试（8 个端到端场景）→ 代码重构（消除重复、RAII 修复）→ 依赖版本锁定 → header-only 支持 → CLI 统一 → 安装钩子 Lua 化 → 代码质量门禁（消除 ~70 行重复、参数压缩、栈安全加固）→ 文档审计与翻译补全

---

## 0.9.10 (2026-07-23) — 代码质量重构

消除 0.9.9 引入的代码重复与技术债务，为 1.0.0 正式发布做最后的代码质量门禁。**只重构、不新增功能、不改变外部行为。**

### 重构

- **提取通用 sandbox 执行框架**：新增内部函数 `run_lua_script_with_ctx()`（`BuildCtxFn` 回调模式），统一 sandbox 构建→脚本加载→chunk 执行→`run()` 调用→退出码提取的完整流水线；`run_hook_script()` 和 `run_install_hook_script()` 退化为薄封装（消除 ~70 行重复代码）
- **压缩 `run_install_script()` 参数**：引入 `InstallHookContext` 结构体（`pkg_name`/`pkg_root`/`install_path`/`scope`），函数参数 9→6
- **Lua 栈安全加固**：`register_api()` 调用前后增加 `lua_gettop` 断言（debug build），防止栈泄漏
- **`detect_install_script()` 可测试化**：从 `static` 函数提升为 `pkg.hpp` 公开 API，新增 5 个单元测试用例（`.lua` 优先、仅 `.lua`、无脚本、无脚本目录、平台脚本 fallback）

### 测试

- 新增 5 个 `detect_install_script` 测试用例
- 全量测试：**538 用例 / 2440 断言**（含 integration），零回归

---

## 0.9.9 (2026-07-22) — 安装钩子 Lua 化

消除最后的技术债务——将安装生命周期钩子从 Shell 脚本迁移至 Lua，与构建钩子（0.2.3）形成统一技术栈。

### 功能

- **安装钩子 Lua 化**：`preinstall`/`postinstall` 钩子支持 `.lua` 脚本（跨平台统一），`detect_install_script()` 优先检测 `.lua` 再 fallback 到平台特定脚本（`.ps1`/`.bat`/`.sh`）
- **统一上下文传递**：Lua 钩子通过 `ctx` 表接收安装上下文（`pkg_name`/`pkg_root`/`install_path`/`scope`/`pkg_version`/`pkg_type`）
- **安全模型对齐**：Lua 安装钩子运行在 sandbox 中（已移除 `os`/`io`，`ezmk.*` API 沙箱限制），无需打开编辑器审查，仅需用户确认
- **向后兼容**：旧包的 `.sh`/`.ps1`/`.bat` 脚本继续工作，无需立即迁移
- **新增 i18n 键**：`install_hook_lua_error`、`install_hook_no_run`（中英文）

### 代码与测试

- 新增 `lua::run_install_hook_script()`（`src/lua_api.cpp` + `include/ezmk/lua_api.hpp`）
- 重构 `run_install_script()` 和 `detect_install_script()` 支持 Lua 优先检测（`src/pkg.cpp`）
- 新增 9 个测试用例覆盖：基本执行、ctx 表完整性、退出码、空 L 指针、缺失 run()、Lua error、语法错误、作用域、无配置文件的降级行为
- 测试总计：533 用例 / 2430 断言，零回归

### 文档

- 更新安装钩子章节（`docs/en/pkg.md` + `docs/zh/pkg.md`）：Lua 钩子规范、`ctx` 表定义、示例代码、检测优先级
- 更新安全模型文档（`docs/en/safety.md` + `docs/zh/safety.md`）：新增安装钩子安全说明与汇总表

---

## 0.9.8 (2026-07-22) — CLI 改进、默认仓库扩充与文档检查

1.0.0 之前最后一个功能版本，聚焦 CLI 输出一致性与仓库生态再扩充。

### 新增功能
- **CLI 输出统一 `[ezmk]` 前缀**：`ezmk pkg info`、`ezmk repo info`、`ezmk repo list` 输出统一添加 `[ezmk]` 前缀，新增 `util::info_line()` 辅助函数（无色、stderr、线程安全），与 `util::info()`/`warn()`/`error()` 形成一致的输出规范
- **`--verbose` 简写展开提示**：使用简写命令（`ri`/`ki`/`pb` 等 18 个）时，`-v`/`--verbose` 显示展开映射（如 `[ezmk] 简写展开: ri → repo info`）；`-v`/`--verbose` 提升为全局标志，所有子命令接受且静默忽略
- **默认仓库新增 20 个包**：
  - **stb 系列（10 个）**：`stb-image`、`stb-image-write`、`stb-image-resize`、`stb-truetype`、`stb-rect-pack`、`stb-perlin`、`stb-sprintf`、`stb-ds`、`stb-textedit`（header-only）+ `stb-vorbis`（需编译），均为 MIT/Public Domain 单文件 C 库
  - **Boost header-only 系列（10 个）**：`boost-config`、`boost-assert`、`boost-core`、`boost-static-assert`、`boost-throw-exception`、`boost-lexical-cast`、`boost-algorithm`、`boost-optional`、`boost-variant2`、`boost-mp11`（BSL-1.0，C++17，v1.87.0）

### 修复
- 子命令解析器返回新 `CliArgs` 时丢失顶层设置的字段（如 `shorthand_expansion`）— 捕获结果后传递
- `-v`/`--verbose` 作为全局标志时错误消费 `--` 之后的位置参数 — 添加 `--` 边界检查

### i18n
- 新增 i18n key：`shorthand_expansion`（en: `"shorthand: {mapping}"`, zh: `"简写展开: {mapping}"`）

### 测试
- 全量测试：**524 用例 / 2413 断言**，零失败、零回归

---

## 0.9.7 (2026-07-21) — 默认仓库生态扩展

仓库从 9 个包扩展到 31 个包，新增 header-only 和预编译包支持。

### 新增功能
- **22 个新包**：5 个独立包（`cli11`/`zlib`/`glfw`/`sdl2`/`yaml-cpp`）+ 1 个 `imgui` 核心 + 16 个 imgui 后端（7 平台 + 9 渲染器）
- **Header-only 包支持**：`ezmk.toml` 新增 `header_only = true`，安装时跳过编译步骤，仅复制头文件
- **预编译包支持**：`ezmk.toml` 新增 `precompiled = true`，支持分发预编译 `.a`/`.lib` 文件
- **包制作指南**：新增 `docs/en/package_authoring.md` + `docs/zh/package_authoring.md`

---

## 0.9.6 (2026-07-18) — 功能补全与生态完善

聚焦最后一个核心功能缺口（依赖版本锁定）和开发体验提升（构建进度、格式化基础设施、启动 Logo）。

### 新增功能
- **依赖版本锁定**：`ezmk.toml` 中 `[depends]` 支持 `pkg@1.2.3`（精确）、`pkg^1.0`（兼容）、`pkg~1.2`（近似）、`pkg>=1.0`（GTE）、`pkg>1.0`（GT）五种版本约束语法；纯字符串格式（无约束）向后兼容
- **构建进度显示**：并行编译（`-j > 1`）时显示 `[N/M] src/file.cpp` 逐文件进度 + `(cached)` 缓存命中标记 + 构建耗时摘要
- **ASCII Logo**：`ezmk` 裸运行时显示彩色 Logo（外部资源 `res/logo.txt`，编译期由 `scripts/embed_logo.py` 嵌入）
- **.clang-format**：项目根目录新增 `.clang-format` 配置（LLVM 风格 + 项目定制）

### 数据结构变更
- `config.hpp`：新增 `VersionConstraint`（含 Op 枚举）和 `DependsEntry`（name + constraint）结构体；`DependsSection::libs` / `want` 从 `std::vector<std::string>` 改为 `std::vector<DependsEntry>`
- `repo.hpp`：新增约束感知的 `search_package()` 重载

### 构建增强
- **构建失败摘要**：并行编译失败时显示 `Build failed (X cached, Y compiled, N error(s))` 摘要
- **版本约束运行时校验**：`build.cpp` 构建阶段验证已安装包版本是否满足 `ezmk.toml` 约束；`pkg.cpp` 安装阶段验证依赖版本

### i18n
- 新增 5 个 i18n key：`config_err_empty_depends_entry`、`config_err_version_missing`、`pkg_constraint_unsatisfied`、`build_elapsed_time`、`build_failed_summary`，含中英双语翻译

### 测试
- 测试套件：**514 个测试用例，2353 个断言全部通过**（+17 用例，+103 断言 vs 0.9.5.1）
- 新增 10 个解析测试：5 种运算符正向 + 新旧格式混用 + 空白处理 + want 约束 + 边界报错
- 新增 7 个约束校验测试：5 种运算符的匹配/不匹配边界
- 新增 1 个集成测试：5 个子场景覆盖精确/兼容约束匹配/不匹配 + 无约束向后兼容

### 文档
- `CONTRIBUTING.md`：新增 `.clang-format` 使用说明、IDE 集成指引、提交前检查清单

---

## 0.9.5.1 (2026-07-17) — 代码重构与质量清理

不新增用户可见功能，专注代码质量：消除重复、修复资源管理、补全测试盲区、移除死代码。

### 重构
- **`build.cpp` — 链接阶段统一**：提取 `execute_link()` 通用函数，`link_phase()` 中 6 个重复块（static/shared/exe × MSVC/GCC）简化为单行调用，消除 ~150 行重复
- **`cache.cpp` — 编译逻辑统一**：`compile_sources()` 改为委托 `compile_one_source()`，消除 ~270 行重复的编译管道代码
- **`file_watcher.cpp` — debounce 统一**：提取 `check_and_flush()` 成员函数，Windows/Linux/macOS 三处 ~20 行重复的 sleep→elapsed→flush 逻辑统一为单行调用
- **`main.cpp` — 仓库更新统一**：提取 `auto_update_repos()`，ProjectBuild/ProjectRun/ProjectWatch 三个分支中的重复 auto-update 代码块统一
- **`config.cpp` — 配置名校验统一**：提取 `is_valid_profile_name()`，`[compile.profile.*]` 和 `[link.profile.*]` 中重复的 profile 名称校验逻辑合并

### 资源管理修复
- `lua_api.cpp`：`g_cached_config` 原始指针 → `std::unique_ptr<config::EzConfig>`（异常安全）
- `file_watcher.cpp`：`OVERLAPPED*` 手动 new/delete → `std::unique_ptr<OVERLAPPED>` 池管理（消除泄漏风险）
- `lua_api.cpp`：全局变量线程安全性假设注释文档化
- `cache.cpp` + `build.cpp`：并行编译 record 只读不变量注释文档化

### 死代码移除
- 移除 `ParsedOptions::count()`（零调用方）
- 移除 `native_path()`（零调用方）

### i18n
- 新增 2 个 i18n key：`toolchain_msvc_detected`、`cache_hit_detail`，含中英双语翻译
- `toolchain.cpp` / `cache.cpp` 硬编码英文字符串 → i18n key

### 测试
- 测试套件：**497 个测试用例，2250 个断言全部通过**（+6 用例，+9 断言）
- **`compare_version()` 完整覆盖**（`test_util.cpp`）：10 个 TEST_CASE，覆盖相等、主/次/补丁差异、缺失段默认 0、预发布标签剥离、构建元数据剥离、宽数字段、长版本号、边界值
- **`extract_archive()` 基础覆盖**（`test_util.cpp`）：不支持格式抛出异常、无效 zip 抛出异常、不存在文件抛出异常
- **`compile_options_signature` 补全**（`test_cache.cpp`）：新增 `msvc_flags` 影响签名、`std_flag` 影响签名两项测试
- **`resolve_dependency_order` 增强**（`test_pkg.cpp`）：验证错误消息包含缺失包名
- **共享测试基础设施**：新建 `test/test_helpers.hpp`，提取 TempDir / CwdGuard / EnvGuard / write_minimal_config 等跨文件复用 fixtures
- `file_watcher.hpp` 平台宏复用 `util.hpp` 的 `EZMK_WIN`/`EZMK_MACOS`/`EZMK_LINUX`，删除重复检测逻辑

---

## 0.9.5 (2026-07-17) — 跨平台体验与质量保障

Windows 原生安装体验、端到端集成测试、三平台冒烟测试准备。1.0.0 之前的质量保障版本。

### 新增
- **PowerShell 安装脚本** (`install.ps1`)：Windows 原生一键安装，对标 `install.sh`。从 GitHub Release 下载预编译 `ezmk.exe`，SHA-256 校验，原子化安装到 `%LOCALAPPDATA%\ezmk\bin`，自动配置用户 PATH + 预注册官方仓库。支持 `-Version` / `-InstallDir` / `-NoPath` / `-DryRun` 参数
- **端到端集成测试** (`test/test_integration.cpp`)：7 个场景、41 个断言，覆盖完整 build pipeline —— 从零创建项目 → 编译 → 运行、增量构建缓存命中、Watch 模式文件变更检测、`ezmk utils cc` 生成 compile_commands.json、项目目录布局验证、CLI 基本命令（version/help）。全部标记 `[integration]` tag，支持按需运行或跳过
- **`build.sh` 测试模式扩展**：新增 `test-all`（单元 + 集成）、`integration`（仅集成测试）目标；test 模式默认跳过 `[integration]` 用例（`~"[integration]"`）；集成测试前自动编译 ezmk 二进制；通过 `EZMK_TEST_BIN` 环境变量传递给测试

### 变更
- **Windows 安装文档**：`README.md` / `README_ZH.md` / `docs/en/cli.md` / `docs/zh/cli.md` 新增 Windows 原生安装章节（`install.ps1` 使用说明 + 参数表）
- **环境变量表扩展** (`docs/en/cli.md` / `docs/zh/cli.md`)：新增 `EZMK_TEST_BIN` 条目

### 测试
- 测试套件：**491 个测试用例，2250 个断言全部通过**（+9 用例，+41 断言）
- 单元测试 482 用例 2209 断言；集成测试 9 用例 41 断言
- Watch 模式测试为时序敏感型（Windows 上可能假阴性），使用 WARN 而非 FAIL

---

## 0.9.4 (2026-07-15) — 文档与质量完善

文档补全与代码质量打磨，不新增核心功能。

### 新增
- **FAQ / 故障排除文档**：`docs/en/faq.md` + `docs/zh/faq.md`，覆盖安装/构建/包管理/配置/跨平台五大类 25+ 条常见问题及排错流程
- **离线场景文档**：FAQ 新增离线使用章节，涵盖本地仓库镜像、手动下载归档、USB/内网共享镜像三种方案
- **Lua API 版本化基础设施**：新增 `EZMK_LUA_API_VERSION` 常量（当前为 1）+ `ezmk.api_version` Lua 字段，脚本可通过 `if ezmk.api_version >= 2 then ... end` 做兼容性判断
- **`util::closest_match()` 模糊匹配函数**：基于 Levenshtein 编辑距离，为未知命令/profile 提供 "did you mean" 建议
- **API 版本化策略文档**：`docs/en/utils.md` + `docs/zh/utils.md` 新增"API 版本化"章节，定义向后兼容策略（仅不兼容变更触发版本号递增；废弃函数保留 ≥2 个 minor 版本后移除）

### 变更
- **错误信息打磨**：修复 `cli.cpp` 空异常消息（`throw std::invalid_argument("")` → i18n 化错误）；未知 profile 和未知命令增加 "did you mean" 模糊匹配建议
- **`std::runtime_error` 审计**：排查 `src/` 中所有裸 `throw std::runtime_error(...)` 位置，面向用户的错误信息改为 i18n 化
- **CHANGES.md 补全**：补全 0.9.0 ~ 0.9.4 版本条目

---

## 0.9.3 (2026-07-14) — 捆绑包迁移

将 7 个捆绑预编译库包迁移至官方仓库，清理主项目冗余文件。

### 变更
- **7 个捆绑包迁移至官方仓库** (`ezmk-repo`)：catch2 (3.6.0), fmt (10.2.1), lua (5.4.7), nlohmann_json (3.11.3), spdlog (1.14.1), sqlite3 (3.46.0), tinyxml2 (11.0.0)
- **逐包标准化**：补全 `version` 字段、TOML 格式更新 (`include_dir` → `include_dirs`)、补 `language` 字段、清理硬编码 `-Wall -O2`
- **仓库侧**：`sources/` 新增 7 个源工程，`packages/` 新增 7 个归档，`index.toml` 含 9 个包条目，`validate.sh` 全部通过
- **主项目清理**：删除 `pkg/` 下 7 个 `.tar.gz` 捆绑归档，`ezmk-cc/` 目录保留（内置工具源码参考）
- **`install.sh` 清理**：移除捆绑包拷贝逻辑（已预注册官方仓库，`pkg install` 自动从仓库拉取）

---

## 0.9.2 (2026-07-13) — 文档多语言

`docs/` 和 `tutorial/` 拆分为 `en/` + `zh/` 双语目录，补齐英文翻译。

### 变更
- **目录重组**：`docs/` → `docs/en/` + `docs/zh/`，`tutorial/` → `tutorial/en/` + `tutorial/zh/`
- **英文翻译补齐**：`cli.md`, `pkg.md`, `repo.md`, `utils.md`, `config_file.md`, `cache.md`, `safety.md` 全部提供英文版
- **术语表** (`glossary.md`)：中英双语对照，随新功能扩展更新
- **CI 文件对应检查**：确保 `docs/en/` ↔ `docs/zh/` 一一对应

---

## 0.9.1 (2026-07-12) — 默认仓库创建

创建官方默认仓库，建立包生态基础设施。

### 新增
- **官方默认仓库** (`ezmk-repo`)：GitHub 托管，Gitee 镜像，符合 `docs/repo.md` 结构
- **预注册策略**：`install.sh` 安装时自动将官方仓库注册到用户作用域，用户装完即可按名装包
- **初始示例包**：`hello-lib` (static) + `example-utils` (utils)，含完整源工程
- **打包流程**：`pack.sh` (打包 + SHA-256) + `validate.sh` (校验)，CI 可复现
- **贡献流程文档**：`CONTRIBUTING.md` + `CONTRIBUTING_ZH.md`

---

## 0.9.0 (2026-07-10) — 准备发布正式版

首个面向用户的正式版准备，聚焦"能装上、能看懂、能上手"。

### 新增
- **一键安装脚本** (`install.sh`)：`curl -fsSL <url> | bash` 一键安装，支持 Linux/macOS/MSYS2，幂等可重入，失败即清理
- **文档整理**：`docs/cli.md` 完整 CLI 参考，`docs/safety.md` 安全性集中化文档
- **README 双语互链**：`README.md` (EN) ↔ `README_ZH.md` (ZH)
- **上手教程** (`tutorial/`)：从零创建项目 → 添加依赖 → 构建运行的分步教程

---

## 0.2.6 (2026-07-11) — 翻译补全与命令行改进

可用性收尾版本，无新增构建/包管理能力，聚焦 i18n 系统性修复与命令行打磨。

### Bug 修复
- **根除 `{???}` 显示 bug**：`ezmk help` / `pkg list` / `repo list` 等命令输出的 `{???}` 占位符消失。根因是 `src/i18n.cpp` 手写的 `key_name()` switch 漏登记了 0.2.3~0.2.5 新增的约 50 个 `I18nKey` 枚举值（枚举 / switch / JSON 三处数据源手动同步时漏了中间一处），而非 JSON 缺翻译
- **POSIX `run_command()` stderr 捕获修复**：用花括号组 `{ cmd ; } 1>out 2>err` 包裹重定向，确保被调命令自身的 fd 重定向（如 `>&2`）不污染 stdout/stderr 捕获（修复 Linux 上暴露的 2 个既有 `test_util` 失败）

### 新增
- **i18n 单一数据源（X-macro）**：新增 `include/ezmk/i18n_keys.def`，`I18nKey` 枚举与 `key_name()` 映射均由它生成，从结构上杜绝三处失配。新增键 = `.def` 加一行 + 两份 JSON 各加一条
- **开发期缺失键审计**：`i18n::init()` 末尾的 `audit_missing_keys()`（仅 `NDEBUG` 未定义时启用），对枚举有键但 JSON 缺翻译的情况逐一告警一次
- **命令简写**：顶层别名在 `cli::parse()` 分发前展开 —— `pn/pb/pr/pc/pw`（project）、`ki/kr/ks/kn/kl/ku`（pkg）、`ra/rr/rl/ru/ri`（repo）、`u/h/v`（utils/help/version）。仅在命令位生效，不进 zsh 补全，仅在帮助页展示
- **全局 `--color=<mode>`**：`always`/`enable`、`auto`/`default`、`never`/`disable`（大小写不敏感）。显式 `always`/`never` 覆盖 `NO_COLOR`，仅 `auto` 尊重之（对齐 git/ls）；`always` 亦尝试开启 Windows VT100

### 变更
- **帮助正文全本地化**：`print_usage()` 每条命令/flag 的说明文字改走 i18n（约 30 个 `help_*` 键），命令用法串保持字面
- **参数校验报错本地化**：`src/cli.cpp` 各 `parse_*_args()` 的硬编码英文 `util::fatal` 替换为 i18n 键（`cli_arg_required` / `cli_too_many_args` / `cli_unknown_subcommand` 等 + `arg_*` 名词键）
- **`repo list` 专属键**：新增 `repo_list_title` / `repo_list_none`，不再复用语义为"已安装包"的 `pkg_list_*`
- **代码卫生**：确认 `src/pkg.cpp` 全局安装确认处注释为正常 `// Safety:`

### 测试
- 测试套件：**476 个测试用例，2180 个断言全部通过**（Windows UCRT64 g++ + Linux Arch g++）
- 新增回归防线：遍历全部 `I18nKey` 枚举，断言 `get(key)` 不以 `{` 开头（直接卡住 `{???}` 类 bug）
- 新增 `[alias]`（26 断言）与 `[color]`（16 断言）用例组

---

## 0.2.5 (2026-07-09) — 生态与安全

### 新增
- **zsh 命令补全**：静态补全脚本 `completions/_ezmk`，覆盖全部命令、子命令与 flag
- **Utils 细粒度权限管理**：`[utils.permissions]` read/write/run 白名单，脚本越权访问被拒；未声明权限的旧包行为不变 + deprecation warning（向后兼容）
- **`ezmk repo info`**：显示仓库名称、作用域、URL、类型、分支、更新时间、缓存路径与包版本列表
- **跨仓库版本选择**：同名包在多个仓库中出现时，按语义化版本比较选取最新
- **仓库本地校验增强**：`index.toml` 中 file 存在性检查与 sha256 格式校验
- **`--auto-update`**：`pkg install` / `search` 前自动 `git pull` 已注册仓库

### 测试
- 测试套件：**测试全部通过**（Windows + Linux）

---

## 0.2.4 (2026-07-08) — 健壮性与完善

### Bug 修复
- **版本比较逻辑统一**：新增 `util::compare_version()`（`src/version.cpp`），替换 `pkg.cpp` 的字符串比较和 `repo.cpp` 的内联数值比较，正确处理 `1.10.0` vs `9.0.0` 等边界
- **Shell 注入风险修复**：`build.cpp` 全部 4 个链接命令构建器 + `cache.cpp` 全部 2 个编译命令构建器，所有路径和标志统一使用 `util::escape_shell_arg()`
- **`/tmp` 硬编码修复**：`run_command()` 中的临时文件改用 `$TMPDIR` 环境变量 + 动态路径拼接，移除硬编码魔数偏移

### 代码质量
- **`build_project()` 重构**：530 行函数拆分为 `BuildState` 结构体 + `prepare_build_state()` + `compile_phase()` + `link_phase()` + `run_hook()` 五个模块，主函数降至 ~20 行编排逻辑
- **CLI 标志解析去重**：`parse_build_flag()` lambda 统一 `build`/`run`/`watch` 三个命令的 `--disable-cache`/`--verbose`/`-j`/`--profile` 解析，减少 ~70 行重复代码
- **帮助文本 i18n**：章节标题全部使用 I18nKey 枚举，支持中英文切换

### 功能补全
- **C23 语言标准支持**：`config.cpp` 语言版本映射已包含 C23（`-std=c23`）
- **`pkg update --all`**：批量更新全部已安装包，自动遍历 → 版本比较 → 安装，输出 `N updated, M up-to-date, K failed` 摘要
- **扩展 GCC→MSVC 标志映射**：新增 17 个标志（`-fno-rtti`→`/GR-`、`-fno-exceptions`→`/EHs-c-`、`-ffast-math`→`/fp:fast`、`-fstack-protector`→`/GS` 等），总计 58 个映射

### 工程规范
- `License` 重命名为 `LICENSE`
- 历史计划文件 checkbox 全部标记为 `[x]`
- 补打 6 个缺失的 git tag（v0.1.6 ~ v0.2.3）

### 测试
- 测试套件：**411 个测试用例，409 通过**（2 个预存 i18n 失败）

---

## 0.2.3 (2026-07-04) — 开发者体验提升

### 新增
- **并行编译 `-j` / `--jobs`**：基于 `ThreadPool`（`include/ezmk/thread_pool.hpp`）的多线程编译，默认自动检测 CPU 核数
- **构建 Profile `--profile`**：`[compile.profile.<name>]` / `[link.profile.<name>]` 预定义配置段，Profile 标志追加到基础标志后
- **Build Hooks `[hooks]`**：`pre_build` / `post_build` / `on_failure` Lua 脚本，在构建生命周期各阶段自动执行
- **Watch 模式 `ezmk project watch`**：跨平台 `FileWatcher`（Windows IOCP / Linux inotify / macOS kqueue），300ms 防抖，配置变更触发全量重建
- **`ezmk pkg list`**：列出全部已安装包（含版本、类型、工具列表）
- **`ezmk pkg update`**：从注册仓库更新指定包到最新版本

### 修复
- `list_sources()` 不再仅扫描 `src/`，跟随 `src_dirs` 配置
- `ezmk-cc` 的 `cc.lua` 不再硬编码 `g++`，改为使用检测到的编译器
- 移除多处裸 `catch(...)`，改为具体异常处理

### 测试
- 测试套件：**333+ 用例，973+ 断言**

---

## 0.2.2 (2026-07-02) — 精细化编译控制

### 新增
- **可选依赖 `[depends].want`**：缺失时 warn + 自动定义 `EZMK_LIB_MISS_<NAME>` 宏，不阻断构建。`lib` 中的硬性依赖仍然缺失即 fatal
- **语义化宏定义 `[compile.macros]`**：独立 TOML 子节管理预处理器宏。支持字符串/整数/布尔值类型，布尔 `false` 自动跳过
- **标准预定义宏 `compile.ezmk_macros`**：默认自动注入 `EZMK` / `EZMK_VERSION` / `EZMK_PROJECT_NAME` / `EZMK_PROJECT_VERSION` / `EZMK_PROJECT_TYPE` / `EZMK_LANG` 六个标准宏，用户可在 `[compile.macros]` 中覆盖
- **多源目录 `compile.src_dirs`**：支持 `["src", "lib", "vendor"]` 等多目录源文件扫描。文件名去重（同文件跨目录 warn），`main.cpp` 跨目录查找，默认 `["src"]` 向后兼容
- **4 个新 API** (`build.hpp`)：`macros_to_flags()`, `generate_ezmk_macros()`, `want_to_macro_name()`, `collect_sources()`

### 变更
- **`include/ezmk/config.hpp`**：`CompileSection` 新增 `src_dirs` / `macros` / `ezmk_macros`；`DependsSection` 新增 `want`
- **`src/config.cpp`**：解析四个新字段；宏名合法性校验；布尔/整数/字符串值类型处理
- **`src/build.cpp`**：有效标志合并（ezmk_macros → flags → macros → want）；多目录源文件收集；可选依赖包扫描
- **`src/pkg.cpp`**：`install()` / `resolve_dependency_order()` / `compile_package()` / `info()` 中 want 依赖处理
- **`src/cache.cpp`**：缓存签名包含 `msvc_flags` + `std_flag` + `extra_includes`；补全 `check_cache` 重载链

### 修复
- GCC 编译命令优先使用运行时检测的编译器（`detected_compiler`），而非硬编码 `"g++"`
- 缓存签名修复：全局签名与逐文件签名一致（修复有依赖包时缓存永久全量重编译）
- `collect_sources` 去重改用 `filename()`（含扩展名），避免 `util.cpp` 和 `util.c` 误判为重复
- 合并 `.ezmk/pkg/` 双重扫描为单次遍历

### 测试
- 测试套件：**333 个测试用例, 973 个断言全部通过**（+71 用例, +175 断言）

---

## 0.2.1 (2026-06-30) — MSVC 支持

### 新增
- **`Toolchain` 抽象层**（`include/ezmk/toolchain.hpp` + `src/toolchain.cpp`）：`CompilerFamily::Gcc/Clang/Msvc` 枚举，自动检测可用工具链
- **GCC→MSVC 标志翻译层**：`translate_compile_flags()` / `translate_link_flags()`，覆盖常用标志（`-Wall`→`/W4`、`-O2`→`/O2`、`-g`→`/Zi` 等）
- **MSVC 依赖解析**：`parse_show_includes()` 解析 `/showIncludes` 输出替代 `-MMD`
- **`cl.exe` / `link.exe` / `lib.exe`** 完整编译/链接/归档流程
- **`vcvars64.bat` 环境自动加载**：捕获环境变量 map，一次加载全流程复用
- **`ezmk.toml` 扩展 `msvc_flags`**：MSVC 专用编译/链接标志，GCC 模式下被忽略

### 变更
- **`src/cache.cpp`**：MSVC 编译命令生成（`/utf-8`、`/MD` 默认标志）
- **`src/build.cpp`**：MSVC 链接命令构建器（EXE/DLL/LIB）；产物路径适配（`.obj` / `.lib` / `.exe`）
- **`include/ezmk/config.hpp`**：`CompileSection` / `LinkSection` 添加 `msvc_flags`

### 测试
- 测试套件：**262 个测试用例, 886 个断言全部通过**（+88 断言）

---

## 0.2.0 (2026-06-28) — Lua 工具链

### 新增
- **嵌入式 Lua 5.4.7**：静态链接进 `ezmk` 二进制（`include/vendor/lua/` + `src/vendor/lua/`，32 源文件）
- **`include/ezmk/lua_api.hpp`** + **`src/lua_api.cpp`**：22 个 C++ → Lua 绑定函数
- **ezmk Lua API**：
  - 项目信息（5）：`project_root`, `project_name`, `project_type`, `project_config`, `build_dir`
  - 编译选项（4）：`compile_flags`, `include_dirs`, `link_flags`, `link_dirs`
  - 文件系统（4）：`list_sources`, `file_exists`, `file_read`, `file_write`
  - 进程执行（2）：`run` → `{exit_code,stdout,stderr}`, `run_capture`
  - 日志输出（3）：`info`, `warn`, `error`
  - 路径工具（3）：`pkg_dir`, `temp_dir`, `cache_dir`
  - JSON（2）：`json_encode`, `json_decode`（基于 nlohmann/json，含 Lua table ↔ JSON 双向转换）
- **Sandbox 安全模型**：每次调用独立环境表（脚本间零污染）、`io`/`os` 库编译期移除、`file_write` 拒绝项目根目录外写入
- **`find_utils_script()`**（`util.cpp`）：按项目 → 用户 → 全局 → 开发作用域查找 Lua 工具脚本
- **内置工具包 `pkg/ezmk-cc/`**：`ezmk utils cc` 生成 clangd 兼容的 `compile_commands.json`
- **`test/test_lua.cpp`**：61 个测试用例、212 个断言

### 变更
- **`main.cpp`**：`Command::Utils` 从占位实现改为完整 Lua 脚本执行；进程启动/退出时 `lua::init()`/`lua::shutdown()`
- **`src/vendor/lua/linit.c`**：移除 `io` 和 `os` 库注册（安全沙箱）
- **`build.sh`**：Lua 源文件加入编译；新增 `-DLUA_COMPAT_5_3` 和 `-I include/vendor/lua/`
- **`src/config.cpp`**：`type` 字段校验（`executable`/`static`/`shared`/`utils`）
- **`src/pkg.cpp`**：`validate_pkg()` 对 `type = "utils"` 包放宽 `include/`/`src/` 检查；无 `src/` 的 utils 包跳过编译
- **i18n**：新增 5 个 Lua 相关 I18nKey（`lua_init_failed`, `lua_error`, `lua_api_type_error`, `lua_api_arg_count`, `utils_not_found`）

### 测试
- 测试套件：**262 个测试用例, 798 个断言全部通过**（+61 用例, +212 断言）

---

## 0.1.8 (2026-06-24) — 跨平台支持与编译器探测

### 新增
- **`detect_compiler()`**（`src/build.cpp`）：多级编译器自动探测（`$CXX`/`$CC` → 平台候选列表 → 安装指引）
- **平台宏完善**（`util.hpp`）：`EZMK_MACOS` / `EZMK_LINUX` / `EZMK_WIN` 三平台互斥宏
- **`EZMK_OBJ_SUFFIX` 修正**：MinGW 上从 `.obj` 改为 `.o`（MinGW g++ 实际产出）
- Apple Clang 检测提示（macOS 上 `g++` 可能是 clang 别名）

### 变更
- **`build.cpp`**：`find_compiler()` 重构为调用 `detect_compiler()`；编译器验证逻辑简化
- **`config.hpp`**：`LanguageInfo` 新增 `detected_compiler` 字段
- **`build.sh`**：macOS 平台 `-static` 处理、`$CXX`/`$CC` 环境变量支持
- **`test/test_build.cpp`**：21 个编译器探测测试用例

---

## 0.1.7 (2026-06-22) — 基本国际化（i18n）

### 新增
- **i18n 模块** (`include/ezmk/i18n.hpp`, `src/i18n.cpp`)：编译期 JSON 嵌入 + I18nKey 枚举方案
- **85 个 I18nKey**，覆盖 build / cache / pkg / repo / project / run / editor / general 全部模块
- **locale/en.json** + **locale/zh.json**：英文和中文资源文件，占位符使用 `{key}` 格式
- **`scripts/embed_locale.py`**：将 `locale/*.json` 编译期嵌入二进制（零外部文件依赖）
- **`include/ezmk/version.hpp`**：由 `build.sh` 自动生成版本号头文件
- **`test/test_i18n.cpp`**：19 个测试用例覆盖 key 一致性 / fmt 替换 / 语言检测 / fallback 行为

### 变更
- **日志系统** (`util.hpp` / `util.cpp`)：新增 `info/warn/error/fatal(I18nKey, args)` 重载，翻译后再着色输出
- **main.cpp**：启动时调用 `i18n::init()` 自动检测语言；`version` 输出使用 i18n
- **build.cpp**：15 处字符串迁移（building / compiling / linking / build_success 等）
- **cache.cpp**：11 处字符串迁移（cache_hit / cache_miss / compilation_failed 等）
- **pkg.cpp**：31 处字符串迁移（installing / downloading / verifying / sha256 等）
- **repo.cpp**：10 处字符串迁移（cloning / pulling / repo_added 等）
- **project.cpp**：6 处字符串迁移（creating_project / init_git 等）
- **util.cpp**：7 处迁移（I18nKey 日志重载 + no_editor / opening_editor / editor_error）
- **build.sh**：编译前自动运行 `embed_locale.py`，生成 `version.hpp`，支持 `EZMK_VERSION` 环境变量
- **test_main.cpp**：全局 `i18n::init("en")` 初始化，测试输出使用英文

### 语言检测
- 优先级：`EZMK_LANG` 环境变量 > 系统语言 (Windows `GetUserDefaultLocaleName` / Linux `$LANG`) > 默认 `en`
- 支持 `zh-CN` → `zh`、`en-US` → `en` 自动规范化
- 运行时 `locale/<lang>.json` 文件 > 嵌入式数据 > 硬编码英文 fallback

### 测试
- 测试套件：**192 个测试用例, 573 个断言全部通过**
- i18n 专项测试：19 个 TEST_CASE, 118 个断言
