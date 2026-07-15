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

`type` 字段支持以下取值：

| 值 | 说明 |
|---|---|
| `"executable"` | 可执行文件（默认） |
| `"static"` | 静态库 |
| `"shared"` | 动态库 |
| `"utils"` | 工具包（提供 `ezmk utils` 子命令，基于 Lua） |

### `[depends]` 节

| 字段 | 类型 | 说明 |
|------|------|------|
| `lib` | string[] | 硬性依赖库名列表。缺失 → 安装失败 |
| `want` | string[] | **0.2.2+** 可选依赖库名列表。安装时若存在则作为正常依赖处理，缺失则跳过；构建时缺失 → warn + 定义 `EZMK_LIB_MISS_<NAME>` 宏 |

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
- 若包含 `src/`：编译 `src/` → `build/*.a`，同时注册 `utils/` 下的 Lua 工具
- 若不包含 `src/`：跳过编译，仅解压并注册 Lua 工具

如果循环依赖或包不存在抛出错误

---

## 安装钩子脚本（0.2.1+）

包根目录下可放置 `script/` 目录，包含安装生命周期钩子：

```
<pkg_dir>/
    script/
        preinstall.sh     # 解压后、安装前执行（Linux/macOS）
        preinstall.ps1    # 解压后、安装前执行（Windows）
        preinstall.bat    # 解压后、安装前执行（Windows 备选）
        postinstall.sh    # 安装完成后执行（Linux/macOS）
        postinstall.ps1   # 安装完成后执行（Windows）
        postinstall.bat   # 安装完成后执行（Windows 备选）
```

**执行流程**：
1. 解压包到临时目录
2. 检测并执行 `preinstall` 脚本（若存在）→ 打开编辑器供用户审查 → 询问确认
3. 检查已有安装 → 若覆盖则二次确认
4. 编译依赖 + 复制文件到安装目录
5. 检测并执行 `postinstall` 脚本（若存在）→ 同样审查+确认

- 平台选择：Windows 优先 `.ps1` 其次 `.bat`；Linux/macOS 使用 `.sh`
- 若用户拒绝执行脚本，安装继续（跳过该阶段）
- 若脚本执行失败（exit ≠ 0），用户可选择继续或中止

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

EazyMake没有中央仓库,包文件可通过以下方式提供:

### 本地文件

```bash
ezmk pkg install -p ./foo-0.1.0.zip
ezmk pkg install -u ~/downloads/bar-1.2.0.tar.gz
```

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

### 仓库查找（0.1.3+）

如果已通过 `ezmk repo add` 注册了仓库，可以直接用包名安装而无需提供完整 URL 或文件路径：

```bash
ezmk repo add -p git@github.com:user/ezmk-repo.git --name my-repo
ezmk repo update
ezmk pkg install -p foo          # 自动在已注册仓库中搜索 "foo"
```

查找顺序：
1. 本地文件路径 / 显式 URL（和之前一样）
2. 已注册仓库的本地缓存中按名称搜索（项目 → 用户 → 全局）
3. 仍未找到 → 报错

详见 [`repo.md`](repo.md)。

---

## 离线 / 无网络使用 [0.9.4+]

在无法访问互联网的环境中，有三种方案安装包：

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
