# EazyMake Homebrew formula
#
# Installs the prebuilt binary for the current platform from the EazyMake
# GitHub Release. Each tarball (ezmk-<os>-<arch>.tar.gz) contains `ezmk`
# (the binary), `ezmk-lua` (standalone Lua hook runtime, 1.2.0-dev.8+),
# `_ezmk` (zsh completion) and `man/` (ezmk(1), ezmk-lua(1), ezmk.toml(5),
# ezmk-workspace.toml(5); 1.4.3+).
#
# NOTE 1.4.3: version/url/sha256 are filled from the v1.4.3 Release asset digests
# (`gh api repos/3667808244/EazyMake/releases/tags/v1.4.3`) — assets now carry man/.
#
#   brew tap 3667808244/eazymake
#   brew install ezmk
#
# Repo: https://github.com/3667808244/EazyMake
# Release assets: https://github.com/3667808244/EazyMake/releases
#
# Note: macOS Intel (x64) has no prebuilt asset yet — the `macos-13` runner is
# not allocated on GitHub's free tier, so the x64 job stalls and the release
# never carries `ezmk-macos-x64.tar.gz`. Intel Macs get a clean "unsupported
# on this architecture" error from brew until a binary is published.

class Ezmk < Formula
  desc "A simple C/C++ build tool (GCC/Clang/MSVC)"
  homepage "https://github.com/3667808244/EazyMake"
  version "1.4.3"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/3667808244/EazyMake/releases/download/v1.4.3/ezmk-macos-arm64.tar.gz"
      sha256 "b82608f27e26d6db43b09b7fa25bee174ba05c45d8dfad435442ea1e7545816a"
    end
  end

  on_linux do
    url "https://github.com/3667808244/EazyMake/releases/download/v1.4.3/ezmk-linux-x64.tar.gz"
    sha256 "72b5ec018774835c708c46397917ae980ce80dd055013b3a82761cab66085466"
  end

  def install
    # Tarball root dir carries the platform triple (e.g. ezmk-macos-arm64).
    dir = stable.url.split("/").last.sub(/\.tar\.gz$/, "")
    chdir dir do
      bin.install "ezmk"
      bin.install "ezmk-lua"
      zsh_completion.install "_ezmk"
      # 1.4.3: man pages, shipped inside the release tarball as man/
      man1.install "man/ezmk.1"
      man1.install "man/ezmk-lua.1"
      man5.install "man/ezmk.toml.5"
      man5.install "man/ezmk-workspace.toml.5"
    end
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/ezmk version")
  end
end
