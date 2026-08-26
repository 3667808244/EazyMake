# Changelog

## API Stability

As of v1.1.0, the following public APIs are **permanently stable**:

**Commands:** `build`, `run`, `clean`, `watch`, `install`, `test`, `pack` (top-level) and their `project <action>` equivalents; `pkg install/remove/search/info/list/update`; `repo add/remove/update/list/info`.

**Configuration:** `[project]`, `[compile]`, `[link]`, `[depends]`, `[test]`, `[install]` core fields in `ezmk.toml`.

**Extended in v1.3.0 (1.3.0-pre.1):**

- **Commands:** `workspace list/build/test/clean` (with `-w` / `--workspace` redirect on `build`/`test`/`clean`, `--member` including the dependency closure, `--stop-on-error`, `-j` / `--jobs`).
- **Configuration:** `ezmk-workspace.toml` (`[workspace]` `name`/`members`, `[workspace.options]` `default_jobs`/`stop_on_error`); the `workspace` field of `[depends]` in member `ezmk.toml` files.
- **Environment:** `EZMK_LANG` variant tags — BCP 47 normalization (`zh_CN`/`zh-CN`/`zh_CN.UTF-8` → canonical) and the variant → base → English fallback chain.

Breaking changes are introduced only in `2.0.0`, preceded by deprecation warnings in at least one minor version (`1.x.0`).

---

## 1.3.6 (2026-08-25) — 代码质量收口（技术债清理）

1.3.6 是 1.3.0 发布后的**补丁版本**，主题为**代码质量收口**（承接 1.3.5 后的质量分析结论）：`-Wall -Wextra` 清零、错误文案与实现一致、校验去噪、归档/运行逻辑去重、`run_tests` 机械拆分、测试文件按主题拆分。**纯重构零新功能**——不引入新 flag/配置/命令/i18n key；每个重构都有既有测试锁定（等价性/集成/全量回归）。**公共 API 无破坏性变更**。

### 变更

- **`-Wall -Wextra` 清零**：`detect_catch2` 删除未用参数 `depends`（同步唯一调用点）；`export.cpp` 的 `&&`/`||` 表达式补括号（逻辑本正确，纯可读性）
- **错误文案与实现一致**：`parse_language` 的 unknown-version 报错改为从 `ver_map` 键生成支持列表（旧硬编码文案缺 `98/03/26`，误导用户以为不支持）
- **校验去噪**：`consumer_std_min` 在消费者 `[project].language` 非法时**每进程只 warn 一次**（多包安装不再刷屏；首次语义不变）
- **归档遍历共享**：新增 `util::collect_stage_entries()`，`create_targz`/`create_zip` 复用同一遍历（相对路径/`/` 分隔/排序），两种归档布局保持逐文件一致（1.3.5 等价性测试锁定）
- **运行逻辑共享**：新增 `run_executable(exe, args, warn_on_nonzero)`，`project run` 与 `watch --run` 复用（running key/参数组装/阻塞运行/回显；`run` 转发退出码、watch 警告不退出）
- **`run_tests` 机械拆分**（~550 行单函数）：提取 `TestRunContext` + `run_catch2_tests`/`run_ezmk_tests`，`run_tests` 变薄调度（前置组装 + 按框架分发）；纯搬运，`git diff -w` 复核 + 全量回归锁定
- **测试文件按主题拆分**：新增 `test/test_integration_helpers.hpp`（共享 helper，`namespace ezi`）+ `test_integration_workspace.cpp` + `test_integration_report.cpp`（1.3.2/1.3.4/1.3.5）；`test_integration.cpp` 3757 → 2610 行；用例/断言数完全不变

### 文档

- `CHANGES.md` 本条目（无 cli.md/README 变更——零新功能）

### 测试

- 新增 3 个单测：unknown-version 文案含 `98/03/26`、`collect_stage_entries`（子目录/空文件/排序/斜杠）、`consumer_std_min` 去噪（双包安装只 warn 一次）
- 全量 **911 用例 / 5302 断言零回归**（1.3.5 基线 908/5283，+3 用例/+19 断言）；测试拆分后用例/断言数不变

### 已知限制 / 跟进项

- **build.cpp/pkg.cpp 全面重构**、**Catch2 结构化解析**：归 1.4.0。
- **cli.cpp 命令组拆文件**（`parse_*` 1272 行单文件）：2.0.0 前评估。
- **1.3.1~1.3.5 延后功能项**（`--run` 参数透传 / `workspace watch` / `tgz` 别名 / sha256 边车自动校验）：归 1.4.0。
- **旧二进制（<1.3.6）**：本版零新功能，无兼容差异。

---

## 1.3.5 (2026-08-25) — pack 多格式输出（`--format zip|tar.gz`）+ SHA-256 边车

1.3.5 是 1.3.0 发布后的**补丁版本**（1.3.x 系列最后一个规划补丁；与 1.3.1 ~ 1.3.4 相互独立、可并行）：`ezmk project pack --format zip|tar.gz` 产**多格式归档**（缺省 `tar.gz` 现状不变；zip 走 vendored miniz）——内容与 tar.gz **逐文件等价**（同一 stage 流程，仅归档器不同），`pkg install` 消费路径（`extract_archive`）早已支持 zip，端到端闭环。附带 **`.sha256` 边车**（两种格式统一）。**公共 API 无破坏性变更**（纯新增 flag + util + i18n key）。

### 新增 / 行为变更

- **`ezmk project pack --format <tar.gz|zip>`**：`zip` 产 `name-version.zip`（缺省 `tar.gz` 行为完全不变）；大小写不敏感；非法格式（如 `deb`）→ fatal（`cli_err_invalid_format`）
- **`util::create_zip`**：miniz `mz_zip_writer_*`；条目名与 `create_targz` 完全一致（相对 stage、`/` 分隔符统一——Windows 不产生反斜杠条目，坑 1；组件级路径安全校验防 `..` 逃逸，坑 2；遍历排序一致）
- **`.sha256` 边车**：pack 成功后写 `<archive>.sha256`（`<hash>  <filename>`，tar.gz 与 zip 统一；纯新增文件不影响既有消费）；与 `--precompiled` 可组合（预编译包也可选 zip）

### 文档

- `docs/cli.md`（zh/en）：pack 节补 `--format` 标志表 + `.sha256` 边车说明；命令表更新
- `README`（en/zh）：命令速览 pack 行补 `--format zip|tar.gz`
- `CHANGES.md` 本条目

### 测试

- CLI 解析：`--format zip`/`ZIP`/`tar.gz`/缺省 + 非法（`deb`/`tgz`）拒绝
- 集成：① zip 端到端（`pack --format zip` → `pkg install <zip>` 消费者侧编译成功）② 等价性（同项目 tar.gz/zip 解包后文件清单 + 内容 sha256 一致）③ 默认回归（无 `--format` → `.tar.gz`，不产 `.zip`）④ 非法格式 fatal ⑤ 边车（两种格式 `.sha256` 与 `crypto::sha256_file` 一致、含文件名）
- i18n：新增 `cli_err_invalid_format` 三向一致；`check_i18n.py` 通过（374 keys）

### 已知限制 / 跟进项

- **`.deb` / `.rpm` 等包管理器格式**：明确不做（`fpm` + `ezmk project install --prefix <staging>` 配方覆盖，non-goals 方向）；发布自动化不进 CLI。
- **`--format` 扩展其他归档（`tgz` 别名等）**与 **sha256 边车纳入 `pkg install --sha256` 自动校验**（与 index.toml 联动）：归 1.4.0 或后续评估。
- **旧二进制（<1.3.5）**：不认识 `--format` → 报 unknown option；新 flag 需 ≥1.3.5。

---

## 1.3.4 (2026-08-25) — watch 重建后自动运行（`ezmk watch --run`）

1.3.4 是 1.3.0 发布后的**补丁版本**（与 1.3.1 语言区间、1.3.2 测试报告、1.3.3 简写相互独立、可并行）：`ezmk watch --run` / `-r` —— 每次**成功**重建后**阻塞运行**产物，程序退出后 watch 继续监听（"改代码自动重跑"）。watcher 线程阻塞 = 程序运行期间天然暂停变更检测，退出后自动恢复——零进程管理。**公共 API 无破坏性变更**（纯新增 flag + i18n key）。

### 新增 / 行为变更

- **`ezmk watch --run` / `-r`**：每次成功重建后在 watcher 线程阻塞运行新产物；复用 `running` key / `run_command` / stdout-stderr 回显（与 `ezmk run` 的 I/O 行为一致）
- **生命周期（定死边界）**：阻塞运行，程序自行退出 → watch 恢复；非零退出 → `watch_run_exit_nonzero` **警告不退出**（watch 是循环非一次性 run）；Ctrl+C 与子进程同前台进程组一起终止（用户意图"全停"）；**拒绝** kill-重启 / detached 启动 / 仅缓存未命中时运行
- **类型门禁**：仅 `executable` 项目；`static`/`shared`/`utils` + `--run` → 启动 fatal（`cli_err_run_needs_executable`）
- **构建失败不运行**：只在 try 成功分支运行（绝不运行旧产物），catch 分支继续 watch
- **首次运行发生在第一次变更后**：初始构建不运行，与 `--no-build-on-start` 正交
- **默认行为完全不变**：无 `--run` 时与现状一致（`build_project()` 返回值本已存在，watch 不再丢弃）

### 文档

- `docs/cli.md`（zh/en）：watch 节补 `--run` 标志表 + 语义/生命周期说明（阻塞运行/非零退出警告/Ctrl+C 同进程组/长驻程序暂停检测）
- `CHANGES.md` 本条目

### 测试

- CLI 解析：`watch --run` / `-r` / 默认关闭 / 与 `--no-build-on-start` 正交
- 集成：① executable + `--run`：改源 → 轮询断言标记输出出现（两次变更 → 运行两次；初始构建不运行）② 编译错误 → 不运行、watch 存活、修复后恢复运行 ③ 非 executable + `--run` → 启动 fatal ④ 无 `--run` → 只有构建输出、无程序输出
- i18n：新增 `cli_err_run_needs_executable` / `watch_run_exit_nonzero` 三向一致；`check_i18n.py` 通过（373 keys）

### 已知限制 / 跟进项

- **`--run` 的 `--` 参数透传**（watch 现 `reject_positionals`）与 **`workspace watch` 命令组**：归 1.4.0 或后续评估。
- **kill-重启 / detached 启动 / 仅缓存未命中时运行**：§3.2 决策记录，定死边界不做。
- **旧二进制（<1.3.4）**：不认识 `--run`/`-r` → 报 unknown option；新 flag 需 ≥1.3.4。

---

## 1.3.3 (2026-08-25) — workspace 双字母命令简写

1.3.3 是 1.3.0 发布后的**补丁版本**（与 1.3.1 语言区间、1.3.2 测试报告相互独立、可并行）：为 1.3.0 的 **`workspace` 命令组**补齐双字母简写——`wl`/`wb`/`wt`/`wc` → `workspace list/build/test/clean`（`kAliases` 表加 4 行，沿用「组首字母 + 子命令首字母」规则，与 p/k/r 一致；无任何 `w*` 键冲突，与 `-w` 重定向 flag 正交）。**公共 API 无破坏性变更**（纯增量别名）。

### 新增 / 行为变更

- **workspace 简写**：`wl`/`wb`/`wt`/`wc` 在命令位置展开（`ezmk wb` ≡ `ezmk workspace build`），下游 `Command::Workspace*` 分发与 `--verbose` 展开记录（`wb → workspace build`）自动生效
- **命令位置限定不变**：`ezmk workspace wb` 仍报未知子命令（简写只在 `argv[1]` 生效）
- **workspace 命令接受 `-v`/`--verbose`**（1.3.3 附带）：此前 workspace 命令组（与 `-w` 重定向）会拒绝 `-v`——与其他命令组不一致，也使简写展开提示无法展示；现接受并忽略（workspace 无逐命令 verbose 语义）。纯增量（原报错输入现可解析）

### 文档

- `docs/cli.md`（zh/en）：命令简写表补 workspace 4 个 + workspace 简写说明（与 `-w` 正交、`w` 单字母与 `example` 组刻意不做）
- `CHANGES.md` 本条目
- **无新 i18n key**（全部复用既有错误消息）——`.def`/JSON/`locale_data.cpp` 零改动

### 测试

- CLI 解析：4 个展开断言（→ `Command::WorkspaceList/Build/Test/Clean`）+ flag/positional 透传（`--report`/`--member`/`-j`）+ 命令位置限定（`workspace wb` 报错）+ `--verbose` 展开记录 + 既有 p/k/r/u/h/v 简写回归
- i18n：三向一致性零改动（`check_i18n.py` 仍通过，371 keys）

### 已知限制 / 跟进项

- **刻意不做**：`completions/_ezmk` 加简写（与既有设计决定一致）、`w` 单字母（workspace 有子命令，单字母歧义）、**`example` 组简写（定死边界，不留后续评估）**。
- **`-w` 与 `w*` 简写组合的文档示例**：归 1.4.0 或后续评估。
- **旧二进制（<1.3.3）**：不认识 `wl`/`wb`/`wt`/`wc` → 报未知命令；新简写需 ≥1.3.3。

### 发布（2026-08-25，tag `v1.3.3`）

