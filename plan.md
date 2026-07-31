# EazyMake 1.1.0-dev.4 执行计划

> 详细设计：[`plans/release/1.1.0-dev.4.md`](plans/release/1.1.0-dev.4.md)

---

## 1 背景

当前 EazyMake 的语言与编译器配置存在三个问题：

1. **无法选择标准库**：使用 `clang++` 的 Linux 用户无法通过语义化配置选择 `libc++` vs `libstdc++`
2. **语言识别过于严格**：`parse_language()` 仅接受 `C++17` / `C11` 格式，`c++17`、`cxx17`、`CPP17` 等常见变体均报错（见 `test_config.cpp:120` — `"c++17"` 预期抛异常）
3. **编译器拓展使用不便**：需要 GNU extensions 时只能手动在 `compile.flags` 中改 `-std=` 标志，无法在 `project.lang` 中声明

**解决方案**：新增 `project.stdlib` 配置项、泛化 `project.lang` 解析（大小写不敏感 + `C++`/`CXX`/`CPP` 统一）、支持 GNU 拓展前缀自动映射。

---

## 2 目标

| # | 目标 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | **`project.stdlib` 支持** | P0 | `ezmk.toml` 的 `[project]` 节新增 `stdlib` 字段，可选 `libstdc++`（默认）/ `libc++`；自动注入 `-stdlib=` 标志 + `EZMK_STDLIB` 宏 |
| 2 | **`project.lang` 泛化** | P0 | 实现 `normalize_lang()`：大小写不敏感、`C++`/`CXX`/`CPP` → 统一为 `CPP`、trim 空白；`parse_language()` 重构以复用此函数 |
| 3 | **编译器拓展支持** | P1 | `project.lang` 支持 `GNUCPP17` / `GNU11` 等前缀，自动映射到 `-std=gnu++17` / `-std=gnu11`；非 GNU 时给出 non-ISO 警告 |

---

## 3 执行阶段

### 阶段一：`normalize_lang()` 泛化函数（P0，基础依赖）

**文件**：`src/config.cpp` + `include/ezmk/config.hpp`

- [ ] 在 `config.cpp` 匿名命名空间中实现 `normalize_lang()`：
  ```cpp
  std::string normalize_lang(const std::string& input) {
      1. to_upper(input)                  // 大小写统一
      2. replace "C++" / "CXX" → "CPP"   // 变体统一
      3. trim whitespace
      4. return result
  }
  ```
- [ ] 处理边界：空字符串 → 报错 `"language not specified"`；仅 `C` / `CPP` 无版本号 → 默认 `C11` / `CPP17`
- [ ] 在 `config.hpp` 中声明该函数（`namespace ezmk::config`）
- [ ] 在 `parse_config()` 中读取 `project.stdlib` 字段（TOML `[project]` 节），存入 `Config::ProjectSection::stdlib`
- [ ] `stdlib` 值也经 `normalize_lang()` 泛化（共用同一函数：`LIBSTDC++` → `LIBSTDCXX`，`LIBC++` → `LIBCXX`）
- [ ] 不可识别的 `stdlib` 值 → 报错并列出可用值（`libstdc++` / `glibcxx` / `gnu` 和 `libc++` / `llvm`）

**关键决策**：`normalize_lang()` 同时处理 `lang` 和 `stdlib`。对于 `stdlib`，输入如 `glibcxx` → 标准化为 `LIBSTDCXX`；对于 `lang`，输入如 `c++17` → 标准化为 `CPP17`。同一个函数，不同使用方各取所需。

### 阶段二：重构 `parse_language()`（P0）

**文件**：`src/config.cpp` + `include/ezmk/config.hpp`

- [ ] `LanguageInfo` 结构体扩展：
  ```cpp
  struct LanguageInfo {
      std::string compiler;
      std::string std_flag;              // e.g. "-std=c++17" or "-std=gnu++17"
      std::string detected_compiler;
      bool gnu_extensions = false;       // NEW: true if GNU prefix detected
      std::string normalized_lang;       // NEW: e.g. "CPP17" (for EZMK_LANG macro)
  };
  ```
- [ ] `parse_language()` 内部逻辑重写：
  1. 调用 `normalize_lang(language)` 得到泛化结果
  2. 检测 `GNU` 前缀：`GNUCPP17` → `is_cxx=true, version=17, gnu=true`；`GNU11` → `is_cxx=false, version=11, gnu=true`
  3. 无 `GNU` 前缀：`CPP17` → `is_cxx=true, version=17, gnu=false`
  4. 版本号映射表保持不变（89/98/99/03/11/14/17/20/23/26），未识别版本 → 报错
  5. `std_flag` 生成：`gnu ? "-std=gnu++" + ver : "-std=c++" + ver`（C 语言同理）
- [ ] 保持向后兼容：旧项目写 `C++17` → 正常解析，行为不变

### 阶段三：`stdlib` 标志注入（P0）

