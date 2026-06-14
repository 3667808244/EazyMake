# EazyMake 实现计划

## 总体分析

EazyMake 是一个 C/C++ 构建工具，本身用 C++ 编写，基于 GCC/g++（Windows 上通过 MSYS2）。设计文档齐全，当前 `include/` 和 `src/` 为空，需要从零实现。

## 技术选型

| 关注点 | 选择 | 理由 |
|---|---|---|
| C++ 标准 | C++17 | `std::filesystem` 用于跨平台文件操作 |
| TOML 解析 | `toml++` (header-only) | 成熟稳定，C++17 native，无需编译 |
| 文件哈希 | 自写 SHA-256 | 标准算法，约 100 行，零依赖 |
| ZIP 解压 | `miniz` (单文件, public domain) | 嵌入源码编译，无需外部命令 |
| tar.gz 解压 | `miniz` (gzip) + 自写 tar 解析 | tar 格式简单（512 字节块头），约 150 行 |
| HTTP 下载 | `WinHTTP` (Win) / 系统 `curl` 命令回退 | Windows 原生 API，Linux 上几乎所有发行版自带 curl |
| 静态链接 | `-static`（GCC/MinGW） | 单可执行文件，无运行时依赖 |

**目标**：单个 `ezmk`（或 `ezmk.exe`）可执行文件，不依赖任何外部动态库或系统命令。

## 目录结构

```
EazyMake/
  include/
    ezmk/
      cli.hpp
      config.hpp
      project.hpp
      build.hpp
      cache.hpp
      pkg.hpp
      util.hpp
    vendor/
      toml.hpp           # toml++ (header-only, 从 GitHub 引入)
      miniz.h            # miniz zip/gzip 压缩解压
  src/
    main.cpp             # 入口，CLI 路由分发
    cli.cpp              # 命令行参数解析
    config.cpp           # ezmk.toml 解析（基于 toml++）
    project.cpp          # ezmk new：脚手架生成
    build.cpp            # 编译 + 链接引擎
    cache.cpp            # 增量编译缓存（哈希比对 + record.json）
    pkg.cpp              # 包管理（install/remove/search/info + 依赖拓扑排序）
    util.cpp             # 文件系统、SHA-256、tar 解析、日志、跨平台工具
    vendor/
      miniz.c            # miniz 实现（直接编译到可执行文件中）
```

## 模块设计与职责

### `util` — 跨平台工具层
- 平台检测宏：`EZMK_WIN`、`EZMK_EXE_SUFFIX`(`.exe`/空)、`EZMK_OBJ_SUFFIX`(`.obj`/`.o`)
- 路径操作：规范化（统一 `/`）、判断绝对/相对路径
- 文件系统：递归创建目录、递归删除、文件复制、文件哈希
- SHA-256 实现
- tar 格式解析（512 字节块头 → 文件名+内容）
- miniz 封装：`unzip(file, dest_dir)`、`untargz(file, dest_dir)`
- 日志输出（带前缀 `[ezmk]`、`[ezmk error]` 等）

### `cli` — 命令行解析
- 子命令枚举：`New`, `Build`, `Run`, `Clean`, `Install`, `Remove`, `Search`, `Info`
- `Build`/`Run` 附带 `--disable-cache` bool 标志
- `Install` 附带作用域 `-p`/`-u`/`-g`（默认 `-p`）
- `Remove`/`Search`/`Info` 附带作用域 `-p`/`-u`/`-g`（默认 `-pug`，可连用）
- `Install` 附带位置参数 `<pkg_file_or_url>`
- 简单的 argc/argv 遍历解析，不引入第三方 CLI 库

### `config` — TOML 配置解析（基于 toml++）
- 读取并解析 `ezmk.toml`
- 提取 `[project]`：`name`(string)、`type`(string, 当前仅 `"executable"`)
- 提取 `[compile]`：`flags`(string array)、`include_dir`(string array)
- 提取 `[link]`：`flags`(string array)、`system_target`(string array)
- 提取 `[depends]`：`lib`(string array)
- 未知节/键静默忽略（向前兼容）

