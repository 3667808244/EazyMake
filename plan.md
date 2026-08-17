# EazyMake 1.2.0-pre.1 执行计划

> **状态：已完成**（2026-08-17，本机 MSYS2 + 远程 Arch Linux 双环境 makepkg 验证通过；全量 775 用例 / 3554 断言零回归）。1.2.0 系列路线图见 [`plans/1.2.0/README.md`](plans/1.2.0/README.md)。
>
> 详细设计：[**1.2.0-pre.1.md**](plans/1.2.0/1.2.0-pre.1.md)。本计划为 1.2.0 系列第一个发布前（pre）子版本：**pacman 分发（Arch Linux / MSYS2）**——为 EazyMake 增加 pacman 分发渠道，与 winget（Windows）、Homebrew（macOS/Linux）并列。
>
> **范围边界**：只做发布流水线补充——`publish/` 目录重组 + `publish/arch/PKGBUILD` + 本机 MSYS2 / 远程 Arch Linux 验证 + 文档收口。**公共 API 无任何变更**；AUR 新账户注册尚未开放，**不提交 AUR**（以「仓库内 `publish/arch/PKGBUILD` 自取 + `makepkg -si`」为主，AUR 延后）。
>
> **⛔ 发布门槛**：① 计划清单全部完成或明确收口；② 公共 API 无破坏性变更；③ 全量测试零回归（dev.11 后基线 775 用例 / 3552 断言）。

---

## 1 背景

- 当前分发只覆盖 winget（Windows）与 Homebrew（macOS/Linux）。Arch Linux / MSYS2 用户只能走 `install.sh`（源码构建），缺少 `pacman` 一条命令安装的体验。
- `release.yml` 已产出 `ezmk-linux-x64.tar.gz`（静态链接 Linux 二进制 + `_ezmk` zsh 补全），但 pacman 渠道采用**源码构建**（标准 Arch 实践，从 git tag 拉源码 + `build.sh` 编译），不依赖 Release 资产。
- **发布约束**：AUR 新账户注册尚未开放，本计划不提交 AUR；pacman 渠道以「自取 PKGBUILD + `makepkg -si`」为主，AUR 上传延后到账户开通后。

## 2 目标

| # | 目标 | 优先级 |
|---|------|--------|
| 1 | 新增 `publish/` 目录：把 `manifests/` → `publish/winget/`、`homebrew-eazymake/` → `publish/homebrew/`，并更新引用 | P0 |
| 2 | 编写 `publish/arch/PKGBUILD`（Arch Linux，`pkgname=eazymake`，安装 `ezmk` 到 `/usr/bin/` + `_ezmk` 补全） | P0 |
| 3 | 本机 MSYS2 环境 `makepkg` 生成并验证产物（`makepkg -si` / 安装后 `ezmk version`） | P0 |
| 4 | 远程 Arch Linux（`ezmk_project@192.168.136.131`）`makepkg` 生成并验证产物 | P0 |
| 5 | 更新 `ezmk-publish` skill + README 分发章节，补充 pacman 渠道与坑位 | P1 |
| 6 | 文档：README 安装章节补「Arch / MSYS2 自取 PKGBUILD + `makepkg -si`」路线（非 AUR），AUR 标注为延后 | P1 |

## 3 执行阶段（每阶段一个 commit）

### 阶段一：publish/ 目录重组（4.1）

- [x] **1.1 重组**：`git mv manifests publish/winget`、`git mv homebrew-eazymake publish/homebrew`；新建 `publish/arch/`
- [x] **1.2 引用更新**：`ezmk-publish` skill §1.1/§2.1/§4 的路径引用改为 `publish/winget/` / `publish/homebrew/`

### 阶段二：编写 PKGBUILD（4.2）

- [x] **2.1 PKGBUILD**：`pkgname=eazymake`、`pkgver=1.2.0`、`makedepends=('gcc' 'python')`、源码构建（`build.sh` + `EZMK_VERSION`）；`package()` 装 `ezmk` + `ezmk-lua` + `_ezmk`（zsh site-functions）；`sha256sums=('SKIP')` 起步
- [x] **2.2 MSYS2 兼容**：`package()` 同时处理 Windows 变体（`build/ezmk.exe`），与 §3.4 MSYS2 渠道对齐

