# 12. 语义化版本约束与确定性构建

`ezmk pkg install` 默认安装可用版本中的最高版本。当项目需要长期维护、团队协作、CI 可复现时，需要两件工具：**版本约束**（声明接受哪些版本）和 **`ezmk.lock`**（把实际安装的精确版本钉死）。

## 给依赖加版本约束

约束写在 `ezmk.toml` 的 `[depends]`（硬性依赖）或 `want`（可选依赖）中：

```toml
[depends]
lib = [
    "fmt",
    "spdlog@1.14.1",     # 精确版本
    "catch2^3.6.0",      # 兼容版本：>=3.6.0, <4.0.0
    "nlohmann_json~3.11" # 近似版本：>=3.11, <3.12
]
want = [
    "yaml-cpp>=0.8.0"
]
```

| 语法 | 含义 | 示例 |
|--------|---------|---------|
| `pkg@1.2.3` | 精确版本 | `fmt@10.2.1` |
| `pkg^1.2.3` | 兼容版本（主版本不变） | `spdlog^1.14.0` → `>=1.14.0, <2.0.0` |
| `pkg~1.2.3` | 近似版本（次版本不变） | `nlohmann_json~3.11.0` → `>=3.11.0, <3.12.0` |
| `pkg>=1.2.3` | 大于等于 | `zlib>=1.2.0` |
| `pkg>1.2.3` | 严格大于 | `boost>1.80.0` |
| `pkg` | 无约束（取最新） | `fmt` — 取可用最高版本 |

写好后安装：

```bash
$ ezmk pkg install
```

> 约束无法满足时安装失败，并列出所有可用版本。

## ezmk.lock：钉死实际安装

`ezmk pkg install` 在项目根目录写入 `ezmk.lock`（TOML 格式），记录每个已安装包的**精确版本**、`sha256`、平台与依赖图：

```toml
[metadata]
version = 1
generated_by = "ezmk 1.1.0"
toolchain = "gcc"
direct_deps = ["fmt", "spdlog@^1.14.0"]

[[packages]]
name = "spdlog"
version = "1.14.1"
sha256 = "..."
type = "static"
scope = "user"
platform = "windows_x86_64_msvc"
dependencies = []
```

- **生成**：每次 `ezmk pkg install` 自动写入/更新。
- **`--locked`**：只按现有 `ezmk.lock` 安装，不一致则**报错**——CI 用它保证"锁里没有的绝不装"。
- **`--no-lock`**：跳过 lockfile 生成。
- **请勿手改**：`ezmk.lock` 是自动生成文件；要变更依赖，编辑 `ezmk.toml` 后重新安装。

```bash
$ ezmk pkg install --locked
```

## deterministic：把校验变成硬性要求

默认情况下 lockfile 缺失或内容不一致只是**警告**。`[compile] deterministic = true` 把它变成构建期的硬性检查：

```toml
[compile]
deterministic = true
```

- lockfile 缺失或校验失败 → **构建报错**（而非警告）
- lockfile 内容哈希纳入编译缓存签名——依赖变化自动失效缓存

```bash
$ ezmk build
```

## 易错点

- **`ezmk.lock` 应随项目提交**（不要加进 `.gitignore`）——它是可复现构建的一部分，团队和 CI 都依赖它。
- **约束在安装时解析**：lockfile 生成后，日常 `install`/`build` 遵循锁定的版本；重新解析发生在 `pkg update` 时。
- **不带运算符的条目保持"取最新"**：`"fmt"` 与 `"fmt@10.2.1"` 语义不同——前者在 `pkg update` 时可能跳到新版本。
