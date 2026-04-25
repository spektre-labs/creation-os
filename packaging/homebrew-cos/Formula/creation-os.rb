# typed: false
# frozen_string_literal: true

# Copy into https://github.com/spektre-labs/homebrew-cos as Formula/creation-os.rb
# Update `url` + `sha256` after each upstream tag (see packaging/homebrew-cos/README.md).

class CreationOs < Formula
  desc "Operating System for Thinking — σ-gated AI runtime (cos CLI)"
  homepage "https://github.com/spektre-labs/creation-os"
  url "https://github.com/spektre-labs/creation-os/archive/refs/tags/v3.3.0.tar.gz"
  sha256 "PLACEHOLDER"
  license :cannot_represent # Dual SCSL-1.0 / AGPL-3.0 — see LICENSE files in tarball

  depends_on "cmake" => :build

  def install
    # Default gate for the shipped tree: front door + demo shim (no weights).
    system "make", "cos", "cos-demo"
    bin.install "cos"
    bin.install "cos-demo"
  end

  test do
    system "#{bin}/cos", "demo", "--batch"
  end
end
