#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Optional BitNet upstream checkout helper (explicit network).
#
# This script does NOT run automatically. It does NOT modify merge-gate.
#
# Usage:
#   ./scripts/v31_setup_bitnet_cpp.sh --clone
#   ./scripts/v31_setup_bitnet_cpp.sh --print-env
#
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

DEST="$ROOT/external/BitNet"

print_env() {
  echo "After you build upstream, export:"
  echo "  export COS_BITNET_CLI=/path/to/llama-cli"
  echo "  export COS_BITNET_MODEL=/path/to/model.gguf"
  echo "Then run:"
  echo "  make standalone-v31 && ./creation_os_v31 --self-test"
}

clone_upstream() {
  mkdir -p "$ROOT/external"
  if [ -d "$DEST/.git" ]; then
    echo "v31_setup: upstream already present: $DEST"
    return 0
  fi
  if ! command -v git >/dev/null 2>&1; then
    echo "v31_setup: git not found" >&2
    return 2
  fi
  echo "v31_setup: cloning Microsoft BitNet into $DEST ..."
  git clone --depth 1 https://github.com/microsoft/BitNet "$DEST"
  echo "v31_setup: clone OK"
  echo "Next: follow upstream build instructions inside $DEST (no auto pip/weights here)."
}

case "${1:-}" in
  --clone) clone_upstream ;;
  --print-env) print_env ;;
  *)
    echo "usage: $0 --clone | --print-env" >&2
    exit 2
    ;;
esac
