# EazyMake 0.1.2 更新计划

---

## 一、命令行参数改为子命令格式

将 0.1.1 的扁平命令结构重构为二级子命令结构，按功能域分组为 `project`、`pkg`、`repo` 三个子命令组。

### 1.1 project 子命令

```bash
ezmk project new <project_name>
ezmk project build [--disable-cache]
ezmk project run [--disable-cache]
ezmk project clean
```

| 命令 | 说明 |
|------|------|
| `new <name>` | 在当前目录下创建 `<name>/` 项目目录，生成 `ezmk.toml`、`src/main.cpp`、`include/`、`README.md` |
| `build` | 扫描 `src/` 下所有源文件，按缓存策略增量编译，最后链接生成可执行文件到 `build/` |
| `run` | 等同于 `build` + 执行 `build/<name>`（或 `build/<name>.exe`） |
| `clean` | 删除 `.ezmk/cache/` 目录（含 `record.json` 和 `obj/`），以及 `.ezmk/temp/` |

`--disable-cache` 标志：忽略已有缓存记录，强制重新编译所有源文件，但编译完成后仍会更新缓存。

### 1.2 pkg 子命令

```bash
ezmk pkg install [-p|-u|-g] <pkg_file_or_url>
ezmk pkg remove [-p|-u|-g] <pkg>
ezmk pkg search [-p|-u|-g] <pkg>
ezmk pkg info [-p|-u|-g] <pkg>
```

作用域参数：

| 参数 | 作用域 | 安装路径 |
|------|--------|----------|
| `-p` | 项目 | `<project_dir>/.ezmk/pkg/` |
| `-u` | 用户 | `~/.local/ezmk/pkg/` |
| `-g` | 全局 | `<ezmk_install_dir>/pkg/` |

规则：
- `install` 仅支持单个作用域参数（`-p`、`-u`、`-g` 三选一），默认 `-p`
- `remove` / `search` / `info` 支持组合参数（如 `-pug` 表示三个作用域都操作），默认 `-pug`
- 搜索/查找时按 `-p` → `-u` → `-g` 顺序，先找到的为准

`<pkg_file_or_url>` 说明：
- 可以是本地文件路径（`.zip` 或 `.tar.gz`）
- 可以是 URL（协议可省略，默认 `https://`）
- 包文件结构须符合 `docs/pkg.md` 定义的格式（含 `include/`、`src/`、`ezmk.toml`）

### 1.3 repo 子命令（占位）

```bash
# 以下子命令暂不实现，仅占位，留待后续版本
ezmk repo add [-p|-u|-g] <address>
ezmk repo update
ezmk repo remove [-p|-u|-g] <name>
ezmk repo list
```

设计意图：`repo` 用于管理包来源仓库。EazyMake 不设中央仓库，用户自行注册本地或远程仓库地址。`repo add` 注册一个仓库路径/URL，`repo update` 刷新仓库索引，`repo remove` 取消注册，`repo list` 列出已注册仓库。

---

## 二、增加编译模式

`project.type` 字段在现有 `"executable"` 基础上新增 `"static"` 和 `"shared"`。

### 2.1 三种模式对比

| type | 产物 | 是否需要 main.cpp | 编译/链接方式 |
|------|------|-------------------|---------------|
| `executable` | `build/<name>`（Win 下 `<name>.exe`） | **是** | `g++` 编译 `.cpp` → `.o`，再链接为可执行文件 |
| `static` | `build/lib<name>.a` | 否 | `g++` 编译 `.cpp` → `.o`，`ar rcs` 打包为静态库 |
| `shared` | `build/lib<name>.dll`（Win）/ `lib<name>.so`（Unix） | 否 | `g++ -shared -fPIC` 编译，链接为动态库 |

### 2.2 各模式详细行为

**executable（可执行文件）**
- 必须存在 `src/main.cpp`（或 `src/main.c`），否则报错
- 链接时自动链接 `[depends]` 中声明的所有依赖库
- 链接系统库（`[link]` 节中的 `system_target`）

