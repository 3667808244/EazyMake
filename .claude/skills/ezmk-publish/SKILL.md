---
name: ezmk-publish
description: How to publish EazyMake releases to winget and Homebrew — manifest/formula preparation, submission workflow to microsoft/winget-pkgs and the homebrew tap, and known pitfalls (hashes, manifest types, CLA, moderation).
---

# EazyMake 分发：winget + Homebrew + pacman

EazyMake 的三个第三方分发渠道，统一在仓库 `publish/` 下管理（`publish/winget/`、`publish/homebrew/`、`publish/arch/`）：

| 渠道 | 目标平台 | 消费的 Release 资产 | 交付物 |
|------|---------|--------------------|--------|
| **winget** | Windows x64 | `ezmk-windows-x64.zip`（含 `ezmk.exe`） | 3 个 split manifest 提交到 `microsoft/winget-pkgs` |
| **Homebrew** | macOS arm64 / Linux x64 | `ezmk-macos-arm64.tar.gz`、`ezmk-linux-x64.tar.gz` | `Formula/ezmk.rb` 更新到 tap 仓库 |
| **pacman** | Arch Linux / MSYS2 x86_64 | 无（源码构建，从 git tag 拉源码） | `publish/arch/PKGBUILD`（自取 + `makepkg -si`；AUR 延后） |

**先决条件**：winget/Homebrew 需要 GitHub Release 已发布（`gh release create vX.Y.Z` 触发 `release.yml` 构建 win/linux/macOS 产物并上传）；pacman 只需 `vX.Y.Z` tag 已推送（源码 tarball URL 立即可用）。

---

## 0. 万事的第一步：拿真实资产哈希

winget/Homebrew 的 sha256 都必须来自 **Release 资产的实际 digest**，不要手算、不要猜（pacman 首次用 `SKIP`，见 §3.1）：

```bash
gh api repos/3667808244/EazyMake/releases/tags/v1.1.3 \
  --jq '.assets[] | {name, digest}'
```

`digest` 形如 `sha256:7743f1...`。winget 的 `InstallerSha256` 和 homebrew 公式的 `sha256` 都从这里抄（去 `sha256:` 前缀）。

---

## 1. Winget

### 1.1 Manifest 结构：必须拆成 3 个文件

提交给 `microsoft/winget-pkgs` 的是 **split manifest**，仓库里 `publish/winget/e/ezmk/1.1.3.yaml`（单文件 `defaultManifest`）只作参考，**不是**提交格式。winget-pkgs 的目录结构：

```
manifests/<publisher首字母小写>/<Publisher>/<Package>/<version>/
└── EazyMake.EazyMake.yaml                    # ManifestType: version
    EazyMake.EazyMake.installer.yaml          # ManifestType: installer
    EazyMake.EazyMake.locale.en-US.yaml       # ManifestType: defaultLocale
```

以 v1.1.3 为例：

**`EazyMake.EazyMake.yaml`**（version）：
```yaml
PackageIdentifier: EazyMake.EazyMake
PackageVersion: 1.1.3
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.6.0
```

**`EazyMake.EazyMake.installer.yaml`**（installer）：
```yaml
PackageIdentifier: EazyMake.EazyMake
PackageVersion: 1.1.3
InstallerLocale: en-US
InstallerType: zip
UpgradeBehavior: install
Installers:
  - Architecture: x64
    InstallerUrl: https://github.com/3667808244/EazyMake/releases/download/v1.1.3/ezmk-windows-x64.zip
    InstallerSha256: 7743f1bae5eb41671a1b846733be927eeff1b9d07df9be2820d35f9e5a4f486f
    NestedInstallerType: portable
    NestedInstallerFiles:
      - RelativeFilePath: ezmk.exe
        PortableCommandAlias: ezmk
ManifestType: installer
ManifestVersion: 1.6.0
```

> Release 产出的是便携 zip（`ezmk.exe`），所以是 `InstallerType: zip` + `NestedInstallerType: portable` + `PortableCommandAlias: ezmk`。

**`EazyMake.EazyMake.locale.en-US.yaml`**（defaultLocale）：
```yaml
PackageIdentifier: EazyMake.EazyMake
PackageVersion: 1.1.3
PackageLocale: en-US
Publisher: EazyMake
PackageName: EazyMake
PackageUrl: https://github.com/3667808244/EazyMake
ShortDescription: A simple C/C++ build tool
Description: ...
Moniker: ezmk
Tags:
  - build-tool
  - cpp
  - c
  - compiler
  - package-manager
ReleaseNotesUrl: https://github.com/3667808244/EazyMake/releases/tag/v1.1.3
License: MIT
ManifestType: defaultLocale
ManifestVersion: 1.6.0
```

