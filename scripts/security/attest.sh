#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v61 — produce an attestation JSON signed by cosign if
# available.  Output goes to ATTESTATION.json (and ATTESTATION.sig if
# cosign is present).
#
# Algorithm:
#   1. Build creation_os_v61 (if not already built).
#   2. Run ./creation_os_v61 --attest > ATTESTATION.json
#   3. If cosign is on PATH, cosign sign-blob ATTESTATION.json.
#   4. Emit SHA-256 of the attestation for provenance.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if ! [ -x ./creation_os_v61 ]; then
  echo "attest: building creation_os_v61 first"
  make standalone-v61 >/dev/null
fi

./creation_os_v61 --attest > ATTESTATION.json
echo "attest: wrote ATTESTATION.json ($(wc -c < ATTESTATION.json) bytes)"

if command -v shasum >/dev/null 2>&1; then
  sha=$(shasum -a 256 ATTESTATION.json | awk '{print $1}')
elif command -v sha256sum >/dev/null 2>&1; then
  sha=$(sha256sum ATTESTATION.json | awk '{print $1}')
else
  sha="(no sha256 tool)"
fi
echo "attest: sha256(ATTESTATION.json) = $sha"

if command -v cosign >/dev/null 2>&1; then
  # Keyless (OIDC) signing would work in CI; local dev typically has
  # no Sigstore identity.  We attempt keyless only when COSIGN_EXPERIMENTAL=1.
  if [ "${COSIGN_EXPERIMENTAL:-0}" = "1" ]; then
    cosign sign-blob --yes --output-signature ATTESTATION.sig ATTESTATION.json \
      && echo "attest: cosign sign-blob OK (ATTESTATION.sig)" \
      || echo "attest: cosign sign-blob FAILED (continuing — provenance step only)"
  else
    echo "attest: cosign present but COSIGN_EXPERIMENTAL!=1 — SKIPping Sigstore sign"
  fi
else
  echo "attest: cosign not on PATH — SKIPping Sigstore sign (ATTESTATION.json still valid)"
fi

echo "attest: OK"