- **GitHub Release**：windows-x64 zip + linux-x64 / macos-arm64 tar.gz + 独立 `ezmk.exe`/`ezmk-lua.exe`（含 `.sha256` 边车）已上传（`macos-x64` 仍无资产——`macos-13` runner 在 free tier 不分配，与 1.2.x/1.3.0 相同）
- **winget**：split manifests 已提交 `microsoft/winget-pkgs#423828`（`InstallerSha256` 取 Release 资产真实 digest `e9a152c4…`；`license/cla` 已 pass，CI 校验进行中，版主审批为发布后跟进项，不阻塞发布）
- **Homebrew**：tap `3667808244/homebrew-eazymake` 公式已更新至 1.3.3（macos-arm64 `c74424b5…` / linux-x64 `a92f2264…` 真实 digest；仓库副本 `publish/homebrew/ezmk.rb` 同步）
- **pacman**：`publish/arch/PKGBUILD` `pkgver=1.3.3` + 源码 tarball 真实 digest `f67602c7…`（2026-08-25 实测下载）；AUR 仍延后

---

## 1.3.2 (2026-08-24) — 单元测试机器可读报告（`ezmk test --report`）

1.3.2 是 1.3.0 发布后的**补丁版本**（与 1.3.1 语言区间相互独立、可并行）：`ezmk test --report <fmt>[:<path>]` 产**机器可读测试报告**（JUnit XML 写文件，交给已有仪表盘/CI 渲染）——non-goals「原生单元测试仪表盘」条款（形态 A/B 拒绝）的**形态 C 替代方案**，文档与实现互相引用。**只做发射不做 UI**：历史/图表/HTML 渲染明确不做。**公共 API 无破坏性变更**（纯新增 flag）。

### 新增 / 行为变更

- **`ezmk test --report <fmt>[:<path>]`**：JUnit XML 写文件；缺省路径 `<proj_root>/.ezmk/test-results/junit.xml`（项目级、可 gitignore），自定义相对路径按项目根解析
- **Catch2 路径**：透传 vendor 自带 reporter（`-r <fmt>::out=<file>`，`junit`/`json`/`xml`/`sonarqube`…均可）；控制台 reporter 保持默认 → 现有摘要文本解析零回归；`--filter` 与 `--report` 可组合（报告只含过滤后用例）
- **EZMK 内置框架路径**：最小 JUnit 发射器——每测试文件一个 `<testsuite>`/`<testcase>`，失败/超时 → `<failure>`，编译/链接失败 → `<error>`（语义区分）；stdout/stderr 摘要**截断**（4KB/条）+ **全量 XML 转义**（`& < > " '`）；temp → rename 原子写；报告在失败门禁**之前**写出（CI 能看到失败详情）
- **格式门禁**：EZMK 仅支持 `junit`；`json` 等格式显式报错并提示改用 Catch2 框架（避免"换框架丢格式"的隐形陷阱）
- **`ezmk workspace test --report` 透传**（P1）：成员子进程透传 flag，每成员写自己的 `.ezmk/test-results/junit.xml`；成员失败汇总语义不变；build/clean/list 下 `--report` 明确拒绝
- **报告是附加产物**：`ezmk test` 退出码语义（失败门禁）完全不变

### 文档

- `docs/cli.md`（zh/en）：test 命令新增 `--report` 标志说明（格式/缺省路径/两框架差异/workspace 透传）；`workspace test` 命令行补 `--report`
- `docs/zh|en/non-goals.md`「原生单元测试仪表盘」条款：替代方案（形态 C）即本版（1.3.2 已引用，核对一致）
- `CHANGES.md` 本条目

### 测试

- CLI 解析：`--report junit` / `--report junit:路径` / 空格式拒绝 / `-w` 透传 / build/clean 拒绝
- EZMK 发射器单测：PASS/FAIL/TIMEOUT/编译失败/链接失败 → XML 内容断言；转义（`& < > " '`）；4KB 截断；原子写（无 `.tmp` 残留）；文件名转义
- 集成：EZMK 框架 `--report junit`（含失败用例 → 报告先于失败门禁写出 + 控制台摘要不变）、全过、自定义路径、非 junit 报错提示；Catch2 路径（离线跳过）；workspace 双成员各写各的报告
- i18n：新增 `cli_err_invalid_report` / `cli_report_test_only` / `test_report_not_supported` 三向一致；`check_i18n.py` 通过（371 keys）

### 已知限制 / 跟进项

- **历史 / 耗时对比 / 抖动分析 / HTML 渲染**：形态 B，non-goals 拒绝；外部工具。
- **`[test]` 配置字段**（声明式报告路径）与 **EZMK 侧 JSON 格式**：归 1.4.0 或后续评估。
- **旧二进制（<1.3.2）**：不认识 `--report` → 报 unknown option；新语法需 ≥1.3.2。

---

## 1.3.1 (2026-08-24) — 区间语言标准 + 包/消费者标准兼容校验

1.3.1 是 1.3.0 发布后的**补丁版本**，围绕 `[project].language` 的区间语法与安装期标准兼容校验。语义定死为 A（元数据 + 校验）：区间只表达**最低要求**（可选上界仅元数据），编译仍用单一精确标准（取 min）；**工具链能力表 + 编译协商（语义 B/C）预留 1.4.0**。**公共 API 无破坏性变更**（新语法 + 新字段 + 新警告，纯增量）。

### 新增 / 行为变更

- **区间语言标准**：`parse_language()` 支持 `">=C++11"`（单边下界）与 `"C++11..C++17"`（双边区间）；`LanguageInfo` 新增 `min_ver` / `max_ver`，`std_flag` 语义变为**生效标志**（取 min，与精确写法完全一致——`EZMK_LANG` 宏、缓存签名对精确/区间写法定点不变，改 max 不触发重建）
- **非法区间拒绝**：`C++17..C++11`（max < min）、`>C++11`（只支持 `>=`）、`C++11+`（不支持 `+` 后缀）、空区间端、`>=X..Y`、跨语言族区间（`C11..C++17`）
- **安装期标准兼容校验**：源码包（`compile_package`）与预编译包（`select_precompiled_archive`，ABI 措辞加强）在安装前比对包 `min_ver` 与消费者项目 `min_ver`，包最低要求更高 → **警告不 fail**（信息性，避免破坏现有包生态；严格化开关预留 1.4.0）；无消费者 `ezmk.toml`（全局/用户级安装）跳过；消费者 language 非法 → 警告并跳过
- **CMake 导出修复**：区间 `normalized_lang` 裸数字提取 bug（`"CPP11..CPP17"` → `CXX_STANDARD 1117`）改为直接读 `lang.min_ver`（→ `CXX_STANDARD 11`；CMake 语义「至少 N」与 min 天然对齐）

### 文档

- `docs/config_file.md`（zh/en）：`language` 新增「区间语法」小节（语法表 + 语义 + 非法形式）；`language` 字段说明更新
- `docs/package_authoring.md` / `docs/pkg.md`（zh/en）：包作者应声明**最低**兼容标准（区间写法）
- `docs/faq.md`（zh/en）：「invalid language format」补区间正例与语义；修正无版本号写法（`"C++"` → C++17 / `"C"` → C11 按默认值）
- `CHANGES.md` 本条目

### 测试

- 解析层：区间合法/非法/精确值回归单测（含 `>=GNUCPP11` / `>=C` 默认 11 / `C++03` min=3）
- 安装校验：高/低/无消费者项目三态 + 区间声明参与 + 预编译 ABI 措辞（stderr 捕获断言）
- 导出：区间导出 `CXX_STANDARD 11`（防 1117）+ 精确值导出不变回归
- i18n：新增 `config_err_invalid_lang_range` / `pkg_warn_std_mismatch` / `pkg_warn_std_mismatch_precompiled` 三向一致 + `config_err_invalid_lang` 文案更新；`check_i18n.py` 通过

### 已知限制 / 跟进项

- **工具链能力表**（`max_supported_std(family, version)`，语义 C 铺路）与**编译协商（语义 B）**（包按 `max(包min, 消费者标准)` 重编）：**预留 1.4.0**，本版明确收口不实现
- **标准校验严格化开关**（warn → error 可配）与 **import.cpp `CXX_STANDARD` 映射**（`src/import.cpp:446-447` 硬编码 C++17）：预留 1.4.0
- **`+` 后缀别名 / 双边区间「验证到 max」（CI 矩阵）/ header-only 包标准声明**：1.4.0 或后续评估
- **旧二进制（<1.3.1）读区间配置** → 报 invalid language：可接受（新语法需 ≥1.3.1）

---

## 1.3.0 (2026-08-21) — Workspace 工作区 + i18n 语言变体 + 消费命令总是自动构建

1.3.0 是 1.2.x 收官后的**首个功能 minor**（延续 1.0.0 → 1.1.0 → 1.2.0 的节奏），按 dev（功能）→ pre（收口）两阶段推进。dev.1 ~ dev.5 落地三大主题，pre.1 完成用户触达文档与发布门槛预核对。**公共 API 无破坏性变更**（新增命令组 / 配置文件 / 字段 / 变体标签均为纯增量，单项目路径零改动；`test`/`pack --precompiled` 行为收敛为总是增量构建属修复性变更）。

### 新增 / 行为变更（dev 汇总）

- **Workspace（工作区）**：`ezmk-workspace.toml`（`[workspace] members` + `[workspace.options]`）声明成员集合；`ezmk workspace list/build/test/clean` 批量管理；Kahn 拓扑分层 + 层内并行；`-w` 重定向（`ezmk build -w` ≡ `ezmk workspace build`）、`--member` 含依赖闭包、`--stop-on-error` 停派发；成员 `[depends] workspace` 声明**单向非循环依赖**，构建时兄弟产物**自发现注入**（`-I/-L/-l` 存在性门控、零环境变量）；编译/链接命令 >16K 走 GCC 响应文件兜底；跨成员增量（库 `.cpp` → 依赖者只重链、库 `.h` → 依赖者经 depfile 重编）
- **i18n 语言变体**：BCP 47 标签归一化（`zh_CN`/`zh-CN`/`zh_CN.UTF-8` → `zh-CN`）；`locale/zh-TW.json` 繁体变体（继承式，只写差异键）；回退链 变体 → 基础 → 英文；`check_i18n.py` 变体校验（子集合法 / 多余键报错 / extends 校验）
- **消费命令总是自动构建**：`ezmk test` / `pack --precompiled` 移除产物存在性门控，一律先增量构建再消费（消除陈旧产物陷阱）
- 各 dev 子版本的详细条目见下（dev.1 ~ dev.5）

### pre.1 文档收口

- `docs/cli.md`（zh/en）：新增 `workspace` 命令组节（命令表 + `-w`/`--member`/`--stop-on-error`/`-j` 语义 + 注入与增量说明 + 纯容器根提示）；`EZMK_LANG` 变体标签与回退链说明；`test`/`pack --precompiled` 总是先构建说明
- `docs/config_file.md`（zh/en）：`[depends] workspace` 字段与约束说明
- README（zh/en）：命令速览补 workspace；高级特性表新增工作区批量管理
- 教程（zh/en）：新增第 15 章「工作区：批量管理一组项目」+ 索引
- non-goals「多项目工作区」条款复核（`e25232d` 已落地，与实现一致）
- **API 稳定性承诺扩展**（本条目上方）：workspace 命令组 / `ezmk-workspace.toml` / `[depends] workspace` / `EZMK_LANG` 变体标签纳入永久稳定

### 测试

- 全量回归：**863 用例 / 5007 断言零失败**（dev.5 基线；3 跳过为既有环境限制）；i18n 三向一致（`check_i18n.py`，365 键 × en/zh + zh-TW 变体）
- dev 各子版本测试汇总：workspace 单测 42 用例（解析/定位/校验/拓扑/注入/响应文件）+ 集成 15 用例 + i18n 变体 8 用例 + 陈旧产物陷阱 2 用例

### 已知限制 / 跟进项

- **shared 运行时成员依赖 / 成员级过滤（按标签）/ `cc` 批量生成**：归 2.0.0 或后续评估
- **完整构建图**（环 / 版本约束 / 平台矩阵 / 可编程图）：non-goals，明确拒绝（见 `docs/zh|en/non-goals.md`）
- **更多语言变体**（`en-GB`/`en-US`）与完整 BCP 47（脚本/地区扩展）：机制就绪，按需添加
- **产物新鲜度时间戳校验**：本版采用「总是构建」语义，不引入时间戳比较；归 2.0.0 评估
- **AUR**：新账户注册未开放，`publish/arch/PKGBUILD` 自取 + `makepkg -si` 为主；账户开通后补 AUR 提交
- **winget**：1.3.0 PR（`microsoft/winget-pkgs#422754`）已提交，待 CI + 版主人工审批（1.2.1 `#419171` / 1.2.4 `#420487` / 1.2.5 `#421464` 仍在队列，为发布后跟进项）
- **macOS Intel（x64）**：仍无预编译产物（`macos-13` runner 在 free tier 不分配）——与 1.2.x 相同；Homebrew 公式仅 arm64 + Linux

---

## 1.3.0-dev.1 (2026-08-19) — Workspace 配置、定位与依赖校验

1.3.0（Workspace 工作区）第一个开发子版本，落地工作区功能的**配置与定位层**：定义独立于 `ezmk.toml` 的 `ezmk-workspace.toml`（`[workspace]` 的 `name` 可选 / `members` 必填非空 + `[workspace.options]` 的 `default_jobs` / `stop_on_error`），`locate_workspace_root()` 从任意子目录最多向上 5 层定位工作区根（与 `locate_project_root` 对称、互不干扰），并在配置期完成**成员校验**（路径逃逸 / 存在性+ezmk.toml / 嵌套）与**成员依赖校验**（`[depends] workspace` 引用解析、DFS 环检测含自环、被依赖成员类型须 `static`）。**公共 API 无破坏性变更**（新配置文件 + `config.hpp` 新可选字段，均为纯增量；不暴露 CLI，命令组归 dev.2）。

