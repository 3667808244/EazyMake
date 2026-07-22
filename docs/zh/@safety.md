# 安全性

EazyMake 的安全模型集中说明。本文件为**单一权威**;其他文档(`repo.md`、`pkg.md`、
`utils.md`、`@cache.md`)相关小节只保留一句话并链接回此处,避免多处维护漂移。

---

## 包安装(`pkg.cpp`)

- **全局安装二次确认**:`pkg install -g` 写入 `<ezmk_install_dir>/pkg/` 前必须二次确认。
- **覆盖二次确认**:安装若会覆盖已有文件,必须二次确认。
- **SHA-256 校验**:命令行 `--sha256 <hash>` 或仓库 `index.toml` 提供 `sha256` 时,
  安装前必须校验归档摘要,不匹配即中止。
- **`-y`**:跳过上述交互确认(非交互场景),但不跳过校验。

详见 [`pkg.md`](pkg.md)。

## 仓库管理(`repo.cpp`)

- `git clone` 失败 → 清除不完整的本地缓存目录后报错。
- `git pull` 失败 → 警告并保留已有缓存继续(不阻断构建)。
- 全局仓库**注册**无需二次确认(clone 不等同于安装);但经全局仓库**安装包**仍触发上方全局安装确认。
- 本地仓库(`type = "local"`)校验:`index.toml` 可解析、`file` 路径存在、`sha256` 格式合法(0.2.5+)。

详见 [`repo.md`](repo.md)。

## 构建缓存(`cache.cpp`)

- 写 `record.json` 与 `.o` 前先写 `.tmp` 临时文件,成功后 `rename` 覆盖(原子写)。
- 构建中途失败不会破坏已有缓存的一致性。

详见 [`@cache.md`](@cache.md)。

## Lua sandbox(`lua_api.cpp` / `linit.c`,0.2.0+)

- `os` 与 `io` 库在**编译期**从 Lua 中移除(`linit.c`);外部命令只能经 `ezmk.run()` 执行。
- `ezmk.file_write()` 拒绝写入项目根目录之外的绝对路径(硬限制,不可绕过)。
- 不暴露 `require` 加载 C 扩展(仅纯 Lua 模块)。
- 每次调用获得独立 sandbox 环境表,脚本间全局变量互不污染。
- **安装钩子（0.9.9+）**：安装生命周期钩子（`preinstall`/`postinstall`）与构建钩子和 utils 共享相同的沙箱基础设施。Lua 安装钩子**不**打开编辑器审查——沙箱边界（已移除 `os`/`io`、`file_write` 限制、`ezmk.run()` 权限检查）已限定了脚本的能力范围。执行前仅需用户确认（`[y/N]`）。

## Utils 权限管理(`[utils.permissions]`,0.2.5+)

包可在其 `ezmk.toml` 中对 `file_read` / `file_write` / `run` 三类受控访问声明白名单/黑名单。
判定顺序固定为 **deny > allow > ask**:

1. 命中 deny 黑名单 → 拒绝(最高优先级);
2. 否则命中 allow 白名单 → 允许;
3. 两者都不命中 → 询问用户。

`file_write` 先经 sandbox 的「禁止越界写」硬限制,再进入 deny/allow/ask。
**向后兼容**:整节缺失的旧包保持无限制行为,但首次调用受控 API 时打印一次 deprecation warning。

完整字段与语义详见 [`utils.md` 权限管理](utils.md#权限管理-version--025)。

---

## 汇总表

| 场景 | 措施 |
|---|---|
| 全局安装包 | 二次确认 |
| 覆盖已有文件 | 二次确认 |
| 归档 `sha256`(命令行/仓库) | 安装前校验 |
| 全局仓库注册 | 无需确认(clone ≠ 安装) |
| `git clone` 失败 | 清缓存后报错 |
| `git pull` 失败 | 警告并沿用缓存 |
| 缓存写入 | `.tmp` + `rename` 原子写 |
| Lua `os`/`io` | 编译期移除 |
| Lua `file_write` 越界 | 拒绝(硬限制) |
| Utils 受控访问 | deny > allow > ask |
| Lua 安装钩子执行 | 沙箱 + 确认提示（无需编辑器审查，0.9.9+） |
| Shell 安装钩子执行 | 打开编辑器审查 + 确认提示（旧版兼容） |