### 1.2 🕳️ 坑：`ManifestType` 必须是 `defaultLocale`

locale 文件的 `ManifestType` 必须写 **`defaultLocale`**，不是 `locale`。
写 `locale` 时 `winget validate` 会报：

```
Manifest Error: The multi file manifest is incomplete. A multi file manifest
must contain at least version, installer and defaultLocale manifest.
```

（winget 把 `locale` 当次级 locale，不认作默认 locale。）**v1.1.3 提交时实测踩中，对照 winget-pkgs 现有包 sharkdp.fd 才确认。**

### 1.3 本机预检

```bash
# 把 3 个文件放同一目录，直接指版本目录（父目录会报 "Subdirectory not supported"）
winget validate --manifest <含3个yaml的目录>
# ✅ 输出 "清单验证成功。"
```

### 1.4 提交流程（走 gh API，全自动）

```bash
# 1. fork 大仓
gh repo fork microsoft/winget-pkgs --fork-name winget-pkgs

# 2. 在 fork 上建分支，指向 winget-pkgs master 最新 SHA
MASTER_SHA=$(gh api repos/microsoft/winget-pkgs/git/ref/heads/master --jq .object.sha)
gh api repos/3667808244/winget-pkgs/git/refs \
  -f ref=refs/heads/EazyMake.EazyMake-1.1.3 -f sha=$MASTER_SHA

# 3. 用 Contents API 写入 3 个文件（中间目录会自动创建）
#    PUT /repos/3667808244/winget-pkgs/contents/manifests/e/EazyMake/EazyMake/1.1.3/<file>
#    body: message, content(base64), branch
#    更新已有文件需带 sha（先 GET 拿 blob sha）

# 4. 开 PR（head 必须是 owner:branch）
gh pr create -R microsoft/winget-pkgs \
  --head "3667808244:EazyMake.EazyMake-1.1.3" --base master \
  --title "New version: EazyMake.EazyMake version 1.1.3" --body-file body.md
```

PR body 用 winget-pkgs 的 checklist 模板（CLA、单 manifest、validate 勾选）。

### 1.5 CLA（必须本人签）

PR 打开后 bot 会留言要求签微软 CLA，并打上 `Needs-CLA` 标签。**在 PR 上回复**：

```
@microsoft-github-policy-service agree            # 个人名义
@microsoft-github-policy-service agree company="X"  # 公司名义
```

> ⚠️ 这是**有法律效力的协议**，只能由账号主人决定。签署后标签移除（`license/cla` check 变 pass）。

### 1.6 CI + 版主

- CI 跑 10 个 check + `license/cla`。**check 08「Installation Validation」实测要 ~40 分钟**（真实安装验证，最慢），07「Installers Scan」约 5-6 分钟，其余秒级。
- 查看状态：`gh pr checks <n> -R microsoft/winget-pkgs`；或按 head SHA 查 check-runs（`/commits/<headSha>/check-runs`）。
- 全绿后标签变为 `Azure-Pipeline-Passed` + `Validation-Completed` + `New-Package`，**此时还需要 winget-pkgs 版主人工批准**（社区志愿者，几小时到几天不等，无法加速，只能等）。
- 合并后 `winget install EazyMake.EazyMake` 可用；验证：`winget show EazyMake.EazyMake` 能找到包。

---

## 2. Homebrew

### 2.1 tap 仓库是独立的

tap 在 **`3667808244/homebrew-eazymake`**（独立 git 仓库），公式路径 `Formula/ezmk.rb`。EazyMake 仓库里也有一份副本 `publish/homebrew/ezmk.rb`——**两份必须保持同步**。🕳️ 坑：线上 tap 已更新后，本地副本容易悄悄过期（v1.1.3 时就差一版），如果哪天从本地重新生成 tap 会把已修好的公式回退掉。更新时两处都改。

### 2.2 公式解剖（v1.1.3）

```ruby
class Ezmk < Formula
  desc "A simple C/C++ build tool (GCC/Clang/MSVC)"
  homepage "https://github.com/3667808244/EazyMake"
  version "1.1.3"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url ".../releases/download/v1.1.3/ezmk-macos-arm64.tar.gz"
      sha256 "4db57a1e..."   # 真实 digest
    end
  end

  on_linux do
    url ".../releases/download/v1.1.3/ezmk-linux-x64.tar.gz"
    sha256 "205d3beb..."
  end

  def install
    # 🕳️ 坑：tarball 根目录带平台 triple（如 ezmk-macos-arm64/），必须 chdir 进去再装
    dir = stable.url.split("/").last.sub(/\.tar\.gz$/, "")
    chdir dir do
      bin.install "ezmk"
      zsh_completion.install "_ezmk"
    end
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/ezmk version")
  end
end
```

