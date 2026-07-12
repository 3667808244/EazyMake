# EazyMake 执行计划 — 0.9.1 默认仓库创建

> 来源:`plans/README.md`(当前执行)+ `plans/0.9.1.md`(详细设计)。
> 仓库已经克隆到 `E:\claude_workspace\ezmk-repo`。
> Gitee 镜像(已配置自动同步 GitHub 仓库):`https://gitee.com/egglzh/ezmk-repo.git`。
> 本版本创建并托管官方默认仓库,让用户"装完 ezmk 就能按名装包";
> **不新增 ezmk 核心 CLI 功能**,仅仓库侧建设 + 预注册策略 + 初始包 + 贡献流程。

---

## 执行概览

| 阶段 | 内容 | 依赖 |
|------|------|------|
| Phase A | 仓库侧:创建 `ezmk-repo` + 打包脚本 + CI 校验 | 无 |
| Phase B | ezmk 侧:`install.sh` 预注册 + 文档链接更新 + 教程示例 | Phase A(仓库就绪) |
| Phase C | 初始包:`hello-lib` + `example-utils` | Phase A(仓库骨架就绪) |
| Phase D | 端到端测试 + 贡献流程文档 | Phase A、B、C |

**硬约束**
- **仓库侧脚本包揽打包/索引**:`pack.sh` 生成 `index.toml`(杜绝手写不一致);CI `validate.sh` 强制"源→归档→哈希"三者一致。
- **预注册在用户作用域**(`-u`):全局安装目录不可写时不受影响;`EZMK_NO_DEFAULT_REPO=1` 可跳过;事后 `repo remove -u official` 可撤销。
- **不做隐式自动 clone**(方案 B):避免在 0.9.1 引入隐式网络行为的惊讶性;方案 A(安装时预注册)+ C(文档引导手动 add)组合覆盖。
- **SHA-256 强制**:公共仓库 `index.toml` **必须**提供 `sha256`;ezmk 安装时强制校验(已有逻辑)。

---

## Phase A — 仓库侧建设

创建 `github.com/3667808244/ezmk-repo`,符合 `docs/repo.md` 结构。已配置 Gitee 镜像 `gitee.com/egglzh/ezmk-repo`(自动同步),缓解国内访问。

```
ezmk-repo/
  index.toml                 # 仓库元数据 + 包索引(pack.sh 生成)
  packages/                  # 归档文件(.tar.gz)
  sources/                   # 源工程(含 ezmk.toml),可审计
  scripts/
    pack.sh                  # 打包 + sha256 + 重写 index.toml
    validate.sh              # 本地/CI 校验
  .github/workflows/ci.yml   # PR 校验
  README.md                  # 仓库说明 + 贡献指南
  CONTRIBUTING.md
```

- [ ] 创建仓库,写 `[repo]` 头 `index.toml` 骨架、`README.md`、`CONTRIBUTING.md`
- [ ] 编写 `scripts/pack.sh`:遍历 `sources/` → 归档 `packages/<name>-<version>.tar.gz` → sha256 → 重写 `index.toml`
- [ ] 编写 `scripts/validate.sh`:校验 `index.toml` 可解析、`file` 存在、sha256 匹配、`pack.sh` 后 `git diff` 为空
- [ ] 配置 `.github/workflows/ci.yml`:PR 上跑 `validate.sh` + 端到端 `pkg install` 冒烟

**关键文件**:`ezmk-repo/`(new,独立仓库)

---

## Phase B — ezmk 侧接入

- [ ] `install.sh` 预注册官方仓库到用户作用域 + 首次 `repo update`,支持 `EZMK_NO_DEFAULT_REPO=1`:
  ```
  ezmk repo add -u https://github.com/3667808244/ezmk-repo.git --name official
  ezmk repo update -u official
  ```
- [ ] `README.md` / `README_ZH.md` / `docs/cli.md`:默认仓库 URL(GitHub + Gitee 镜像) + 手动 `repo add` 一行命令 + 说明用户作用域可撤销
- [ ] 教程(0.9.0 §4"用包"章)改用官方仓库的 `hello-lib` 作示例

**关键文件**:`install.sh`、`README.md`、`README_ZH.md`、`docs/cli.md`、`tutorial/`

---

## Phase C — 初始包

| 包 | 类型 | 用途 |
|----|------|------|
| `hello-lib` | `static` | 最小示例静态库,供教程"用包"章节直接 `pkg install hello-lib` |
| `example-utils` | `utils` | Lua utils 工具样例,演示 `[utils.permissions]`(0.2.5 权限模型) |

