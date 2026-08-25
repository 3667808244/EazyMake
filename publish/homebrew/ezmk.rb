# EazyMake Homebrew formula
#
# Installs the prebuilt binary for the current platform from the EazyMake
# GitHub Release. Each tarball (ezmk-<os>-<arch>.tar.gz) contains `ezmk`
# (the binary), `ezmk-lua` (standalone Lua hook runtime, 1.2.0-dev.8+) and
# `_ezmk` (zsh completion).
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
  version "1.3.3"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/3667808244/EazyMake/releases/download/v1.3.3/ezmk-macos-arm64.tar.gz"
      sha256 "c74424b5390d0a9e1cf387282adbbc7337bef051f32633d070e700991b92b960"
    end
  end

  on_linux do
    url "https://github.com/3667808244/EazyMake/releases/download/v1.3.3/ezmk-linux-x64.tar.gz"
    sha256 "a92f2264f6c46bb093482c2312ae15c33f5dadff9ec50466e453c3389bcc1da0"
  end

  def install
    # Tarball root dir carries the platform triple (e.g. ezmk-macos-arm64).
    dir = stable.url.split("/").last.sub(/\.tar\.gz$/, "")
    chdir dir do
      bin.install "ezmk"
      bin.install "ezmk-lua"
      zsh_completion.install "_ezmk"
    end
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/ezmk version")
  end
end
