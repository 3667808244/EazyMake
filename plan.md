# EazyMake 0.1.5 代码质量修复与用户体验改进

---

## 1 编译循环 DRY 重构 [`build.cpp`, `pkg.cpp`, `cache.cpp`, `cache.hpp`]

**问题**: `build_project()`（`build.cpp:240-343`）和 `compile_package()`（`pkg.cpp:187-272`）的编译循环约 80% 重复，包括：
- 缓存记录加载 / 编译选项签名检查
- 遍历源文件 → 查缓存 / 编译到临时文件 → 原子 rename → 更新缓存条目
- 依赖文件解析与哈希更新

**修复**: 提取共享编译核心到 `cache::compile_sources()`，返回编译结果结构体。

```cpp
// cache.hpp 新增
struct CompileResult {
    std::vector<fs::path> objects;   // 编译产物 .o/.obj 路径
    int cache_hits = 0;
    int cache_misses = 0;
};

struct CompileInput {
    std::vector<fs::path> sources;           // 源文件列表
    fs::path obj_dir;                        // .o 输出目录（build 用 temp/，pkg 用 build/）
    fs::path dep_dir;                        // .d 输出目录
    fs::path proj_root;                      // 项目/包根目录（用于相对路径缓存 key）
    config::CompileSection compile;          // 编译选项
    config::LanguageInfo lang;               // 编译器 + std flag
    std::vector<fs::path> extra_includes;    // 额外 -I（依赖包的头文件）
    fs::path cache_obj_dir;                  // 缓存 .o 存储目录
    fs::path cache_record_path;              // 缓存记录 JSON 路径
    bool disable_cache = false;              // --disable-cache
    bool use_pic = false;                    // -fPIC（shared 库）
    bool verbose = false;                    // --verbose
};

CompileResult compile_sources(const CompileInput& in);
```

**关键细节**:
- `build_project` 和 `compile_package` 各自调用 `compile_sources()`，仅保留差异逻辑（链接阶段、archive 阶段）
- `compile_package` 已有的"依赖路径归一化到 pkg_dir 相对路径"逻辑（`pkg.cpp:262-269`）需要保留在 `compile_sources` 的参数化回调或后处理中
- `compile_package` 中 `compiler = "g++"` 硬编码（`pkg.cpp:258`）应改为 `lang.compiler`

---

## 2 `--verbose` 参数支持 [`cli.cpp`, `cli.hpp`, `build.cpp`, `main.cpp`]

**目标**: 增加 `--verbose` / `-V`（注意 `-V` 已被 `--version` 占用，改用 `-v`），打印详细的编译过程。

**实现**:
1. `BuildOptions` 新增 `bool verbose = false;`
2. `ezmk project build --verbose` 和 `ezmk project run --verbose` 解析该 flag
3. 在 `compile_sources()` 中，当 `verbose=true` 时：
   - 打印完整的编译器命令行
   - 打印缓存命中/未命中的原因（源码哈希变化 / 编译选项变化 / 哪个头文件变化）
4. 链接阶段也打印完整链接命令

**示例输出**:
```
[ezmk]   Compiling src/main.cpp
[ezmk]     cmd: g++ -std=c++17 -c "src/main.cpp" -o ".ezmk/temp/main.tmp.o" -Wall -Wextra -O2 -I"include" -MMD -MF ".ezmk/temp/main.d"
[ezmk]   [cached] src/util.cpp
[ezmk]     cache hit: source hash matches, all 3 headers unchanged
```

---

## 3 彩色输出 [`util.cpp`, `util.hpp`]

**目标**: 使用 VT100/ANSI 转义序列实现彩色日志输出（Windows 10+ 原生支持）。

**实现**:
- `util.cpp` 中定义颜色常量：
  - `CLR_RESET`, `CLR_GREEN` (info), `CLR_YELLOW` (warn), `CLR_RED` (error/fatal), `CLR_CYAN` (cache hit), `CLR_BOLD` (emphasis)
