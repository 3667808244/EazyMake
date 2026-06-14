# 包管理

---

## 包结构

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

---

## 包配置`ezmk.toml`

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

每个包都会按照依赖链逐个编译为`*.a`文件

如果循环依赖或包不存在抛出错误

---

## 作用域参数

`-p` : 项目作用域
`-u` : 用户作用域
`-g` : 全局作用域

`-p`,`-u`,`-g`参数可以连用例如`-pug`

执行操作时会安装参数顺序查找

注: `ezmk install`不支持多作用域

---

还有EazyMake没有仓库,包文件需要手动下载后执行命令安装,或者提供包的URL(可省略协议头,默认https)由EazyMake下载