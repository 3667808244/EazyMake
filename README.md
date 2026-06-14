# EazyMake

一个简单的C/C++构建工具
基于GCC-g++(Windows下可以使用MSYS2环境)
包管理使用即时编译

(本工具只专注于易于使用,想要复杂功能可以用CMake)

---

## 命令行

```bash
# 基本项目管理
ezmk new <project_name> # 创建项目
ezmk build [--disable-cache] # 构建项目
ezmk run [--disable-cache] # 构建并运行
ezmk claer # 清理缓存和临时文件

# 包管理 -p(project) -u(user) -g(globl)
ezmk install [-p|-u|-g] <pkg_file> # 安装包文件,作用域参数默认为 -p
ezmk remove [-p|-u|-g] <pkg> # 移除包,作用域参数默认为 -pug
ezmk search [-p|-u|-g] <pkg> # 查找包,作用域参数默认为 -pug
ezmk info [-p|-u|-g] <pkg> # 查看包信息,作用域参数默认为 -pug
```

---

## EazyMake 默认生成项目结构

```
<project_dir>/
    .ezmk/
        pkg/
            <pkg_dirs>/
        temp/ # 该目录及子目录,子文件,不用于构建时识别项目
            *.*
        cache/  # 该目录及子目录,子文件,不用于构建时识别项目
            record.json
            *.*
    include/
        *.h
        *.hpp
    src/
        *.c
        *.cpp
        *.cxx
        main.cpp
    build/
    ezmk.toml
    README.md # 不用于构建时识别项目
```

## EazyMake 包结构

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

包文件即为以上目录的zip或tar.gz包
安装包时(如果符合以上包结构)会复制所有压缩包中的文件,不仅仅是以上列举的文件

---

其他说明参见`docs/*.md`
注: `docs/@*.md`为规范AI代码的风格而写,使用者阅读作用不大