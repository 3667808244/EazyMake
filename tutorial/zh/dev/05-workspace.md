# 15. 工作区：批量管理一组项目

> 1.3.0+。`ezmk workspace` 把**一个目录下若干独立项目（成员）**组织起来统一管理：一次命令构建/测试/清理全部成员，成员之间可以声明依赖并自动复用产物——monorepo 最常见的形态「共享基础库 + 多个可执行文件」用一条 `ezmk workspace build` 搞定。

## 场景

假设你有一个 monorepo：一个静态库 `strutil`（字符串工具）+ 两个可执行文件 `tool-a` / `tool-b`，后两者都依赖 `strutil`：

```
ws/
├── ezmk-workspace.toml          # 工作区配置
├── libs/strutil/                # 静态库成员
│   ├── ezmk.toml
│   ├── include/strutil.hpp
│   └── src/strutil.cpp
└── apps/
    ├── tool-a/                  # 可执行成员，依赖 strutil
    │   ├── ezmk.toml
    │   └── src/main.cpp
    └── tool-b/                  # 可执行成员，依赖 strutil
        ├── ezmk.toml
        └── src/main.cpp
```

没有工作区时，你需要 `cd` 进每个项目分别 `ezmk build`，而且 `tool-a` 要手动把 `strutil` 的头文件目录和 `libstrutil.a` 加进自己的配置。工作区把这些都自动化了。

## 第 1 步：声明工作区

在根目录写 `ezmk-workspace.toml`：

```toml
[workspace]
members = ["apps/tool-a", "apps/tool-b", "libs/strutil"]
```

`members` 是相对工作区根的路径（必填、非空）。可选 `[workspace.options]`：

```toml
[workspace.options]
default_jobs = 4        # 层内并行任务数（默认 0 = 自动）
stop_on_error = false   # 首个失败后停止派发（默认 false）
```

## 第 2 步：成员声明依赖

每个成员仍然是**独立的 `ezmk` 项目**（有自己的 `ezmk.toml`）。`tool-a` 只需在 `ezmk.toml` 里加一行声明对兄弟成员 `strutil` 的依赖：

```toml
[depends]
workspace = ["strutil"]        # 兄弟成员：末段（唯一时）或完整相对路径 "libs/strutil"
```

`strutil` 自己**什么都不用改**。构建 `tool-a` 时，`ezmk` 会自动注入 `strutil` 的头文件与静态库：

```
-I ws/libs/strutil/include -L ws/libs/strutil/build -lstrutil
```

> **零环境变量**：注入是成员进程**自发现**的（读取工作区文件 + 自己的 `[depends] workspace`），不依赖任何 `EZK_WS_*` 环境变量——工作区再大，命令行长度也不会随之增长。

## 第 3 步：一次构建全部成员

```bash
$ cd ws
$ ezmk workspace build -j 4
workspace build: 3 member(s), 4 job(s)
[libs/strutil] build...
[apps/tool-a] build...
[apps/tool-b] build...
workspace build: 3 succeeded, 0 failed, 0 skipped
```

- **拓扑顺序**：依赖层先构建——`strutil` 先于 `tool-a` / `tool-b`。
- **层内并行**：互不依赖的成员同时构建（`-j` 控制并行度，`-w` 与 `workspace build -w` 等价：`ezmk build -w` ≡ `ezmk workspace build`，从工作区内任意子目录都能用）。

## 第 4 步：跨成员增量

工作区的核心价值：**改库的代码，依赖者自动重链/重编，不用手动 clean**。

```bash
# 改 strutil 的实现（.cpp）→ 下一次 workspace build：
#   strutil 重编 + tool-a/tool-b 只重新链接（它们的编译全部缓存命中）
$ ezmk workspace build -j 4

# 改 strutil 的头文件（.hpp）→ 下一次 workspace build：
#   引用该头的成员自动重新编译（depfile 收录注入的头文件）
$ ezmk workspace build -j 4
```

## 第 5 步：测试与清理

```bash
$ ezmk workspace test             # 逐成员 ezmk test；没有测试的成员自动跳过（不报错）
$ ezmk workspace clean            # 按依赖逆序清理成员（清缓存/临时目录，build/ 产物保留）
```

## 只构建一部分成员

- **`--member <name>` 含依赖闭包**：`ezmk workspace build --member tool-a` 会先构建 `tool-a` 的依赖 `strutil`，再构建 `tool-a`——保证产物新鲜。`--member apps/tool-a`（完整路径）与 `--member tool-a`（末段）等价。
- **只构建单个成员、不要闭包**：`cd apps/tool-a && ezmk build`——只构建当前成员，注入**已存在**的兄弟产物（`strutil` 没构建过就会提示并可能导致链接失败）。

## 失败时怎么办

```bash
$ ezmk workspace build --stop-on-error -j 4
```

`--stop-on-error` 的精确语义：某个成员失败后**不再派发新任务**——同一层还没启动的成员和所有后续层标记 `skipped`（摘要里能看到计数），已经在运行的成员**自然结束、不会被打断**。不带这个标志时，全部成员跑完再汇总，任何一个失败退出码非零。`clean` 不支持该标志。

## 约束与限制

- 成员依赖必须**单向非循环**——环（含自环）在配置加载时就报错。
- 被依赖的成员必须是 `type = "static"`（`executable` / `shared` 不能被依赖）。
- 成员依赖**无版本**——开发中即改即用；要版本化、可分发复用，请用包（`[depends] lib` + `ezmk pkg install`）。
- 完整语义见 [`docs/zh/cli.md`](../../../docs/zh/cli.md) 的 `workspace` 节；配置见 [`docs/zh/config_file.md`](../../../docs/zh/config_file.md) 的 `[depends] workspace`。
