# Homebrew formula for EazyMake
#
# Install:
#   brew tap 3667808244/eazymake
#   brew install ezmk
#
# Or directly:
#   brew install 3667808244/eazymake/ezmk

class Ezmk < Formula
  desc "A simple C/C++ build tool"
  homepage "https://github.com/3667808244/EazyMake"
  license "MIT"

  # version is extracted from the tag; update on new releases
  version "1.1.0"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/3667808244/EazyMake/releases/download/v#{version}/ezmk-macos-arm64.tar.gz"
      sha256 "" # Replace with actual SHA-256 after release
    else
      url "https://github.com/3667808244/EazyMake/releases/download/v#{version}/ezmk-macos-x64.tar.gz"
      sha256 "" # Replace with actual SHA-256 after release
    end
  end

  on_linux do
    url "https://github.com/3667808244/EazyMake/releases/download/v#{version}/ezmk-linux-x64.tar.gz"
    sha256 "" # Replace with actual SHA-256 after release
  end

  def install
    bin.install "ezmk"
    # Install zsh completion
    zsh_completion.install "_ezmk"
  end

  test do
    system "#{bin}/ezmk", "version"
  end
end