- Windows 需在启动时调用一次 `SetConsoleMode()` 启用 VT100 处理
- 修改 `info()`, `warn()`, `error()`, `fatal()` 自动附加颜色
- 新增 `util::supports_color()` 检测（`NO_COLOR` 环境变量 / 非 TTY 输出），管道或 CI 场景自动降级为纯文本

**颜色方案**:
| 级别 | 颜色 |
|------|------|
| `[ezmk]` info | 绿色 |
| `[ezmk warn]` | 黄色 |
| `[ezmk error]` / `[ezmk fatal]` | 红色 |
| `[cached]` | 青色 |
| 文件名 / 路径 | 粗体 |
| 编译命令 (verbose) | 暗灰色 |

---

## 4 更清晰的报错信息 [`build.cpp`, `pkg.cpp`, `config.cpp`]

**改进点**:
1. **编译失败时**：打印失败文件的完整路径和编译器退出码，并在 verbose 模式下打印完整命令
   ```
   [ezmk error] compilation failed for src/main.cpp (exit code 1)
   [ezmk error]   g++ -std=c++17 -c "src/main.cpp" ... 
   ```
2. **链接失败时**：同上，打印链接命令（当前只打印 stderr）
3. **配置解析失败时**：明确指出 ezmk.toml 的行号和出错的 key（toml++ 的 `parse_error` 已包含位置信息，当前未利用）
   ```
   [ezmk error] ezmk.toml:5:12: invalid value for 'compile.flags'
   ```
4. **缺少编译器时**：当前已给出 MSYS2 安装提示，增加 Linux (`apt install g++`) 和 macOS (`brew install gcc`) 提示
5. **包管理错误**：SHA-256 校验失败时，提示用户可使用 `--sha256` 手动指定或 `-y` 跳过交互

---

## 5 `ezmk project new` 增强 [`project.cpp`, `cli.cpp`]

**当前状态**: `.gitignore` 已生成（`project.cpp:44-52`），`git init` 未执行。

**新增**:
1. 生成项目后自动执行 `git init`（如果 git 可用）
2. 新增 `--disable-git-init` flag，跳过 git 初始化
3. 新增 `--disable-gitignore` flag，跳过 .gitignore 生成

**CLI**:
```
ezmk project new <name> [--type <type>] [--disable-git-init] [--disable-gitignore]
```

---

## 6 `utils` 子命令占位 [`cli.cpp`, `cli.hpp`, `main.cpp`]

**命令结构**: `ezmk utils <util_name> [more_args]`

**用途**: 预留给未来的实用工具（如 `ezmk utils hash <file>` 计算 SHA-256、`ezmk utils info` 打印系统信息）。

**0.1.5 实现**: 仅占位——CLI 解析 + 输出 `"utils subcommand is reserved for future use"`，不实现具体工具。

---

## 7 补充修复（从代码审查中发现）

### 7.1 `build.cpp:45` 硬编码 `fs::current_path()` 修复
`make_compile_cmd()` 中默认 include 路径使用了 `fs::current_path()` 而非参数 `proj_root`。当从项目外调用构建时会导致错误。改为使用 `proj_root`。

### 7.2 `compile_package` 编译器硬编码修复
`pkg.cpp:258` 行 `entry.compiler = "g++"` 应改为从语言解析结果获取：
```cpp
auto lang = config::parse_language(cfg.project.language);
entry.compiler = lang.compiler;
```

### 7.3 `pkg.cpp` 中缺少 stale temp 清理
`compile_package()` 清理了 `build_dir` 下的 `.tmp` 文件，但没有清理 `.d` 依赖文件（上次构建残留）。增加 `.d` 文件清理逻辑，或在 `compile_sources()` DRY 重构时统一处理。

### 7.4 `util.cpp` 中 `download()` Linux fallback 的 curl 单引号转义
`util.cpp:407` 的 `escape_sq()` 转义了路径中的单引号，但 URL 中的单引号也应该被转义。当前 URL 参数直接拼接，如果 URL 包含特殊字符会导致命令注入风险。改为对 URL 也做转义。

