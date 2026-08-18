# 9. 测试你的项目

`ezmk test` 会在需要时先构建项目，然后编译并运行你的测试。
一条命令即可得到通过/失败汇总，任何失败都会返回非零退出码——因此测试可以用在 CI 和提交前的钩子中。

## 配置测试

测试默认放在 `test/` 目录，在 `[test]` 中配置：

```toml
[test]
dirs = ["test"]        # 测试源码目录
framework = "catch2"   # "catch2"（默认）或 "ezmk"
flags = []             # 测试构建的额外编译参数
```

有两种框架。两者都遵循"零配置"哲学——把文件交给 `ezmk test`，其余由它处理。

## 框架一：Catch2（默认）

默认框架使用 [Catch2](https://github.com/catchorg/Catch2)。可以通过包安装（在 `[depends]` 中 `ezmk pkg install catch2`），或将单头文件放入 `include/vendor/catch2.hpp`。

```cpp
// test/math_test.cpp
#include <catch2/catch_all.hpp>   // 或使用 vendored 版本时写 "catch2.hpp"

int add(int a, int b);

TEST_CASE("add works", "[math]") {
    REQUIRE(add(2, 3) == 5);
}
```

```bash
$ ezmk test
Running tests (Catch2)...
===============================================================================
All tests passed (1 assertion in 1 test case)
```

用 `--filter` 按测试名过滤，用 `-V` 详细输出（包括通过的用例）：

```bash
$ ezmk test --filter "add works"
$ ezmk test -V
```

## 框架二：ezmk 内置

想要零依赖——完全不使用框架头文件——就使用 `framework = "ezmk"`。
每个测试文件会被编译成**独立的可执行文件**（与项目目标文件链接，排除 `main`）。退出码即判定：`0` = 通过，非零 = 失败。挂起的测试会在 30 秒后被终止，并报告为超时。

```cpp
// test/basic_test.cpp
#include <cstdlib>

int add(int a, int b);

int main() {
    if (add(2, 3) != 5) return 1;   // 非零 → 失败
    if (add(0, 0) != 0) return 1;
    return 0;                        // 零 → 通过
}
```

```bash
$ ezmk test -f ezmk
Running tests (ezmk)...
  [PASS] basic_test  (12ms)
```

此模式下 `--filter` 匹配文件名子串（`ezmk test --filter basic`）。

## 运行

| 命令 | 含义 |
|---|---|
| `ezmk test` | 构建 + 运行测试（默认 Catch2） |
| `ezmk test -f ezmk` | 使用内置框架运行 |
| `ezmk test --filter <name>` | 只运行匹配 `<name>` 的测试 |
| `ezmk test -V` | 详细输出——显示每个测试，包括通过的 |

如果项目尚未构建，`ezmk test` 会先构建。测试套件失败时返回非零退出码，可直接接入你的 CI。

> 💡 想直接跑完整示例？运行 `ezmk example with-tests` 生成带 Catch2 测试的项目（运行前先 `ezmk pkg install catch2 -y`；示例列表见 [`examples/README.md`](../../../examples/README.md)）。
