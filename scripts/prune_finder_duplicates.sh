#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Commercial:    licensing@spektre-labs.com
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Remove macOS Finder "duplicate" trees at repo root (e.g. "README 2.md", "core 2/").
# These are never canonical sources; they clutter disk and confuse tooling.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

found=0
while IFS= read -r -d '' p; do
  found=1
  printf 'Removing: %s\n' "$p"
  rm -rf "$p"
done < <(find "$root" -mindepth 1 -maxdepth 1 \( -name '* 2' -o -name '* 2.*' \) -print0)

if [[ "$found" -eq 0 ]]; then
  echo "prune_finder_duplicates: nothing to remove at repo root."
else
  echo "prune_finder_duplicates: OK"
fi