### 阶段三：本机 MSYS2 验证（4.3）

- [x] **3.1 源码 tarball**：`git archive` 生成 `eazymake-1.2.0.tar.gz`（`--prefix=EazyMake-1.2.0/`，与 GitHub tag tarball 根目录一致）
- [x] **3.2 makepkg**：MINGW64 环境 `makepkg -f` 生成 `.pkg.tar.zst`；解包验证 `usr/bin/ezmk.exe`、`usr/bin/ezmk-lua.exe`、`usr/share/zsh/site-functions/_ezmk` 落位
- [x] **3.3 产物验证**：运行打包内的 `ezmk version` 输出 `1.2.0`

### 阶段四：远程 Arch Linux 验证（4.4）

- [x] **4.1 推送**：`scp` PKGBUILD + 源码 tarball 到 `ezmk_project@192.168.136.131`
- [x] **4.2 makepkg**：远程 `makepkg -f` 生成并验证（Linux 二进制 `ezmk` + `_ezmk`）

### 阶段五：文档收口（4.5）

- [x] **5.1 ezmk-publish skill**：三渠道总览表加 pacman；新增 pacman 章节（PKGBUILD 结构、源码构建 vs 二进制重打包、验证流程、AUR 延后）；坑位清单补 pacman 坑
- [x] **5.2 README 分发章节**：分发渠道表补 pacman（若 README 有该章节）

### 阶段六：README 安装章节（4.6）

- [x] **6.1 README_ZH.md**（中文基准）：安装章节补「Arch Linux / MSYS2：自取 PKGBUILD + `makepkg -si`」路线，AUR 标注延后
- [x] **6.2 README.md**：同步英文翻译

### 阶段七：收口

- [x] **7.1 收口**：CHANGES.md pre.1 条目 + plan.md 全勾选 + 系列 README 状态更新 + 发布门槛复核（API 无破坏性变更 + 全量零回归）

> 门槛未满足即停止，禁止带着未收口项进入正式版发布。

---

## 4 关键设计决策

| 决策 | 说明 |
|------|------|
| 源码构建 vs 二进制重打包 | 首选源码构建（PKGBUILD 标准形态，makepkg 从 git tag 拉源码 + build.sh 编译）；备选 `-bin` 风格重打包（source= 指向 Release 资产 + 真实 digest）——若远端网络受限用备选 |
| `pkgname=eazymake` | 项目名做包名；安装的二进制名保持 `ezmk`（与 CLI 一致） |
| `_ezmk` 补全 | 安装到 `zsh/site-functions/_ezmk`，与 Homebrew 的 `zsh_completion.install "_ezmk"` 对齐 |
| `package()` 双变体 | Linux 装 `build/ezmk`、Windows/MSYS2 装 `build/ezmk.exe`——MSYS2 也是 §3.4 的一等渠道 |
| 版本绑定 | `pkgver=1.2.0` 指向 `v1.2.0` 正式 tag；最终验证须在 1.2.0 Release 发布后进行；本机/远程验证用 `git archive` 生成的同名 tarball 代替（makepkg 识别本地文件不下载） |
| AUR 延后 | 新账户注册未开放，不提交 AUR；自取 PKGBUILD + `makepkg -si` 无需 AUR 账户 |

## 5 兼容性矩阵

| 变更 | 影响 | 处理 |
|------|------|------|
| `publish/` 目录重组 | 仅本地路径搬迁，不影响线上 winget-pkgs / Homebrew tap（独立仓库，本地只是参考副本） | `git mv` 保历史；更新引用文档 |
| 新增 pacman PKGBUILD | 纯新增分发渠道 | 不影响既有 winget/Homebrew 流程 |
| 公共 API | 无任何变更 | 纯发布流水线 |

## 6 延后项（明确收口）

- **AUR 提交**：账户开通后把 `publish/arch/PKGBUILD` 提交为 AUR 包（`pkgbase=eazymake`），届时 `ezmk-publish` skill 补 AUR 章节。
- **sha256sums 填真值**：首次用 `SKIP`；1.2.0 Release 发布后填 `gh api` 拿到的真实 digest（源码 tarball）。
- **1.2.0 tag 后最终验证**：`pkgver=1.2.0` 指向 `v1.2.0` tag，正式发布后 makepkg 才能拉到 tag 源码做最终验证。