**static（静态库）**
- 不要求 `main.cpp`，编译 `src/` 下所有源文件为目标文件
- 使用 `ar rcs` 将所有 `.o` 打包为 `lib<name>.a`
- 编译时默认添加 `-fPIC`（便于被其他库链接）
- 其 `[depends]` 中声明的依赖在链接阶段处理：当其他可执行文件依赖此静态库时，递归链接所有传递依赖

**shared（动态库）**
- 不要求 `main.cpp`，编译 `src/` 下所有源文件为目标文件
- 使用 `g++ -shared` 链接为动态库
- 编译时必须加 `-fPIC`
- Windows 平台生成 `.dll`（含导入库 `.dll.a`），Unix 平台生成 `.so`

---

## 三、增加配置字段

### 3.1 `project.version`（必须）

```toml
[project]
version = "0.1.0"
```

- **必须字段**，`ezmk project new` 自动生成默认值 `"0.1.0"`
- 解析 `ezmk.toml` 时若缺失该字段，报错并提示补充
- 格式建议为 `<major>.<minor>.<patch>`（SemVer），但不做强校验（非空即可）
- 用途：包管理时标识版本、`ezmk pkg info` 展示

### 3.2 `project.language`（选填，默认 `"C++17"`）

```toml
[project]
language = "C++17"
```

格式为 `<语言><版本>`，解析规则：

| 配置值 | 语言 | 标准版本 | 编译器 | 编译标志 |
|--------|------|----------|--------|----------|
| `C++17`（默认） | C++ | C++17 | `g++` | `-std=c++17` |
| `C++20` | C++ | C++20 | `g++` | `-std=c++20` |
| `C++23` | C++ | C++23 | `g++` | `-std=c++23` |
| `C++14` | C++ | C++14 | `g++` | `-std=c++14` |
| `C++11` | C++ | C++11 | `g++` | `-std=c++11` |
| `C17` | C | C17 | `gcc` | `-std=c17` |
| `C11` | C | C11 | `gcc` | `-std=c11` |
| `C99` | C | C99 | `gcc` | `-std=c99` |

解析逻辑：
1. 若以 `C++` 开头 → 语言为 C++，编译器选 `g++`
2. 若以 `C` 开头且不是 `C++` → 语言为 C，编译器选 `gcc`
3. 版本号部分必须为合法值（`89`/`99`/`11`/`14`/`17`/`20`/`23`），否则报错
4. 缺省时默认 `C++17`

### 3.3 `compile.include_dirs`（选填，默认 `["include"]`）

```toml
[compile]
include_dirs = ["include", "third_party/foo/include"]
```

- 替代旧字段 `compile.include_dir`（注意：旧字段为单数，新字段为复数）
- 类型：字符串数组，每个元素为相对于项目根目录的路径
- 默认值 `["include"]`（保持向后兼容）
- 编译时每个路径展开为 `-I<path>` 传给编译器

### 3.4 `link.link_dirs`（选填，默认 `[]`）

```toml
[link]
link_dirs = ["third_party/foo/lib"]
```

- 类型：字符串数组，每个元素为相对于项目根目录的路径
- 默认值 `[]`（空列表）
- 链接时每个路径展开为 `-L<path>` 传给链接器
- 用于指定第三方预编译库的搜索路径

### 3.5 完整 `ezmk.toml` 示例（0.1.2）

```toml
[project]
name = "myapp"
type = "executable"
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]

[link]
flags = []
link_dirs = []
system_target = []

[depends]
lib = []
```

---

## 四、字段命名调整

为保持一致性，0.1.2 对旧字段名做如下调整：

| 旧字段名 | 新字段名 | 说明 |
|----------|----------|------|
| `compile.include_dir` | `compile.include_dirs` | 改为复数，与 `link.link_dirs` 风格统一 |

注：`link.system_target` 保持不变（`system_target` 作为集合名词可接受单数形式）；若解析时遇到旧字段名 `include_dir`（单数），可做兼容处理，自动映射到 `include_dirs`。

---

## 五、默认项目模板更新

`ezmk project new <name>` 生成的 `ezmk.toml` 需同步新字段：

