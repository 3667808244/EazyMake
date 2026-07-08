# utils 工具系统 [version >= 0.2.0]

`ezmk utils <name> [args...]` 提供一个可扩展的插件式工具入口。工具以 **Lua 脚本** 形式编写，以**包**的形式分发和安装，不硬编码任何具体工具到 C++ 代码中。

---

## 设计动机

| 对比维度 | Shell (.sh/.ps1/.bat) | Lua 内嵌 |
|---|---|---|
| 跨平台 | 需为每个平台编写不同脚本 | 一份 `.lua` 到处运行 |
| 依赖 | 依赖 bash / powershell / cmd | 无外部依赖（lua 编译进 ezmk） |
| 功能边界 | 只能调用外部命令 | 可调用 ezmk 暴露的 C++ API + 外部命令 |
| 错误处理 | 各平台语法不同，易出错 | 统一的 `pcall` / `error()` |
| 分发 | 文本脚本，但需注意换行符 | 纯文本，天然跨平台 |

---

## 包结构

utils 工具遵循与普通包相同的结构（参见 `pkg.md`），并增加 `utils/` 目录：

```
<utils_pkg>/
    ezmk.toml         # [project] 节声明 name, version, type = "utils"
    utils/            # Lua 脚本（必需）
        <name>.lua    # 入口脚本（文件名即工具名）
        lib/          # 可选：Lua 辅助模块
    include/          # 可选：C/C++ 头文件
    src/              # 可选：C/C++ 源码（编译为 .a，供 Lua 通过 FFI 调用）
```

### `ezmk.toml` 格式

```toml
[project]
name = "ezmk-cc"
version = "0.1.0"
type = "utils"              # 新增类型：标记此包为工具包

[utils]
tools = ["cc", "compile-commands"]   # 本包提供的工具名列表
```

| 字段 | 类型 | 必须 | 说明 |
|---|---|---|---|
| `[project].type` | string | 是 | 取值 `"utils"`，标记为工具包 |
| `[utils].tools` | string[] | 是 | 提供的工具名列表，每个对应 `utils/<name>.lua` |

- 若包同时包含 `src/` 和 `utils/`：安装时先编译 `src/` → `build/*.a`，再注册 tools
- 若包仅有 `utils/`（无 `src/`）：安装时跳过编译，仅解压到目标路径

---

## Lua 入口脚本约定

每个工具入口脚本（如 `utils/cc.lua`）需实现：

```lua
-- 可选：返回帮助文本。ezmk 在用户传 -h / --help 时调用
function help()
    return [[
usage: ezmk utils cc [options]

Generate compile_commands.json for the current project.

Options:
  -o, --output  <path>   Output path (default: <project_root>/compile_commands.json)
  -h, --help             Show this help
]]
end

-- 必选：入口函数。args 为字符串数组（ezmk utils <name> 之后的部分）
-- 返回整数 exit code（0 = 成功），或调用 error("msg") 让 ezmk 处理
function run(args)
    -- ...
    return 0
end
```

### 调用流程

1. ezmk 在已安装包中找到 `utils/<name>.lua`
2. 加载并执行脚本，注册 `help()` 和 `run()`
3. 若 args 包含 `-h` / `--help` → 调用 `help()` 并输出返回值
4. 否则调用 `run(args)`，透传剩余参数
5. `run()` 返回整数 → 作为进程 exit code；`error()` → ezmk 打印错误并返回 1

---

## 查找顺序

`ezmk utils <name>` 按以下优先级查找 `<name>.lua`：

| 优先级 | 路径 | 说明 |
|---|---|---|
| 1（项目级） | `<project>/.ezmk/pkg/*/utils/<name>.lua` | 扫描项目作用域所有已安装包 |
| 2（用户级） | `~/.local/ezmk/pkg/*/utils/<name>.lua` | 扫描用户作用域所有已安装包 |
| 3（全局级） | `<ezmk_install_dir>/pkg/*/utils/<name>.lua` | 扫描全局作用域所有已安装包 |

- 每个作用域内按包名字典序扫描；同一作用域找到第一个匹配即停止
- 查找基于文件系统：直接检测 `utils/<name>.lua` 是否存在（不依赖 `ezmk.toml` 中的 `[utils].tools`，toml 声明仅用于 `pkg info` 展示）