---

## 实现顺序建议

| 优先级 | 条目 | 原因 |
|--------|------|------|
| P0 | 1. DRY 重构 | 后续所有编译相关改动的基础 |
| P0 | 7.1 ~ 7.3 补充修复 | 小修复，可与 DRY 重构一起做 |
| P1 | 2. --verbose | 依赖 DRY 重构后的 `compile_sources` |
| P1 | 3. 彩色输出 | 独立改动，范围小 |
| P2 | 4. 报错改进 | 独立改动，影响面小 |
| P2 | 5. project new 增强 | 独立改动，范围小 |
| P3 | 6. utils 占位 | 低优先级，纯占位 |
| P3 | 7.4 安全修复 | 边界情况 |

---

---

## 8 代码审查修复：HTTP 状态码检查 [`util.cpp`]

**问题**: `download()` 的 WinHTTP 分支未检查 HTTP 响应状态码。404 或 500 错误页面的内容会被当作有效数据写入目标文件。

**修复**: `WinHttpReceiveResponse` 成功后，调用 `WinHttpQueryHeaders` 获取状态码，非 2xx 时抛出异常并关闭所有 handle。

```cpp
// WinHttpReceiveResponse 之后新增:
DWORD statusCode = 0;
DWORD statusCodeSize = sizeof(statusCode);
if (!WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX)) {
    // 无法获取状态码，继续（保守策略）
} else if (statusCode < 200 || statusCode >= 300) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    throw std::runtime_error(
        "download failed: HTTP " + std::to_string(statusCode) + " for " + url_sv);
}
```

---

## 9 代码审查修复：`parse_depfile_and_hash` 防御性检查 [`cache.cpp`]

**问题**: `cache.cpp:153` 中 `tok.back() == ':'` 在 `tok` 为空时是未定义行为。虽然当前 token 生成逻辑保证不会产生空 token，但缺乏防御。

**修复**: 添加 `!tok.empty()` 前置检查。
```cpp
// 改前: if (tok.back() == ':') continue;
// 改后:
if (!tok.empty() && tok.back() == ':') continue;
```

---

## 10 代码审查修复：Shell 命令参数转义 [`util.cpp`]

**问题**: `git_clone()`、`git_pull()`、`git_last_commit_time()` 中 URL 和路径用双引号包裹，若内容本身包含双引号会导致命令解析错误甚至命令注入。同样的问题存在于 `run_script()` 和 Linux 的 `curl` 回退路径。

**修复**: 新增 `util::escape_shell_arg()` 工具函数，统一处理 Shell 参数中的特殊字符转义，替换所有直接拼接双引号的命令构造。

```cpp
// util.hpp / util.cpp 新增
// Escape a string for safe use inside double-quoted shell arguments.
// Escapes double-quote, backslash, backtick, and dollar sign.
std::string escape_shell_arg(std::string_view s);

// 实现:
std::string escape_shell_arg(std::string_view s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\' || c == '`' || c == '$')
            r += '\\';
        r += c;
    }
    return r;
}
```

**影响范围**:
- `git_clone()` — URL + dest 路径
- `git_pull()` — repo_dir 路径
- `git_last_commit_time()` — repo_dir 路径
- `run_script()` — script 路径 + cwd 路径
- Linux `download()` curl fallback — 确认已有的 `escape_sq` 足够（单引号包裹，仅需转义内部单引号）

---

## 实现顺序

| 优先级 | 条目 | 说明 |
|--------|------|------|
| P1 | 10. Shell 转义 | 安全相关，影响面广 |
| P1 | 8. HTTP 状态码 | `pkg install` 下载可靠性 |
| P2 | 9. 防御性检查 | 低风险，单行改动 |

---

## 版本号

0.1.5 为 patch 级别版本（代码质量 + 用户体验改进，无破坏性 API 变更）。
