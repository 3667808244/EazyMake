# EazyMake 1.4.6 — 代码质量审计修复（第三轮）

1.4.6 承接 1.4.0-dev.6（第一轮）与 1.4.2（第二轮）的同一主题：**修错误路径与不可信输入边界**。零功能新增、**公共 API 无破坏性变更**。

### 修复（Q-01~Q-32 摘要）

- **内存 / 数据完整性**：zip 解压句柄二次 `fclose`；`pkg` 目录 / git 安装事务回滚（失败不再留下半成品）；`atomic_rename` 仅在复制成功后才删除源；`create_zip` 走临时文件原子替换；目录 / 归档扫描排序。
- **编译正确性与确定性**：MSVC 响应文件参数转义（`\` / `"`）；`report_fmt` 转义；TOML 引号控制字符；link-only profile 允许；find-before-insert。
- **归档 / 下载健壮性**：tar 八进制解析严格化 + 减法边界 + 负 tinfl 状态；gzip 头越界；`curl --fail` + URL 文件名净化 + 临时下载守卫；Windows 下载错误处理；`CreatePipe` / `mz_deflateInit2` 返回值检查。
- **并发 / 平台**：`detect_toolchain` 互斥缓存；macOS `close` 后置 `fd = -1`；inotify `EAGAIN` 重试；Win32 清理先排空 IOCP 再释放 `OVERLAPPED`；默认 `-j` 上限收敛到 0..1024。
- **版本解析**：`stoul` 边界 → `util::parse_version_component`（饱和到 `ULONG_MAX`）。
- **锁文件 / 仓库**：仅头文件 git 依赖不再短路；缺失产物视为不匹配；`save` / `save_repo_list` 写失败改为致命；损坏 `list.json` 抛错。

**测试**：全量 **1130 用例 / 6536 断言零失败**（基线 1.4.5 发布态 1113 / 6491）。

**安装**：`install.sh`（Linux/macOS）、`install.ps1`（Windows）、`winget install EazyMake.EazyMake`、`brew install 3667808244/eazymake/ezmk`、`publish/arch/PKGBUILD`（Arch/MSYS2）。

**完整变更**：见 `CHANGES.md` 的 1.4.6 节；设计与执行计划见 `plans/1.4.x/1.4.6.md`。
