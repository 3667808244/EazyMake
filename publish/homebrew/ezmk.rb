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
  version "1.3.6"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/3667808244/EazyMake/releases/download/v1.3.6/ezmk-macos-arm64.tar.gz"
      sha256 "c889c5ee82eae902aac3dd5e35040b294f477bcfc051217afee001b93c567481"
    end
  end

  on_linux do
    url "https://github.com/3667808244/EazyMake/releases/download/v1.3.6/ezmk-linux-x64.tar.gz"
    sha256 "a98f5b755f6dd65078622f539e4cf8cdee306caf3c689d3507a30ed6855d3c84"
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
