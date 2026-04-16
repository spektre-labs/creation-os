#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v51 — one-line installer (SAFE DRY-RUN).
#
# Honest scope: the public target `curl -fsSL https://get.creation-os.dev | sh`
# is aspirational. This script is the repo-local version. It DOES NOT:
#   - make any network call
#   - write to /usr/local/bin
#   - download weights
#   - require sudo
#
# It prints what a real installer would do, verifies the host platform is
# supported, and exits cleanly. To actually run an installer, wait for a
# signed release that flips the DRY_RUN switch off.
#
# Usage:
#   bash scripts/v51/install.sh              # dry-run (default)
#   COS_INSTALL_DRY_RUN=1 bash scripts/...   # explicit dry-run
#
set -euo pipefail

DRY_RUN="${COS_INSTALL_DRY_RUN:-1}"

OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS-$ARCH" in
    Linux-x86_64)   BIN="creation-os-linux-x64"   ; SUPPORTED=1 ;;
    Linux-aarch64)  BIN="creation-os-linux-arm64" ; SUPPORTED=1 ;;
    Darwin-arm64)   BIN="creation-os-macos-arm64" ; SUPPORTED=1 ;;
    Darwin-x86_64)  BIN="creation-os-macos-x64"   ; SUPPORTED=1 ;;
    *)              BIN="creation-os-unknown"     ; SUPPORTED=0 ;;
esac

echo "╔══════════════════════════════════════════════════╗"
echo "║  Creation OS v51 — installer (DRY-RUN)          ║"
echo "╚══════════════════════════════════════════════════╝"
echo "host:       $OS / $ARCH"
echo "binary:     $BIN"
echo "dry-run:    $DRY_RUN"
echo ""

if [ "$SUPPORTED" != "1" ]; then
    echo "This host is not in the v51 scaffold's supported matrix." >&2
    echo "(supported: Linux-x86_64, Linux-aarch64, Darwin-arm64, Darwin-x86_64)" >&2
    exit 1
fi

echo "The production installer would:"
echo "  1. curl -fsSL https://github.com/spektre-labs/creation-os/releases/latest/download/$BIN \\"
echo "         -o /usr/local/bin/creation-os"
echo "  2. chmod +x /usr/local/bin/creation-os"
echo "  3. creation-os --setup      # download BitNet 2B4T weights (~1 GB)"
echo ""
echo "Today (v51 scaffold tier) use the repo directly:"
echo "  make merge-gate         # portable C11 gate (v6–v29)"
echo "  make check-v51          # v51 integration scaffold self-test"
echo "  make v50-benchmark      # STUB benchmark rollup + assurance logs"
echo "  make certify            # DO-178C-aligned assurance pack"
echo ""

if [ "$DRY_RUN" != "1" ]; then
    echo "Non-dry-run mode is not enabled yet. Re-running as dry-run." >&2
fi

echo "(no files were written; no network calls were made.)"
