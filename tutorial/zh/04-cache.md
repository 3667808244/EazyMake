# 4. 增量构建与缓存

`ezmk` 只重新编译发生变化的部分。它会对每个源文件的内容（以及它包含的每一个
头文件）进行哈希计算，并与记录的指纹进行比对。未变化的文件则直接复用缓存的
`.o`。

## 实际演示

先添加第二个源文件，这样就有东西可以*不用*重新编译：

```bash
$ cat > src/greet.cpp <<'EOF'
#include <iostream>
void greet(){ std::cout << "hi\n"; }
EOF
$ ezmk project build --verbose
```

构建一次，然后在没有任何改动的情况下再构建一次：

```bash
$ ezmk project build --verbose
[ezmk] Building hello (executable, C++17)...
[ezmk]   [cached] src/main.cpp  (source hash matches, 1 headers unchanged)
[ezmk]   [cached] src/greet.cpp  (source hash matches, 1 headers unchanged)
[ezmk]   2 cached, 0 compiled
[ezmk] Build successful: build/hello
```

两个文件都命中缓存——没有任何重新编译。现在只修改一个文件：

```bash
$ echo '// tweak' >> src/greet.cpp
$ ezmk project build --verbose
[ezmk]   [cached] src/main.cpp  (source hash matches, 1 headers unchanged)
[ezmk]   1 cached, 1 compiled
[ezmk] Build successful: build/hello
```

只有 `greet.cpp` 被重新编译（`1 cached, 1 compiled`）。编辑某个**头文件**
会重新编译所有直接或间接 `#include` 它的源文件。

## 缓存存放位置

```
.ezmk/cache/
  record.json     # 每个文件的源文件 + 头文件哈希及编译标志
  obj/            # 缓存的 .o 文件
```

缓存写入是原子的（先写 `.tmp` 再重命名），因此构建中途中断绝不会损坏缓存。

## 强制重新构建

```bash
$ ezmk project build --disable-cache   # 全部重新编译（之后缓存仍会更新）
$ ezmk project clean                   # 删除 .ezmk/cache 及临时文件
```

修改编译标志（例如编辑 `[compile].flags`）会自动使缓存失效——因为标志是
指纹的一部分。

完整的算法说明请参阅 [`docs/@cache.md`](../../docs/zh/@cache.md)。

下一章：[构建配置与并行编译 →](05-profiles-parallel.md)
