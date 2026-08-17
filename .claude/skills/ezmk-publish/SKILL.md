---
name: ezmk-publish
description: How to publish EazyMake releases to winget and Homebrew — manifest/formula preparation, submission workflow to microsoft/winget-pkgs and the homebrew tap, and known pitfalls (hashes, manifest types, CLA, moderation).
---

# EazyMake 分发：winget + Homebrew

EazyMake 的两个第三方分发渠道，都消费 GitHub Release 的产物：

| 渠道 | 目标平台 | 消费的 Release 资产 | 交付物 |
|------|---------|--------------------|--------|
| **winget** | Windows x64 | `ezmk-windows-x64.zip`（含 `ezmk.exe`） | 3 个 split manifest 提交到 `microsoft/winget-pkgs` |
| **Homebrew** | macOS arm64 / Linux x64 | `ezmk-macos-arm64.tar.gz`、`ezmk-linux-x64.tar.gz` | `Formula/ezmk.rb` 更新到 tap 仓库 |

**先决条件**：GitHub Release 已发布（`gh release create vX.Y.Z` 触发 `release.yml` 构建 win/linux/macOS 产物并上传）。

---

## 0. 万事的第一步：拿真实资产哈希

两个渠道的 sha256 都必须来自 **Release 资产的实际 digest**，不要手算、不要猜：

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

## 3. 坑位清单（速查）

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

## 4. 相关文件

- winget 提交记录：`publish/winget/e/ezmk/1.1.3.yaml`（单文件参考格式）
- winget-pkgs PR：`microsoft/winget-pkgs#416835`（v1.1.3，含 CI 全绿标签）
- homebrew 公式：`publish/homebrew/ezmk.rb`（本地副本，需与 tap 同步）
- Release 资产哈希来源：`gh api repos/3667808244/EazyMake/releases/tags/<tag>`
- 发布流程/跟进项：`plans/1.1.x/1.1.0.md`（发布流水线章节）、记忆 `eazymake-110-release`
