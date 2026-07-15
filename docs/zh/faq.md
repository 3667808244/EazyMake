# 常见问题 / 故障排除

本页汇总了安装、构建、包管理、配置和跨平台使用中的常见问题及解决方案。如果这里没有你遇到的问题，请查阅 `docs/zh/` 中的其他文档或提交 issue。

---

## 安装

### Q: `install.sh` 报错 "curl: command not found"

**原因**: 系统中未安装 `curl`。

**解决**:
1. **MSYS2**: `pacman -S curl`
2. **Linux (Debian/Ubuntu)**: `sudo apt install curl`
3. **Linux (Arch)**: `sudo pacman -S curl`
4. **macOS**: `brew install curl`

---

### Q: `install.sh` 报错 "Permission denied"

**原因**: 对安装目录没有写入权限（默认：Windows 上为 `~/ezmk/`，Linux/macOS 上为 `/usr/local/bin/`）。

**解决**:
1. 用户级安装，指定可写目录：`bash install.sh --prefix ~/.local`
2. Linux/macOS 系统级安装：`sudo bash install.sh`
3. Windows 上安装到用户目录时，请以普通用户（而非管理员）身份运行终端

---

### Q: `build.sh` 报错 "g++: command not found"

**原因**: GCC/g++ 未安装或不在 PATH 中。

**解决**:
1. **MSYS2**: `pacman -S mingw-w64-ucrt-x86_64-gcc`
2. **Linux (Debian/Ubuntu)**: `sudo apt install g++`
3. **Linux (Arch)**: `sudo pacman -S gcc`
4. **macOS**: `brew install gcc`（或使用 Apple Clang：`CXX=clang++ bash build.sh`）

---

### Q: 运行 `install.sh` 时出现网络错误

**原因**: `install.sh` 从 GitHub 下载 ezmk 和默认仓库，防火墙、代理或网络问题可能导致失败。

