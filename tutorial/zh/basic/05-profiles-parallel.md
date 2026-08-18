# 5. 构建配置与并行编译

## 并行编译

默认情况下 `ezmk` 会使用所有可用核心并行编译源文件：

```bash
$ ezmk build          # -j 0（自动）为默认值
$ ezmk build -j4      # 恰好 4 个并行任务
$ ezmk build -j1      # 串行编译（便于阅读错误输出）
```

`-j 0` 通过硬件线程数自动检测。

## 构建配置（Profile）

构建配置是一组通过 `--profile` 主动选择的额外编译/链接选项。
将它们添加到 `ezmk.toml` 中：

```toml
[compile.profile.debug]
flags = ["-g", "-O0"]

[compile.profile.debug.macros]
DEBUG = true

[compile.profile.release]
flags = ["-O3", "-DNDEBUG"]

[link.profile.release]
flags = ["-s"]           # 去除符号
```

应用某个构建配置：

```bash
$ ezmk build --profile debug
$ ezmk run   --profile release
```

规则：

- 构建配置**不会**自动应用——你必须手动传入 `--profile`。
- 构建配置的 `flags` **追加在**基础标志之后，因此冲突时后者覆盖前者
  （与 GCC/Clang "最后的标志生效" 行为一致）。
- 构建配置的 `macros` **合并进**基础宏中；键冲突时以构建配置为准。

不同的构建配置会产生不同的编译指纹，因此切换构建配置时会按需重新构建，
而不会破坏其他构建配置的缓存。

> 💡 想直接跑完整示例？运行 `ezmk example greeter` 生成一个静态库项目（示例列表见 [`examples/README.md`](../../../examples/README.md)）。

下一章：[使用包 →](../packages/01-packages.md)