### 新增 / 行为变更

- **`include/ezmk/workspace.hpp` + `src/workspace.cpp`**：独立于单项目解析的 workspace 模块——`Options` / `Member` / `Workspace` 结构；`locate_workspace_root()`（5 层上溯）；`load_from()`（定位 + 解析 + 成员校验 + 依赖校验一站式）；`validate_member()`（路径安全违规抛错，存在性/嵌套标记 `valid=false`）；`validate_ws_deps()`（引用解析按「完整相对路径或唯一末段」，歧义末段报错要求全路径；三色 DFS 环检测含自环，错误携带环路径如 `a -> b -> a`；被依赖成员非 `static` → 配置期报错；依赖无效成员 → 引用方标记无效）
- **`ezmk-workspace.toml` 解析**（`src/workspace.cpp`）：复用 toml11；`[workspace]`（`name` 可选、`members` 必填非空字符串数组）+ `[workspace.options]`（`default_jobs` 非负整数缺省 0、`stop_on_error` 布尔缺省 false）；非法格式（缺/空 members、非字符串数组、负 jobs、未知节/键）→ 明确报错（含文件路径与字段名）
- **成员安全校验**：相对路径、无 `..` 逃逸、无绝对/盘符/UNC；`weakly_canonical` 解析后仍在根内（符号链接出根 → 拒绝）；成员目录不存在 / 无 `ezmk.toml` / 内含 `ezmk-workspace.toml`（嵌套）→ 标记 invalid（执行时跳过不阻断）
- **`[depends] workspace` 新字段**（`include/ezmk/config.hpp` `DependsSection` + `src/config.cpp`）：成员 `ezmk.toml` 可声明对兄弟成员的依赖（末段或完整相对路径）；未声明时为空向量，既有解析/构建零影响
- **i18n**：新增 `workspace_err_*` 16 键（X-macro 三向一致，`check_i18n.py` 通过）

### 测试

- 新增 `test/test_workspace.cpp` **28 用例 / 89 断言**：解析（members/options/缺省/非法格式）+ 定位（0/5 层/无文件/与项目根互不干扰）+ 成员校验各拒绝分支（缺失目录 / 无 ezmk.toml / 嵌套 / `../` / 绝对路径 / 盘符 / 符号链接出根）+ 依赖（basename/完整路径引用、未知引用、自环、A→B→A 环、非 static 被依赖、依赖无效成员、同名末段歧义）
- 全量回归：**822 用例 / 3922 断言零失败**（基线 794 / 3769，+28 用例；3 跳过为既有环境限制）

### 已知限制 / 跟进项

- **`ezmk workspace` 命令组未落地**（`list`/`build`/`test`/`clean` 与拓扑构建、并行执行、兄弟产物注入归 **dev.2**）：本版仅提供配置/定位/校验能力，`Workspace` 结构由 dev.2 消费
- **dev.2 依赖本版**：命令执行消费 `Workspace` 结构与校验结果；拓扑排序消费 `ws_deps`；环与类型在配置期 fail-fast 已由本版保证

---

## 1.3.0-dev.2 (2026-08-21) — workspace 构建命令与并行

1.3.0（Workspace 工作区）第二个开发子版本，落地工作区功能的**命令与执行层**：`ezmk workspace` 命令组（`list`/`build`/`test`/`clean`）+ 成员依赖图的拓扑执行（Kahn 分层、层内并行）+ **兄弟产物注入（成员自发现，零环境变量）** + 命令行长度兜底（响应文件）。至此「共享基础库 + 多个可执行文件」的 monorepo 可一次命令全量构建，增量语义正确（改库 `.cpp` → 依赖者只重链；改库 `.h` → 依赖者经 depfile 自动重编）。**公共 API 无破坏性变更**（新命令组 + `-w` 重定向 + 新 flag 均为纯增量，单项目路径零改动）。

### 新增 / 行为变更

- **`ezmk workspace` 命令组**（`src/workspace_build.cpp` + `src/main.cpp`）：
  - `list`：打印根路径与每成员（name/type/deps），invalid 成员标 `(invalid: <原因>)`
  - `build` / `test`：`[-j N] [--stop-on-error] [--member <name>...]`；`test` 无测试的成员跳过（不报错）
  - `clean`：按拓扑逆序逐成员 `ezmk clean`（不支持 `--stop-on-error`，parse 期拒绝）
  - **`--member` = 目标成员 + 依赖闭包**（按拓扑先构建依赖保证产物新鲜）；单成员不构建闭包 → `cd <member> && ezmk build`
  - **`-w` / `--workspace` 重定向**（附在 `build`/`test`/`clean` 上）：`ezmk build -w` ≡ `ezmk workspace build`；非「项目 + workspace 叠加」
  - 纯容器根（有 `ezmk-workspace.toml` 无 `ezmk.toml`）`ezmk build` → 提示改用 `ezmk workspace build`
- **Kahn 拓扑分层**（`src/workspace.cpp` `topo_layers`）：依赖层先构建、同层互相无依赖可并行；环已在 dev.1 配置期拒绝，此处仅防御断言
- **子进程执行模型**：每成员独立 `<ezmk> build/test/clean` 子进程（cwd = 成员目录，缓存/Lua 状态/输出天然隔离）；层内 `util::ThreadPool` 并行；输出带成员前缀；摘要含 succeeded/failed/skipped；**`--stop-on-error` 精确语义**：失败后停派发（本层未启动 + 后续层 skipped）、在跑成员自然结束不 kill、`clean` 不支持
- **兄弟产物注入 = 成员自发现**（`src/build.cpp` `resolve_ws_injection`）：成员 `[depends] workspace` 非空时，构建进程自行 locate/load workspace → 注入 `-I <ws>/<m>/include`（存在才加）+ `-L <ws>/<m>/build -l<m>`（`lib<m>.a` 存在才加；MSVC 走完整 `<m>.lib` 路径）；**零环境变量**（长度与 workspace 规模无关，大型项目不炸，坑 1）；注入 `-I` 进入 `extra_includes` → **编译签名含注入参数**（注入变化 → 依赖者自动重编）；兄弟产物缺失 → 提示「先 `ezmk workspace build`」后继续（链接失败自然报错）
- **命令行长度兜底**（`src/cache.cpp` `join_args_with_response_file`）：编译/链接命令 >16K 字符 → 改写为 GCC 响应文件 `@<rsp>`（参数逐行写入 `<tmp>/ezmk-<n>.rsp.tmp`，一行一个参数、含空格路径天然安全）；`<compiler> @<rsp>` 保持命令短小；使用后删除、残留由既有 stale-temp 清理兜底；MSVC 侧不触发（其响应文件语法不同，见 dev.2 §3.5）
- **增量语义验证**（2.6）：库 `.cpp` 变 → 库重编 + 依赖者**重新链接**（main.o 缓存命中不重编）；库 `.h` 变 → 依赖者**重新编译**（注入 `-I` 进入预处理器，depfile `-MD` 自动收录兄弟头文件，头哈希驱动重编）
- **i18n**：新增 `workspace_*`/`help_*` 命令相关 28 键（X-macro 三向一致，`check_i18n.py` 通过）

### 测试

- 新增 `test/test_workspace_build.cpp` **14 用例 / 204 断言**：拓扑分层（无依赖单层/线链/菱形/扇出/无效成员排除）+ 注入解析（include 注入/产物缺失上报/`-L -l` 拼装/MSVC `<name>.lib` 命名/无 workspace/未知引用/非 static 拒绝）+ 响应文件（阈值不触发保持原样/超阈值 `compiler @<rsp>` 与内容逐行校验/含空格路径引号与单行语义）+ **子进程冒烟**（临时 lib+app workspace：构建成功与产物、应用运行输出、noop 重建 main.o 字节不变、库 `.cpp` 变只重链不重编（输出 sum=5→6）、库 `.h` 变触发重编（OFFSET 内联 → main.o 变化 + sum=106）、clean 语义与重建）
- 全量回归：**836 用例 / 4126 断言零失败**（dev.1 基线 822 / 3922，+14 用例 / +204 断言；3 跳过为既有环境限制；Windows 子进程冒烟带 AV 文件锁重试守卫）

### 已知限制 / 跟进项

- **`ezmk workspace test` 的 `[test-opts]` 透传**（`--framework`/`--filter` 等）与集成测试（3 成员依赖构建顺序 / 失败汇总 / 校验拒绝矩阵 / CI 冒烟步骤）归 **dev.3**
- **dev.3 依赖本版**：集成测试消费命令组、拓扑执行与摘要格式
- **Windows 平台**：成员子进程链接重写产物偶发 `Permission denied`（杀软占用，与既有 rename 测试同类）——冒烟测试已带单次重试守卫；`workspace clean` 与单项目 `clean` 一致只清缓存/临时目录（`build/` 产物保留）

---

## 1.3.0-dev.3 (2026-08-21) — workspace 测试与 CI

1.3.0（Workspace 工作区）第三个开发子版本，用**集成测试 + CI 自举**把 dev.1/dev.2 的行为固化：依赖构建顺序（库先于可执行文件）、跨成员增量（库 `.cpp` → 依赖者只重链；库 `.h` → 依赖者经 depfile 重编）、并行构建、失败汇总与 `--stop-on-error`、`--member` 闭包、成员内独立构建、注入零环境变量、以及全部校验拒绝分支（路径逃逸 / 环 / 非 static 被依赖 / 嵌套 / 成员缺失）都有可重复的断言；CI 追加 workspace 冒烟步骤。**公共 API 无破坏性变更**（本版无产品代码变更，仅一处可测试性重构：`resolve_jobs` 从匿名命名空间提取为公共纯函数，行为逐字节不变）。

### 新增 / 行为变更

- **集成测试**（`test/test_integration.cpp`，tag `[integration][workspace][1.3.0]`，15 用例，夹具为含空格路径的 3 成员 workspace）：
  - `workspace list` 成员/类型/依赖展示；`workspace build -j 2` 全成功 + 产物落位 + 应用运行输出
  - **依赖构建顺序**：strutil 层输出先于两个 app 层（层序确定性，层内顺序不断言）
  - **跨成员增量①（库 .cpp 变）**：strutil 重编（"0 cached, 1 compiled"）+ 两 app 缓存命中（"1 cached, 0 compiled"）→ 只重链（运行输出 sum=5→6）
  - **跨成员增量②（库 .h 变）**：三成员全部重编（"0 cached, 1 compiled" ×3，依赖者 depfile 含注入 `-I` 的库头）→ 重编生效（输出 sum=105）
  - `workspace test`：tool-a 带 `[test]`（ezmk 框架）→ `[PASS]`；无测试成员跳过（不报错）
  - `workspace clean`：清各成员 `.ezmk/cache`（与单项目 clean 语义一致，`build/` 产物保留——设计文档同步修正）
  - **失败汇总**：tool-b 编译失败 → strutil/tool-a 完成 + "2 succeeded, 1 failed"，退出码非零
  - **`--stop-on-error`（坑 3 锁定）**：strutil（依赖层）失败 → 两个依赖者整层 skipped（"0 succeeded, 1 failed, 2 skipped"），无产物残留
  - **`--member` 闭包（坑 2 锁定）**：`--member tool-a` 构建 tool-a + 依赖 strutil（tool-b 不构建）；完整相对路径等价；未知成员 → fatal
  - **成员内独立构建**：`cd apps/tool-a && ezmk build` 只构建 tool-a（注入已存在产物、无 "Archiving libstrutil.a"、不触发闭包）
  - **注入零环境变量（坑 1 锁定）**：`EZK_WS_DEPS`/`EZK_WS_ROOT` 置垃圾值 → 构建仍成功（注入来自 workspace 文件自发现，非环境变量）
  - **校验拒绝**：`../` 逃逸 / tool-a↔strutil 环 / 依赖 executable 非 static / 嵌套 workspace 文件（成员 + 引用方标记 invalid，剩余成员照常构建）/ 缺失成员目录（invalid 标记 + 不阻断）
  - **纯容器根提示**：仅 workspace 文件无 ezmk.toml → `ezmk build` fatal 提示；根同时是项目 → 行为不变
- **单元测试补全**（`test/test_workspace.cpp` + `test/test_workspace_build.cpp`）：`resolve_member_ref`（完整路径优先 / 唯一末段 / 未知 / 歧义末段 → nullopt）+ `resolve_jobs`（`-j` 显式 > `default_jobs` > hardware_concurrency）
- **CI**（`.github/workflows/ci.yml`）：ubuntu job 追加「Workspace smoke (1.3.0-dev.3)」——`$RUNNER_TEMP` 内构造 3 成员 workspace → `workspace list` + `build -j 3` + **二次构建增量断言**（`grep -qE "[1-9] compiled"` 不命中 = 零重编）+ 产物落位检查 + `workspace clean`（`EZMK_LANG=en` 保证断言确定性）

### 测试

- 新增 **17 用例**：2 单测（`resolve_member_ref` / `resolve_jobs`）+ 15 集成（workspace）
- 全量回归：**853 用例 / 4229 断言零失败**（dev.2 基线 836 / 4126，+17 用例 / +103 断言；3 跳过为既有环境限制；workspace 集成测试 5 连跑稳定）

### 已知限制 / 跟进项

- **dev.4（i18n 语言变体）与 dev.5（消费命令总是自动构建）**：与 workspace 主线并行，规划中
- **`--stop-on-error` 同层并行成员**：失败时同层尚未启动的成员按调度时机计 skipped 或已完成——确定性断言用「依赖层失败 → 后续层整层 skipped」的构型（本版用例 9）；同层时序用例留待后续评估
- **CI workspace 冒烟**：增量断言依赖英文 locale（已显式 `EZMK_LANG=en`）；GitHub runner 默认 en_US 一致

