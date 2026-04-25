# typed: false
# frozen_string_literal: true

# Copy into https://github.com/spektre-labs/homebrew-cos as Formula/creation-os.rb
# Replace sha256 after tagging (see packaging/homebrew-cos/README.md).

class CreationOs < Formula
  desc "Operating System for Thinking — σ-gated AI runtime (cos CLI)"
  homepage "https://github.com/spektre-labs/creation-os"
  url "https://github.com/spektre-labs/creation-os/archive/refs/tags/v3.3.0.tar.gz"
  sha256 "PLACEHOLDER"
  license :cannot_represent # Dual SCSL-1.0 / AGPL-3.0 — see LICENSE in tarball

  depends_on "make" => :build
  depends_on "sqlite"
  depends_on "curl"

  def install
    system "make", "cos", "cos-demo"
    bin.install "cos"
    bin.install "cos-demo"
  end

  test do
    system "#{bin}/cos", "demo", "--batch"
  end
end