---

## ezmk Lua API

ezmk 向 Lua 脚本暴露一个 `ezmk` 全局模块：

### 项目信息（只读）

```lua
ezmk.project_root()       -- → string   项目根目录绝对路径
ezmk.project_name()       -- → string   项目名（ezmk.toml [project].name）
ezmk.project_type()       -- → string   "executable" | "static" | "shared" | "utils"
ezmk.project_config()     -- → string   ezmk.toml 绝对路径
ezmk.build_dir()          -- → string   build/ 目录绝对路径
```

### 编译选项（只读）

```lua
ezmk.compile_flags()      -- → {string...}  [compile].flags 数组
ezmk.include_dirs()       -- → {string...}  [compile].include_dirs 绝对路径数组
ezmk.link_flags()         -- → {string...}  [link].flags 数组
ezmk.link_dirs()          -- → {string...}  [link].link_dirs 绝对路径数组
```

### 文件系统

```lua
ezmk.list_sources()       -- → {string...}  src/ 下所有源文件绝对路径
ezmk.file_exists(path)    -- → bool
ezmk.file_read(path)      -- → string | nil, err
ezmk.file_write(path, content)  -- → bool, err
```

- 相对路径以项目根目录为基准
- `file_write` 拒绝写入项目根目录之外的绝对路径

### 进程执行

```lua
ezmk.run(cmd)             -- → {exit_code=int, stdout=string, stderr=string}
ezmk.run_capture(cmd)     -- → string   stdout 内容，失败则 error()
```

- `run_capture` 在 exit code ≠ 0 时抛出 Lua error，包含 stderr 信息

### 日志输出

```lua
ezmk.info(msg)            -- 绿色 [ezmk] 前缀
ezmk.warn(msg)            -- 黄色 [ezmk warn] 前缀
ezmk.error(msg)           -- 红色 [ezmk error] 前缀
```

### 路径工具

```lua
ezmk.pkg_dir()            -- → string   当前工具所在包的根目录
ezmk.temp_dir()           -- → string   .ezmk/temp/ 目录
ezmk.cache_dir()          -- → string   .ezmk/cache/ 目录
```

### JSON

```lua
ezmk.json_encode(table)   -- → string   将 Lua table 编码为 JSON
ezmk.json_decode(string)  -- → table    将 JSON 字符串解码为 Lua table
```

---

## 安装与卸载

utils 包使用与普通库包完全相同的命令（参见 `pkg.md`）：

```bash
# 从本地文件安装
ezmk pkg install -u ./ezmk-cc-0.1.0.zip

# 从 URL 安装
ezmk pkg install -g https://example.com/tools/ezmk-cc-0.1.0.tar.gz

# 从已注册仓库安装（按名称）
ezmk pkg install -p ezmk-cc

# 卸载
ezmk pkg remove -u ezmk-cc
```

- 安装后，包内 `[utils].tools` 声明的全部工具立即可用
- 卸载后，对应工具自动不可用（无需手动清理）

---

## 安全性

Lua 脚本运行在受限环境：

- `os.execute` 和 `io.popen` 被移除 — 必须通过 `ezmk.run()` 执行外部命令
- `ezmk.file_write()` 拒绝写入项目根目录外的绝对路径
- 不暴露 `require` 加载 C 扩展（仅允许纯 Lua 模块）

---

## 与 `pkg info` 集成

对 `type = "utils"` 的包执行 `ezmk pkg info` 时，除常规字段外，额外展示：

```
Package: ezmk-cc
Version: 0.1.0
Type: utils
Tools: cc, compile-commands
...
```

---

## 实现说明

- Lua 5.4 静态链接到 ezmk 二进制，进程启动时初始化 `lua_State*`，全局复用
- 每次 `run()` 前以 sandbox 环境表隔离，避免脚本间的全局变量污染
- API 绑定通过 Lua C API (`lua_pushcfunction` / `lua_setglobal`) 实现
- JSON 编解码复用 `nlohmann/json.hpp`（已作为 vendor 库存在于项目中）