### `project` — 项目脚手架
- `ezmk new <name>`：
  1. 创建 `<name>/` 目录
  2. 创建 `src/main.cpp`（Hello World 模板）
  3. 创建 `ezmk.toml`（填入项目名、默认编译标志 `-Wall -Wextra -O2`）
  4. 创建空 `README.md`
  5. 创建 `include/`、`build/`、`.ezmk/pkg/`、`.ezmk/temp/`、`.ezmk/cache/` 目录
- 若目标目录已存在，报错并退出

### `build` — 编译引擎
- **扫描**：遍历 `src/` 收集 `.c`/`.cpp`/`.cxx` 文件
- **编译**：对每个源文件调用 `g++ -c <src> -o <obj>`，附带：
  - `[compile].flags`（如 `-Wall -Wextra -O2`）
  - `-I<project>/include`
  - `-I` 每个 `[compile].include_dir`
  - `-I` 每个已安装包的 `include/` 目录
  - `-MMD -MF <depfile>`（生成 Make 依赖文件，供缓存使用）
  - 目标文件输出到 `.ezmk/temp/`，保持目录结构以处理同名文件
- **链接**：`g++ <all .o> <all pkg .a> -o build/<name>[.exe]`，附带：
  - `[link].flags`
  - `-l` 每个 `[link].system_target`
  - `-L` 每个已安装包的库路径
- 调用编译器前检查 `g++` 是否可用

### `cache` — 增量编译缓存
- **哈希**：SHA-256 of 文件内容（二进制读取）
- **依赖解析**：读取 `.d` 文件（Makefile 规则格式），提取头文件路径列表
- **record.json 结构**（按 `docs/@cache.md`）：
  ```json
  {
    "version": 1,
    "compile_options_signature": "sha256...",
    "files": {
      "src/main.cpp": {
        "source_hash": "...",
        "object_file": ".ezmk/cache/obj/main.o",
        "compiler": "g++",
        "compile_opts": ["-Wall", "-O2"],
        "dependencies": [
          {"path": "include/foo.h", "hash": "..."},
          {"path": "/usr/include/iostream", "hash": "..."}
        ],
        "last_build_time": "2025-03-01T12:34:56Z"
      }
    }
  }
  ```
- **缓存命中判断**：
  1. 源文件哈希相同？
  2. 编译选项签名相同？
  3. 所有依赖头文件哈希相同且数量相同？
  4. 全部命中 → 跳过编译；否则 → 重新编译并更新 record
- **原子写入**：先写 `.tmp` 再 `rename`
- **`--disable-cache`**：跳过缓存检查，全部重编译，但结果依然更新 record
- **`ezmk clean`**：删除 `.ezmk/cache/` 整个目录

### `pkg` — 包管理
- **作用域路径**：
  - 全局：`<ezmk_install_dir>/pkg/`
  - 用户：`~/.local/ezmk/pkg/`（Windows: `%LOCALAPPDATA%/ezmk/pkg/`）
  - 项目：`<project_dir>/.ezmk/pkg/`
- **安装流程** (`ezmk install [-p|-u|-g] <pkg_file_or_url>`)：
  1. 若参数是 URL（含 `://` 或 `://`省略时以 `xxx.xxx/` 形式），用 HTTP 下载到临时文件（默认补 `https://`）
  2. 解压（`.zip` → miniz；`.tar.gz` → miniz+tar解析）
  3. 验证包结构（存在 `include/`、`src/`、`ezmk.toml`）
  4. 解析 `[depends].lib`，递归检查/安装缺失的依赖到相应作用域
  5. 拓扑排序全部依赖包，检测循环依赖
  6. 按依赖顺序逐个编译包为 `<pkg_name>.a` 静态库
  7. 复制包文件到作用域安装目录
  8. 全局安装或覆盖文件时二次确认
- **移除** (`ezmk remove [-p|-u|-g] <pkg>`)：
  - 按 `-p` → `-u` → `-g` 顺序查找，删除第一个匹配的包目录
- **查找** (`ezmk search [-p|-u|-g] <pkg>`)：
  - 按 `-p` → `-u` → `-g` 顺序查找，列出匹配的包路径