---

## 1.3.0-dev.5 (2026-08-21) — 消费命令总是自动构建

1.3.0 第五个开发子版本（与 workspace 主线并行的最后一个 dev），把**消费构建产物**的命令语义统一为「**先增量构建 → 产物新鲜 → 再消费**」：`ezmk test` 与 `ezmk project pack --precompiled` 移除「产物存在性门控」，改为**总是先 `build_project`**（增量缓存保证产物新鲜时仅一行提示 + 秒过），消除「改源码后旧产物仍在 → 测试跑旧代码 / 打包打旧包」的陈旧产物陷阱。**公共 API 无破坏性变更**（纯行为收敛，无新命令/配置/文件；`run`/`install`/`watch` 本就总是构建、源码包 `pack` 与 `cc`/`export`/`import`/`clean` 维持现状）。

### 新增 / 行为变更

- **`ezmk test` 总是先构建**（`src/build.cpp` `run_tests()`）：删除 `check_built()` 存在性门控与 `project_built` 变量，无条件 `build_project`（增量）后再收集/编译/运行测试；`utils` 类型例外保留（无编译产物，构建无意义，与旧门控对 utils 恒返回「已构建」一致）；构建失败 → fatal「project build failed, cannot run tests」
- **`ezmk project pack --precompiled` 总是先构建**（`pack_project()`）：`--precompiled` 分支删除存在性门控，无条件 `build_project`（增量）；**打包前产物复核保留**——构建未产出 `lib<name>.a` 时 fatal，不静默打包过期产物
- **源码包（默认 `pack`）不变**：平台无关、消费者侧编译，打包 `src/`+`include/`+`ezmk.toml` 与产物无关，不触发构建
- **语义统一表**：run / install / watch（总是构建，不变）| test（存在性门控 → **总是构建**，utils 除外）| pack --precompiled（存在性门控 → **总是构建**）| pack 源码包 / cc / export / import / clean（不消费产物，不变）
- **workspace 联动**（自动受益）：`ezmk workspace test` 逐成员透传单项目 `ezmk test`，成员测试同样获得「总是构建」语义，无需额外改动

### 测试

- 新增 `test/test_integration.cpp` **2 用例**（`[integration][1.3.0-dev.5]`，陈旧产物陷阱回归锁定）：
  - **`ezmk test` 用新产物**：改项目源码（`answer()` 1→2）+ 改测试预期后**直接** `ezmk test`（中间不构建）——总是构建重编 `answer.o`，测试链接新值通过（旧门控下跳过构建、链接旧 `.o`、测试必失败）；产物新鲜时第二次 `ezmk test` 全缓存命中（"2 cached, 0 compiled"，无重编译）
  - **`pack --precompiled` 用新产物**：改静态库源码后**直接** pack——总是构建重编 `sp.o`，归档 sha256 变化（旧门控下打包过期 `lib<a>.a`、hash 相同）；产物新鲜时再次 pack 全缓存命中（"1 cached, 0 compiled"，无重编译）
- 全量回归：**863 用例 / 5007 断言零失败**（dev.4 基线 861 / 4990，+2 用例 / +17 断言；3 跳过为既有环境限制；连续两轮稳定）

### 已知限制 / 跟进项

- **`check_built` 产物新鲜度校验**（比较时间戳而非存在性）未引入：本版采用最简「总是构建」语义，增量缓存保证近零开销；如需时间戳比较归 **2.0.0** 或后续评估
- **dev 阶段全部完成**：dev.1 ~ dev.5 收口；下一阶段 **pre.1（文档与发布收口）**——cli.md workspace 节 / README 命令速览 / 教程 / `EZMK_LANG` 变体说明 / non-goals 条款更新 / 发布门槛复核

---

## 1.3.0-dev.4 (2026-08-21) — i18n 语言变体（locale variants）

1.3.0 第四个开发子版本（与 workspace 主线并行），落地**语言变体**支持：BCP 47 风格变体标签（`zh-TW`/`en-US`/`zh_CN`/`zh_CN.UTF-8` 等）可显式选择并命中变体专属文案；变体文件**继承基础语言**（只写差异键，缺键回退）；回退链 **变体 → 基础 → 英文** 保证任何新键（如 workspace 的 `workspace_*`）在变体未转写时不出现 `{???}`。首个变体文件 `locale/zh-TW.json`（繁体中文 365 键全量转写）。**公共 API 无破坏性变更**（`i18n.hpp` 仅新增 `normalize_locale_tag`；`EZMK_LANG=en`/`zh` 精确值结果与现状逐字节一致）。

### 新增 / 行为变更

- **`normalize_locale_tag()`**（`src/i18n.cpp`，新公共 API）：BCP 47 风格标签归一化——去 `.编码` 后缀（`zh_CN.UTF-8` → `zh_CN`）、`_`/`-` 统一分隔、首段小写语言（2-3 字母）+ 次段大写地区（2 字母）→ 规范形 `zh-TW`；非法输入（空 / 非字母 / 超两段，含脚本标签 `zh-Hant-TW`）→ 空串回退检测逻辑
- **`detect_language()` 重写**：EZMK_LANG / Windows `GetUserDefaultLocaleName` / POSIX `$LANG`/`$LC_ALL` 三路统一走 `normalize_locale_tag`；有数据守卫放宽为「规范标签或其首段有 embedded/runtime 数据」——`zh-CN` 现返回完整标签 `zh-CN`（init 层回退到 `zh`，行为等同现状）；`zh-TW` 系统/POSIX `zh_TW.UTF-8` 现命中繁体
- **`init()` 继承加载**：目标标签拆分为**基础语言 + 变体**——先加载基础语言（runtime > embedded，优先级不变），再叠加变体文件（`locale/<tag>.json` 存在则逐键覆盖，缺键继承基础；不存在 → 纯回退不报错）；`g_current_lang` 记录实际生效标签（纯回退时记基础语言）；未知基础语言 → 英文兜底
- **`parse_locale_json` 覆盖模式**（overlay 参数）：变体加载时不 `g_strings.clear()`，逐键覆盖；新增 `meta.language` 与加载标签一致性校验（变体文件必须声明自己的标签）
- **`locale/zh-TW.json`**（365 键全量繁体转写）：术语对齐（構建/編譯/連結/快取/封存/標頭檔/原始碼/原始檔/依賴/專案/測試/執行/監聽/偵測/組態/範圍/使用者/全域/鉤子/巨集/字串/陣列/布林值/函式/回傳/指令/儲存庫/套件/範例/預設/建立/產生/旗標/指令碼/執行緒 等），占位符/路径/命令名/代码记号原样保留；`meta.extends = "zh"` 继承式
- **`check_i18n.py` 变体规则**：自动发现 `locale/*.json` 中除 en/zh 外的变体——键**只允许子集**（缺键=继承，合法）/ **多余键报错**（防漂移）/ `meta.language` 必须等于文件名 / `meta.extends` 缺省取标签首段、显式声明时基文件必须存在

### 测试

- 新增 `test/test_i18n.cpp` **8 用例**：归一化（规范形/下划线/编码后缀/大小写/前导尾随分隔符宽容/非法含脚本标签）+ detect 全路径（`zh-TW` 完整标签 / `zh_CN.UTF-8` 归一化 / 未知标签回退不粘滞）+ 继承加载（`zh-TW` 覆写 `zh` 且互异 / `zh-CN` 无变体文件回退 `zh`（含下划线拼写）/ 临时部分变体 `zh-HK.json` 验证缺键继承基础 + 占位符可格式化 / `xx` 英文兜底）+ **穷举回归扩到 en/zh/zh-TW 三语言**（365 键 × 3 语言零 `{???}`）
- `check_i18n.py` 通过（365 键 × 2 基础 + 1 变体）；负向验证：变体多键 → 报漂移错误
- 全量回归：**861 用例 / 4990 断言零失败**（dev.3 基线 853 / 4229，+8 用例 / +761 断言——穷举回归新增 zh-TW 语言 365 键；3 跳过为既有环境限制）

### 已知限制 / 跟进项

- **仅 `zh-TW` 一个变体**：`en-GB`/`en-US`（英文变体差异极小，收益低）与完整 BCP 47（脚本/地区扩展如 `zh-Hant-TW`）按需添加——机制已就绪（`normalize_locale_tag` 对超两段标签返回空串回退，属明确收口）；归 2.0.0 或后续评估
- **`zh-TW.json` 术语以台湾 IT 习惯转写**：个别术语如后续需微调（如「快取」vs「緩存」），直接改差异键即可，回退链保证不缺键
- **dev.5（消费命令总是自动构建）**：与主线并行的最后一个 dev 子版本，规划中

---

## 1.2.5 (2026-08-19) — 测试缓存签名修复 + 默认包格式改为源码包

1.2.x 稳定线补丁，合并两项：① 合入 1.2.4 之后 main 上未发布的修复（`ezmk test` 测试源缓存签名校验、`embed_examples.py` 剪枝）；② **`ezmk project pack` 默认包格式改为源码包**——`src/`（按 `[compile].src_dirs`）+ `include/` + `ezmk.toml`（原样），平台无关、消费者侧编译；旧预编译行为收敛为显式 `--precompiled`（产物逐字节等价）。**公共 API 无破坏性变更**（新 flag 纯新增；默认值调整以 `--precompiled` 显式兼容）。

### 新增 / 行为变更

- **`ezmk project pack` 默认源码包**（`src/build.cpp`）：默认产出源码包（`src_dirs` + `include/` + `ezmk.toml` 原样、无 `precompiled` 标记）；类型校验放宽为任意类型（安装侧本就按静态库编译）；不要求先构建；打包前剪枝 `build/` / `.ezmk/` / `.git` 与二进制残留
- **`--precompiled`**（`src/cli.cpp` + `include/ezmk/cli.hpp`）：保留旧行为——仅 `static` 项目、产物缺失自动构建、归档 `include/` + `lib/` + 追加 `precompiled = true` 标记（与旧默认逐字节等价）；`-v` 的 index.toml 片段 `platform` 字段仅预编译包输出（源码包平台无关）
- **修复（合入 01807bb，随本版发布）**：`ezmk test` 测试源缓存从未做编译选项签名校验（`record.compile_options_signature` 永空 → 每次全量重编译）——新增 `validate_test_cache_signature`（Catch2 与 ezmk 框架两路径），第二次 `ezmk test` 即缓存命中；`embed_examples.py` 剪枝构建残留与二进制扩展名（UnicodeDecodeError）
- 同批祖先修复随行：CI 双失败修复（.gitattributes eol=lf / embed 字节精确输出 / bootstrap repo add）、pkg `expected_sha256` string_view 悬垂 UB 修复

### 测试

- 新增集成测试（`[1.2.5]`）：源码包 `pkg install` 端到端（消费者侧编译、无 precompiled 标记）+ `--precompiled` 回归（标记 + `lib/` 产物）+ 非 static 项目 `--precompiled` 拒绝 + 任意类型源码包打包成功
- 全量回归：**794 用例 / 3769 断言零失败**（基线 793 / 3755，+1 用例；3 跳过为既有环境限制）

### 已知限制 / 跟进项

- **源码包与预编译包归档同名**（`<name>-<version>.tar.gz`）：一次 pack 仅一种格式；同时发布两格式需用不同 `--output` 目录
- **macOS Intel（x64）**：仍无预编译产物（`macos-13` runner 在 free tier 不分配，job 持续排队）——与 1.2.0/1.2.1 相同
- **winget**：1.2.5 PR（`microsoft/winget-pkgs#421464`）已提交，待 CI + 版主人工审批（1.2.4 PR `#420487`、1.2.1 PR `#419171` 仍在队列）

---

## 1.2.4 (2026-08-18) — 仓库文件夹包支持（repo 托管目录形式包）

1.2.x 稳定线补丁：打通「仓库托管目录包」——按名安装解析出的包路径为目录时复用 dev.7 的文件夹安装（`install_from_directory`），`index.toml` 增可选 `type = "dir"` 标注（目录包无归档 hash，sha256 省略且跳过校验）；header-only / 源码包可免打包、以 git 目录形式托管。**归档包零影响，公共 API 无破坏性变更**（index 字段纯增量，命令/配置不变）。

### 新增 / 行为变更

- **按名安装目录分支**（`src/pkg.cpp`）：repo 解析出的 `archive_path` 为目录时走 `install_from_directory`（目录结构校验 + 源码编译/安装，与 `pkg install <dir>` 同路径）；归档分支（`extract_archive`）不动、互斥
- **`index.toml` 目录包标注**（`src/repo.cpp`）：`[[packages]]` 增可选 `type` 字段——`"dir"` = 目录包（sha256 省略且跳过校验）；省略 = 归档包（向后兼容）；`type` 省略但 `file` 指向目录时 `is_directory` 自动兜底
- **`validate_local_repo` / `repo info` / 版本约束解析**：天然兼容 dir 包（sha256 非空才校验；`file_exists` 兼容目录）

### 测试

- 新增集成测试：local 仓库 dir 包 → `pkg install <name>` 端到端（目录分支触发 + 源码编译归档 + 无 sha256 不报错）；`file` 指向缺失目录 → `repo add` 友好报错
- 全量回归：**793 用例 / 3755 断言零失败**（基线 791 / 3746，+2 用例；1 跳过为既有环境限制）

### 已知限制 / 跟进项