### 2.3 要点 / 坑

- **`on_macos` 只有 arm64**：Intel x64 一直没有 Release 资产（`macos-13` runner 在 GitHub free tier 长期不分配），Intel Mac 会得到 brew 的 "unsupported" 错误。不要加 x64 分支。
- **必须 `chdir dir`**：`release.yml` 打包的 tarball 根目录是平台 triple 名，不 chdir 会装不到 `ezmk`。
- **sha256 用真实 digest**（见 §0），空字符串 = 公式不可用。
- **版本号手工同步**：`version "1.1.3"` 随 Release 更新。
- 安装测试：`brew tap 3667808244/eazymake && brew install ezmk`。
- 验证：`brew install` 前可用 `gh api .../releases/tags/<tag>` 的 `.assets[].digest` 交叉核对公式里的 sha256。

### 2.4 真机烟测

- `brew install` 只能在真 Mac 上测（Linux 上的 Linuxbrew 对 Arch 支持度差、且 VM 到 github 大文件传输会被 reset，不适合作为 brew 烟测环境）。
- Linux 分发建议走 `install.sh`（源码构建）真机烟测：安装/升级、zsh 补全、`project new`+build+run。

---

## 3. Pacman（Arch Linux / MSYS2）

> 1.2.0 起新增的第三个分发渠道（`publish/arch/PKGBUILD`）。形态为**自取 PKGBUILD + `makepkg -si`**（用户从仓库拉 PKGBUILD 本地构建安装）；**不提交 AUR**——AUR 新账户注册尚未开放，账户开通后补（届时本 skill 补 AUR 章节）。

### 3.1 PKGBUILD 结构（`publish/arch/PKGBUILD`）

```bash
pkgname=eazymake
pkgver=1.2.0            # 随版本号手工同步（指向 v1.2.0 正式 tag）
arch=('x86_64')
makedepends=('gcc' 'python')    # 无 depends：build.sh 在 Linux 产出静态链接二进制
source=("$pkgname-$pkgver.tar.gz::https://github.com/3667808244/EazyMake/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')     # 首次用 SKIP；稳定后填真实 digest（源码 tarball）
```

- **源码构建 vs 二进制重打包**：首选**源码构建**（PKGBUILD 标准形态：从 git tag 拉源码 + `build.sh` 编译，`EZMK_VERSION="$pkgver"` 注入版本号）；备选 `-bin` 风格（`source=` 直接指向 Release 资产 + `sha256sums` 用 §0 的真实 digest）——若远端网络受限用备选。
- **`pkgname=eazymake`**：包名用项目名；安装的二进制名保持 `ezmk`（与 CLI 一致）。
- **package() 双变体**：Linux 产出 `build/ezmk`；Windows/MSYS2 产出 `build/ezmk.exe`（`if [ -f build/ezmk.exe ]` 分支），保证 MSYS2 渠道可用。
- **`ezmk-lua` 一并安装**：dev.8 的 CMake 导出钩子独立运行时，保证 pacman 渠道下导出钩子可用。
- **`_ezmk` 补全**：安装到 `zsh/site-functions/_ezmk`，与 Homebrew 的 `zsh_completion.install "_ezmk"` 对齐。

### 3.2 验证流程（无需 AUR 账户）

- **本机 MSYS2**（已实测通过 2026-08-17）：`export MSYSTEM=MINGW64` + `export PATH=/mingw64/bin:/usr/bin:/bin` 后 `makepkg -fd`（`-d`：MINGW 工具链已装、msys 包名 `gcc`/`python` 不满足依赖检查）生成 `.pkg.tar.zst`；解包验证 `usr/bin/ezmk.exe`、`usr/bin/ezmk-lua.exe`、`usr/share/zsh/site-functions/_ezmk` 落位；`ezmk.exe version` 输出正确版本。
- **远程 Arch Linux**：`scp` PKGBUILD + 源码 tarball 到真机（VM 到 github 大文件传输被 reset，需自带 tarball），`makepkg -f` 生成并验证（Linux 二进制 `ezmk` + `_ezmk`；依赖 `gcc`/`python` 在 Arch 正常解析）。
- **tag 未发布时的本地替代**：`v1.2.0` tag 不存在时，用 `git archive --prefix=EazyMake-1.2.0/ HEAD -o eazymake-1.2.0.tar.gz` 生成同名 tarball 放 PKGBUILD 同目录——makepkg 识别本地文件不下载（GitHub tag tarball 根目录同为 `EazyMake-1.2.0/`）。