**文件**：`src/toolchain.cpp` + `include/ezmk/toolchain.hpp`

- [ ] 新增 `get_compile_flags()` 函数（或直接在 `cache.cpp` 编译命令构建处处理）：
  ```cpp
  // 返回需要追加的 stdlib 相关编译标志
  std::vector<std::string> get_stdlib_flags(const std::string& stdlib,
                                             CompilerFamily family);
  ```
- [ ] 逻辑：
  - `stdlib == "libc++"`：
    - GCC → 添加 `-stdlib=libc++`（附带 warning：GCC libc++ 支持有限）
    - Clang → 添加 `-stdlib=libc++`
    - MSVC → 不添加（MSVC 仅使用 STL，无 `-stdlib` 标志）
  - `stdlib == "libstdc++"`（默认）：
    - Clang → 添加 `-stdlib=libstdc++`（显式指定，避免 Clang 在某些平台上默认选 libc++）
    - GCC / MSVC → 不添加（已是默认）
- [ ] **实际实现位置**：`cache.cpp` 的 `compile_one_source()` 函数（第 467–489 行是 GCC/Clang 编译命令构建处）。将 `stdlib` 信息通过 `CompileInput` 传入，在命令构建时追加标志。

**注意**：设计文档 §4.2 提到在 `toolchain.cpp` 中实现，但实际编译命令构建在 `cache.cpp`。建议在 `toolchain.cpp` 中实现纯函数（便于单测），在 `cache.cpp` 中调用。

### 阶段四：`EZMK_STDLIB` 宏注入（P0）

**文件**：`src/build.cpp`

- [ ] 在 `generate_ezmk_macros()` 函数中（第 68–89 行），添加 `EZMK_STDLIB` 宏：
  ```cpp
  // 与 EZMK_LANG 并列（第 85 行之后）
  if (!cfg.project.stdlib.empty()) {
      result.push_back("-DEZMK_STDLIB=\"" +
          util::escape_shell_arg(cfg.project.stdlib) + "\"");
  }
  ```
- [ ] 宏值规范：`libstdc++` → `"libstdcxx"`，`libc++` → `"libcxx"`（`++` → `xx`，避免 `+` 字符被误解析）
- [ ] `CompileInput` 结构体（`cache.hpp`）需添加 `stdlib` 字段，由 `build.cpp` 在构建 `CompileInput` 时传入

### 阶段五：编译器拓展警告（P1）

**文件**：`src/config.cpp` 或 `src/build.cpp`

- [ ] 在 `parse_language()` 检测到 `GNU` 前缀时，输出 warning：
  ```
  warn: using GNU extensions (non-ISO C++), use 'language = "CPP17"' for standard C++
  ```
- [ ] 警告可被 `--quiet` 抑制（复用现有 `util::warn()` 机制）
- [ ] 无需修改 `toolchain.cpp` 的 flag 映射表（现有映射已包含 `-std=` 变体，`-std=gnu++17` 是 GCC/Clang 原生支持的标志，无需额外映射）

### 阶段六：测试（P0）

**文件**：`test/test_config.cpp` + `test/test_toolchain.cpp`（或新建 `test/test_build.cpp` 用例）

- [ ] `normalize_lang()` 测试（15+ 用例）：
  - 基本变体：`c++17` → `CPP17`、`C++17` → `CPP17`、`cxx17` → `CPP17`、`CXX17` → `CPP17`、`cpp17` → `CPP17`
  - C 语言：`c11` → `C11`、`c17` → `C17`
  - 未来标准：`C++2B` → `CPP2B`、`c++20` → `CPP20`
  - 边界：空字符串 → 抛异常、`Rust` → 抛异常、仅 `C` → `C11`、仅 `CPP` → `CPP17`
  - GNU 前缀：`GNUCPP17` → `GNUCPP17`（保留 GNU 前缀供 `parse_language` 检测）
- [ ] `stdlib` 解析测试（5+ 用例）：
  - 默认值（不设 `stdlib`）→ `libstdc++`
  - `stdlib = "libstdc++"` → 解析成功
  - `stdlib = "libc++"` → 解析成功
  - `stdlib = "glibcxx"` / `"llvm"` → 映射到 `libstdc++` / `libc++`
  - `stdlib = "invalid"` → 抛异常并列出可用值
  - 大小写不敏感：`LibStdC++` → `libstdc++`
- [ ] `parse_language()` 扩展测试（10+ 用例）：
  - GNU 前缀：`GNUCPP17` → `std_flag="-std=gnu++17"`, `gnu_extensions=true`
  - GNU C：`GNU11` → `std_flag="-std=gnu11"`
  - 无 GNU 前缀：`CPP17` → `std_flag="-std=c++17"`, `gnu_extensions=false`
  - 泛化输入：`c++17` → `std_flag="-std=c++17"`（通过 normalize 后正常解析）
  - `cxx20` → `std_flag="-std=c++20"`