- **目录包内容哈希 / 增量同步语义**（非归档 sha256）：归 2.0.0 或后续评估；本版以 `is_directory` + 可选 `type` 标注的最小实现为准
- **官方仓库是否切换 header-only 包为目录形式**：由 ezmk-repo 维护决定，本版只提供能力
- **macOS Intel（x64）**：仍无预编译产物（`macos-13` runner 在 free tier 不分配，job 持续排队）——与 1.2.0/1.2.1 相同
- **winget**：1.2.4 PR（`microsoft/winget-pkgs#420487`）已提交，待 CI + 版主人工审批（1.2.1 PR `#419171` 仍在队列）

---

## 1.2.3 (2026-08-18) — `ezmk example` 命令组 + 内置示例

1.2.x 稳定线补丁：新增顶层命令组 `ezmk example`（`list` / `<name>` / `-o`），内置 6 个示例（hello / greeter / with-packages / with-tests / with-hooks / cmake-interop），与教程章节一一对应。示例内容以**构建期嵌入资源**存储（`examples/` 源目录为单一事实源，`scripts/embed_examples.py` 生成 `src/example_data.cpp` 嵌入二进制）——装好即用、离线可用、与版本同源。**纯新增命令组，公共 API 无破坏性变更**。

### 新增 / 行为变更

- **`ezmk example` 顶层命令组**：`ezmk example` / `example list` 列出全部内置示例（名称 + 一句话说明）；`ezmk example <name> [-o <dir>]` 生成到 `./<name>/`（或 `<dir>/<name>/`）；目标目录已存在 / 示例名未知 → fatal 并列出可用项；无别名
- **6 个内置示例**（中文注释，对应教程）：`hello` 最简可执行 · `greeter` 静态库骨架（对齐 1.2.1 库模板）· `with-packages` 依赖 + 版本约束（`fmt^10.0`）+ lockfile · `with-tests` Catch2 测试（`[test] framework="catch2"`）· `with-hooks` pre/post Lua 钩子 · `cmake-interop` export/import 互操作
- **构建期嵌入管线**：`examples/` 源目录 = 单一事实源 → `scripts/embed_examples.py` → `src/example_data.cpp`（对齐 embed_locale/embed_logo 机制；python 缺失时空表 stub；加入 `.gitignore`）
- **示例索引**：`examples/README.md`（每项一句话 + 对应教程 + 运行方式）；教程对应章节尾部加「运行 `ezmk example <name>`」指引

### 测试

- 新增单测：嵌入表非空 + 与 `examples/` 源目录文件集合与内容逐一一致（防漂移）
- 新增集成测试：list 6 项 / 生成内容与源文件一致 / `--output` / 已存在 fatal / 未知名 fatal / 生成后逐个 build+test（with-tests 装 catch2 后 `ezmk test`；with-packages 联网安装 fmt，安装不可用时 SKIP）
- CI：ubuntu job 追加「自举验证 6 示例」步骤（`ezmk example` → build/test，with-packages 联网装 fmt）
- 全量回归：**791 用例 / 3745 断言零失败**（基线 785 / 3629，+6 用例；2 跳过为既有环境限制）

### 已知限制 / 跟进项

- **with-packages / with-tests 依赖安装需网络**：生成后首次构建/测试前需 `ezmk pkg install fmt -y` / `ezmk pkg install catch2 -y`（CI 有完整出站网络可自举验证）
- **官方仓库基准为 GitHub**（`install.sh` 的 `OFFICIAL_REPO_URL`）：gitee 镜像若未同步会拿到旧包（如 catch2 缺实现源文件、fmt 索引哈希失配）——遇到包问题先 `repo update` 或切换到 GitHub 源

---

## 1.2.2 (2026-08-18) — 教程分类重组（子目录迁移）

1.2.x 稳定线的**文档补丁**：教程 14 章按主题移入分类子目录（`basic/` 入门 · `packages/` 包管理 · `dev/` 开发体验 · `interop/` 工具链互操作），**组内重新编号（每组从 01 起）**，README 索引改为分组展示；全仓既有链接（README 高级特性表 / docs / 教程内部交叉引用）按新旧映射全部同步更新。**纯文档变更，零代码/API 影响**。

### 文档

- **教程目录重组**（zh/en 同步）：14 章平铺 → 四分类子目录——`basic/`（01~05 名称不变）、`packages/`（原 06/12/13 → 01/02/03）、`dev/`（原 07~10 → 01~04）、`interop/`（原 11/14 → 01/02）；`git mv` 保历史
- **索引分组展示**：`tutorial/zh|en/README.md` 按「入门 / 包管理 / 开发体验 / 工具链互操作」四组小标题 + 组内编号列表，基础组按序阅读、进阶组按需阅读
- **链接全量同步**：README/README_ZH 高级特性表（教程链接新路径 + 显示编号如「包 02」「开发 02」「互操作 01」）、`docs/migrate-from-cmake`（钩子教程引用）、教程内部交叉引用（跨组 `../` 相对路径 + 文字编号按映射更新）；子目录化后的 docs 相对链接深度修正
- **验收**：全仓 grep 零死链 + 旧编号文件名（`06-`~`14-`）零残留；历史文档（CHANGES.md / plans）保留当时路径与编号

### 测试

- 纯文档变更，无代码/API 影响；全量回归：785 用例 / 3629 断言零失败（基线不变）

---

## 1.2.1 (2026-08-17) — 按项目类型差异化模板生成（.cpp / .h）与默认配置补全

1.2.0 正式发布后的补丁版本：`ezmk project new` 按项目类型差异化生成源码模板——`static` / `shared` 库项目生成 `include/<name>.hpp` + `src/<name>.cpp` 库骨架（不再生成无意义的 `main.cpp`），`executable` / `utils` 保持现状；默认配置模板追加注释掉的 `[test]` 示例节，降低测试配置发现成本。公共 API 无破坏性变更（`create_project()` 签名与 `project new` CLI 不变）。

### 新增 / 行为变更

- **按类型生成源码模板**：`project new` 的 `static` / `shared` 类型改为生成库骨架 `include/<name>.hpp`（`#pragma once` + `namespace <ns>` + `greeting()` 示例公共 API）+ `src/<name>.cpp`（实现），**不再生成 `main.cpp`**；`executable` 保持 Hello world 入口不变；`utils` 仍无 C++ 代码（`utils/` 目录放 Lua 脚本）
- **项目名净化**：库模板文件名保留原始项目名（`my-lib` → `include/my-lib.hpp`），C++ namespace 将 `-` / `.` / 空格替换为 `_`（`my-lib` → `namespace my_lib`），头文件用 `#pragma once` 保护
- **默认配置模板追加注释 `[test]` 示例节**：`project new` 生成的 `ezmk.toml` 末尾附带注释掉的 `[test]`（`framework` / `dirs` / `default_profile` / `include_dirs` / `link_targets`）——取消注释即可启用 `ezmk test`；纯注释对解析零影响，字段与 `[test]` 配置一致（含 1.2.0-dev.12 新字段，刻意不展示已弃用的 `flags`）

### 测试

- 新增单测：`sanitize_namespace()` 净化、static/shared 库骨架文件集合与内容、executable 无头文件、utils 无 C++ 代码、默认模板注释 `[test]` 节存在性（含零解析影响）
- 新增集成测试：四类型生成物集合 + 连字符项目名净化 + 新库项目（static/shared）`build` 通过
- 全量回归：**785 用例 / 3629 断言零失败**（1.2.0 基线 775 / 3554，+10 用例；1 跳过为既有 symlink 环境限制）

### 已知限制 / 跟进项

- **macOS Intel（x64）**：仍无预编译产物（`macos-13` runner 在 free tier 不分配，job 持续排队）——与 1.2.0 相同
- **winget**：1.2.1 PR（`microsoft/winget-pkgs#419171`）已提交，待 CI + 版主人工审批；1.2.0 PR（`#418815`）仍在待审批队列
- **Homebrew / pacman**：1.2.1 已同步（tap 公式真实 digest + PKGBUILD `pkgver=1.2.1` 源码 digest）；真机 `brew install` 需真 Mac 环境验证

---

## 1.2.0 (2026-08-17) — 工具链互操作与开箱工程化

正式发布版，聚合 dev.1 ~ dev.12 与 pre.1 / pre.2。主题：**工具链互操作 + 开箱工程化**。公共 API 保持 1.1.0 起的永久稳定（破坏性变更仅 2.0.0 引入）；`ezmk utils cc` 自本版起**弃用**（保留可用并提示转用 `ezmk project cc`，2.0.0 移除）。

### 新增 / 行为变更

- **`ezmk project cc`**（dev.1）：内置 compile_commands.json 生成命令（`-o/--output`、`--profile`），零外部包依赖；`ezmk utils cc` 弃用提示
- **`ezmk project export cmake`**（dev.2）：从 `ezmk.toml` 生成 CMakeLists.txt（project/compile/link/deps 全映射），默认拒绝覆盖手写文件，`--overwrite`/`--profile`/`--resolve`/`--glob`
- **默认模板内建 Debug/Release Profile**（dev.3）：`ezmk project new` 模板内置 `[compile.profile.debug/release]`；基准去 `-O2`、优化归 profile（新建项目需显式 `--profile release` 才优化）
- **`ezmk project import --from cmake`**（dev.4，实验性）：标准 CMake 命令映射 + `find_package` best-effort + 非标准写法事务性拒绝
- **catch2 v3 测试主程序兼容**（dev.5）：`ezmk test` test_main 生成改 v3 显式 `main` + `Catch::Session().run()`；v2 vendor 路径不回归
- **各源文件构建耗时统计**（dev.6）：`-v` 全量明细 / 总耗时 >5s 自动 top-N；零配置、不新增 flag
- **本地包源 + 项目向上查找**（dev.7）：`ezmk pkg install <dir>` 从文件夹安装；`ezmk.toml` 向上查找最多 5 层父目录
- **CMake 导出钩子运行时 `ezmk-lua`**（dev.8）：独立无黑白名单运行时；`export cmake` 对 `[hooks]` 生成 `add_custom_command`
- **包构建配置收敛**（dev.9）：包 `[compile].src_dirs`/`include_dirs` 生效（`collect_sources` 复用 + include 去重 + utils 门控对齐 + `pkg info` 增显）
- **平台标识符扩展**（dev.10）：`lib<name>.<os>-<arch>[-<compiler>][-<abi>]` 命名 + 4 级 ABI 安全匹配 + 降级警告 + 可选 `precompiled_strict`
- **代码质量审查与改进**（dev.11）：全库审查 68 条问题，P0 全部 + P1 大部（run_tests 命令构造收口 / 钩子沙箱安全 / 编码修复 / 配置 CLI 校验 / 死代码清理）
- **测试配置收口**（dev.12）：`[test].default_profile` / `include_dirs` / `link_targets` + `ezmk test --profile`；`[test].flags` 弃用（2.0.0 移除）
- **README 整理与高级特性触达**（pre.2）：README 重组（目录 / 安装独立章节 / 高级特性索引）+ 教程 12/13/14 + 预编译包 ABI 四层警告
- **pacman 分发**（pre.1）：`publish/arch/PKGBUILD`（Arch Linux / MSYS2 自取 + `makepkg -si`；AUR 延后）
- **可移植性修复**：`src/toolchain.cpp` 补 `#include <climits>`（pre.1 远程 Arch 验证发现，Arch gcc 下 `ULONG_MAX` 未声明）

### 测试

- 全量回归：775 用例 / 3554 断言零失败（dev.11 基线 775 / 3552，零回归）
- 分发验证：本机 MSYS2 + 远程 Arch Linux `makepkg` 双环境产物验证；Release 产物（win/linux/macOS）构建核对

### 已知限制 / 跟进项

- **macOS Intel（x64）**：无预编译产物（`macos-13` runner 在 free tier 不分配，job 持续排队）
- **AUR**：账户注册未开放，pacman 以自取 PKGBUILD + `makepkg -si` 为主，AUR 延后
- **winget**：v1.1.3 PR（`microsoft/winget-pkgs#416835`）待版主审批；1.2.0 新 PR 提交后同样待审批
- **P2 收口项**：`export cmake` 的 profiles/test/install/deterministic 进阶映射（dev.2 §4.10）未实现，2.0.0 窗口评估

---

## 1.2.0-pre.2 (2026-08-17) — README 整理与高级特性触达

发布前文档检查点：README 重组 + 高级特性教程 + 预编译 ABI 警告加强，让「能力可见、上手有路、坑位说透」。**纯文档交付**——无代码变更、无新增 i18n key；**中文为基准**（README_ZH / tutorial/zh / docs/zh 先行），英文同步翻译。

### 文档

- **README 重组**（中英）：新增目录；快速开始去掉安装流程（保留第一个项目 + 安装包）；安装提升为独立章节（Homebrew / pacman / install 脚本 + 安装选项表）；**新增「高级特性」章节**——6 项特性一行索引（semver 约束 / `ezmk.lock` / 预编译共包 / 第三方仓库 / CMake 互操作 / compile_commands）+ 文档与教程链接
- **新增高级特性教程**（中英，编号接续 11）：
  - `12-version-lockfile.md`：`@`/`^`/`~`/`>=` 版本约束（`[depends]` 配置）+ `ezmk.lock` 生成/校验/`--locked`/`--no-lock` + `[compile].deterministic = true` 复现构建
  - `13-third-party-repos.md`：`repo add`（git/本地目录、`-p/-u/-g`、`--name/--branch`）+ `repo update` + index.toml `[platform]` 平台映射
  - `14-precompiled-packages.md`：dev.10 命名 `lib<name>.<os>-<arch>[-<compiler>][-<abi>].<ext>` + 4 级匹配 + **ABI 兼容性警告**（libstdc++ CXX11 / MSVC 工具集）+ 失败案例 + 源码分发优先立场
