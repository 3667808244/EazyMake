# 规划

EazyMake 版本规划与路线图。每个版本对应一个 Markdown 文件，详细描述该版本的目标、设计方案、实施步骤和注意事项。

---

## 当前执行

(None)

## 待执行

(None)

## 已完成

- `0.1.1` ~ `0.1.5`（文件已移除）
- **[0.1.6](0.1.6.md)** — 测试（Catch2 单元测试 + 集成测试）
- **[0.1.7](0.1.7.md)** — 基本国际化（i18n）：编译期 JSON 嵌入 + I18nKey 枚举，85+ 个用户可见字符串，中英文双语
- **[0.1.8](0.1.8.md)** — 跨平台支持与编译器探测：Linux/macOS/Windows 多编译器自动检测（g++/clang++），环境变量覆盖，标准库验证
- **[0.2.0](0.2.0.md)** — Lua 工具链：嵌入 Lua 5.4.7 解释器，22 个 ezmk C++ API（项目信息/编译选项/文件系统/进程执行/日志/JSON），sandbox 安全模型，`find_utils_script()` 三作用域查找，内置 `ezmk-cc`（compile_commands.json 生成器），798 断言、零回归
- **[0.2.1](0.2.1.md)** — MSVC 支持：`cl.exe` + `link.exe` + `lib.exe` 完整编译流程，GCC→MSVC 标志翻译层，`/showIncludes` 依赖解析
- **[0.2.2](0.2.2.md)** — 精细化编译控制：可选依赖（`want.lib`）、语义化宏定义（`compile.macros`）、非标项目结构支持（`compile.src_dirs`）
- **[0.2.3](0.2.3.md)** — 开发者体验提升：并行编译（`-j`）、构建 Profile（`--profile`）、Build Hooks（`[hooks]`）、Watch 模式、`pkg list`/`pkg update`、Bug 修复
- **[0.2.4](0.2.4.md)** — 健壮性与完善：Bug 修复（版本比较统一、Shell 注入、`/tmp` 路径）、代码质量（`build_project` 重构、CLI 去重、帮助 i18n）、功能补全（C23、`pkg update --all`、MSVC 标志扩展）

---

## 版本主题概览

| 版本  | 主题       | 关键交付                                                                                               | 依赖                                        |
| ----- | ---------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------- |
| 0.1.6 | 测试       | Catch2 单元测试（10 个模块）、集成测试（50+ 场景）、>80% 关键路径覆盖率                                | —                                           |
| 0.1.7 | 国际化     | i18n 模块、85 个 I18nKey、en.json + zh.json、`EZMK_LANG` 环境变量                                      | —                                           |
| 0.1.8 | 跨平台     | `detect_compiler()`、`$CXX/$CC` 覆盖、macOS/Linux/Windows 平台宏完善                                   | —                                           |
| 0.2.0 | Lua 工具链 | Lua 5.4.7 静态链接、ezmk Lua API（22 函数）、sandbox 安全模型、`find_utils_script()`、内置 `ezmk-cc`   | 0.1.7（i18n 集成）、0.1.8（编译器检测集成） |
| 0.2.1 | MSVC 支持  | `Toolchain` 抽象层、GCC→MSVC 标志翻译、`/showIncludes` 依赖解析、MSVC 编译/链接/归档流程               | 0.1.8（编译器探测基础）                     |
| 0.2.2 | 精细化控制 | `want.lib` 可选依赖、`compile.macros` 宏定义、`compile.src_dirs` 多源目录                              | 0.2.0（Lua 工具链可配合使用）               |
| 0.2.3 | 开发者体验 | `-j` 并行编译、`--profile` 构建配置、`[hooks]` 构建钩子、Watch 模式、`pkg list`/`pkg update`、Bug 修复 | 0.2.2（编译控制基础）                       |
| 0.2.4 | 健壮性与完善 | Bug 修复（版本比较、Shell 注入、`/tmp`）、代码质量（`build_project` 重构、CLI 去重、帮助 i18n）、功能补全（C23、`pkg update --all`、MSVC 标志扩展） | 0.2.3（所有功能模块） |

## 依赖关系图

```
0.1.6 (testing) ──── 独立，可随时开始
0.1.7 (i18n)    ──── 独立，可在 0.1.6 之前或之后
0.1.8 (cross-plat)── 独立，可在 0.1.6 之前或之后
                        │
                        ├──── 0.2.1 (MSVC) ── 依赖 0.1.8 的 detect_compiler() 基础
                        │
0.2.0 (Lua) ──────────── 依赖 0.1.7（i18n API 集成）和 0.1.8（编译器信息 API）
     │
     └──── 0.2.2 (精细化) ── 可选依赖 0.2.0（Lua 工具链可配合使用）
                              │
                              └──── 0.2.3 (开发者体验) ── 依赖 0.2.2 的编译控制基础
                                                │
                                                └──── 0.2.4 (健壮性与完善) ── 依赖 0.2.3 的所有功能模块
```

---

## 跨版本关注点

以下关注点跨越多个版本，需在各版本计划中协同考虑：

### 向后兼容性
- `ezmk.toml` 格式扩展添加可选字段（`msvc_flags`、`[utils]`、`want.lib`、`compile.macros`），不影响已有配置
- `record.json` 的 `version` 字段支持缓存格式演进
- CLI 接口保持稳定（新增 flag 不破坏已有 flag）

### 安全模型
- 全局安装确认（已有，`pkg.cpp`）：0.1.6 测试已覆盖
- SHA-256 校验（已有）：0.1.6 测试已覆盖，0.2.0 的 Lua API 中也可使用
- Lua sandbox（0.2.0）：`os.execute`/`io.popen` 编译期移除、文件写入限制、独立环境表、脚本间隔离
- repo 校验（已有）：`index.toml` 解析 + clone 失败清理

### 跨平台一致性
- 0.1.8 编译器探测 + 0.2.1 MSVC 支持：同一份 `ezmk.toml` 可在 Windows/MSVC 和 Linux/GCC 下编译
- 缓存记录中的 `compiler` 字段天然隔离不同编译器的缓存
- 路径处理：`native_path()` 在 Windows/MinGW 用反斜杠、Linux/macOS 用正斜杠
