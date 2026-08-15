# 配置文件`ezmk.toml`

---

## `project` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `name` | string | 是 | — | 项目名称 |
| `type` | string | 否 | `"executable"` | 项目类型：`"executable"` / `"static"` / `"shared"` / `"utils"` |
| `version` | string | 是 | — | 项目版本，建议 SemVer 格式（如 `"0.1.0"`） |
| `language` | string | 否 | `"C++17"` | 语言标准，如 `"C++17"`、`"C11"`、`"GNUCPP17"`。大小写不敏感，`C++`/`CXX`/`CPP` 统一 |
| `stdlib` | string | 否 | `"libstdc++"` | **1.1.0-dev.4+** 标准库：`"libstdc++"`（默认）或 `"libc++"`。别名：`"glibcxx"` / `"gnu"` → `libstdc++`；`"llvm"` → `libc++`。大小写不敏感 |
| `precompiled_strict` | bool | 否 | `false` | **1.2.0-dev.10+** 预编译包严格模式：L2/L1 工具链降级（可能 ABI 不兼容）改为 fail-fast 报错。仅对包生效（`precompiled = true` 时），详见 [包制作指南](package_authoring.md#33-预编译包precompiled--true097) |

> **为什么 `type` 用字符串而非枚举？** 为了让旧配置平滑升级——以后新增项目类型（如 `"header-only"`）只需引入一个新字符串值，已有的 `ezmk.toml` 无需改动。字符串在声明式配置里读起来也更自然。

### `type` 取值说明

| 值 | 产物 | 是否要求 main.cpp |
|----|------|-------------------|
| `"executable"` | 可执行文件 | 是 |
| `"static"` | 静态库 `lib<name>.a` | 否 |
| `"shared"` | 动态库 `lib<name>.dll` / `lib<name>.so` | 否 |
| `"utils"` | 工具包（无编译产物，或 `lib<name>.a`） | 否 |

### `language` 格式

格式为 `<语言><版本>`。解析器大小写不敏感，并接受多种变体拼写：

| 用户输入 | 标准化后 | `-std=` 标志 | 编译器 |
|----------|---------|-------------|--------|
| `C++17` / `c++17` / `cpp17` / `cxx17` | `CPP17` | `-std=c++17` | `g++` |
| `C11` / `c11` | `C11` | `-std=c11` | `gcc` |
| `C++`（无版本号） | `CPP17` | `-std=c++17` | `g++` |
| `C`（无版本号） | `C11` | `-std=c11` | `gcc` |

- 语言：`C++` / `CXX` / `CPP`（均映射为 C++），或 `C`
- 版本：`89` / `98` / `99` / `03` / `11` / `14` / `17` / `20` / `23` / `26`
- 默认版本：C++ → `17`，C → `11`

> **为什么接受这么多种拼写？** 用户习惯各不相同（`c++17` / `cpp17` / `cxx17` 混用）。统一标准化成一个规范值（如 `CPP17`），内部可直接比较，也能以稳定形式注入 `EZMK_LANG` 宏。

#### GNU 拓展（1.1.0-dev.4+）

在语言前加 `GNU` 前缀以启用 GNU 编译器拓展：

| 用户输入 | 标准化后 | `-std=` 标志 | 警告 |
|----------|---------|-------------|------|
| `GNUCPP17` / `gnuc++17` | `GNUCPP17` | `-std=gnu++17` | non-ISO 警告 |
| `GNU11` / `gnu11` | `GNU11` | `-std=gnu11` | non-ISO 警告 |

> 使用 GNU 拓展时会输出警告，建议使用标准写法（如 `language = "CPP17"`）。

> **为什么使用 GNU 拓展要警告？** GNU 拓展不可移植——依赖它的代码在严格模式或其他编译器下可能编不过。警告是引导用户改回标准写法，而不是禁用该功能。

---

## `compile` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 编译时添加的标志（如 `-Wall`、`-O2`）。GCC/Clang 格式，MSVC 下自动翻译 |
| `msvc_flags` | string[] | 否 | `[]` | **0.2.1+** MSVC 专用编译标志（不翻译，仅 MSVC 工具链时追加） |
| `include_dirs` | string[] | 否 | `["include"]` | 编译时 `-I` 搜索路径，相对于项目根目录；对包同样生效（1.2.0-dev.9+，相对包根解析，与默认 `include/` 保序去重） |
| `src_dirs` | string[] | 否 | `["src"]` | **0.2.2+** 源文件搜索目录，支持多个目录（如 `["src", "lib"]`）。显式设为 `[]` 会报错；对包同样生效（1.2.0-dev.9+，包编译按此收集源文件，缺失目录 warn+跳过） |
| `ezmk_macros` | bool | 否 | `true` | **0.2.2+** 是否自动注入 `EZMK_*` 标准预处理器宏（`EZMK`/`EZMK_VERSION`/`EZMK_PROJECT_*`） |
| `compile_commands` | bool | 否 | `false` | **1.1.1+** 构建成功后自动生成 `compile_commands.json`（clangd 索引） |
| `default_profile` | string | 否 | `""` | **1.2.0+** 未传 `--profile` 时默认使用的 profile。非空时，裸 `ezmk build` 会按该名字执行一次 profile 合并（与显式 `--profile` 走同一 lookup/合并/报错路径）；为空时不应用任何 profile |

注：旧字段 `include_dir`（单数）已废弃，解析时若遇到可自动映射到 `include_dirs`。

> **`compile_commands`（1.1.1+）：** 为 `true` 时，`ezmk build` 链接成功后写入 `compile_commands.json`。索引由与构建相同的命令构造（单一事实源）生成，因此不会与真实编译参数漂移——`-D` 宏、include 目录、`@link:` 解析结果与当前 profile 都会反映在内。对标 CMake 的 `CMAKE_EXPORT_COMPILE_COMMANDS`。`ezmk utils cc` 可随时按需生成；`--compile-commands` 构建 flag 可在不改配置的情况下单次启用。

> **为什么单独提供不翻译的 `msvc_flags`？** GCC→MSVC 自动翻译覆盖不了所有差异（如 `/Zi`、`/Od` 在 GCC 下没有对应项）。`msvc_flags` 原样透传，MSVC 用户可精确控制而无需与翻译层博弈。

### `[compile.macros]` 节（0.2.2+）

独立子节，定义预处理器宏。比在 `flags` 中用 `-D` 更语义化，且 MSVC 下自动翻译为 `/D`。

| TOML 写法 | 生成的标志（GCC） | 生成的标志（MSVC） | 说明 |
|-----------|-------------------|---------------------|------|
| `DEBUG = ""` | `-DDEBUG` | `/DDEBUG` | 空值 → 仅定义符号 |
| `VERSION = "0.2.0"` | `-DVERSION="0.2.0"` | `/DVERSION="0.2.0"` | 字符串值 → key=value |
| `MAX_SIZE = 4096` | `-DMAX_SIZE=4096` | `/DMAX_SIZE=4096` | 整数值 → 不加引号 |
| `ENABLED = true` | `-DENABLED=1` | `/DENABLED=1` | 布尔 true → 1 |
| `ENABLED = false` | （不生成） | （不生成） | 布尔 false → 跳过 |

- key 必须是合法 C 标识符（`[A-Za-z_][A-Za-z0-9_]*`），非法时报错
- 宏的生效顺序：`ezmk_macros`（标准宏）→ `flags` 中的 `-D` → `[compile.macros]` → want.lib 缺失宏。后者覆盖前者同名定义

> **为什么是这个生效顺序？** 后定义覆盖先定义，让最具体的来源压过最通用的来源：你的 `[compile.macros]` 覆盖 `flags` 里的 `-D`，`-D` 再覆盖标准 `EZMK_*` 宏。这是一个可预测的优先级链。

### 标准预定义宏（`ezmk_macros = true` 时）

| 宏名 | 类型 | 示例值 | 说明 |
|------|------|--------|------|
| `EZMK` | 整数 | `1` | 始终定义为 `1`，标识构建系统为 EazyMake |
| `EZMK_VERSION` | 字符串 | `"0.2.2"` | EazyMake 自身版本号 |
| `EZMK_PROJECT_NAME` | 字符串 | `"myapp"` | `[project].name` |
| `EZMK_PROJECT_VERSION` | 字符串 | `"1.0.0"` | `[project].version` |
| `EZMK_PROJECT_TYPE` | 字符串 | `"executable"` | `[project].type` |
| `EZMK_LANG` | 字符串 | `"CPP17"` | **1.1.0-dev.4+** 标准化后的 `[project].language`（如 `c++17` → `CPP17`） |
| `EZMK_STDLIB` | 字符串 | `"libstdcxx"` | **1.1.0-dev.4+** `[project].stdlib`，`++` 替换为 `xx`（`libstdc++` → `libstdcxx`） |

设置 `ezmk_macros = false` 可完全禁用标准宏注入。

---

## `link` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 链接时添加的标志 |
| `msvc_flags` | string[] | 否 | `[]` | **0.2.1+** MSVC 专用链接标志（不翻译，仅 MSVC 工具链时追加） |
| `link_dirs` | string[] | 否 | `[]` | 链接时 `-L` 搜索路径，相对于项目根目录 |
| `system_target` | string[] | 否 | `[]` | 需要链接的系统库（如 `"pthread"`、`"m"`） |

---

## `depends` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `lib` | string[] | 否 | `[]` | 硬性依赖库名列表。缺失 → 构建失败 |
| `want` | string[] | 否 | `[]` | **0.2.2+** 可选依赖库名列表。缺失 → warn + 定义 `EZMK_LIB_MISS_<NAME>` 宏，不阻断构建 |

> **为什么缺失时只警告而不是失败？** `want` 表达的是可选依赖。缺失时定义 `EZMK_LIB_MISS_<NAME>` 宏，代码可据此 `#ifdef` 绕过该特性、优雅降级，而不是中断构建。

> **`ezmk project import` 生成的配置（1.2.0+）** —— CMake 导入器把 `find_package`
> 命中的包写成**注释掉的** `[depends]` 条目（如 `# lib = ["boost@1.82"]`），并带
> `# TODO:` 提示。取消注释并调整后，执行 `ezmk pkg install <name>`。生成文件头部还有
> 说明其自动生成、属实验性的注释块。详见 [migrate-from-cmake.md](migrate-from-cmake.md)。

### 版本约束（0.9.6+）

每个依赖项可以附加版本约束，使用以下运算符：

| 语法 | 含义 | 示例 |
|--------|---------|---------|
| `pkg@1.2.3` | 精确版本 | `fmt@10.2.1` |
| `pkg^1.2.3` | 兼容版本（主版本不变） | `spdlog^1.14.0` → `>=1.14.0, <2.0.0` |
| `pkg~1.2.3` | 近似版本（次版本不变） | `nlohmann_json~3.11.0` → `>=3.11.0, <3.12.0` |
| `pkg>=1.2.3` | 大于等于 | `zlib>=1.2.0` |
| `pkg>1.2.3` | 严格大于 | `boost>1.80.0` |
| `pkg` | 无约束（取最新） | `fmt` — 取可用最高版本 |

**设计说明：**
- **向后兼容**：不带运算符的条目（`"fmt"`）行为与之前版本完全一致。
- **锁定文件（`ezmk.lock`，1.1.0+）**：版本解析在安装时执行，随后 `ezmk.lock` 钉扎实际安装的精确版本。详见下文 Lockfile 小节。
- **约束无法满足**：若无可满足约束的版本，安装失败并列出所有可用版本。

> **为什么向后兼容？** 不带运算符的条目（`"fmt"`）保持旧的"取最新"语义，已有配置行为不变。锁文件（1.1.0+）在此基础上叠加：安装解析仍遵循 `[depends]` 约束，但写入 `ezmk.lock` 后，记录的是实际安装的精确内容，用于可复现构建。

**示例：**
```toml
[depends]
lib = [
    "fmt",
    "spdlog@1.14.1",
    "catch2^3.6.0",
    "nlohmann_json~3.11"
]
want = [
    "sqlite3",
    "yaml-cpp>=0.8.0"
]
```

同一包名同时出现在 `lib` 和 `want` 中时，`lib` 优先（作为硬性依赖）并 warn 冗余配置。

`want` 包名到宏名的转换规则：
- 大写转换
- `-` / `.` / 空格 → `_`
- 去除其他特殊字符
- 示例：`sqlite3` → `EZMK_LIB_MISS_SQLITE3`，`boost-filesystem` → `EZMK_LIB_MISS_BOOST_FILESYSTEM`

### Lockfile（`ezmk.lock`）（1.1.0+）

`ezmk pkg install` 在项目根目录写入 `ezmk.lock`（TOML 格式），钉扎每个已安装包的**精确版本**、`sha256`、平台与依赖图，实现可复现构建。

- **生成**：每次 `ezmk pkg install` 自动写入/更新。
- **校验**：`ezmk build` 启动时校验：
  - `[compile] deterministic = true` 时——lockfile 缺失或校验失败 → **报错**；lockfile 内容哈希纳入编译缓存签名。
  - 非 deterministic——依赖变化 / sha256 不匹配仅 **警告**。
- **相关 flag**：`ezmk pkg install --locked`（仅按 lockfile 安装，不一致则报错）；`--no-lock`（跳过 lockfile 生成）。
- **请勿手改**：`ezmk.lock` 为自动生成文件——如需变更依赖，编辑 `ezmk.toml` 的 `[depends]` 后重新安装。

```toml
[metadata]
version = 1
generated_by = "ezmk 1.1.0"
toolchain = "gcc"
direct_deps = ["fmt", "spdlog@^1.14.0"]

[[packages]]
name = "spdlog"
version = "1.14.1"
sha256 = "..."
type = "static"
scope = "user"
platform = "windows_x86_64_msvc"
dependencies = []
```

---

## `compile.profile.<name>` 节（0.2.3+）

通过 `--profile <name>` 激活的构建配置。profile 名称必须是字母数字（支持 `-` 和 `_`），不允许空格。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 追加到 `[compile].flags` 之后的编译标志 |
| `msvc_flags` | string[] | 否 | `[]` | 追加到 `[compile].msvc_flags` 之后的 MSVC 专用标志 |
| `macros` | table | 否 | `{}` | 合并到 `[compile.macros]` 的宏定义，同名 key 覆盖 |

合并规则：
- `flags` / `msvc_flags`：profile 标志**追加**到基础标志之后（GCC/Clang 行为：后面的覆盖前面的）
- `macros`：合并到基础宏表，**profile 的 key 覆盖同名基础 key**

示例：

```toml
[compile.profile.debug]
flags = ["-g", "-O0"]
msvc_flags = ["/Zi", "/Od"]

[compile.profile.debug.macros]
DEBUG = "1"

[compile.profile.release]
flags = ["-O3", "-DNDEBUG"]
msvc_flags = ["/O2", "/DNDEBUG"]
```

Profile 默认**不会**自动应用——没有 `default_profile` 时，用户必须显式传 `--profile <name>`。

**`default_profile`（1.2.0+）是唯一的例外。** 若设置了 `[compile].default_profile`（如 `default_profile = "debug"`），裸 `ezmk build` **会**自动应用该 profile。生效优先级：

1. 显式 `--profile <name>`——始终优先
2. `[compile].default_profile`——非空且未传 `--profile` 时应用
3. 基准-only——两者皆无

> **为什么允许例外？** `default_profile` 是**声明的状态**而非隐藏状态——由项目作者显式选定默认构建形态，用户开箱即得合理构建（如可调试），同时仍可用 `--profile release` 覆盖。字段缺省时，旧有的"仅显式"规则依旧成立：profile 永不自动应用，每次调用保持确定可预测。

---

## `link.profile.<name>` 节（0.2.3+）

与 `compile.profile` 对应的链接阶段配置，通过同一个 `--profile <name>` 激活。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 追加到 `[link].flags` 之后的链接标志 |
| `msvc_flags` | string[] | 否 | `[]` | 追加到 `[link].msvc_flags` 之后的 MSVC 专用链接标志 |

合并规则与 compile profile 相同：profile 标志追加到基础标志之后。

示例：

```toml
[link.profile.debug]
flags = []

[link.profile.release]
flags = ["-flto"]
```

---

## `hooks` 节（0.2.3+）

构建生命周期钩子——在编译/链接的关键节点执行 Lua 脚本。钩子脚本接收 `ctx` 表（`ctx.output`、`ctx.project_root`、`ctx.profile`），运行在沙箱 Lua 环境中。脚本不存在 → warn + 跳过（非致命）。仅对用户项目生效，包编译时不执行。

> **为什么沙箱化且非致命？** 钩子会在构建过程中执行任意代码，沙箱（以及包编译时不执行）限制了第三方钩子的破坏范围。脚本缺失只是配置疏漏而非构建失败，因此 warn 后继续。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `pre_build` | string | 否 | `""` | 编译开始前执行的 Lua 脚本路径（相对于项目根目录） |
| `post_build` | string | 否 | `""` | 链接成功后执行的 Lua 脚本路径 |
| `on_failure` | string | 否 | `""` | 编译或链接失败时执行的 Lua 脚本路径 |

示例：

```toml
[hooks]
pre_build = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
```

详见 [`utils.md`](utils.md)（Lua API 参考）和 CLAUDE.md（构建钩子实现细节）。

> **CMake 导出（1.2.0-dev.8+）：** `ezmk project export cmake` 把 `pre_build` / `post_build` 映射为 `add_custom_command(TARGET ... PRE_BUILD/POST_BUILD)`，调用**独立 `ezmk-lua` 运行时**（`find_program(EZMK_LUA ezmk-lua)`），传入 `--project-root`、`--output $<TARGET_FILE:...>` 与导出时的 profile。`ezmk-lua` 在**无沙箱**的 Lua 环境运行钩子（构建沙箱的严格超集），随 `ezmk` 进入所有分发渠道。若 `PATH` 中找不到 `ezmk-lua`，生成的 CMake 回退为 `message(WARNING)`（best-effort——跳过钩子后处理，非致命）。`on_failure` 在 CMake 中无等价物，不导出。为保持 `ezmk build`（沙箱）与导出的 CMake 构建行为一致，钩子应只使用 `ezmk.*` API 子集。

---

## `install` 节（1.1.0+）

控制 `ezmk install` 将构建产物复制到何处。单次调用可用 `--prefix <path>` 覆盖。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `prefix` | string | 否 | `~/.local`（Unix）· `%LOCALAPPDATA%\ezmk`（Windows） | 安装根目录；支持 `~` 展开 |
| `bindir` | string | 否 | `"bin"` | 可执行文件子目录（相对于 `prefix`） |
| `libdir` | string | 否 | `"lib"` | 静态/动态库子目录 |
| `includedir` | string | 否 | `"include"` | 头文件子目录 |
| `sharedir` | string | 否 | `"share"` | 数据文件子目录 |

安装布局：
- `executable` → `<bindir>/`
- `static` → `<libdir>/`
- `shared` → `<bindir>/`（DLL）+ `<libdir>/`（导入库）
- 头文件 → `<includedir>/<name>/`

> **为什么 DLL 进 `bindir`、导入库进 `libdir`？** Windows 下 DLL 在加载时必须能被找到，即在 PATH（`bin`）里；而导入库是链接期产物，应与其他库一起放在 `lib`。

示例：

```toml
[install]
prefix = "~/.local"
bindir = "bin"
libdir = "lib"
includedir = "include"
sharedir = "share"
```

对应 CLI 命令：`ezmk install`（`ezmk project install` 的别名）。

---

## `test` 节（1.1.0+）

`ezmk test`（构建并运行项目测试）的配置。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `dirs` | string[] | 否 | `["test"]` | 测试源文件目录 |
| `framework` | string | 否 | `"catch2"` | 测试框架：`"catch2"` 或 `"ezmk"`（大小写不敏感） |
| `default_profile` | string | 否 | `""` | **1.2.0-dev.12+** 未传 `--profile` 时测试默认应用的 profile（复用 `[compile.profile.<name>]` / `[link.profile.<name>]`，与 `[compile].default_profile` 对称） |
| `include_dirs` | string[] | 否 | `[]` | **1.2.0-dev.12+** 测试专属 `-I` 目录（相对项目根解析、缺失跳过），不污染主构建 |
| `link_targets` | string[] | 否 | `[]` | **1.2.0-dev.12+** 测试专属 `-l` 链接目标，不污染主构建 |
| `flags` | string[] | 否 | `[]` | ⚠️ **已弃用（1.2.0-dev.12+，2.0.0 移除）**——仍生效但输出警告；请改用 `default_profile` + `[compile.profile.<name>]`，或 `include_dirs` / `link_targets` |

示例：

```toml
[test]
dirs = ["test"]
framework = "catch2"
default_profile = "release"        # 1.2.0-dev.12+：默认按 release profile 跑测试
include_dirs = ["test/helpers"]    # 1.2.0-dev.12+：测试专属头文件目录
link_targets = ["pthread"]         # 1.2.0-dev.12+：测试专属链接库
```

对应 CLI 命令：`ezmk test`（`ezmk project test` 的别名），支持 `--framework <name>`、`--filter <pattern>`、`--profile <name>`（1.2.0-dev.12+，覆盖 `default_profile`）、`-V`（详细输出）。

---

## `utils` 节 [version >= 0.2.0]

仅当 `[project].type = "utils"` 时有效。

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `tools` | string[] | 是 | — | 本包提供的工具名列表，每个对应 `utils/<name>.lua` |

示例：

```toml
[utils]
tools = ["cc", "compile-commands"]
```

详见 [`utils.md`](utils.md)。

---

## 完整示例

### 普通项目（0.2.3）

```toml
[project]
name = "myapp"
type = "executable"
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
msvc_flags = []
include_dirs = ["include"]
src_dirs = ["src", "lib"]
ezmk_macros = true

[compile.macros]
DEBUG = ""
VERSION = "0.1.0"
MAX_CONNECTIONS = 64

[compile.profile.debug]
flags = ["-g", "-O0"]

[compile.profile.debug.macros]
DEBUG = "1"

[compile.profile.release]
flags = ["-O3", "-DNDEBUG"]

[link]
flags = []
msvc_flags = []
link_dirs = []
system_target = ["pthread"]

[link.profile.release]
flags = ["-flto"]

[depends]
lib = ["foo", "bar"]
want = ["sqlite3", "zlib"]

[hooks]
pre_build = "scripts/pre.lua"
post_build = "scripts/post.lua"
on_failure = "scripts/fail.lua"
```

### utils 工具包

```toml
[project]
name = "ezmk-cc"
version = "0.1.0"
type = "utils"

[utils]
tools = ["cc", "compile-commands"]
```