- [ ] `-stdlib=` 注入测试（8+ 用例，在 `test_toolchain.cpp` 或 `test_build.cpp`）：
  - `libstdc++` + GCC → 不注入
  - `libstdc++` + Clang → 注入 `-stdlib=libstdc++`
  - `libc++` + GCC → 注入 `-stdlib=libc++` + warning
  - `libc++` + Clang → 注入 `-stdlib=libc++`
  - `libc++` + MSVC → 不注入
  - 默认（不设 stdlib）+ Clang → 注入 `-stdlib=libstdc++`
  - 默认 + GCC → 不注入

### 阶段七：编译与回归验证

- [x] `bash build.sh` 编译通过（MSYS2 / Windows）
- [x] 全量测试通过，544 用例 / 2539 断言（新增 13 用例 / ~69 断言），零回归
- [x] 验证 `EZMK_STDLIB` 宏在条件编译中可用（手动编译简单测试文件）

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| `normalize_lang()` 双用途 | 同一函数处理 `lang` 和 `stdlib` 的泛化，减少重复逻辑；通过调用方区分语义 |
| 不创建新的 `build_compile_flags()` | 设计文档建议在 `toolchain.cpp` 中新建此函数，但在当前代码结构中，编译命令在 `cache.cpp` 的 `compile_one_source()` 中组装；将 stdlib 标志逻辑放在 `toolchain.cpp` 作为纯函数，在 `cache.cpp` 中调用，避免大面积重构 |
| `GNU` 前缀检测在 `parse_language()` | 而非 `toolchain.cpp` — `LanguageInfo` 直接携带 `gnu_extensions` 字段，下游无需重复判断 |
| `EZMK_STDLIB` 宏值中 `++` → `xx` | 与设计文档一致，避免某些工具链中 `+` 被误解析 |
| 默认 `libstdc++` | 不设 `stdlib` 时行为完全不变；Clang 平台显式注入 `-stdlib=libstdc++` 避免意外选择 libc++ |
| Clang 默认也注入 `-stdlib=libstdc++` | 部分 Linux 发行版的 Clang 默认使用 libc++，显式注入确保一致性 |

---

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| 新增 `project.stdlib` | 旧项目无此字段 | 默认 `libstdc++`，行为不变 |
| `project.lang` 泛化 | 旧项目写 `C++17` / `cxx17` 等变体 | `normalize_lang()` 将大小写和变体统一；`C++17` → `CPP17`，标志不变 |
| `LanguageInfo` 新增字段 | 下游代码读取 `LanguageInfo` | 新增字段有默认值（`gnu_extensions=false`, `normalized_lang=""`），不破坏现有使用 |
| 编译器拓展自动检测 + 警告 | 旧项目写 `GNUCPP17`（此前不存在） | 新功能，无旧项目影响；首次使用给出 non-ISO 警告 |
| `-stdlib=` 注入 | 旧项目未设 `stdlib` | GCC 不注入额外 flag；Clang 注入 `-stdlib=libstdc++`（此变更对 Clang 用户可能有影响，但属于正确行为 — 明确指定标准库避免歧义） |
| `EZMK_STDLIB` 宏 | 旧项目代码中无 `#ifdef EZMK_STDLIB` | 纯增量，不破坏现有条件编译 |

---

## 6 涉及文件清单

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `include/ezmk/config.hpp` | 修改 | `ProjectSection` 加 `stdlib`；`LanguageInfo` 加 `gnu_extensions` + `normalized_lang`；声明 `normalize_lang()` |
| `src/config.cpp` | 修改 | 实现 `normalize_lang()`；`parse_config()` 解析 `stdlib`；重构 `parse_language()` |
| `include/ezmk/toolchain.hpp` | 修改 | 声明 `get_stdlib_flags()` |
| `src/toolchain.cpp` | 修改 | 实现 `get_stdlib_flags()` |
| `include/ezmk/cache.hpp` | 修改 | `CompileInput` 加 `stdlib` 字段 |
| `src/cache.cpp` | 修改 | 编译命令构建处调用 `get_stdlib_flags()` 注入标志 |
| `src/build.cpp` | 修改 | `generate_ezmk_macros()` 加 `EZMK_STDLIB`；传入 `stdlib` 到 `CompileInput` |
| `test/test_config.cpp` | 修改 | 新增 `normalize_lang()` + `stdlib` 解析 + `parse_language()` 扩展测试 |
| `test/test_toolchain.cpp` | 修改 | 新增 `get_stdlib_flags()` 测试 |

---

## 7 延后项（1.1.0-dev.5+）

- 跨平台 CI 验证：在 Linux (Clang+libc++) / macOS (Apple Clang) / Windows (MSVC) 三个平台上验证 `stdlib` 功能 — 延后至 dev.5 冒烟测试阶段
- `EZMK_STDLIB` 宏的用户文档更新（`docs/zh/config_file.md` + `docs/en/config_file.md`）— 延后至 dev.5 文档整理阶段
- `.cursor/rules/` 同步生成（从 dev.3 延后项继承）