**解决**:
1. 检查网络连接
2. 如果使用了代理，设置 `http_proxy` / `https_proxy` 环境变量
3. 离线安装方案见[离线/无网络使用](#离线无网络使用)

---

## 构建

### Q: 构建时报 "fatal error: xxx.h: No such file or directory"

**原因**: 编译器在配置的头文件目录中找不到指定的头文件。

**解决**:
1. 检查 `ezmk.toml` → `[compile]` → `include_dirs` 是否包含头文件所在目录
2. 确认头文件名拼写正确（Linux/macOS 区分大小写）
3. 如果头文件来自依赖包，确认已安装：`ezmk pkg install <name>`
4. 对于系统头文件，检查编译器工具链是否正确安装
5. 尝试清除缓存：删除 `.ezmk/cache/` 后重新构建

---

### Q: "undefined reference to ..."（链接错误）

**原因**: 链接器找不到函数或符号的定义。

**解决**:
1. 检查所有 `.cpp` / `.c` 文件是否在 `[compile]` → `src_dirs` 列出的目录中
2. 如果符号来自某个库，在 `ezmk.toml` 的 `[depends]` → `lib` 中添加该库
3. 如果符号来自系统库，在 `[link]` → `flags` 中添加 `-l<库名>`
4. 确保库文件在 `[link]` → `link_dirs` 列出的目录中
5. 对于 C++ 代码中调用的 C 库，确保有 `extern "C"` 包裹

---

### Q: 构建成功但程序立即崩溃

**原因**: 常见原因包括编译/链接标志不匹配、过期的目标文件或运行时依赖问题。

**解决**:
1. 清理并重建：`ezmk project clean && ezmk project build`
2. 使用 `--disable-cache` 强制全量重编译
3. 检查 `[compile]` → `flags` 和 `[link]` → `flags` 是否一致（例如不要混用 debug 和 release 标志）
4. Windows 上检查所需的 DLL 是否在 PATH 中

---

### Q: 缓存似乎损坏 — 同一源文件每次编译行为不同

**原因**: 构建缓存（`.ezmk/cache/`）中可能包含过期或损坏的条目。

**解决**:
1. 删除缓存目录：`rm -rf .ezmk/cache/`（或 `ezmk project clean`）
2. 重新构建：`ezmk project build`
3. 如果问题持续，使用 `--disable-cache --verbose` 检查实际的编译命令

---

### Q: 运行 build 时提示 "ezmk.toml not found"

**原因**: 当前不在项目目录中，或项目未正确初始化。

**解决**:
1. 确保你在项目根目录（`ezmk.toml` 所在目录）中
2. 如果还没有项目，创建一个：`ezmk project new <名称>`
3. 检查 `ezmk.toml` 是否存在且可读

---

### Q: 编译标志未生效

**原因**: Profile 标志是追加到基础标志之后的，并非替换。或者你编辑了错误的配置段。

**解决**:
1. 基础标志在 `[compile]` → `flags` 中；profile 特定标志在 `[compile.profile.<名称>]` → `flags` 中
2. Profile 标志**追加**到基础标志（不会替换）
3. 确认你在命令行中传递了 `--profile <名称>` — profile 不会自动应用
4. 使用 `--verbose` 查看实际执行的编译命令

---

### Q: 提示 "src/ directory not found" 但源文件在其他目录

**原因**: 默认 `src_dirs` 为 `["src"]`，但你的源文件在其他位置。

**解决**:
在 `ezmk.toml` 中添加源文件目录：
```toml
[compile]
src_dirs = ["src", "lib", "vendor"]
```

---

## 包管理

### Q: "Package not found: xxx"

**原因**: 任何已注册仓库的索引中都找不到该包名。

**解决**:
1. 检查已注册的仓库：`ezmk repo list`
2. 如果未注册任何仓库，添加默认仓库：`ezmk repo add https://github.com/3667808244/ezmk-repo.git`
3. 更新仓库索引：`ezmk repo update`
4. 搜索正确的包名：`ezmk pkg search <关键词>`
5. 离线/手动安装：下载归档文件后使用 `ezmk pkg install ./<文件>.tar.gz --type file`

---

### Q: SHA-256 校验失败

**原因**: 下载的归档与预期哈希不匹配 — 可能损坏或被篡改。

**解决**:
1. 清除下载缓存并重试：先 `ezmk repo update`，再重试安装
2. 如果使用了 `--sha256 <hash>`，仔细核对哈希值
3. 如果包最近在仓库中更新过，索引可能过期 — 运行 `ezmk repo update`
4. 最后的办法：删除仓库缓存并重新克隆

---

### Q: 检测到循环依赖

**原因**: 包 A 依赖 B，而 B 又依赖 A（直接或间接）。

**解决**:
1. 这是某个包的打包错误 — 从错误信息中确认涉及哪些包
2. 向包/仓库维护者报告此问题
3. 临时方案：尝试从源码或其他仓库安装其中一个包

---

### Q: 全局安装失败，提示权限错误

**原因**: 全局安装目录需要提升的权限。

**解决**:
1. 改用用户作用域安装：`ezmk pkg install -u <名称>`（安装到 `~/.local/ezmk/pkg/`）
2. 改用项目作用域安装：`ezmk pkg install -p <名称>`（安装到 `.ezmk/pkg/`）
3. Linux/macOS 上如果必须全局安装，使用 `sudo`

---

### Q: 包安装成功但构建时找不到头文件

**原因**: 包的头文件目录未自动添加到项目中。

**解决**:
1. 确认包已在项目 `ezmk.toml` 的 `[depends]` → `lib` 中列出
2. 安装后重新构建 — ezmk 会自动发现依赖包的头文件
3. 检查包的作用域是否匹配：如果用了 `-u`（用户作用域）安装，确认构建能访问用户作用域的包

---

### Q: `pkg update` 显示"无可更新版本"，但我确定有新版本

**原因**: 仓库索引可能过期，或新版本在其他仓库中。

**解决**:
1. 更新仓库索引：`ezmk repo update`
2. 检查已注册的仓库：`ezmk repo list`
3. 如有需要，添加包含新版本的仓库

---

## 配置

### Q: ezmk.toml 中的 TOML 语法错误

**原因**: TOML 语法严格 — 常见错误包括引号类型错误、缺少等号、表嵌套无效等。

**解决**:
1. 字符串值使用双引号：`name = "my-project"`，而非 `name = 'my-project'`
2. 节头使用方括号：`[compile]`，而非 `(compile)`
3. 内联表使用等号：`{key = value}`，而非冒号
4. 布尔值小写：`true` / `false`
5. 数组使用方括号：`src_dirs = ["src", "lib"]`
6. 检查是否有尾随逗号（TOML 数组不允许）

---

### Q: "invalid project type" 错误

**原因**: `[project]` 中的 `type` 字段值不被识别。

**解决**:
有效类型：
- `"executable"` — 生成可执行文件
- `"static"` — 生成静态库（`.a` / `.lib`）
- `"shared"` — 生成动态库（`.dll` / `.so` / `.dylib`）
- `"utils"` — 工具包，提供基于 Lua 的实用程序

---

### Q: "invalid language format" 错误

**原因**: `language` 字段必须遵循 `<语言><版本>` 格式。

**解决**:
有效示例：`"C++17"`、`"C++20"`、`"C++23"`、`"C11"`、`"C17"`、`"C23"`、`"C99"`。
不要使用独立名称如 `"C"` 或 `"C++"` — 必须包含版本号。

---

### Q: Profile 未生效

**原因**: Profile 不会自动应用 — 必须显式传递 `--profile <名称>`。

**解决**:
```bash
ezmk project build --profile debug
ezmk project run --profile release
```

---

### Q: "macro name invalid" 错误

**原因**: `[compile.macros]` 中的宏名称不是合法的 C 标识符。

**解决**:
合法的宏名称匹配 `[A-Za-z_][A-Za-z0-9_]*`。示例：
- ✅ `ENABLE_FEATURE`、`BUFFER_SIZE`、`_DEBUG`
- ❌ `123abc`、`my-macro`、`enable feature`

---

## 跨平台

### Q: Linux 上构建成功但 Windows 上失败

**原因**: 平台特定的 API 使用、路径分隔符或编译器差异。

**解决**:
1. `ezmk.toml` 中的路径使用正斜杠（`/`）— ezmk 会在所有平台上自动规范化
2. 对 Windows 特定代码使用 `#ifdef _WIN32` 守卫
3. 检查 MSYS2/MinGW 工具链是否安装了所有必需的库
4. MSVC 构建时，使用 `[compile]` → `msvc_flags` 设置 MSVC 专用标志
5. 确保使用可移植的系统 API（如 `std::filesystem` 而非仅 POSIX 的调用）

---

### Q: MSVC 标志在 Linux 上被忽略（或 GCC 标志在 Windows MSVC 上不生效）

**原因**: `msvc_flags` 仅在检测到 MSVC 时使用；常规 `flags` 在使用 MSVC 时会自动翻译为 MSVC 等效标志。

**解决**:
1. 将 GCC/Clang 标志放在 `[compile]` → `flags` — 它们会自动翻译为 MSVC 格式
2. 仅 MSVC 的标志放在 `[compile]` → `msvc_flags` — 非 MSVC 工具链上会被静默忽略
3. 使用 `--verbose` 查看实际传递给编译器的标志

---

### Q: Linux/macOS 上出现 `-lwinhttp` 链接错误

**原因**: `-lwinhttp` 是 Windows 专用库。

**解决**:
如果你在 Linux/macOS 上从源码构建 ezmk 本身，使用不带 `-lwinhttp` 的手动构建命令：
```bash
g++ -std=c++17 src/*.cpp src/vendor/*.c src/vendor/lua/*.c \
    -I include/ -I include/vendor/ -I include/vendor/lua/ \
    -DLUA_COMPAT_5_3 -o build/ezmk -static
```

---

## 离线 / 无网络使用

### Q: 如何在没有网络的情况下使用 ezmk？

**解决**: 三种方案：

**1. 本地仓库镜像**
```bash
# 在有网络的机器上克隆仓库
git clone https://github.com/3667808244/ezmk-repo.git /path/to/ezmk-repo

# 复制到离线机器，然后注册为本地仓库
ezmk repo add /path/to/ezmk-repo --type local
```

**2. 手动下载包归档并安装**
```bash
# 在有网络的机器上从 GitHub Releases 下载 .tar.gz 归档
# 传输到离线机器后：
ezmk pkg install ./<包名>-<版本>.tar.gz --type file
```

**3. USB / 内网共享的预置镜像**
```bash
# 在 USB 或内网共享上准备完整的仓库镜像
git clone https://github.com/3667808244/ezmk-repo.git /mnt/usb/ezmk-repo

# 在每台离线机器上
ezmk repo add /mnt/usb/ezmk-repo --type local
```

### Q: 如何离线安装 ezmk 本身？

**解决**:
1. 在有网络的机器上从 [GitHub Releases](https://github.com/3667808244/EazyMake/releases) 下载发行版二进制文件
2. 将二进制文件复制到离线机器
3. 放到 PATH 中的某个目录
4. 对于包，使用上述离线包方案之一

---

## Lua / 工具

### Q: "unknown utils command" 错误

**原因**: 没有已安装的包提供该名称的工具。

**解决**:
1. 查看可用的工具：检查 `.ezmk/pkg/*/utils/`、`~/.local/ezmk/pkg/*/utils/` 和 `<安装目录>/pkg/*/utils/`
2. 安装工具包：`ezmk pkg install example-utils`
3. 确认包的 `type = "utils"` 且在 `[utils].tools` 中列出了工具名称

---

### Q: Lua 脚本失败，提示 "permission denied"

**原因**: 包的 `[utils.permissions]` 配置限制了请求的访问权限。

**解决**:
1. 检查包的 `ezmk.toml` 中的 `[utils.permissions]` 设置
2. 将被拒绝的路径/命令添加到相应的允许列表（`read`、`write` 或 `run`）
3. 如果包没有 `[utils.permissions]` 段，它运行在旧版（无限制）模式下 — 拒绝来自 ezmk 的硬沙箱限制（如禁止在项目根目录外写入）

---

### Q: 如何在 Lua 脚本中检查 API 版本以做兼容？

**解决**:
在 Lua 脚本中读取 `ezmk.api_version`：
```lua
if ezmk.api_version >= 2 then
    -- 使用新版 API
else
    -- 兼容旧版 ezmk
end
```
详见 [Utils 工具系统](utils.md) 中的完整 API 参考和版本化策略。
