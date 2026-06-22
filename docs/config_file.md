# 配置文件`ezmk.toml`

---

## `project` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `name` | string | 是 | — | 项目名称 |
| `type` | string | 否 | `"executable"` | 项目类型：`"executable"` / `"static"` / `"shared"` / `"utils"` |
| `version` | string | 是 | — | 项目版本，建议 SemVer 格式（如 `"0.1.0"`） |
| `language` | string | 否 | `"C++17"` | 语言标准，格式为 `<语言><版本>`，如 `"C++17"`、`"C11"` |

### `type` 取值说明

| 值 | 产物 | 是否要求 main.cpp |
|----|------|-------------------|
| `"executable"` | 可执行文件 | 是 |
| `"static"` | 静态库 `lib<name>.a` | 否 |
| `"shared"` | 动态库 `lib<name>.dll` / `lib<name>.so` | 否 |
| `"utils"` | 工具包（无编译产物，或 `lib<name>.a`） | 否 |

### `language` 格式

格式为 `<语言><版本>`：
- 语言：`C` 或 `C++`
- 版本：`89` / `99` / `11` / `14` / `17` / `20` / `23`

常见取值：`C++17`（默认）、`C++20`、`C11`、`C99`。

以 `C++` 开头 → 使用 `g++` 编译；以 `C` 开头（非 `C++`）→ 使用 `gcc` 编译。

---

## `compile` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 编译时添加的标志（如 `-Wall`、`-O2`） |
| `include_dirs` | string[] | 否 | `["include"]` | 编译时 `-I` 搜索路径，相对于项目根目录 |

注：旧字段 `include_dir`（单数）已废弃，解析时若遇到可自动映射到 `include_dirs`。

---

## `link` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `flags` | string[] | 否 | `[]` | 链接时添加的标志 |
| `link_dirs` | string[] | 否 | `[]` | 链接时 `-L` 搜索路径，相对于项目根目录 |
| `system_target` | string[] | 否 | `[]` | 需要链接的系统库（如 `"pthread"`、`"m"`） |

---

## `depends` 节

| 字段 | 类型 | 必须 | 默认值 | 说明 |
|------|------|------|--------|------|
| `lib` | string[] | 否 | `[]` | 依赖的库名列表 |

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

详见 `docs/utils.md`。

---

## 完整示例

### 普通项目

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
system_target = ["pthread"]

[depends]
lib = ["foo", "bar"]
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
