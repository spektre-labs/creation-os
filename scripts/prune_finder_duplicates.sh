#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Commercial:    spektrelabs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
#
# Remove macOS Finder "duplicate" files and directories anywhere in the tree
# (e.g. "README 2.md", "core 2/", "test_pipeline 2.c"). These are never
# canonical sources; they clutter disk, inflate search results, and confuse
# tooling. Protected paths (.git, third_party, models, .venv*) are skipped.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

# Directories we must never recurse into (vendored, downloaded, or sandboxed).
PRUNE_ARGS=(
  -path "$root/.git" -prune -o
  -path "$root/third_party" -prune -o
  -path "$root/models" -prune -o
  -path "$root/.venv-bitnet" -prune -o
  -path "$root/.venv" -prune -o
  -path "$root/node_modules" -prune -o
)

found=0

# Finder-duplicate files: anything matching '* 2.*'.
while IFS= read -r -d '' p; do
  found=1
  printf 'Removing file: %s\n' "${p#$root/}"
  rm -f "$p"
done < <(find "$root" "${PRUNE_ARGS[@]}" -type f -name '* 2.*' -print0)

# Finder-duplicate directories: anything matching '* 2'.
while IFS= read -r -d '' p; do
  found=1
  printf 'Removing dir:  %s\n' "${p#$root/}"
  rm -rf "$p"
done < <(find "$root" "${PRUNE_ARGS[@]}" -type d -name '* 2' -print0)

# Also catch the common bare-file forms at repo root ("Makefile 2", "LICENSE 2", etc.)
for name in 'Makefile 2' 'LICENSE 2' '.gitignore 2' '.envrc 2' 'Earthfile 2' 'Justfile 2'; do
  if [[ -e "$root/$name" ]]; then
    found=1
    printf 'Removing file: %s\n' "$name"
    rm -f "$root/$name"
  fi
done

if [[ "$found" -eq 0 ]]; then
  echo "prune_finder_duplicates: nothing to remove (tree is clean)."
else
  echo "prune_finder_duplicates: OK"
fi
