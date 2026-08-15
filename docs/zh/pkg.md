# 包管理

---

## 包结构

普通库包：

```
<pkg_dir>/
    include/
        *.h
        *.hpp
    src/
        *.c
        *.cpp
        *.cxx
    ezmk.toml
```

utils 工具包（`type = "utils"`，详见 [`utils.md`](utils.md)）：

```
<utils_pkg>/
    ezmk.toml         # type = "utils"
    utils/            # Lua 脚本（必需）
        <name>.lua
    include/          # 可选
    src/              # 可选
```

---

## 包配置`ezmk.toml`

### `[project]` 节

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `name` | string | **是** | — | 包名（小写，可用连字符，如 `"my-lib"`） |
| `version` | string | **是** | — | 语义化版本号，如 `"1.2.3"` |
| `type` | string | 否 | `"executable"` | 包类型（见下方取值表） |
| `language` | string | 否 | `"C++17"` | 格式：`<语言><版本>`，如 `"C11"`、`"C++17"`、`"C++20"` |
| `header_only` | bool | 否 | `false` | **0.9.7+** Header-only 库（无需 `src/`，跳过编译） |
| `precompiled` | bool | 否 | `false` | **0.9.7+** 预编译包（`lib/` 提供预编译 `.a`，无需 `src/`），详见 [包制作指南](package_authoring.md#33-预编译包precompiled--true097) |
| `precompiled_strict` | bool | 否 | `false` | **1.2.0-dev.10+** 预编译包严格模式：工具链降级（L2/L1，可能 ABI 不兼容）改为 fail-fast 报错 |

`type` 字段支持以下取值：

| 值 | 说明 |
|---|---|
| `"executable"` | 可执行文件（默认） |
| `"static"` | 静态库 |
| `"shared"` | 动态库 |
| `"utils"` | 工具包（提供 `ezmk utils` 子命令，基于 Lua） |

> **为什么会有 header-only 和预编译包？** 两者都省去本地编译：`header_only = true` 仅分发头文件，纯头文件库安装即可用；`precompiled = true` 直接分发编译好的 `.a`，重型库无需长时间本地编译。

### `[depends]` 节

| 字段 | 类型 | 说明 |
|------|------|------|
| `lib` | string[] | 硬性依赖库名列表。缺失 → 安装失败 |
| `want` | string[] | **0.2.2+** 可选依赖库名列表。安装时若存在则作为正常依赖处理，缺失则跳过；构建时缺失 → warn + 定义 `EZMK_LIB_MISS_<NAME>` 宏 |

> **为什么同时提供 `lib` 和 `want`？** 硬依赖（`lib`）必须已安装否则构建失败；可选依赖（`want`）则优雅降级——缺失时仅警告并定义 `EZMK_LIB_MISS_<NAME>` 宏，代码可用回退实现继续构建。

---

## 包安装路径及缓存目录

| 安装模式 | 路径                       |
| -------- | -------------------------- |
| 全局     | `<ezmk_install_dir>/pkg/`  |
| 用户     | `~/.local/ezmk/pkg/`       |
| 项目     | `<project_dir>/.ezmk/pkg/` |

缓存一律保存到`<project_dir>/.ezmk/cache/`,区分编译标志和文件内容

---

## 包编译

每个普通库包都会按照依赖链逐个编译为 `*.a` 文件。

对于 `type = "utils"` 的工具包：
- 若任一 `[compile].src_dirs` 目录（默认 `src/`）包含源文件：编译 → `build/*.a`，同时注册 `utils/` 下的 Lua 工具
- 若全部 `src_dirs` 目录缺失或为空：跳过编译，仅解压并注册 Lua 工具

**`[compile]` 配置对包生效（1.2.0-dev.9+）**：

- **`src_dirs`**（默认 `["src"]`）：包源文件从配置的目录收集，支持多个目录（如 `["src", "generated"]`）；缺失目录 warn + 跳过、文件名重复时去重——与项目构建的 `collect_sources` 完全一致。包**总是**编译成静态库，`[project].type` 不触发 `main.cpp` 校验（包文档默认 `type = "executable"` 亦可）。
- **`include_dirs`**（默认 `["include"]`）：包自编译时相对包根解析为 `-I`，与默认 `include/` 重复时保序去重、缺失目录跳过；消费者侧同样生效——项目依赖包时，每个 `<pkg>/<include_dir>` 都会加入编译 `-I`。
- **空源 fatal**：非 header_only / precompiled / utils 却没有任何源文件的退化包，安装时报错（`no source files` / `src/ directory not found`），不再静默生成空库。

如果循环依赖或包不存在抛出错误

---

## 安装钩子脚本（0.2.1+，Lua 支持 0.9.9+）

包根目录下可放置 `script/` 目录，包含安装生命周期钩子。

**目录结构**：

```
<pkg_dir>/
    script/
        preinstall.lua    # 解压后、安装前执行（跨平台，0.9.9+，**推荐**）
        preinstall.sh     # 解压后、安装前执行（Linux/macOS，旧版兼容）
        preinstall.ps1    # 解压后、安装前执行（Windows，旧版兼容）
        preinstall.bat    # 解压后、安装前执行（Windows 备选，旧版兼容）
        postinstall.lua   # 安装完成后执行（跨平台，0.9.9+，**推荐**）
        postinstall.sh    # 安装完成后执行（Linux/macOS，旧版兼容）
        postinstall.ps1   # 安装完成后执行（Windows，旧版兼容）
        postinstall.bat   # 安装完成后执行（Windows 备选，旧版兼容）
```

**检测优先级（0.9.9+）**：
1. `.lua` — 跨平台，最高优先级（若存在则直接使用）
2. 平台特定脚本作为 fallback：`.ps1` → `.bat`（Windows）或 `.sh`（Linux/macOS）

> **为什么优先使用 `.lua` 钩子？** 一份 Lua 脚本即可跨平台运行且沙箱安全，替代各自独立的 `.sh` / `.ps1` / `.bat` 脚本。Shell 变体保留给尚未迁移的旧包作为兼容。

**执行流程**：
1. 解压包到临时目录
2. 检测并执行 `preinstall` 脚本（若存在）：
   - **Lua 脚本**（0.9.9+）：询问确认 → 在沙箱中执行（无需编辑器审查，API 受沙箱限制）
   - **Shell 脚本**（旧版）：打开编辑器供用户审查 → 询问确认 → 执行
3. 检查已有安装 → 若覆盖则二次确认
4. 编译依赖 + 复制文件到安装目录
5. 检测并执行 `postinstall` 脚本（若存在）→ 流程同步骤 2

- 若用户拒绝执行脚本，安装继续（跳过该阶段）
- 若脚本执行失败（exit ≠ 0），用户可选择继续或中止

### Lua 钩子脚本（0.9.9+）

Lua 安装钩子提供了跨平台、沙箱安全的替代方案，与 utils 和构建钩子共享相同的 `ezmk.*` API（参见 [Utils 文档](utils.md)）。

**入口约定**：每个 Lua 钩子需定义 `run(ctx)` 函数，返回整数 exit code（0 = 成功，非 0 = 失败）。

```lua
-- script/preinstall.lua
function run(ctx)
    ezmk.info("preinstall hook for " .. ctx.pkg_name)

    -- 示例：备份已有的配置文件
    if ezmk.file_exists(ctx.install_path .. "/config.ini") then
        ezmk.warn("发现已有 config.ini，正在备份...")
        local backup = ctx.install_path .. "/config.ini.bak"
        local ok = ezmk.file_write(backup,
            ezmk.file_read(ctx.install_path .. "/config.ini"))
        if not ok then
            ezmk.error("备份 config.ini 失败")
            return 1
        end
    end

    return 0
end
```

**上下文表 `ctx`**：

| 字段 | 类型 | 说明 |
|---|---|---|
| `ctx.pkg_name` | string | 包名（来自 `ezmk.toml` `[project].name`） |
| `ctx.pkg_root` | string | 包解压后的根目录（绝对路径） |
| `ctx.install_path` | string | 目标安装路径（绝对路径） |
| `ctx.scope` | string | 安装作用域：`"project"` / `"user"` / `"global"` |
| `ctx.pkg_version` | string | 包版本号（来自 `ezmk.toml` `[project].version`） |
| `ctx.pkg_type` | string | 包类型：`"executable"` / `"static"` / `"shared"` / `"utils"` |

**安全性**：Lua 钩子在沙箱环境中运行（无 `os.execute`、无 `io.open`，且禁用了 `dofile`/`loadfile`/`load`/`require`/`debug`/`package` 等文件加载与内省函数）。所有系统访问必须通过 `ezmk.*` API。与 Shell 脚本不同，Lua 钩子无需打开编辑器审查——沙箱边界已限定了脚本的能力范围。

**注意**：若未定义 `run()` 函数，ezmk 打印警告并跳过该钩子（继续安装流程）。

---

## 作用域参数

`-p` : 项目作用域
`-u` : 用户作用域
`-g` : 全局作用域

`-p`,`-u`,`-g`参数可以连用例如`-pug`

执行操作时会安装参数顺序查找

注: `ezmk pkg install`不支持多作用域

---

## 包来源

安装时已预注册官方默认仓库，可直接按包名安装（如 `ezmk pkg install fmt -u`）。包文件也可通过以下方式提供：

> **为什么一个命令同时接受文件、URL 和包名？** 一条 `pkg install` 即可覆盖获取包的三种途径——本地归档、远程下载、或在已注册仓库中按名查找。按名安装是注册仓库后的日常用法，文件/URL 形式处理其余场景。

### 本地文件

```bash
ezmk pkg install -p ./foo-0.1.0.zip
ezmk pkg install -u ~/downloads/bar-1.2.0.tar.gz
```

### 从文件夹安装（1.2.0-dev.7+）

参数为**已存在的目录**时，直接从该目录安装——目录结构须符合包规范（`include/` + 源码目录 + `ezmk.toml`，或预编译 `lib/` / header-only）。源码目录默认 `src/`，可用 `[compile].src_dirs` 自定义（1.2.0-dev.9+，校验与编译均按 `src_dirs` 生效）。开发/调试本地包时无需先打包成归档：

```bash
ezmk pkg install -p ./mylib          # ./mylib 为包源目录
ezmk pkg install -u ~/dev/bar        # 绝对路径亦可
```

- 目录安装跳过解压与 SHA-256 校验（无归档可校验）；显式传 `--sha256` 会提示跳过。
- 安装路径、作用域（`-p/-u/-g`）、安装钩子、依赖解析与归档安装完全一致。
- 本地 checkout 的 `ezmk-repo` 中 `packages/` 目录下**解包后的包目录**同样可直接安装。

### URL 下载

```bash
ezmk pkg install -p https://example.com/packages/foo-0.1.0.zip
ezmk pkg install -g example.com/packages/bar-1.2.0.tar.gz   # 省略协议头,默认 https://
```

URL 格式说明:
- 完整 URL: `https://<host>/<path>/<pkg>.zip` 或 `.tar.gz`
- 省略协议: `<host>/<path>/<pkg>.zip` → 自动补全为 `https://`
- 支持协议: `https://`、`http://`
- URL 自动识别：若参数包含 `://`，或同时包含 `.` 和 `/` 且并非本地已存在文件，则视为 URL
- 下载到 `.ezmk/temp/` 后解压安装，安装完成删除临时文件

> **为什么自动识别 URL？** 启发式规则（含 `://`，或同时含 `.` 与 `/` 且不是本地已存在文件）让 `pkg install` 能区分 URL 与文件路径，从而可以省略协议头——`example.com/path/pkg.zip` 默认按 `https://` 处理——同时不会把真实存在的本地归档误判为 URL。

### 仓库查找（0.1.3+）

如果已通过 `ezmk repo add` 注册了仓库，可以直接用包名安装而无需提供完整 URL 或文件路径：

```bash
ezmk repo add -p git@github.com:user/ezmk-repo.git --name my-repo
ezmk repo update
ezmk pkg install -p foo          # 自动在已注册仓库中搜索 "foo"
```

查找顺序：
0. 目录（参数为已存在的目录）→ 直接安装（1.2.0-dev.7+）
1. 本地文件路径 / 显式 URL（和之前一样）
2. 已注册仓库的本地缓存中按名称搜索（项目 → 用户 → 全局）
3. 仍未找到 → 报错

> **为什么搜索所有已注册仓库？** 这样按包名安装时，无论哪个仓库（或作用域）提供该包都能解析成功——无需记住包来自哪个仓库，镜像也能作为备用透明生效。

详见 [`repo.md`](repo.md)。

---

## 离线 / 无网络使用 [0.9.4+]

在无法访问互联网的环境中，有三种方案安装包：

> **为什么需要专门的离线方案？** 捆绑包已迁移到官方仓库（0.9.3），安装默认需要网络。这三个方案（本地镜像、手动归档、预置镜像）恢复了离线可用性。

### 方案一：本地仓库镜像

在有网络的机器上克隆仓库，然后在离线机器上注册为本地仓库：

```bash
# 在有网络的机器上
git clone https://github.com/3667808244/ezmk-repo.git /path/to/ezmk-repo

# 复制到离线机器后：
ezmk repo add /path/to/ezmk-repo --type local
ezmk pkg install <名称>
```

### 方案二：手动下载归档并安装

从 GitHub Releases（或任何来源）下载 `.tar.gz` / `.zip` 归档，传输到离线机器，然后从文件安装：

```bash
ezmk pkg install ./<包名>-<版本>.tar.gz --type file
```

### 方案三：USB / 内网共享上的预置镜像

在便携介质或内网共享上准备完整的仓库镜像：

```bash
# 在有网络的机器上准备
git clone https://github.com/3667808244/ezmk-repo.git /mnt/usb/ezmk-repo

# 在每台离线机器上
ezmk repo add /mnt/usb/ezmk-repo --type local
```

> 更多离线场景参见 [常见问题](faq.md)。
