# EazyMake 1.5.0 — 用户/全局配置层

1.5.0 是 1.4.11 之后的第一个功能 minor：在项目级 ezmk.toml 之外新增**用户级 / 全局级 config.toml**，提供个人或整机默认值；不创建任何 config.toml 时行为与 1.4.11 完全一致。本版聚合 dev.1~dev.12 + pre.1/pre.2，公共 API 无破坏性变更。

### 重点

- **配置层**：全局/用户 config.toml、[base_dir]、[settings]、项目级覆盖、--config / --no-config / --lang、日志子系统；合并顺序 内置 < 全局 < 用户 < 项目 < CLI/env。
- **开发体验**：项目级 agent skills（gen_agent_skills / gen_agent_md）、compiler_output / linker_output、cxx / cc / linker / use_cache / network_timeout / retries / proxy / install_prefix、首次运行生成全注释模板。
- **质量与安全**：55 项审核修复——Lua 沙箱 512 MiB + 900 s 限额、三平台递归 watch、临时目录 0700、原子写 + fsync、pack/tar 加固、waitpid EINTR、linker/C 缓存、export CMake 引号、ar/MSVC response file、--locked 预搜索、TOML 严格校验、i18n 清理、MSVC 实机验证与 PDB 修复。
- **C-02**：预发布版本约束使用 semver precedence，=1.2.3 不再匹配 1.2.3-rc.1。

### 行为变更

- **[project].name 必填**：缺失 name 的项目在解析期报错；旧配置请在 [project] 补 name（安全 basename）。
- **递归 watch**：Linux/macOS 监听整棵子树（每目录一个 watch，2 s 补挂新目录；隐藏/build/目录符号链接剪枝），Windows 保持 bWatchSubtree。
- **TOML 类型严格校验**：顶层节/子表/permissions 类型错误从静默忽略变为解析期报错。
- **[settings].linker 生效**：仅显式配置时作为 GCC/Clang 链接驱动；空值保持原行为。
- **i18n**：删除 25 个死键（440→415），新增 12 条用户可见错误键（→427）；测试/诊断输出保留英文。

### 已知限制

- macOS kqueue 递归分支未实机验证（Linux 已由 CI 覆盖）；macos-x64 Release job 默认 skipped。
- MSVC shared 模板未内置 __declspec(dllexport)，需自行导出才生成导入库。
- A-23（lockfile 批量保存）延后 2.0.0；C-06/C-07/C-12/D-06 延后 1.5.x 补丁线。
- 配置加载期诊断保持英文；[settings].editor 仍按单一可执行路径处理。

### 测试

- 全量 test-all（EZMK_TEST_OFFLINE=1）：**1243 用例 / 7928 断言 / 8 跳过 / 0 失败**；CI run 43709022084 全绿（Ubuntu test-all / Windows unit / man / zsh）。
- check_i18n（427 键）、check_man_sync、check_config_docs、check_docs_sync 全绿。

**安装**：install.sh（Linux/macOS）、install.ps1（Windows）、winget install EazyMake.EazyMake、brew install 3667808244/eazymake/ezmk、publish/arch/PKGBUILD（Arch/MSYS2）。

**完整变更**：见 CHANGES.md 的 1.5.0 节；设计与执行计划见 plans/1.5.x/1.5.0.md。
