#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v61 — cosign sign the hardened v60/v61 binaries and the
# SBOM.  Honest SKIP if cosign is missing or if no signing identity is
# available (keyless OIDC only works in CI unless COSIGN_EXPERIMENTAL=1
# is set and a browser-capable environment is present).

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if ! command -v cosign >/dev/null 2>&1; then
  echo "sign: SKIP (cosign not on PATH; brew install sigstore/tap/cosign)"
  exit 0
fi

targets=()
for f in creation_os_v60 creation_os_v60_hardened creation_os_v61 creation_os_v61_hardened SBOM.json; do
  [ -f "$f" ] && targets+=("$f")
done
if [ "${#targets[@]}" -eq 0 ]; then
  echo "sign: SKIP (no binaries to sign; run 'make standalone-v60 standalone-v61 harden sbom' first)"
  exit 0
fi

if [ "${COSIGN_EXPERIMENTAL:-0}" != "1" ]; then
  echo "sign: SKIP (keyless Sigstore requires COSIGN_EXPERIMENTAL=1; in CI this is set automatically)"
  echo "sign: would sign: ${targets[*]}"
  exit 0
fi

for t in "${targets[@]}"; do
  echo "sign: cosign sign-blob --yes $t"
  cosign sign-blob --yes --output-signature "$t.sig" "$t"
done

echo "sign: OK (${#targets[@]} artifact(s) signed, .sig files written)"
