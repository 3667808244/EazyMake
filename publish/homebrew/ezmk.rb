# EazyMake Homebrew formula
#
# Installs the prebuilt binary for the current platform from the EazyMake
# GitHub Release. Each tarball (ezmk-<os>-<arch>.tar.gz) contains `ezmk`
# (the binary), `ezmk-lua` (standalone Lua hook runtime, 1.2.0-dev.8+),
# `_ezmk` (zsh completion) and `man/` (ezmk(1), ezmk-lua(1), ezmk.toml(5),
# ezmk-workspace.toml(5); 1.4.3+).
#
# NOTE 1.4.11: version/url/sha256 are filled from the v1.4.11 Release asset digests
# (`gh api repos/3667808244/EazyMake/releases/tags/v1.4.11`) — assets carry man/.
#
#   brew tap 3667808244/eazymake
#   brew install ezmk
#
# Repo: https://github.com/3667808244/EazyMake
# Release assets: https://github.com/3667808244/EazyMake/releases
#
# Note: macOS Intel (x64) has no prebuilt asset by default — the x64 release job
# is skipped unless the repository variable ENABLE_MACOS_X64 is 'true' (the
# `macos-13` runner is not allocated on GitHub's free tier, so an always-on job
# stalls the whole Release run; see .github/workflows/release.yml). Intel Macs
# get a clean "unsupported on this architecture" error from brew until then.

class Ezmk < Formula
  desc "A simple C/C++ build tool (GCC/Clang/MSVC)"
  homepage "https://github.com/3667808244/EazyMake"
  version "1.4.11"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/3667808244/EazyMake/releases/download/v1.4.11/ezmk-macos-arm64.tar.gz"
      sha256 "ee1a2a7df98bbd55b1c304cf3674510ec5ff6e8f498d86e67c4f6d24ba302082"
    end
  end

  on_linux do
    url "https://github.com/3667808244/EazyMake/releases/download/v1.4.11/ezmk-linux-x64.tar.gz"
    sha256 "ada8740bf505821e83b0febb65c2ed697f128d35fb490cda18290c021c633817"
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
