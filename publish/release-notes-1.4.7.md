# EazyMake 1.4.7 — MSVC 工具链支持修复 + 工具链优先级调整

本版来自 1.4.6 Q-20 的真实 MSVC 验证：**MSVC 支持此前实际上从未可用**——Build Tools 探测、vcvars 命令引用、MSVC 环境注入三处缺陷使该路径成为死代码。1.4.7 把它修好，并落实优先级：**g++/clang++ 优先，MSVC 回退**。**公共 API 无破坏性变更**（仅新增 `toolchain::msvc_env` / `util::merge_env`）。

### 修复

- **Build Tools 探测**：vswhere 查询加 `-products *`（默认过滤会排除 VS Build Tools，实测无输出），回退补 `2022\BuildTools`（Program Files 两处）。
- **vcvars 命令引用**：探测与 `load_msvc_env()` 从 `cmd /c "call \"...\""`（cmd 收到字面 `\"`，探测恒失败）改为 `cmd /c call "..."`；并**剥离 `set` 输出行尾的 CR**——原先每个环境变量值都带尾部 `\r`，使 `TMP` 成为非法路径（cl 报 D8037）。
- **MSVC 环境注入**：新增 `toolchain::msvc_env()`（进程内缓存一次）+ `util::merge_env()`，把 vcvars 环境注入编译 / 链接 / 归档 / 测试 / 包编译子进程；并按 vcvars 的 `PATH` 解析 **cl.exe / link.exe / lib.exe 的绝对路径**（`CreateProcessW` 只用父进程 `PATH` 查找可执行文件，不看子进程 env block）。MSVC 现在**无需“开发者命令行”**即可工作。

### 行为变更

- **工具链优先级反转**：探测顺序由“MSVC 优先 → MinGW”改为 **`$CXX`/`$CC` → 系统 g++/clang++ → MSVC 回退**。本机/CI 的 MinGW 工作流不变；装了 Build Tools 的机器不再默认切 MSVC。
- **新增 `EZMK_TOOLCHAIN=gcc|clang|msvc`**：显式选择工具链；非法值明确报错；指定但不可用时报错而非静默回退（`$CXX`/`$CC` 仍最优先）。

**测试**：全量 **1133 用例 / 6541 断言零失败**（基线 1.4.6 收口态 1130 / 6536）。本机 VS 2022 Build Tools 实测：`cl` 编译、`link.exe` 可执行与 DLL、`lib.exe` 静态库全部成功且产物可运行（含空格路径），**未从开发者命令行启动**。

**安装**：`install.sh`（Linux/macOS）、`install.ps1`（Windows）、`winget install EazyMake.EazyMake`、`brew install 3667808244/eazymake/ezmk`、`publish/arch/PKGBUILD`（Arch/MSYS2）。

**完整变更**：见 `CHANGES.md` 的 1.4.7 节；设计与执行计划见 `plans/1.4.x/1.4.7.md`。
