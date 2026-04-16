#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# LibreLane driver (RTL → GDSII) for the v38 σ-tile. Tooling is optional; CI skips cleanly.
#
# Install (host): see https://librelane.org/ and https://pypi.org/project/librelane/
# This script prefers a `librelane` CLI if present, otherwise tries `python3 -m librelane`.
#
# Note: LibreLane/OpenLane JSON keys evolve; treat hdl/asic/config.json as a template and
# align it with the LibreLane version you install before expecting a green run.

set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"

if ! command -v librelane >/dev/null 2>&1 && ! python3 -c "import librelane" >/dev/null 2>&1; then
  echo "librelane_run.sh: SKIP (LibreLane not installed; pip install librelane + PDK setup)" >&2
  exit 0
fi

mkdir -p hdl/asic

if command -v librelane >/dev/null 2>&1; then
  exec librelane run --config hdl/asic/config.json "$@"
fi

exec python3 -m librelane run --config hdl/asic/config.json "$@"