- **教程索引**（中英）：追加 11/12/13/14（顺带修复既有缺漏——`11-import-cmake` 存在但未入索引）
- **`package_authoring.md` §3.3 警告加强**（中英）：兼容性**四层维度**（OS+架构 / 编译器族 / 工具链版本+标准库 ABI / MSVC 运行时）+ 失败案例（`std::__cxx11` 叙事）+ 最佳实践声明（同一包内 `os-arch[-compiler][-abi]` 并排多产物，源码分发仍远优于预编译）
- **文档缺陷修复**：README 高级特性表版本约束示例改为 `[depends]` 配置语法（设计文档示例 `ezmk pkg install fmt@1.2.3` 与实现不符——CLI `pkg install` 单位置参数不接受 `name@` 约束，约束在配置层解析）

### 测试

- 全量回归：775 用例 / 3554 断言零失败（dev.11 基线 775 / 3552，零回归；纯文档变更）

---

## 1.2.0-pre.1 (2026-08-17) — pacman 分发（Arch Linux / MSYS2）

发布流水线补充：新增 pacman 分发渠道，与 winget（Windows）、Homebrew（macOS/Linux）并列。形态为「仓库内 `publish/arch/PKGBUILD` 自取 + `makepkg -si`」（源码构建，Linux 静态链接）；**AUR 新账户注册未开放，不提交 AUR**（延后，账户开通后补）。公共 API 无任何变更。

### 新增 / 行为变更

- **`publish/` 目录重组**：`manifests/` → `publish/winget/`、`homebrew-eazymake/` → `publish/homebrew/`（`git mv` 保历史；`ezmk-publish` skill 路径引用同步）——纯本地路径搬迁，不影响线上 winget-pkgs / Homebrew tap 提交流程
- **`publish/arch/PKGBUILD`**（新）：`pkgname=eazymake`、`pkgver=1.2.0`（指向 `v1.2.0` 正式 tag）、`makedepends=('gcc' 'python')`（无 depends——Linux 静态链接）；`build()` 以 `EZMK_VERSION="$pkgver" bash build.sh` 源码构建；`package()` 安装 `ezmk` + `ezmk-lua`（dev.8 CMake 导出钩子运行时）+ `_ezmk`（`zsh/site-functions`），Linux / MSYS2 双变体（`build/ezmk` vs `build/ezmk.exe`）；`sha256sums=('SKIP')` 起步，稳定后填真实 digest
- **可移植性修复**：`src/toolchain.cpp` 补 `#include <climits>`（`ULONG_MAX` 在 Arch Linux gcc 下未声明，pre.1 远程验证发现；MSYS2 g++16 被传递包含故本机未暴露）
- **文档**：README 中英安装章节补「Arch Linux / MSYS2 自取 PKGBUILD + `makepkg -si`」路线（AUR 标注延后）；`ezmk-publish` skill 扩为三渠道总览 + pacman 章节（PKGBUILD 结构 / 验证流程 / 用户安装 / 坑位）+ 坑位清单补 3 行

### 测试

- 本机 MSYS2（MINGW64 环境）：`makepkg -fd` 生成 `eazymake-1.2.0-1-x86_64.pkg.tar.zst`；解包验证 `usr/bin/ezmk.exe` / `usr/bin/ezmk-lua.exe` / `usr/share/zsh/site-functions/_ezmk` 落位；`ezmk version` 输出 1.2.0
- 远程 Arch Linux（`ezmk_project@192.168.136.131`）：`makepkg -f` 生成 Linux 产物并验证（`<climits>` 可移植性 bug 在此发现并修复）
- 全量回归：775 用例 / 3554 断言零失败（dev.11 基线 775 / 3552，零回归）

---

## 1.2.0-dev.11 (2026-08-15) — 代码质量审查与改进（全库审查 + P0/P1 收口）

1.2.0 系列第十一个开发子版本，**系统性质量收口**：6 个并行审查代理按模块精读全库（配置/CLI、构建/缓存、包/仓库、Lua/异步、工具、测试），共发现 68 条问题（high 13 / medium ~40 / low ~15）+ 30 条值得保留的设计。本版落地 **P0 全部 + P1 大部**，分 9 个阶段逐项修复并带验收测试；P2（大规模拆分、`compare_version` 预发布语义、测试卫生、macOS FSEvents 逐文件重写）明确收口到后续子版本。公共 API 无破坏性变更（`load_msvc_env` 等公共声明保留；全部为内部重构 + 新增 i18n key；见文首 API Stability）。

### 新增 / 行为变更

- **`run_tests` 命令构造收口**（`src/build.cpp`，P0）：测试编译/链接复用 `build_compile_args`/`join_shell_args`/`translate_compile_flags`——S4 转义 + MSVC 翻译 + 依赖包 include + 缓存一次解决，test 路径命令注入面关闭；`project_objs` 由项目源收集推导（修复嵌套 `src/` 子目录对象缺失）；`test_filter` 经 `escape_shell_arg`
- **`package_include_dirs` 共享 helper**：`prepare_build_state` 与 `run_tests` 共用包 include 收集（单一维护点）
- **钩子/沙箱安全不对称修复**（`src/lua_api.cpp`，P0）：安装钩子（`preinstall`/`postinstall`）进入脚本上下文时加载包的 `[utils.permissions]`（`enforce_utils_permissions`）——与 utils 脚本同级门控，恶意包钩子不再无提示读任意路径；构建钩子保持 legacy 模型（项目自身代码，用户权限）；`norm_path` 改 `weakly_canonical` 堵 symlink/junction 逃逸（Windows junction 亦生效）；沙箱黑名单补 `io`/`os`（纵深防御）
- **编码事故全局修复**（P0）：build.cpp/util.cpp 用户可见乱码（`鈥?`→`—` 等 8 处）与 test_integration.cpp 24+ 处（含运行时字符串 `鏈煡`→`未知`）+ 测试 `EZMK_LANG` 护栏
- **配置/CLI 校验收口**（`src/config.cpp`/`cli.cpp`/`build.cpp`）：`extract_string_array` 非字符串元素报错（含字段名）；版本约束格式校验 + 空名报错；`default_profile` 交叉校验；`source_date_epoch` 负值报错；约束操作符按**最早出现**匹配（`pkg@^1.0` 修复）；`project clean` 参数解析；`project test -v` 别名（与 `-V` 同义）；`--locked`+`--no-lock` 互斥；`--sha256` 格式校验；profile 错误 i18n 化（7 新 key）
- **包/导入/导出正确性**（`src/import.cpp`/`export.cpp`/`pkg.cpp`）：import `target_*` 关键字感知（PRIVATE/PUBLIC 分组）；export `VERSION` 数字校验 + precompiled 复用 `select_precompiled_variant`；互依赖自动安装护栏（`g_auto_installing`）；自动安装约束回读校验；事务安装 `.new` 换位（拷贝窗口内旧版保留）+ 回滚 ec 检查 + backup 隐藏唯一名；选择器 `lib/` 缺失友好错误 + MSVC 平局偏好 `.lib`
- **pack 阻断修复**（`src/util.cpp`/`build.cpp`）：`create_targz` 数值字段改 '0' 左填充（修复提取 size=0 致包损坏）；pack 产物 ezmk.toml 注入 `precompiled = true`（预编译校验通过）
- **缓存/工具健壮性**（`src/cache.cpp`/`toolchain.cpp`/`util.cpp`/`lockfile.cpp`/`pkg.cpp`）：depfile 转义空格路径解析；SDE 入编译签名；`compiler_tag`/`parse_digits` stoul 溢出保护；macOS 家族 `--version` 双信号判定；targz 长路径 prefix 读出 + size 越界报错；`escape_cmd_arg`（`cmd /c` 转义）；下载 1GiB 上限 + curl `--max-filesize`；`find_package_archive` 确定性选择（lockfile/pkg）
- **死代码/一致性清理**：watch 内联 profile 死代码、`parse_catch2_xml`/`Catch2TestResult`（~130 行）、`load_msvc_env` 死调用（vcvars 探测/版本合并为单次运行）、`auto_update_repos` 死调用删除；**MSVC 产物路径共享 helper**（install/pack 修复 `.lib`/`_implib.lib`/MinGW `.dll` 查找，与 link_phase 对齐）；CliArgs optional 收敛；i18n `fmt` 单遍扫描（值含 `{...}` 不再嵌套替换）；`ThreadPool(0)` 拒绝；file_watcher Windows IOCP 错误恢复 + macOS kqueue 语义文档化
- **空断言测试修复**：pkg install e2e 去空转（本地打包归档安装，断言退出码 0 + 产物 + `pkg list`）；dev.10 预编译测试 oracle 去耦（独立构造期望标签，不再自指 `compiler_tag`）；钩子越界 e2e 真断言

### 文档

- `docs/en|zh/safety.md`：Lua 安装钩子权限门控（1.2.0-dev.11+）、构建钩子 legacy 模型说明、安全汇总表更新
- `docs/en|zh/pkg.md`：Lua 钩子安全性补 `[utils.permissions]` 门控说明
- `docs/en|zh/utils.md`：权限管理适用面扩展（安装钩子与 utils 同级）

### 测试

- `test_build.cpp`/`test_compile_db.cpp`：run_tests 注入回归、MSVC 翻译、project_objs 嵌套对象
- `test_integration.cpp`：项目对象链接、注入安全、钩子越界 e2e 真断言、symlink/junction 逃逸、含 tagged 归档 precompiled 导出、互依赖、约束不满足自动安装、pkg 本地归档安装、precompiled oracle 去耦
- `test_config.cpp`/`test_cli.cpp`：校验前移各字段 + clean 解析/别名/互斥/sha256 格式
- `test_cache.cpp`：depfile 转义空格、缓存签名差分 + SDE
- `test_toolchain.cpp`：stoul 溢出、macOS 家族双信号、vcvars 单次运行
- `test_util.cpp`：targz 长路径/越界往返、`escape_cmd_arg`
- `test_pkg.cpp`：互依赖护栏、约束回读、事务 `.new` 换位、选择器 `lib/` 缺失 + MSVC 平局
- `test_i18n.cpp`：fmt 单遍（嵌套替换回归 2 用例）
- `test_thread_pool.cpp`：`ThreadPool(0)` 拒绝
- 全量回归：775 用例 / 3552 断言零失败（基线 747 / 3442，+28 用例 +110 断言）

---

## 1.2.0-dev.9 (2026-08-15) — 包构建配置收敛（`src_dirs` / `include_dirs` 对包生效）

1.2.0 系列第九个开发子版本，**dev.7 的延伸**：让包的 `[compile]` 配置与项目语义对齐——`src_dirs` 从「被静默忽略」变为「真正生效」（复用 `build::collect_sources` 多目录收集 + 文件名去重 + 缺失目录 warn），`include_dirs` 自编译与消费者两侧行为固化（相对包根解析、与默认 `include/` 保序去重），包不再受 `[project].type` 的 `main.cpp` 校验影响。公共 API 无破坏性变更（`collect_sources` 新增默认参数 `require_main = true`，项目路径零变化；见文首 API Stability）。

### 新增 / 行为变更

- **包源收集改用 `[compile].src_dirs`**（`src/pkg.cpp` `compile_package()`）：默认 `["src"]` 与现状一致，绝大多数包零影响；多目录 / 自定义目录为纯新增能力；`header_only` 短路前移到收集前（无 src 的 header-only 包不触发 fatal）；`precompiled` 短路不变
- **空源收紧为 fatal**：非 header_only / precompiled / utils 却无任何源文件的退化包，安装报错（复用 `no_source_files` / `src_dir_missing`），不再静默生成空库
- **`collect_sources` 新增 `require_main`**（`include/ezmk/build.hpp`，默认 `true`）：项目调用点零改动；包路径显式传 `false`——包文档默认 `type = "executable"` 但包永远编译成静态库，不再误触发 `main_missing`
- **自编译 `-I` 保序去重**（`src/cache.cpp`，MSVC `/I` 与 GCC `-I` 两分支）：`def_inc`（`proj_root/include`）与 `include_dirs` 解析结果去重（首次出现顺序保留），compile_commands.json 输出更干净，编译器语义不变
- **`validate_pkg` src_dirs 感知**（设计补充）：自定义 `src_dirs` 的包不再被安装校验误拒；错误消息保留 `src/` 字样
- **utils 门控 src_dirs 感知**：utils 包「任一 src_dir 存在且有源文件才编译」，否则跳过
- **`pkg info` 增显 `src_dirs`**（i18n `pkg_info_src_dirs`）

### 文档

- `docs/en|zh/pkg.md`：`[compile]` 对包生效（`src_dirs` / `include_dirs` 语义、空源 fatal）、utils 门控 src_dirs 感知、目录安装校验按 `src_dirs`
- `docs/en|zh/package_authoring.md`：§2.3 `[compile]` 补充包语义说明；§3.1 静态库编译来源改为 `src_dirs`
- `docs/en|zh/config_file.md`：`include_dirs` / `src_dirs` 字段补充对包生效说明
- 新增 i18n key `pkg_info_src_dirs`（en/zh），`check_i18n.py` 三向一致（302 keys）

### 测试

- `test_build.cpp`：`collect_sources` `require_main=false` 2 个用例（`"executable"` 类型无 main 不抛、多目录收集）
- `test_pkg.cpp`：`compile_package` 5 个用例（多 `src_dirs` 编译 / 自定义 `include_dirs` 自编译 / header_only·precompiled 短路 / 空源 fatal）
- `test_cache.cpp`：自编译 `-I` 保序去重 2 个用例（GCC / MSVC 分支）
- 集成测试：自定义 `src_dirs`+`include_dirs` 包端到端（目录安装 → 编译 → 链接 → 运行输出 → `pkg info` 增显 → compile_commands 含包 `-I`）
- 全量回归：719 用例 / 3328 断言零失败（基线 709 / 3296，+10 用例 +32 断言）