```toml
[project]
name = "{project_name}"
type = "executable"
version = "0.1.0"
language = "C++17"

[compile]
flags = ["-Wall", "-Wextra", "-O2"]
include_dirs = ["include"]

[link]
flags = []
link_dirs = []
system_target = []

[depends]
lib = []
```

`src/main.cpp` 和 `README.md` 内容与 0.1.1 保持一致（见 `docs/default_craete.md`）。

---

## 六、相关文档需同步更新

| 文档 | 更新内容 |
|------|----------|
| `docs/config_file.md` | 新增 `version`、`language`、`include_dirs`、`link_dirs` 字段说明；更新 `type` 可选值 |
| `docs/default_craete.md` | 更新 `ezmk.toml` 默认模板 |
| `docs/pkg.md` | 补充 `pkg_file_or_url` 的 URL 格式说明 |
| `CLAUDE.md` | 更新命令行表格（子命令格式）、配置节说明 |

---

## 七、实现要点

### 7.1 CLI 解析

入口 `main()` 解析 `argv`：
1. 第一参数为子命令组（`project` / `pkg` / `repo`），不匹配则打印帮助
2. 第二参数为具体操作（`new` / `build` / `run` / `clean` / `install` / `remove` / `search` / `info` / `add` / `update` / `list`）
3. 后续参数为该操作的选项和值
4. `repo` 组直接打印 "not implemented yet" 并退出

### 7.2 配置解析

- 读取 `ezmk.toml`，解析四个节：`[project]`、`[compile]`、`[link]`、`[depends]`
- `project.version` 缺失 → 报错退出
- `project.language` 缺失 → 默认 `C++17`
- `project.type` 缺失 → 默认 `executable`
- `compile.include_dirs` 缺失 → 默认 `["include"]`
- `link.link_dirs` 缺失 → 默认 `[]`
- 兼容旧字段 `compile.include_dir`（单数），自动映射

### 7.3 构建逻辑

```
1. 解析 ezmk.toml
2. 根据 project.language 选择编译器（g++ 或 gcc）
3. 扫描 src/ 下所有源文件（*.c, *.cpp, *.cxx）
4. 对每个源文件：
   a. 检查缓存（见 docs/@cache.md）
   b. 若缓存命中 → 跳过
   c. 若缓存未命中 → 编译为 .o，更新缓存记录
5. 根据 project.type：
   - executable: g++ *.o -o build/<name> + 链接依赖
   - static: ar rcs build/lib<name>.a *.o
   - shared: g++ -shared *.o -o build/lib<name>.dll + 链接依赖
```

### 7.4 包安装逻辑

```
1. 判断参数是本地路径还是 URL
2. 本地：直接读取；URL：下载到 .ezmk/temp/
3. 解压到临时目录，校验结构（include/ + src/ + ezmk.toml）
4. 解析包内 ezmk.toml 的 [depends]，递归检查依赖是否已安装
5. 编译依赖链（从叶子到根），每个依赖输出 lib<name>.a
6. 将 .a 文件和 include/ 头文件复制到对应作用域的 pkg/ 目录
```

### 7.5 `--disable-cache` 处理

- 设置该标志后，跳过 `record.json` 读取，所有源文件强制重编译
- 编译完成后正常更新缓存（保证下次构建可受益）

---

## 八、安全性注意事项

回顾 `docs/@safety.md`，0.1.2 中需注意的安全点：

1. **全局安装二次确认**：`ezmk pkg install -g` 时提示 "Installing globally. Continue? [y/N]"
2. **覆盖确认**：安装时若目标路径已存在同名包，提示 "Package already exists. Overwrite? [y/N]"
3. **URL 下载警告**：从 URL 下载包时，显示下载地址并要求确认

---

## 九、实现进度

### ✅ 已完成（2026-06-15）

#### 9.1 设计文档更新