- **信息** (`ezmk info [-p|-u|-g] <pkg>`)：
  - 读取找到的包的 `ezmk.toml` 并显示关键信息

### `main` — 入口 + 路由
- 解析命令行 → 分发到对应模块
- 顶层异常捕获，友好报错

## 实现阶段

### 第一阶段：骨架（util + vendor + cli + config + project + main）

**目标**：`ezmk new` 能生成完整项目，`ezmk build` 能打印解析出的配置。

- 引入 `toml.hpp`（header-only，放到 `include/vendor/`）
- 引入 `miniz.h` / `miniz.c`（放到 `include/vendor/` 和 `src/vendor/`）
- 编写 `util`（SHA-256、tar 解析、文件操作、平台宏）
- 编写 `cli`（argc/argv 解析）
- 编写 `config`（toml++ 封装）
- 编写 `project`（脚手架）
- 编写 `main.cpp`（路由骨架）

### 第二阶段：构建引擎（build）

**目标**：`ezmk build` 能编译并链接出可执行文件。

- 编写 `build`（扫描 → 编译 → 链接）
- 正确传递编译/链接标志和 include/lib 路径
- 修改 `main.cpp` 对接 build 模块

### 第三阶段：增量缓存（cache）

**目标**：重复 build 跳过未改动文件，`--disable-cache` 和 `ezmk clean` 可用。

- 编写 `cache`（SHA-256 哈希、.d 依赖解析、record.json 读写、缓存判断）
- 集成到 build 流程
- 实现 `ezmk clean`

### 第四阶段：包管理（pkg）

**目标**：`ezmk install`/`remove`/`search`/`info` 完整可用。

- 编写 `pkg`（下载、解压、验证、依赖拓扑排序、编译 .a、安装/移除/查找/信息）
- 全局安装和覆盖文件二次确认

### 第五阶段：`ezmk run` + 收尾

**目标**：完整的命令行体验 + 静态链接单文件产物。

- `ezmk run`：先 build，成功则 `exec` 执行产物
- 确保 `-static` 链接，验证生成的 `ezmk.exe` 不依赖外部 DLL

## 关键设计决策

### 编译命令模板

```
g++ -std=c++17 -c <src> -o <obj> \
  <compile.flags> \
  -I<project_include> -I<pkg_includes>... \
  -MMD -MF <depfile>
```

链接命令：
```
g++ <all .o> <all pkg .a> -o build/<name>[.exe] \
  <link.flags> \
  -L<pkg_lib_dirs> -l<system_targets>
```

### 依赖拓扑排序（Kahn 算法）

```
输入：包名列表
1. 读取每个包的 [depends].lib，构建邻接表 + 入度表
2. 队列初始化为入度为 0 的包
3. BFS：每弹出一个包，将其后继的入度减 1，入度为 0 则入队
4. 若弹出数量 != 总包数 → 存在循环依赖，报错
```

### 跨平台适配点

| 事项 | Windows (MinGW) | Linux |
|---|---|---|
| 可执行文件 | `name.exe` | `name` |
| 目标文件 | `.obj` | `.o` |
| 静态库 | `lib<name>.a` | `lib<name>.a` |
| 编译器 | `g++.exe` | `g++` |
| 用户安装目录 | `%LOCALAPPDATA%/ezmk/pkg/` | `~/.local/ezmk/pkg/` |
| HTTP 下载 | WinHTTP API | `curl` 命令（回退） |
| 路径风格 | MSYS2 接受 `/`，内部统一 `/` | `/` |

### 编译器检测

构建前检查 `g++` 是否在 PATH 中，不可用时给出明确提示（如 "请安装 MSYS2 并安装 gcc 包"）。

---

## 当前进度（2026-06-14）

### 已完成：框架搭建 + 依赖引入

项目骨架已就绪，所有头文件接口声明完整，源文件均为空实现（`// TODO`），`build.sh` 可编译通过。

**目录结构：**

