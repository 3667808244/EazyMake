# 2. 你的第一个项目

## 脚手架

```bash
$ ezmk project new hello
$ cd hello
```

这会创建：

```
hello/
  ezmk.toml        # 项目配置
  src/
    main.cpp       # 一个 "Hello world!" 入口文件
  include/         # 你的头文件放在这里
  .gitignore
  .git/            # 已初始化的 git 仓库
```

> 使用 `--disable-git-init` / `--disable-gitignore` 可以跳过 git 或 `.gitignore` 的生成。
> 使用 `--type static|shared|utils` 可以选择不同的项目类型（默认为 `executable`）。

生成的 `src/main.cpp`：

```cpp
#include <iostream>

int main(int argc, char **argv){
    std::cout << "Hello world!" << std::endl;
    return 0;
}
```

## 构建

```bash
$ ezmk project build
[ezmk] Building hello (executable, C++17)...
[ezmk]   0 cached, 1 compiled
[ezmk]   Linking hello...
[ezmk] Build successful: build/hello
```

`ezmk` 编译 `src/` 下的所有源文件，将它们链接，然后将可执行文件输出到
`build/`。（在 Windows/MSYS2 上，二进制文件是 `hello.exe`。）

## 运行

```bash
$ ezmk project run
[ezmk] Building hello (executable, C++17)...
[ezmk]   1 cached, 0 compiled
[ezmk]   Linking hello...
[ezmk] Build successful: build/hello
[ezmk] Running hello...
Hello world!
```

`run` 会先构建（如果需要的话），然后执行。在 `--` 之后可以传递参数给*你的*程序：

```bash
$ ezmk project run -- --name world
```

`--` 之后的所有内容都会直接传给你的程序，而不是传给 `ezmk`。

## 命令简写

每个命令都有两字母的别名：

```bash
$ ezmk pn hello     # project new
$ ezmk pb           # project build
$ ezmk pr           # project run
$ ezmk pc           # project clean
```

下一步：[理解 `ezmk.toml` →](03-config.md)