- [ ] `hello-lib`:最小 `ezmk.toml` + `src/` + `include/`,打包为 `hello-lib-0.1.0.tar.gz`
- [ ] `example-utils`:含 `[utils]` + `[utils.permissions]`,打包为 `example-utils-0.1.0.tar.gz`
- [ ] `pack.sh` 生成对应 `index.toml` 条目(含 sha256)

---

## Phase D — 测试与贡献流程

- [ ] 端到端:`ezmk repo add -u <url>` → `repo update` → `repo info -u official` → `pkg search hello-lib` → `pkg install -p hello-lib` → 在示例项目 `[depends]` 引用并 `build` 通过
- [ ] sha256 防篡改:篡改归档后安装应触发 `sha256_mismatch` 报错
- [ ] 多版本:仓库含 `hello-lib` 1.0.0 与 1.1.0 时,`pkg install` 取 1.1.0;`pkg update` 从旧版升级
- [ ] utils 包:`example-utils` 安装后 `ezmk utils <tool>` 可运行,`pkg info` 展示权限;命中 `run_deny` 被拒
- [ ] CI 反例:构造"sha256 不匹配 / file 缺失 / 归档与源不一致"的坏 PR,`validate.sh` 均应失败
- [ ] 预注册:`install.sh` 后 `repo list -u` 含 `official`;`EZMK_NO_DEFAULT_REPO=1` 时不含
- [ ] `CONTRIBUTING.md` 定义第三方贡献流程:Fork → `sources/<pkg>/` → `pack.sh` → PR → CI → 审阅合入

---

## 安全

| 场景 | 措施 |
|------|------|
| 公共仓库包完整性 | `index.toml` **必须**提供 `sha256`;CI 校验;ezmk 安装时强制校验 |
| 恶意 utils 包 | 强制声明 `[utils.permissions]`;PR 人工审阅;`pkg info` 可预览 |
| 供应链(仓库被篡改) | `sources/` 可审计;CI 保证"源→归档→哈希"一致;git 历史可追溯 |
| 预注册的隐式性 | 预注册在**用户作用域且显式可见**(`repo list`),可关闭可撤销 |
| 全局安装 | `pkg install -g` 仍触发全局安装二次确认(现有机制) |

---

## 开放问题

1. **是否内置默认仓库常量(方案 B)**:为覆盖所有安装途径,是否在 `repo.cpp` 内置 `DEFAULT_REPO_URL` 并在"无仓库且找不到包"时提示/兜底注册?需权衡隐式网络行为与开箱即用。0.9.1 先不做,收集反馈。
2. **`ezmk pkg pack` 内置命令**:把打包能力从仓库脚本提升为 ezmk 子命令。属新功能,宜放 post-1.0。
3. **国内镜像**:Gitee 镜像已配置自动同步(`gitee.com/egglzh/ezmk-repo`),待决问题:ezmk 是否支持"同一仓库多 URL 回退"?`install.sh` 国内环境是否默认走 Gitee?
4. **索引规模与缓存**:仓库包数增长后 `search_package()` 每次读全量 `index.toml` 的 I/O,是否需要客户端 TTL 缓存。
5. **签名**:sha256 保完整性但不防"仓库所有者本人作恶"。是否引入 GPG/minisign 对 `index.toml` 或归档签名?
6. **包的许可证与元数据**:`index.toml` 是否增加 `license`/`homepage`/`description` 字段(需同步扩展 `docs/repo.md` 与 `repo.cpp` 解析)。

---

## 已完成版本

- **0.9.0** — 准备发布正式版:一键安装脚本(`install.sh` + `curl | bash`)、文档整理(`docs/cli.md`、`@safety.md` 集中化、README 双语互链)、基本教程(`tutorial/` 8 章 + 可运行示例)、发布前检查清单。详见 [`plans/0.9.0.md`](plans/0.9.0.md)
- **0.1.6 ~ 0.2.6** — 11 个功能版本(测试、国际化、跨平台、Lua 工具链、MSVC 支持、精细化控制、开发者体验、健壮性、生态与安全、翻译与命令行)。详见 [`plans/README.md`](plans/README.md)

---

## 下一版本(待执行)

- **0.9.2 文档多语言**:`docs/` 和 `tutorial/` 按语言拆分为 `en/` + `zh/` 子目录,补齐英文翻译,建立术语表与维护规则,CI 检查文件一一对应。详见 [`plans/0.9.2.md`](plans/0.9.2.md)