### 3.3 用户安装

- **Arch Linux**：`curl -fsSL https://raw.githubusercontent.com/3667808244/EazyMake/main/publish/arch/PKGBUILD -o PKGBUILD && makepkg -si`（需 `base-devel`）
- **MSYS2**：MINGW64 环境 `makepkg -si`（若提示缺 `gcc`/`python`，加 `--nodeps` 跳过——MINGW 工具链已就绪；亦可 `makepkg-mingw -si`）
- **AUR**：账户开通后提交为 AUR 包（`pkgbase=eazymake`），验证流程与 §3.2 一致。

### 3.4 🕳️ 坑（MSYS2 本机验证实测）

- **MSYS2 bash 依赖 Cygwin 信号管道**：受限环境（文件沙箱）下 bash 无法启动（Win32 error 5「couldn't create signal pipe」），验证需在非受限环境跑。
- **`set -u` + `source /etc/profile` 冲突**：MSYS2 的 /etc/profile 引用未定义变量，`set -u` 下直接杀死脚本；验证脚本显式设 PATH（`/mingw64/bin:/usr/bin:/bin`）更稳。
- **MSYS 环境无 g++/python**：默认 MSYS root 环境没有编译器（只有 MINGW64 有 mingw-w64-x86_64-gcc/python），makepkg 须在 MINGW64 环境（`export MSYSTEM=MINGW64`）跑。
- **依赖检查按 msys 包名**：MINGW64 下 `pacman -Q gcc` 不存在（实为 mingw-w64-x86_64-gcc），`makepkg` 依赖检查失败——用 `-d`/`--nodeps`。
- **版本绑定**：`pkgver` 指向正式 tag；Release 前 makepkg 拉不到 tag 源码——用 git archive 本地 tarball 做功能验证，最终验证在发布后。

---

## 4. 坑位清单（速查）

| # | 坑 | 后果 | 正确做法 |
|---|----|------|---------|
| 1 | locale 写 `ManifestType: locale` | `winget validate` 报缺 defaultLocale | 写 **`defaultLocale`** |
| 2 | 用单文件 `defaultManifest` 提交 | winget-pkgs 拒绝 | 拆成 version/installer/defaultLocale 三文件 |
| 3 | sha256 手填/留空 | winget CI 校验失败 / brew 安装失败 | 从 `gh api releases` 的 `assets[].digest` 抄 |
| 4 | tarball 不 `chdir` 就 install | brew 找不到二进制 | 按 URL 文件名取平台 triple 目录再 chdir |
| 5 | 本地 `publish/homebrew/ezmk.rb` 与线上 tap 不同步 | 日后回退线上公式 | 双处同步更新 |
| 6 | 提交时漏签 CLA | PR 卡在 `Needs-CLA` | PR 上回 `@microsoft-github-policy-service agree` |
| 7 | 以为 CI 全绿就完事 | 不知道还要等版主 | CI 绿后还有版主人工批准（几小时~几天） |
| 8 | 查 check-runs 用 merge commit | 拿不到数据 | 用 PR 的 **head SHA**（`headRefOid`） |
| 9 | Intel macOS 加进公式 | 误导用户 | 明确只在 arm64 下提供（缺 x64 资产） |
| 10 | MSYS2 环境跑 makepkg 用错环境 | MSYS 环境无 g++/python | `export MSYSTEM=MINGW64`（MINGW64 环境） |
| 11 | MINGW64 下依赖检查失败 | 报缺 `gcc`/`python`（msys 包名） | `makepkg -d`/`--nodeps`（MINGW 工具链已装） |
| 12 | `pkgver` 指向未发布 tag | makepkg 拉不到源码 | `git archive` 本地同名 tarball 做功能验证，最终验证延后到 Release 后 |

## 5. 相关文件

- winget 提交记录：`publish/winget/e/ezmk/1.1.3.yaml`（单文件参考格式）
- winget-pkgs PR：`microsoft/winget-pkgs#416835`（v1.1.3，含 CI 全绿标签）
- homebrew 公式：`publish/homebrew/ezmk.rb`（本地副本，需与 tap 同步）
- pacman PKGBUILD：`publish/arch/PKGBUILD`（源码构建；AUR 延后）
- Release 资产哈希来源：`gh api repos/3667808244/EazyMake/releases/tags/<tag>`
- 发布流程/跟进项：`plans/1.1.x/1.1.0.md`（发布流水线章节）、`plans/1.2.x/1.2.0-pre.1.md`（pacman 渠道）、记忆 `eazymake-110-release`
