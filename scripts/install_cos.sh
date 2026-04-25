#!/usr/bin/env bash
# One-liner friendly installer for Creation OS front-door binaries.
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/spektre-labs/creation-os/main/scripts/install_cos.sh | bash
# Optional:
#   CREATION_OS_REF=v3.3.0   (Linux git clone; default: main)
#   CREATION_OS_PREFIX=...   (Linux install dir; default: /usr/local/bin)
set -euo pipefail

echo "Installing Creation OS (cos + cos-demo)..."

CREATION_OS_REF="${CREATION_OS_REF:-main}"
CREATION_OS_PREFIX="${CREATION_OS_PREFIX:-/usr/local/bin}"

case "$(uname -s)" in
Darwin)
    if command -v brew >/dev/null 2>&1; then
        brew tap spektre-labs/cos 2>/dev/null || true
        brew install creation-os
    else
        echo "Homebrew not found. Install from https://brew.sh then re-run this script." >&2
        exit 1
    fi
    ;;
Linux)
    tmpd="$(mktemp -d "${TMPDIR:-/tmp}/creation-os-install.XXXXXX")"
    trap 'rm -rf "$tmpd"' EXIT
    git clone --depth 1 --branch "$CREATION_OS_REF" \
        "https://github.com/spektre-labs/creation-os.git" "$tmpd/src"
    ( cd "$tmpd/src" && make cos cos-demo )
    if ! mkdir -p "$CREATION_OS_PREFIX" 2>/dev/null; then
        echo "Cannot create $CREATION_OS_PREFIX (try sudo or CREATION_OS_PREFIX=\$HOME/.local/bin)." >&2
        exit 1
    fi
    install -m755 "$tmpd/src/cos" "$CREATION_OS_PREFIX/cos"
    ln -sf cos "$CREATION_OS_PREFIX/cos-demo"
    echo "Installed to $CREATION_OS_PREFIX"
    ;;
*)
    echo "Unsupported OS: $(uname -s)" >&2
    exit 1
    ;;
esac

echo ""
echo "Done. Run: cos demo --batch"