---

## 1.2.0-dev.10 (2026-08-15) — 平台标识符扩展（工具链/ABI）

1.2.0 系列第十个开发子版本，**承接 package_authoring §3.3 多平台共包**：现有命名 `lib<name>.<os>-<arch>.<ext>` 刻意省略工具链，这对 C ABI 成立、对 **C++ ABI 不成立**（GCC/Clang/MSVC 互不兼容，同平台同架构也可能链接失败）。本版把平台标识符扩展为 `os-arch[-compiler][-abi]`，`select_precompiled_archive()` 按 **ABI 安全的 4 级匹配优先级**选择，降级匹配（可能跨工具链）显式警告，可选 `[project].precompiled_strict = true` fail-fast。公共 API 无破坏性变更（`select_precompiled_archive` 签名不变、两处调用点零改动；新增纯函数与可选字段；见文首 API Stability）。

### 新增 / 行为变更

- **`toolchain::compiler_tag()`**：从已缓存的 `tc.version` 生成编译器标签——GCC/Clang 按 `major.minor` 模式提取 major（`gcc13` / `clang18` / Apple `clang15`）；MSVC 解析 cl 版本 `19.<minor>` → `_MSC_VER` 等价数 → **查表**（1900→msvc140 / 1910-1919→msvc141 / 1920-1929→msvc142 / ≥1930→msvc143，不用算术避免 1943→144 错算）；无法解析返回空
- **平台标识符扩展** `os-arch[-compiler][-abi]`：ABI 标签按工具链默认值生成（GCC/Clang+libstdc++ → `abi11`，libc++/MSVC → 无），零配置
- **4 级匹配**（`src/pkg.cpp` `select_precompiled_archive` 重写）：L4 完整标签 > L3 同编译器（产物无 abi 段）> L2 os-arch > L1 裸名；同编译器但 abi 段显式不同 → **ABI 不兼容跳过**；同分文件名字典序（确定性）；未知段不识别
- **ABI 降级警告**（i18n `precompiled_toolchain_fallback_warn`）：消费端带工具链标签却落 L2/L1 → 显式警告（指明工具链标签 + 可用产物），不再静默拿到错误 ABI 的库到链接期才炸；无匹配报错补当前完整标签
- **`[project].precompiled_strict`**（默认 `false`，P1）：L2/L1 降级改 fail-fast（`precompiled_strict_mismatch`）；消费端 build.cpp 调用点对 fatal 传播（不降级为 skip 警告）
- **`pkg info` 增显**：precompiled 包列出 `lib/` 可用产物标签（含裸名，i18n `pkg_info_precompiled_variants`）

### 文档

- `docs/en|zh/package_authoring.md` §3.3：命名约定 `os-arch[-compiler][-abi]`、编译器/ABI 标签表、4 级优先级、ABI 降级警告、严格模式、已知局限（Apple Clang / clang-cl / 旧 ABI 覆盖）
- `docs/en|zh/pkg.md`、`config_file.md`：`precompiled_strict` 字段
- 新增 i18n key 3 个（en/zh），`check_i18n.py` 三向一致（306 keys）

### 测试

- `test_toolchain.cpp`：`compiler_tag` 5 个用例（GCC/Clang/Apple Clang/MSVC 工具集/查表边界 1900/1910/1930/1943/不可解析）
- `test_pkg.cpp`：匹配矩阵 11 个用例（L4/L3/L2/L1、ABI 不匹配跳过、未知段忽略、不同编译器不匹配、字典序 tie-break、降级仍选中、strict fatal、无编译器标签不警告/不 strict、报错含工具链+available）
- 集成测试 3 个：工具链标签产物选中并链接（裸名诱饵含不同符号，选错即链接失败）+ `pkg info` variants；os-arch 降级警告；`precompiled_strict` 安装 fail-fast
- 全量回归：747 用例 / 3442 断言零失败（基线 727 / 3361，+20 用例 +81 断言）

---

## 1.2.0-dev.12 (2026-08-15) — 测试配置收口（`[test].default_profile` / `include_dirs` / `link_targets`）

1.2.0 系列第十二个开发子版本，**dev.3 的延伸**：`ezmk test` 引入 profile 支持——`[test].default_profile` + `ezmk test --profile`（复用 `[compile.profile.*]` / `[link.profile.*]`，与 `ezmk build` 完全对称），并补齐测试专属 include / 链接目标（`[test].include_dirs` / `[test].link_targets`），弃用与 `[compile].flags` 重叠的 `[test].flags`（使用点 warn，2.0.0 移除）。公共 API 无破坏性变更（纯新增可选字段 + CLI 选项；见文首 API Stability）。

### 新增 / 行为变更

- **`[test].default_profile`**：`ezmk test` 未传 `--profile` 时回退应用 `[compile.profile.<name>]`（flags/macros/msvc_flags）+ `[link.profile.<name>]`（link flags）——测试编译/链接与 build 的 profile 语义逐字节一致
- **`ezmk test --profile <name>`**：CLI 覆盖 `default_profile`（CLI > default_profile > 无），与 `ezmk build --profile` 对称
- **`[test].include_dirs`**：测试编译追加的 `-I` 目录（相对项目根解析、缺失跳过），测试专属，不污染主构建
- **`[test].link_targets`**：测试 runner 链接追加的 `-l` 系统库目标（catch2 与 ezmk 双框架一致），测试专属，不污染主构建
- **`apply_profile` 共享 helper**（`src/build.cpp`）：build 路径（`prepare_build_state`）内联的 profile 合并抽取为共享 helper，`run_tests` 复用——profile 解析/合并/未知 profile fatal（含 closest-match 建议）单一事实源，build 路径行为逐字节不变
- **`[test].flags` 弃用**：非空时在 `ezmk test` 使用点输出弃用警告（i18n `test_flags_deprecated`），行为不变；2.0.0 移除；替代写法 `[compile.profile.<name>]` + `default_profile`，include/链接用新字段

### 文档

- `docs/en|zh/config_file.md`：`[test]` 节新增 `default_profile` / `include_dirs` / `link_targets` 字段，`flags` 标弃用
- `docs/en|zh/cli.md`：`ezmk test` 新增 `--profile <name>` 标志
- 新增 i18n key `test_flags_deprecated`（en/zh），`check_i18n.py` 三向一致（303 keys）

### 测试

- `test_config.cpp`：`[test]` 新字段解析 3 个用例（default_profile / include_dirs / link_targets；默认空；flags 仍解析）
- `test_cli.cpp`：`ezmk test --profile` 解析 4 个用例（分离值 / `=` 形式 / 缺省为空 / 缺值抛错）
- 集成测试：`[test]` 新字段端到端（ezmk 内建框架，无 catch2 依赖）——default_profile 宏生效（`PROFILE=1`）、`--profile debug` 覆盖（`PROFILE=2`）、include_dirs 头文件解析（`HELP=7`）、link_targets `-lm` 出现在编译命令（MSVC 跳过）、flags 弃用 warn
- 全量回归：727 用例 / 3361 断言零失败（基线 719 / 3328，+8 用例 +33 断言）

---

## 1.2.0-dev.8 (2026-08-15) — CMake 导出钩子运行时（`ezmk-lua`）

1.2.0 系列第八个开发子版本，**dev.2 的范围收口**：为 `export cmake` 补上 `[hooks]` 钩子映射——新增**独立、无黑白名单的 Lua 运行时二进制 `ezmk-lua`**，由导出的 CMake 在构建节点调用它复现 `ezmk build` 的钩子后处理，消除导出产物与本体构建的行为漂移。**`ezmk` 本体沙箱/黑白名单零改动**（纯新增产物 + 导出文本变化，见文首 API Stability）。

### 新增

- **`ezmk-lua` 独立运行时**（`src/ezmk_lua_main.cpp` + `src/lua_api.cpp` `run_script_unrestricted()`）：复用 Lua VM 与 `register_api` bindings（共享编译单元，`ezmk.*` 不双维护），入口**无沙箱**——不建 restricted globals、不加载 `[utils.permissions]`、补开 `os`/`io` 库（沙箱编译期移除的），是构建沙箱的**严格超集**；CLI 契约 `ezmk-lua <hook.lua> [--project-root <dir>] [--profile <name>] [--output <path>]`，`ctx`（output/project_root/profile）由 CLI 注入，`run(ctx)` 返回值透传为退出码
- **`export cmake` 钩子映射**（`src/export.cpp`）：hooks 段从「注释 + WARNING」改为 `find_program(EZMK_LUA ezmk-lua)` + `add_custom_command`（`pre_build` → `PRE_BUILD`、`post_build` → `POST_BUILD`，脚本路径相对 `${CMAKE_CURRENT_SOURCE_DIR}`、`--output $<TARGET_FILE:<name>>`、`--profile` 内联）；找不到 `ezmk-lua` 回退 `message(WARNING)`（best-effort，非硬依赖）；**`on_failure` 保持不导出**（CMake 无原生失败钩子，范围边界同 dev.2）；导出时打印 hooks 提示（i18n `export_hook_note`）
- **能力面超集约定**：`ezmk-lua` 下钩子可用 `os.execute`/`io` 等沙箱外能力；文档约定导出钩子只用 `ezmk.*` 子集，保证「`ezmk build` 沙箱」与「导出 CMake 无沙箱」行为一致

### 文档

- `docs/en|zh/cli.md` 新增 `ezmk-lua` 伴侣运行时用法；`docs/en|zh/config_file.md` hooks 节补充 CMake 导出映射说明
- 新增 i18n key `export_hook_note` + `ezmk_lua_usage`/`ezmk_lua_missing_script`/`ezmk_lua_need_value`/`ezmk_lua_unknown_option`/`ezmk_lua_extra_arg`（en/zh），`check_i18n.py` 三向一致（301 keys）

### 测试

- 新增 `run_script_unrestricted` 单测 8 个（基本执行/ctx 注入/返回码/null L/`os`·`io` 可用/配置注入/缺失 run()/Lua error）
- `test_export.cpp` hooks 用例更新为 `find_program` + `add_custom_command` + 回退 warning + `on_failure` 注释 + `--profile` 内联
- 新增集成测试 3 个：`ezmk-lua` 跑样例钩子（ctx + 返回码）、`ezmk build` 钩子沙箱路径零变化、`export cmake` 产物含钩子调用 + 导出提示
- 新增 `test_i18n.cpp` dev.8 key 非空 + fmt 断言
- 全量回归：709 用例 / 3296 断言零失败（基线 695 / 3234）

---

## 1.2.0-dev.7 (2026-08-15) — 本地包源 + 项目向上查找

1.2.0 系列第七个开发子版本：聚合两个相互独立的改进——① **`ezmk pkg install <dir>` 从文件夹安装包**（开发/调试本地包免打包归档）；② **`ezmk.toml` 向上查找**（进入项目子目录直接 `ezmk build` / `ezmk test`，如同 `git`）。**纯增量、不破坏任何公共 API**（新增目录入参形态 + 内部项目根定位，见文首 API Stability）。

### 新增

- **`pkg install <dir>` 目录安装**（`src/pkg.cpp`）：参数为已存在目录时直接从源目录安装——复用 `validate_pkg` 校验 + 与归档安装完全相同的后处理（钩子 → 依赖 → 编译 → 复制 → postinstall）；跳过解压与 SHA-256 校验（无归档，显式 `--sha256` 提示跳过）；作用域/钩子/依赖语义不变；本地 `ezmk-repo` checkout 解包后的 `packages/<name>/` 目录同样可安装
- **`ezmk.toml` 向上查找**（`util::locate_project_root()`，`src/util.cpp`）：从 CWD 向上最多 5 层父目录查找含 `ezmk.toml` 的目录；`main.cpp` 各项目命令统一定位根并以绝对路径 `parse_config`；`pkg`/`cache`/`build`/`repo` 的 Project 作用域路径（`.ezmk/pkg`、cache、repo、lockfile）同步改定位根；5 层内未找到时给出明确报错（i18n `config_not_found_upward`），无配置场景（`project new` / `project import` / `clean` 回退）行为不变
- **代码重构**：`install()` 解压后处理抽为 `process_installed_pkg()`（归档/目录共用），lockfile 生成抽为 `maybe_write_lockfile()`

### 文档

- `docs/en|zh/pkg.md` 新增「从文件夹安装」小节 + 查找顺序前置目录分支
- `docs/en|zh/cli.md`、README/README_ZH 说明 `ezmk.toml` 向上查找行为（5 层边界）
- 新增 i18n key `pkg_install_from_dir` / `pkg_sha256_skipped_dir` / `config_not_found_upward`（en/zh），`check_i18n.py` 三向一致

### 测试

- 新增 `locate_project_root` 单测（0/1/5/6 层、无 toml、`max_up` 覆盖）
- 新增集成测试：目录安装成功（`pkg list` 可见）/ 非法目录拒绝；子目录内 `build` 成功、6 层超出边界失败、无配置回退报错
- 新增 `test_i18n.cpp` dev.7 key 非空 + fmt 断言
- 全量回归：695 用例 / 3234 断言零失败

---

## 1.2.0-dev.6 (2026-08-14) — 各源文件构建耗时统计

1.2.0 系列第六个开发子版本：为 `ezmk build` 并行编译路径补上 **per-file 编译耗时明细**，让"慢在哪一步"一目了然——`-v` 时始终按耗时降序打印本次实际编译（非缓存命中）的源文件，默认构建总耗时超过 5 秒时自动打印最慢的 10 个 + 汇总行。**纯诊断增强、零配置、不新增 flag**（见文首 API Stability）。