| 文档 | 状态 |
|------|------|
| `docs/config_file.md` | ✅ 表格化、新增 `version`/`language`/`include_dirs`/`link_dirs`，修正 `systam_target` 拼写 |
| `docs/default_craete.md` | ✅ 模板同步新字段 |
| `docs/pkg.md` | ✅ 新增 URL 格式说明、下载流程 |
| `CLAUDE.md` | ✅ CLI 改为子命令格式、config 节同步新字段 |

#### 9.2 配置层 (`config.hpp` / `config.cpp`)

- ✅ `ProjectSection` 新增 `version`（必须）、`language`（默认 `"C++17"`）
- ✅ `LinkSection` 新增 `link_dirs`
- ✅ 新增 `LanguageInfo` 结构体和 `parse_language()` 函数（C++17 → `g++ -std=c++17`，C11 → `gcc -std=c11`）
- ✅ `parse_config()` 读取新字段，`project.version` 缺失时抛错
- ✅ `include_dirs` 兼容旧字段 `include_dir`（单数），默认值 `["include"]`
- ✅ `write_default_config()` 输出完整 0.1.2 模板（含 `version`/`language`/`include_dirs`/`link_dirs`）

#### 9.3 CLI 层 (`cli.hpp` / `cli.cpp` / `main.cpp`)

- ✅ `Command` 枚举改为二级子命令：`ProjectNew`/`ProjectBuild`/`ProjectRun`/`ProjectClean`/`PkgInstall`/`PkgRemove`/`PkgSearch`/`PkgInfo`/`RepoAdd`/`RepoUpdate`/`RepoRemove`/`RepoList`
- ✅ `parse()` 解析 `ezmk <group> <action> [args]` 格式
- ✅ `Repo*` 命令在 `main.cpp` 打印 "not yet implemented"
- ✅ `print_usage()` 输出新的子命令帮助信息

#### 9.4 构建层 (`build.hpp` / `build.cpp`)

- ✅ 根据 `project.language` 选择编译器（`g++` 或 `gcc`）和 `-std=` 标志
- ✅ `executable`：链接为可执行文件
- ✅ `static`：`ar rcs` 打包为 `lib<name>.a`，不要求 `main.cpp`
- ✅ `shared`：`-fPIC` 编译 + `-shared` 链接为 `lib<name>.dll`/`.so`，不要求 `main.cpp`
- ✅ 链接时添加 `link.link_dirs` 的 `-L` 路径

#### 9.5 包管理 (`pkg.cpp`)

- ✅ `compile_package()` 改为使用 `parse_language()` 获取编译器和标准，不再硬编码 `g++ -std=c++17`

#### 9.6 `--version` 全局标志（2026-06-15）

- ✅ `ezmk --version` / `ezmk version` / `ezmk -V` 三种形式均打印 `EazyMake 0.1.2`
- ✅ 在 CLI 解析早期截获，不支持与其他子命令组合

#### 9.7 `ezmk project new --type` 参数（2026-06-15）

- ✅ `ezmk project new <name> --type executable|static|shared`
- ✅ 默认 `executable`，非法值报错
- ✅ `create_project()` / `write_default_config()` 链路完整传递 type

#### 9.8 Bug 修复：作用域标志误匹配（2026-06-15）

- 🐛 `parse_scope_flags()` 未检查前导 `-`，导致含 `p`/`u`/`g` 字符的包名（如 `testpkg`）被误认为作用域标志
- ✅ 修复：在函数入口添加 `if (a.empty() || a[0] != '-') return false;`

#### 9.9 包信息扩展（2026-06-15）

- ✅ `ezmk pkg info` 输出新增字段：
  - `Version` — 从包内 `ezmk.toml` 的 `project.version` 读取
  - `Language` — 语言标准
  - `Scope` — 包所在作用域（project/user/global）
  - `Installed` — 安装时间（目录最后修改时间）
  - `Include dirs` — 编译包含目录列表
  - `Artifacts` — 构建产物（`.a`/`.dll`/`.so`），无产物时显示 `(none)`
- ✅ 编译标志和依赖为空时显示 `(none)` 而非留空

### 🔜 待完成

| 项目 | 说明 |
|------|------|
| `repo` 子命令真实实现 | 当前仅占位，需实现 repo 目录扫描和索引 |
