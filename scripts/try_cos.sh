#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Ephemeral try: shallow-clone Creation OS, build cos, run `cos demo`.
# Usage (from README):
#   curl -fsSL https://raw.githubusercontent.com/spektre-labs/creation-os/main/scripts/try_cos.sh | bash
set -euo pipefail

TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/cos_try.XXXXXX")
trap 'rm -rf "$TMPDIR"' EXIT

echo "Cloning Creation OS (shallow)..."
git clone --depth 1 -q https://github.com/spektre-labs/creation-os.git "$TMPDIR/cos"
cd "$TMPDIR/cos"

echo "Building cos + cos-demo (this may take a minute)..."
make cos cos-demo -j"${TRY_COS_JOBS:-4}" -s

echo ""
./cos demo --batch