### 新增

- **per-file 编译耗时统计**（`src/build.cpp` `compile_phase()` 并行分支）：单次 `compile_one_source()` 粒度计时，只计入缓存未命中（实际编译）的文件；串行路径（`compile_sources`）仅总耗时、不输出明细
- **明细输出**（完成汇总块扩展）：`-v/--verbose` 全量降序明细；默认总耗时 > `BUILD_TIME_SLOW_THRESHOLD`（5s）时自动 top-`BUILD_TIME_TOP_N`（10）+ 汇总行；`build_elapsed_time` 总耗时输出不变
- **零配置**：阈值/条目数为命名常量，复用 `-v`，不新增 CLI flag 与配置字段

### 文档

- `docs/en|zh/cli.md` 补充 build 耗时明细触发规则（`-v` 全量 / 慢构建自动 top-N）
- 新增 i18n key `build_time_header` / `build_time_entry` / `build_time_truncated`（en/zh），`check_i18n.py` 三向一致

### 测试

- 新增集成测试 `integration: build timing detail (dev.6)`（`-v` 明细存在 / 小项目默认不刷屏，不校验具体耗时——非确定性）
- 新增 `test_i18n.cpp` `build_time_*` key 非空 + fmt 断言
- 全量回归：685 用例 / 3194 断言零失败

---

## 1.2.0-dev.4 (2026-08-13) — CMake 项目导入（实验性）

1.2.0 系列第四个开发子版本：新增 **`ezmk project import --from cmake`**，把标准 CMake 项目的 `CMakeLists.txt` **单向转换**为 `ezmk.toml`（与 dev.2 的 `export cmake` 反向互补）。**实验性**——转换 best-effort，非标准写法明确拒绝且事务性中止。**不破坏任何公共 API**（纯新增命令 + flag，见文首 API Stability）。

### 新增

- **`ezmk project import` 命令**：读取当前目录 `CMakeLists.txt` → 生成 `ezmk.toml`；`--from`（默认 cmake、大小写不敏感）、`--overwrite`（已存在 `ezmk.toml` 时默认拒绝，避免覆盖手写配置）
- **轻量 CMake 解析器**：识别命令调用（双引号/括号嵌套/`#` 注释/`[[...]]` 长字符串）+ 有限 `set()` 变量表 + 单层 `${VAR}` 展开（§3.2 变量展开策略）
- **核心命令映射**：`project` / `add_executable` / `add_library` / `target_sources` / `target_include_directories` / `target_compile_definitions` / `target_compile_options` / `target_link_libraries` → `[project]` / `[compile]` / `[link]` / `[compile.macros]`
- **`find_package` best-effort**：常见包别名表（抽取至 `src/pkg_alias.hpp`，与 dev.2 导出共享单一事实源）→ 注释掉的 `[depends]` 条目 + `# TODO:` 提示
- **条件编译 best-effort**：按当前平台取 `if(WIN32)` / `if(UNIX)` 等分支；无法求值的条件跳过 + `# TODO: 未求值的条件块`
- **事务性拒绝**：`add_custom_command` / `add_custom_target` / `function()` / `macro()` / `pkg_check_modules` / `execute_process` / 生成器表达式 `$<...>` → 中止且**不产出半成品** `ezmk.toml`

### 文档

- 新增 `docs/en|zh/migrate-from-cmake.md`（支持/拒绝清单 + 手动迁移：用 Lua `[hooks]` 复刻自定义步骤）
- `docs/en|zh/cli.md`、`docs/en|zh/config_file.md`、README/README_ZH 补充 `project import`
- 新增 `tutorial/en|zh/11-import-cmake.md`（从零导入 → build → run）

### 已知限制

- 多 target 项目仅导入第一个/主 target
- `${VAR}` 仅做有限单层展开（顶层常量 `set()`），不实现作用域/`CACHE`/递归
- 非声明式写法（自定义命令/生成器表达式/函数宏/外部探测）不支持，事务性拒绝

---

## 1.2.0-dev.5 (2026-08-14) — catch2 v3 测试主程序兼容

1.2.0 系列第五个开发子版本：修复 `ezmk test` 在 catch2 v3（官方仓库当前版本 3.6.0）下无法链接的问题。catch2 v3 已移除 `CATCH_CONFIG_MAIN` 宏，`ezmk test` 原先固定生成的 `#define CATCH_CONFIG_MAIN` + `#include <catch2/catch_all.hpp>` 在 v3 下**不产生任何 `main`**，测试链接无入口点（本机表现为 mingw 报 `undefined reference to WinMain`）。**不破坏任何公共 API**（纯内部 test_main 生成逻辑，见文首 API Stability）。

### 修复

- **v3 多头路径生成 v3 兼容 main**：`src/build.cpp` `run_tests` 的 test_main 生成按 v2/v3 分支——v3（无 `include/vendor/catch2.hpp`）改为 `#include <catch2/catch_session.hpp>` + 显式 `int main` 调 `Catch::Session().run(argc, argv)`（v2/v3 均有的稳定 API，未来 catch2 升级无需再改）
- **v2 vendor 单头路径不回归**：`include/vendor/catch2.hpp`（v2 单头）仍走 `#define CATCH_CONFIG_MAIN` 原逻辑，行为不变

### 前置

- **catch2 3.6.0 包内容修复**（`ezmk-repo` `3b74cc1`）：107 个实现 .cpp 平铺进 `src/`，`libcatch2.a` 含 106 成员 / 24939 实现符号。本计划只修 EazyMake 侧 CLI，包修复作为前置不在本文重复

### 测试

- 新增集成测试 `integration: ezmk test works with catch2 v3`（建项目 + `[depends] catch2` + test 源 → `ezmk test` 端到端；离线/无 repo 时 SKIP）
- 全量回归：683 用例 / 3170 断言零失败

---

## 1.2.0-dev.3 (2026-08-12) — 默认模板内建 Debug/Release Profile

1.2.0 系列第三个开发子版本：把 Debug/Release profile 固化进 `ezmk project new` 的默认模板，基准 `[compile].flags` 收敛为警告-only（优化归 profile），并新增 `[compile].default_profile` 配置项（模板内建 `"debug"`）——无 `--profile` 的默认构建开箱即可调试（`-g -O0`、断言开启），需优化时显式 `--profile release`。**不破坏任何公共 API**（纯模板变更 + 可选字段，见文首 API Stability）。

### 新增

- **`[compile].default_profile` 配置项**：声明"未显式 `--profile` 时默认使用哪个 profile"。非空时，`ezmk build` / `ezmk project watch` / `ezmk project cc` / `ezmk project export cmake` 四处消费点在无 `--profile` 时统一回退到该 profile（复用同一 profile 合并/unknown 报错路径）——compile_commands.json 与 CMake 导出和默认构建一致，避免"构建用 debug、索引/导出用基准"的分叉
- **默认模板内建 profile**：`ezmk project new` 生成的 `ezmk.toml` 现包含 `[compile.profile.debug]`（`-g -O0` / `/Zi /Od`）与 `[compile.profile.release]`（`-O2 -DNDEBUG` / `/O2 /DNDEBUG`），跨 GCC/Clang/MSVC 一致

### 行为变更

| 变更 | 影响范围 | 说明 |
|------|----------|------|
| 基准 `-O2` 移除，改为警告-only（`-Wall -Wextra`） | **仅新建项目** | 旧项目 `ezmk.toml` 不被改写，行为不变 |
| 模板内建 `default_profile = "debug"` | 新建项目 | 无 `--profile` 的默认构建 = debug（`-g -O0`、断言开启、无优化）；需优化时显式 `--profile release`——"优化属于 profile"的明确语义 |
| 新增 debug/release profile | 新建项目 | 开箱可用 `--profile debug` / `--profile release`；默认构建即 debug |
| 旧配置无 `default_profile` | 不受影响 | 无 `--profile` 时仍基准-only，行为不变 |

---

## 1.2.0-dev.2 (2026-08-11) — CMakeLists.txt 导出

1.2.0 系列第二个开发子版本：新增 **`ezmk project export cmake`**，从 `ezmk.toml` 一键生成 `CMakeLists.txt`（单向快照——`ezmk.toml` 为事实源，重新生成勿手改），让同一项目既可用 `ezmk build` 也可被 CMake 生态构建/索引。**不破坏任何公共 API**（纯新增命令，见文首 API Stability）。

### 新增

- **`ezmk project export cmake`**：命令挂在 `project` 命名空间下，`<target>` 参数区分导出格式（首个 `cmake`，为未来 `make`/`meson` 预留）
  - **项目级映射**：`project()` / `add_executable` / `add_library(STATIC|SHARED)`；`utils` 类型跳过 + `message(WARNING)`；`header_only` → `INTERFACE`；`precompiled` → `IMPORTED`
  - **编译映射**：`file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 源收集（`--no-glob` 显式列表）；`include_dirs` 项目内 `${CMAKE_CURRENT_SOURCE_DIR}/` 前缀、`@link:` 外部路径绝对+注释；宏与 `ezmk build` 注入一致（复用 `generate_ezmk_macros()`）；`-std` → `CXX_STANDARD`/`CXX_EXTENSIONS`；flags 拆 `-I`/`-D`；`msvc_flags`/stdlib genex
  - **链接映射**：`link_dirs` → `target_link_directories`；`system_targets` 与 `-l` → `target_link_libraries`；`link.flags` 去 `-L`/`-l` → `target_link_options`；`link.msvc_flags` genex
  - **覆盖安全**：目标已存在且无 `--overwrite` → 拒绝（exit 1）；文件头标注生成来源
  - **依赖映射（P1 best-effort）**：内置常见包别名表 + `find_package` 便携模式（`if(TARGET)` + `message(STATUS)` 提示）；`--resolve` 输出已安装依赖具体路径（不可移植）
  - **`[hooks]` 不映射**：`pre_build`/`post_build`/`on_failure` 是 EazyMake 沙箱 Lua（`ezmk.*` API），CMake 无等价运行时——生成注释块 + `message(WARNING)`，避免 CMake 构建静默丢失钩子后处理
  - flags：`-o/--output`、`--overwrite`、`--profile`、`--resolve`、`--glob`/`--no-glob`

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

## 1.1.0-dev.2 (2026-07-28) — 多平台共包与 project pack

1.1.0 系列第二个开发子版本：多平台共包支持、`index.toml` 平台映射，以及 `ezmk project pack` 一键打包命令。

### 多平台共包支持

- **`util::detect_platform_tag()`**：新增简化平台标签（`win-x64` / `linux-x64` / `mac-arm64`），用于文件名匹配和索引过滤
- **`select_precompiled_archive()`**：预编译包 `lib/` 下按 `lib<name>.<tag>.a` 自动选择当前平台产物；fallback 到无后缀文件（向后兼容）
- **`index.toml` `platform` 字段**：`[[packages]]` 条目新增可选 `platform` 字段（`os-arch` 格式），`read_pkg_from_index()` 搜索时自动按平台过滤；缺失字段的旧条目视为全平台可用

### `ezmk project pack`

- **`util::create_targz()`**：新增 tar.gz 创建函数（ustar tar + raw deflate gzip），基于现有 miniz 库，零新依赖
- **CLI**：`ezmk project pack [--output <dir>] [-v]`，简写 `pp`；将 `static` 项目一键打包为 `<name>-<version>.tar.gz`
- **输出**：自动构建（如未构建）→ 收集 `include/` + `lib<name>.a` + `ezmk.toml` → 打包 → 打印 SHA-256；`-v` 模式额外输出 `index.toml` 条目模板

### 其他

- **`build.cpp`**：依赖包预编译归档收集改为平台感知（`select_precompiled_archive()`），避免多平台文件冲突
- **i18n**：新增 5 个 key（`pack_*`），中英双语翻译

---

## 1.1.0-dev.1 (2026-07-28) — MSVC 包编译、确定性构建与产物安装

1.1.0 系列首个开发子版本。补齐 MSVC 工具链在包管理中的完整支持，引入确定性构建与 lockfile 机制，新增 `project install` 命令。

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

---

## 0.1.6 (2026-06-20) — 测试基础设施与集成测试

建立单元测试与端到端集成测试体系，为后续所有版本提供回归防线。

### 新增
- **Catch2 v3 单头集成**：`include/vendor/catch2.hpp`（header-only，无需额外编译）+ `test/test_main.cpp` 入口
- **`build.sh test` 目标**：编译测试二进制（排除含自有 `main()` 的 `src/main.cpp`），`test -v` 编译并运行
- **模块单元测试**：`test/test_config.cpp`、`test/test_crypto.cpp`（SHA-256 NIST 向量）、`test/test_util.cpp`、`test/test_cache.cpp`、`test/test_cli.cpp`、`test/test_build.cpp`、`test/test_project.cpp`、`test/test_pkg.cpp`、`test/test_repo.cpp` 覆盖各模块核心路径
- **端到端集成测试**（`test/test_integration.cpp`）：项目生命周期（new/build/run/clean 各类型）、缓存正确性（命中/失效/损坏重建）、包生命周期（安装/校验/依赖链/钩子）、仓库生命周期（add/update/remove/scope 隔离）、CLI 集成

### 注意事项
- 测试独立于外部状态：文件系统测试用 `temp_directory_path()` + RAII 清理，不依赖特定项目结构
- 编译器/git 相关测试先查工具可用性，不可用时 `SKIP`
- 网络相关测试用本地文件/mock 绕过（URL 下载用本地 HTTP server、git 用临时 `git init` 仓库）