```
EazyMake/
  include/
    ezmk/
      util.hpp      # 跨平台工具层接口
      cli.hpp       # CLI 解析接口
      config.hpp    # ezmk.toml 解析接口
      project.hpp   # 项目脚手架接口
      build.hpp     # 构建引擎接口
      cache.hpp     # 增量缓存接口
      pkg.hpp       # 包管理接口
    vendor/
      toml.hpp          # toml++ v3.4.0 (header-only, 485KB)
      miniz.h           # miniz 2.2.0 主头文件
      miniz_export.h    # DLL export stub (静态链接用)
      miniz_common.h    # miniz 公共定义
      miniz_tdef.h      # miniz 压缩
      miniz_tinfl.h     # miniz 解压
      miniz_zip.h       # miniz ZIP 读写
  src/
    main.cpp         # 入口（CLI 路由为空）
    util.cpp         # 空实现
    cli.cpp          # 空实现
    config.cpp       # 空实现
    project.cpp      # 空实现
    build.cpp        # 空实现
    cache.cpp        # 空实现
    pkg.cpp          # 空实现
    vendor/
      miniz.c            # miniz 主实现
      miniz_tdef.c       # miniz 压缩实现
      miniz_tinfl.c      # miniz 解压实现
      miniz_zip.c        # miniz ZIP 读写实现
  build/
    ezmk.exe         # 编译产物（当前为空壳，无功能）
  build.sh           # 构建脚本（自动检测 MSYS2/Linux，-static 链接）
```

**构建方式：**

```bash
bash build.sh
# 产物: build/ezmk.exe (Windows/MSYS2) 或 build/ezmk (Linux)
```

**下一步：** 第一阶段实现——编写 `util.cpp`、`cli.cpp`、`config.cpp`、`project.cpp`、`main.cpp` 的具体实现，达成 `ezmk new` 生成项目 + `ezmk build` 打印配置。

---

## 当前进度（2026-06-14，更新）

### Phase 1 ✓ — 骨架

- `util.cpp` — 日志、文件系统（基于 std::filesystem）、SHA-256 自实现、tar 格式解析（512 字节块头）、ZIP 解压（miniz）、tar.gz 解压（miniz tinfl + 自写 tar 解析）、HTTP 下载（Windows: WinHTTP，Linux: curl 回退）、进程执行（Windows: CreateProcess，Linux: popen）、跨平台路径规范化
- `cli.cpp` — 完整 argc/argv 解析，支持全部子命令、`--disable-cache`、`-p`/`-u`/`-g` 及组合（`-pug`）
- `config.cpp` — 基于 toml++ v3.4.0 解析/写入 ezmk.toml（四个 section：project/compile/link/depends）
- `project.cpp` — `ezmk new` 脚手架（目录结构 + 模板 main.cpp + ezmk.toml + 空 README.md）
- `main.cpp` — CLI 路由分发全部 8 个子命令

### Phase 2 ✓ — 构建引擎

- `build.cpp` — 完整的编译链接流程
  - 扫描 `src/` 收集 `.c`/`.cpp`/`.cxx`
  - 对每个源文件调用 `g++ -c`，传递 `[compile].flags`、`-I` include 目录、`-MMD -MF` 生成 `.d` 依赖文件
  - `.o` 文件输出到 `.ezmk/temp/`，保持源码目录结构
  - 链接：`g++ <all .o> -o build/<name>[.exe]`，传递 `[link].flags` 和 `-l<system_targets>`
  - 找不到编译器时给出提示

### Phase 3 ✓ — 增量编译缓存

- `cache.cpp` — 按 `docs/@cache.md` 规范完整实现
  - SHA-256 哈希源码和所有依赖头文件
  - `.d` 文件解析（处理 CRLF、续行符 `\`）
  - `record.json` 读写（含自定义最小 JSON 解析器/写入器），原子写入（`.tmp` → `rename`）
  - 缓存命中三条件：源码哈希相同、编译选项签名相同、所有记录的头文件当前哈希匹配
  - 全局编译选项签名改变 → 清空全部缓存
  - `--disable-cache` 强制全量重编译但依然更新缓存
- `build.cpp` 集成：编译前查缓存，命中则复制 `.o` 跳过编译，未命中则编译并更新 record

### Phase 4 ✓ — 包管理

- `pkg.cpp` — 完整包管理
  - 作用域路径：项目 `.ezmk/pkg/`、用户 `~/.local/ezmk/pkg/`（Win: `%LOCALAPPDATA%`）、全局 `<exe_dir>/pkg/`
  - `install`：下载（URL 省略协议默认 `https://`）→ 解压（tar.gz/ZIP）→ 验证结构 → 依赖递归检查 → Kahn 拓扑排序 → 逐包编译为 `.a` → 复制安装
  - `remove`：按作用域顺序查找删除
  - `search`：按作用域顺序查找返回路径
  - `info`：显示包名、类型、位置、编译标志、依赖
  - 安全确认：全局安装、覆盖文件时 `[y/N]` 确认
  - 循环依赖检测
