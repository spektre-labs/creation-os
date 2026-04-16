#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v61 — emit a SLSA v1.0 provenance statement for the v61
# binary.  Real SLSA-3 builds live in `.github/workflows/slsa.yml`
# (via slsa-framework/slsa-github-generator); this local script emits
# a SLSA v1.0-shaped JSON predicate for **inspection** only — it is
# NOT a verified SLSA-3 attestation by itself.
#
# Usage:
#   scripts/security/slsa.sh > PROVENANCE.json

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if ! [ -x ./creation_os_v61 ]; then
  make standalone-v61 >/dev/null
fi

if command -v shasum >/dev/null 2>&1; then
  sha=$(shasum -a 256 ./creation_os_v61 | awk '{print $1}')
elif command -v sha256sum >/dev/null 2>&1; then
  sha=$(sha256sum ./creation_os_v61 | awk '{print $1}')
else
  echo "slsa: ERROR (no sha256sum / shasum)" >&2
  exit 1
fi

commit="$(git rev-parse HEAD 2>/dev/null || echo '0000000000000000000000000000000000000000')"
builder="local-$(uname -s)-$(uname -m)"
now="$(date -u +'%Y-%m-%dT%H:%M:%SZ')"

cat <<JSON
{
  "_type": "https://in-toto.io/Statement/v1",
  "predicateType": "https://slsa.dev/provenance/v1",
  "subject": [
    {
      "name": "creation_os_v61",
      "digest": { "sha256": "$sha" }
    }
  ],
  "predicate": {
    "buildDefinition": {
      "buildType": "https://slsa.dev/container-based-build/v0.1",
      "externalParameters": {
        "source": "github.com/sigmaosorg/creation-os-kernel",
        "commit": "$commit"
      },
      "internalParameters": {
        "builder": "$builder",
        "make_target": "standalone-v61",
        "compile_flags": "-O2 -std=c11 -Wall -D_FORTIFY_SOURCE=3 -fstack-protector-strong -fPIE"
      },
      "resolvedDependencies": [
        { "uri": "src/v61/citadel.c" },
        { "uri": "src/v61/citadel.h" },
        { "uri": "src/v61/creation_os_v61.c" }
      ]
    },
    "runDetails": {
      "builder": {
        "id": "local-make-slsa-stub",
        "version": { "make": "$(make --version | head -1 || echo unknown)" }
      },
      "metadata": {
        "invocationId": "$now",
        "startedOn": "$now",
        "finishedOn": "$now"
      },
      "byproducts": []
    }
  },
  "_note": "Local SLSA-v1.0-shaped predicate.  Real SLSA-3 attestation comes from .github/workflows/slsa.yml via slsa-github-generator."
}
JSON
