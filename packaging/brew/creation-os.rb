# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Homebrew formula for Creation OS (v107 distribution).
#
#   brew tap  spektre-labs/creation-os
#   brew install creation-os
#
# This is a source formula: it clones the canonical repo, builds the
# full forty-kernel stack, runs the merge-gate as its `test do` block,
# and installs three binaries into `#{prefix}/bin`:
#
#   cos                 — Apple-tier CLI
#   creation_os_server  — v106 σ-server (OpenAI-compatible HTTP)
#   creation_os         — portable C11 reference kernel
#
# Homebrew will auto-derive universal macOS (arm64 + x86_64) bottles
# from the GitHub Actions release workflow in
# `.github/workflows/release.yml`.
class CreationOs < Formula
  desc "Local OpenAI-compatible runtime with σ-governance (forty branchless kernels)"
  homepage "https://github.com/spektre-labs/creation-os"
  url "https://github.com/spektre-labs/creation-os.git",
      tag: "v111.0",
      revision: :head
  version "111.0"
  license "AGPL-3.0-or-later"
  head "https://github.com/spektre-labs/creation-os.git", branch: "main"

  depends_on "make" => :build

  def install
    system "make", "cos"
    system "make", "standalone-v106"
    system "make", "standalone"

    bin.install "cos"
    bin.install "creation_os_server"
    bin.install "creation_os"

    # install default web-root + AGI architecture reference under share
    (share/"creation-os").install "web"
    (share/"creation-os").install "docs/AGI_ARCHITECTURE.md"
    (share/"creation-os").install "docs/AGI_ARCHITECTURE.svg"
    (share/"creation-os").install "README.md"
  end

  def caveats
    <<~EOS
      Creation OS is installed.  Next steps:

          creation_os_server                           # start the σ-server
          open http://127.0.0.1:8080                   # σ-UI (v108)
          cos demo                                     # 30-second kernel tour

      The default config lives at ~/.creation-os/config.toml — point
      its `[model].gguf_path` at any GGUF to serve it.  Bring-your-own-
      model is first-class: the bridge is model-agnostic (v109).
    EOS
  end

  test do
    system "#{bin}/cos",                "--help"
    system "#{bin}/creation_os_server", "--help"
    system "#{bin}/creation_os",        "--self-test"
  end
end
