# 从 CMake 项目迁移到 EazyMake

`ezmk project import --from cmake` 把当前目录的 `CMakeLists.txt` 转换为全新的
`ezmk.toml`。它面向**最标准的 CMake 项目**（通常是小型、单 target 项目），并且是
**实验性**功能：

- **单向快照** —— 导入后 `ezmk.toml` 为唯一事实源。之后直接编辑 `ezmk.toml`，
  **不要再回头改 `CMakeLists.txt`**。
- **best-effort** —— 部分写法会近似映射并留 `# TODO:` 注释供你校对；另有部分写法
  会被直接拒绝。
- **事务性** —— 遇到不支持的写法时立即停止，**不产出半成品** `ezmk.toml`。

## 用法

```bash
cd /path/to/cmake-project
ezmk project import                 # --from cmake（默认）
ezmk project import --from CMAKE    # 大小写不敏感
ezmk project import --overwrite     # 覆盖已存在的 ezmk.toml
```

前置条件：当前目录必须有 `CMakeLists.txt`。若 `ezmk.toml` 已存在，导入默认拒绝，
需显式 `--overwrite`。

## 支持的写法

以下标准命令会映射到 `ezmk.toml`：

| CMake 命令 | 映射到 |
|---|---|
| `project(name VERSION x.y.z LANGUAGES CXX/C)` | `[project]` `name` / `version` / `language` |
| `add_executable(t ...)` | `[project]` `type = "executable"`，`name = t` |
| `add_library(t STATIC/SHARED ...)` | `type = "static"` / `"shared"` |
| `target_sources(t PRIVATE <src...>)` | `[compile].src_dirs`（源文件所在目录） |
| `target_include_directories(t PRIVATE <dir...>)` | `[compile].include_dirs` |
| `target_compile_definitions(t PRIVATE <NAME=VAL...>)` | `[compile.macros]` |
| `target_compile_options(t PRIVATE <flag...>)` | `[compile].flags` |
| `target_link_libraries(t PRIVATE <lib...>)` | `[link].system_targets`（无法识别的库） |

**多 target 项目**：仅导入第一个/主 target，其余忽略。多 target 建议拆分为多个
EazyMake 项目。

## Best-effort（近似映射，留 `# TODO:`）

- **`set(...)` 变量 + `${VAR}`** —— 采用**有限、单层**展开：仅捕获顶层、条件块外的
  常量 `set()`；被后续命令修改的变量会从表中剔除。展开后仍含 `${...}` 的参数会留
  `# TODO: 未解析的参数` 注释。
- **`find_package(...)`** —— 包名映射到 EazyMake 包名（内置常见别名，如
  `Boost`→`boost`、`OpenSSL`→`openssl`），写成**注释掉的** `[depends]` 条目：

  ```toml
  [depends]
  # TODO: 原 CMake 引用了 boost，请手动执行 `ezmk pkg install boost` 后取消注释
  # lib = ["boost@1.82"]
  ```

- **条件块**（`if(WIN32)`、`if(UNIX)`、`if(APPLE)`、`if(MSVC)` 等）—— 取**当前平台**
  对应的分支。无法求值的条件（自定义变量、`$ENV{...}`、复杂表达式）会跳过并标记
  `# TODO: 未求值的条件块`。

## 明确拒绝的写法（中止，不产出）

以下**非声明式**写法不被支持，会事务性中止并指引到本文档：

- 自定义构建步骤：`add_custom_command`、`add_custom_target`
- 自定义函数/宏：`function()`、`macro()`
- 外部依赖探测：`pkg_check_modules`、`execute_process`
- 生成器表达式：`$<...>`（如 `$<TARGET_FILE:...>`、`$<JOIN:...>`）

## 被拒绝写法的手动迁移

需要自定义构建步骤的项目，改用 EazyMake 的 **Lua 钩子**复刻：

```toml
[hooks]
pre_build  = "scripts/gen_headers.lua"   # 编译前执行
post_build = "scripts/strip_symbols.lua" # 链接后执行
```

钩子示例见 `docs/zh/config_file.md`（`[hooks]` 节）与 `tutorial/zh/07-watch-hooks.md`。
`execute_process` 式逻辑对应 Lua API 的 `ezmk.run_command()` / `ezmk.file_write()`；
`pkg_check_modules` 对应 `ezmk pkg install <name>`（或上面的 `[depends]` 注释条目）。

## 导入后检查清单

1. 逐条审阅生成的 `ezmk.toml` 里的 `# TODO:` 注释。
2. 取消注释并修正 `[depends]` 条目，执行 `ezmk pkg install <name>`。
3. 核对 `[compile.macros]`（尤其平台宏）与 `[link].system_targets`。
4. 运行 `ezmk build` 再 `ezmk run`。用 `ezmk project cc` 重新生成
   `compile_commands.json`（clangd/LSP）。
