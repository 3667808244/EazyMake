# 术语表

EazyMake 文档的标准化中文术语。维护者和翻译者应参考此术语表，确保所有文档的术语一致性。

## 核心概念

| 术语（中文） | 术语（英文） | 说明 |
|-------------|-------------|------|
| 包 | package | 可重分发的代码单元（静态库或 utils 工具），通过 `ezmk pkg install` 安装，以 `.zip` 或 `.tar.gz` 归档格式分发 |
| 仓库 | repository (repo) | 包含 `index.toml` + `packages/` 的 git 仓库，托管包以供按名安装 |
| 作用域 | scope | 安装/注册级别：项目（`-p`）、用户（`-u`）、全局（`-g`） |
| 项目 | project | 由根目录的 `ezmk.toml` 文件定义的 EazyMake 工作区 |
| 构建 | build | 完整流程：编译源文件 → 链接目标文件 → 生成最终产物（可执行文件或库） |
| 编译 | compile / compilation | 将单个源文件（`.cpp`、`.c`）翻译为目标文件（`.o`、`.obj`） |
| 链接 | link / linking | 将目标文件和库组合为最终可执行文件或库 |
| 缓存 | cache | 基于内容哈希的增量构建缓存，存储在 `.ezmk/cache/` |
| 钩子 | hook | 在构建生命周期节点执行的 Lua 脚本：`pre_build`、`post_build`、`on_failure` |
| 工具链 | toolchain | 编译器/链接器抽象层，支持 GCC、Clang 和 MSVC |
| 构建配置 | profile | `ezmk.toml` 中一组命名的编译/链接标志覆盖，通过 `--profile <name>` 激活 |

## 包与仓库术语

| 术语（中文） | 术语（英文） | 说明 |
|-------------|-------------|------|
| 归档 | archive | 压缩的包文件（`.zip` 或 `.tar.gz`） |
| 依赖 | dependency | `[depends].lib` 中声明的必需库。缺失 → 构建报错 |
| 可选依赖 | optional dependency | `[depends].want` 中声明的库。缺失 → 警告 + `EZMK_LIB_MISS_*` 宏，构建继续 |
| 索引 | index | 仓库中的 `index.toml` 文件，列出所有可用包及其版本和 SHA-256 哈希 |
| 注册表 | registry | 各作用域的 `list.toml` 文件，记录已注册仓库 |
| 上游 | upstream | 包所包装的原始第三方项目（如 fmt、spdlog） |

## 配置术语

| 术语（中文） | 术语（英文） | 说明 |
|-------------|-------------|------|
| 宏 | macro | 预处理器定义，通过 `-D` 标志、`[compile.macros]` 或自动注入的 `EZMK_*` 宏 |
| 标志 | flag | 编译器或链接器的命令行选项（如 `-Wall`、`-O2`） |
| 头文件目录 | include directory | `#include` 头文件的搜索路径，在 `[compile].include_dirs` 中配置 |
| 源文件目录 | source directory | 扫描源文件的目录，在 `[compile].src_dirs` 中配置 |
| 链接目录 | link directory | 链接时库文件的搜索路径，在 `[link].link_dirs` 中配置 |
| 系统目标 | system target | 需要链接的系统库（如 `pthread`、`m`），在 `[link].system_target` 中声明 |

## Lua 与 Utils 术语

| 术语（中文） | 术语（英文） | 说明 |
|-------------|-------------|------|
| 工具包 | utils package | `type = "utils"` 的包，通过 `ezmk utils <name>` 提供 Lua 工具 |
| 沙箱 | sandbox | 受限的 Lua 环境：编译期移除 `os` 和 `io`；文件写入限制在项目根目录内 |
| 权限 | permission | `[utils.permissions]` 中对 `file_read`/`file_write`/`run` 的细粒度白名单/黑名单 |
| 入口脚本 | entry script | `utils/<name>.lua` 文件，实现工具的 `run(args)` 和可选的 `help()` 函数 |
| 内置工具 | built-in tool | 直接编译进 ezmk 二进制的工具（目前仅 `ezmk-cc`） |

## 安全术语

| 术语（中文） | 术语（英文） | 说明 |
|-------------|-------------|------|
| SHA-256 | SHA-256 | 用于验证包归档完整性的密码学哈希 |
| 原子写入 | atomic write | 先写入临时文件，再 `rename` 到目标路径，防止构建中途失败导致损坏 |
| 二次确认 | secondary confirmation | 敏感操作（全局安装、文件覆盖）需要的额外交互式确认 |
| 校验 | validation | 检查数据是否满足预期约束（如 `index.toml` 可解析、sha256 匹配、文件存在） |

## 构建与编译术语

| 术语（中文） | 术语（英文） | 说明 |
|-------------|-------------|------|
| 目标文件 | object file | 编译后的翻译单元（Unix 为 `.o`，Windows 为 `.obj`） |
| 单文件合并 | amalgamation | C/C++ 库的单文件分发形式（如 SQLite 的 `sqlite3.c`） |
| 纯头文件 | header-only | 无需编译的库——所有代码在头文件中（如 Catch2、nlohmann_json） |
| 增量构建 | incremental build | 仅重新编译自上次构建以来内容或依赖发生变化的源文件 |
| 并行编译 | parallel compilation | 使用 `-j <N>` 同时编译多个源文件 |
| 监视模式 | watch mode | 源文件变化时自动重新构建，通过 `ezmk project watch` |
