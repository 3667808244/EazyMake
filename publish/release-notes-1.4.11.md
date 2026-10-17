# EazyMake 1.4.11 — 代码质量审计修复（第四轮）

本版为第四轮代码质量审计修复，共 47 项（P1 4 / P2 20 / P3 23）。**零功能新增；公共 API 无破坏性变更。**

### 重点修复

- **进程安全**：POSIX \`run_command\` 不再在 \`fork()\` 后调用 \`setenv()\`（多线程 \`-jN\` 下的死锁风险）。
- **Lua 权限**：\`ezmk.run\` / \`run_capture\` 改为逐段校验（修复 \`git ; rm -rf ~\` 绕过 allow/deny/Ask）；含 shell 高级构造的命令拒绝执行。
- **数据完整性**：\`cache.cpp\` 对象与 \`pkg.cpp\` 归档改走 \`atomic_rename\`（回退 copy 失败不再删除唯一成品/报告成功）；\`.tmp\` 清理；\`ThreadPool\` 线程创建失败不再 terminate。
- **构建正确性**：test 缓存随编译器版本/deterministic 失效；executable 接受 \`main.cc\`/\`main.cxx\`；单源失败保留其余缓存；测试源按路径去重。
- **包/锁**：\`pkg remove\` 剪除 lockfile（确定性构建不再 fatal）；跨仓库版本选择保留预发布语义；precompiled 包内容固定；\`compare_version\` 分量饱和；lockfile 约束可被 \`ezmk.toml\` 读回。
- **Lua/i18n**：\`lua_pushlstring\` 保留内嵌 NUL；深 JSON 报错而非栈溢出；权限 glob 锚定项目根；\`ezmk-lua\` 捕获异常。
- **配置**：语言区间按年份序号比较（\`C++98..C++11\` 不再误拒）；链式 link 解析 + 循环检测；已知布尔/workspace 键类型错误 fail-fast；\`normalized_lang\` 补默认版本。
- **import/export/workspace**：条件链只执行首个为真分支；平台常量按宿主；导出 CMake 路径引号化、写失败报错；workspace 重复成员拒绝、test-less 依赖仍构建、绝对 \`--report\` 拒绝。
- **project/CLI/util**：\`project new\` 名称校验；\`--glob\` 生效并拒绝与 \`--no-glob\` 冲突；Windows 引号修正；\`$CXX\` 引号化；\`/INCREMENTAL\` 等不再误伤；Windows 下载 \`host:port\`；\`safe_extract_path\`；gzip CRC32/ISIZE 校验。

### 行为收紧

- 未知顶层命令、\`pkg remove/update\`、\`repo remove/update/info\` 未找到目标时**退出非零**。
- 损坏的 gzip 归档、超深 JSON、含 shell 构造的受限 \`ezmk.run\` 从「静默通过」变为明确报错。
- i18n：新增 12 个 fail-fast 键（418 → 430），en/zh/zh-TW 三向一致。

### 测试

- 全量 **1169 用例 / 6716 断言 / 4 跳过 / 0 失败**（Windows，发布前实测）；Linux 侧另做单元与集成验证。
- \`check_i18n.py\`、\`check_man_sync.py\`、\`check_docs_sync.sh\` 通过，groff 零告警。

**安装**：\`install.sh\`（Linux/macOS）、\`install.ps1\`（Windows）、\`winget install EazyMake.EazyMake\`、\`brew install 3667808244/eazymake/ezmk\`、\`publish/arch/PKGBUILD\`（Arch/MSYS2）。

**完整变更**：见 \`CHANGES.md\` 的 1.4.11 节；设计与执行计划见 \`plans/1.4.x/1.4.11.md\`。