- `build.cpp` 集成：构建时自动检测 `.ezmk/pkg/` 下的已安装包，添加 `-I` 和链接 `.a`

### 全部命令状态

| 命令 | 状态 |
|---|---|
| `ezmk new <name>` | 完整可用 |
| `ezmk build [--disable-cache]` | 完整可用（含增量缓存） |
| `ezmk run [--disable-cache]` | 完整可用 |
| `ezmk clean` | 完整可用 |
| `ezmk install [-p\|-u\|-g] <file_or_url>` | 完整可用 |
| `ezmk remove [-p\|-u\|-g] <pkg>` | 完整可用 |
| `ezmk search [-p\|-u\|-g] <pkg>` | 完整可用 |
| `ezmk info [-p\|-u\|-g] <pkg>` | 完整可用 |

### 构建

```bash
bash build.sh
# 产物: build/ezmk.exe (MSYS2/MinGW 静态链接) 或 build/ezmk (Linux)
```

### 偶后事项

- ~~包编译时的依赖链 include 传递~~ ✓ (2026-06-14)
- ~~`ezmk.toml` `[compile]` 的 `include_dir` 相对路径处理~~ ✓ (2026-06-14)
- ~~包编译缓存（当前包的 `.a` 不参与项目缓存体系）~~ ✓ (2026-06-14)
- 系统包源 / 中央仓库（当前每次需手动提供包文件或 URL）

### 缓存体系（2026-06-14 更新）

**包编译缓存**（新增）：

- 每个包的 `build/` 目录下存储 `.pkg_cache.json`，结构复用项目缓存的 `CacheRecord`
- `compile_package()` 流程：
  1. 加载 `<pkg_dir>/build/.pkg_cache.json`
  2. 对比全局编译选项签名，变化则清空全部条目
  3. 对每个源文件：查缓存（源码哈希 + 选项签名 + 依赖头文件哈希），命中跳过编译，未命中则编译并生成 `.d` 文件
  4. 依赖路径标准化为 pkg_dir 相对路径，使缓存在包重新安装后仍有效
  5. 所有文件命中 + `.a` 存在 → 跳过归档步骤
- `check_cache()` 新增 `proj_root` 参数的重载，支持非 cwd 根目录
- `load_record()`/`save_record()` 新增自定义路径重载
- `compile_options_signature()` 新增 `extra_includes` 参数重载

**bug 修复**：
- `install()` 中 `std::rand()` 未播种 → 改用 `std::mt19937` 随机引擎避免临时目录名冲突
- `install()` 异常时未清理临时目录 → 添加 try/catch 确保清理 staging 目录
- 运算符优先级括号警告修复

### 第五阶段：收尾 ✓ (2026-06-14)

- 零警告编译（`-Wall -Wextra`）
- 静态链接验证：仅依赖 Windows 系统 DLL（ntdll, kernel32, WINHTTP 等），无 libstdc++/libgcc 运行时依赖
- 全部 8 个子命令端到端测试通过
- 全部 CLI 标志测试通过（`--help`/`-h`/`help`、`--disable-cache`、`-p`/`-u`/`-g`/`-pug`）
- 边缘情况测试通过（未知命令、缺少 ezmk.toml、目录已存在、无项目名等）
- 多源文件 + 头文件依赖变更 → 缓存正确失效并重编译
- 包依赖链 include 传递 + 编译缓存 + 项目缓存联动验证通过

### 剩余事项

- 系统包源 / 中央仓库（当前每次需手动提供包文件或 URL）
