# EazyMake 1.4.8 — 内嵌依赖与 CI 依赖更新

本版把三处内嵌依赖与 CI 依赖推到上游当前版本，**零功能新增、公共 API 无破坏性变更**：miniz 2.2.0 → 3.1.2、Lua 5.4.7 → 5.4.9、Catch2 3.8.0 → 3.16.0，以及 GitHub Actions 主版本（checkout / upload-artifact → v7，action-gh-release → v3）。

### 更新

- **miniz 2.2.0 → 3.1.2**（ZIP/压缩解压）：读 zip 头时中央目录偏移溢出、`tinfl_decompress` 死循环、Windows `mz_utf8z_to_widechar` 缓冲区溢出、`MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_UTF8` 未设置、MinGW32 Unicode 路径等修复；`pkg install` 解外部归档更健壮。
- **Lua 5.4.7 → 5.4.9**（内嵌脚本引擎）：5.4 线的终版，修掉 5.4.7 / 5.4.8 的 6 个上游 bug（含代码生成正确性与 GC 正确性）。保留既有的 `io` / `os` 沙箱移除补丁。
- **Catch2 3.8.0 → 3.16.0**（仅测试依赖，不进 `ezmk` 产物）。
- **GitHub Actions**：`checkout` / `upload-artifact` v4 → v7、`action-gh-release` v2 → v3（Node 20 → Node 24 运行时；入参未变）。

### 行为变更

- **Catch2 ≥ 3.16 的 `--order` 默认值由 `decl` 改为 `rand`**：`build.sh` 的两个测试调用补 `--order decl`，保持回归确定性（本套件多个 `[lua]` 用例共享进程级 `lua_State`）。
- miniz 3.x 会为 zip 设置 UTF-8 general purpose flag → `project pack --format zip` 的产物字节与 1.4.7 不同（`.sha256` 边车按产物现算，不受影响）；解包对畸形归档更严格。

**测试**：全量 **1133 用例 / 6525 断言**，失败集合与本机立项基线**完全一致**（9 个 `test_integration_git.cpp` 环境性失败：MSYS2 下 `file:///D:/...` 被当 POSIX 路径，与依赖升级无关）。

**安装**：`install.sh`（Linux/macOS）、`install.ps1`（Windows）、`winget install EazyMake.EazyMake`、`brew install 3667808244/eazymake/ezmk`、`publish/arch/PKGBUILD`（Arch/MSYS2）。

**完整变更**：见 `CHANGES.md` 的 1.4.8 节；设计与执行计划见 `plans/1.4.x/1.4.8.md`。
