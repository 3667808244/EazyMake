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

utils 工具包（`type = "utils"`，详见 `docs/utils.md`）：

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

### `depends` 节

 - `lib` : 依赖的其他库名

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

详见 `docs/repo.md`。
