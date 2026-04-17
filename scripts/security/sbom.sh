#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Commercial:    licensing@spektre-labs.com
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Creation OS — CycloneDX-lite 1.5 SBOM generator.
#
# Emits a minimal SBOM covering every src/v*/ subsystem as a distinct
# component with its own version, hash, and relationship back to the
# Creation OS umbrella.  No network, no registry lookup — purely a
# local digest.  Suitable as input to sigstore attestations or SLSA L3
# build provenance upstream.

set -euo pipefail
cd "$(dirname "$0")/../.."

serial="urn:uuid:$(uuidgen 2>/dev/null || echo 00000000-0000-0000-0000-000000000000)"
timestamp="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

# Top-level repo version = short commit ref if available, else "local".
# Top-level hash is always a fresh SHA-256 over all src/ C+H files
# (deterministic, LC_ALL=C sort) — never a git SHA-1, which would be
# a different algorithm family mislabelled as SHA-256.
if [ -d .git ] && command -v git >/dev/null 2>&1; then
    repo_version=$(git describe --tags --always 2>/dev/null || echo "local")
else
    repo_version="local"
fi
repo_hash=$(find src -type f \( -name '*.c' -o -name '*.h' \) 2>/dev/null \
            | LC_ALL=C sort \
            | xargs shasum -a 256 2>/dev/null \
            | shasum -a 256 | awk '{print $1}')
[ -n "$repo_hash" ] || repo_hash="0000000000000000000000000000000000000000000000000000000000000000"

printf '{\n'
printf '  "bomFormat": "CycloneDX",\n'
printf '  "specVersion": "1.5",\n'
printf '  "version": 1,\n'
printf '  "serialNumber": "%s",\n' "$serial"
printf '  "metadata": {\n'
printf '    "timestamp": "%s",\n' "$timestamp"
printf '    "tools": [{"vendor": "Creation OS", "name": "scripts/security/sbom.sh", "version": "1.0"}],\n'
printf '    "component": {\n'
printf '      "type": "application",\n'
printf '      "bom-ref": "pkg:generic/creation-os@%s",\n' "$repo_version"
printf '      "name": "creation-os",\n'
printf '      "version": "%s",\n' "$repo_version"
printf '      "hashes": [{"alg": "SHA-256", "content": "%s"}],\n' "$repo_hash"
printf '      "licenses": [{"license": {"id": "AGPL-3.0-or-later"}}]\n'
printf '    }\n'
printf '  },\n'
printf '  "components": [\n'

first=1
for dir in src/v*; do
    [ -d "$dir" ] || continue
    name=$(basename "$dir")
    # Hash all .c/.h files in this component, deterministic order.
    hash=$(find "$dir" \( -name '*.c' -o -name '*.h' \) -type f 2>/dev/null \
            | LC_ALL=C sort \
            | xargs shasum -a 256 2>/dev/null \
            | shasum -a 256 \
            | awk '{print $1}')
    if [ -z "$hash" ]; then
        hash="empty"
    fi
    if [ "$first" -eq 0 ]; then printf ',\n'; fi
    first=0
    printf '    {\n'
    printf '      "type": "library",\n'
    printf '      "bom-ref": "pkg:generic/creation-os/%s",\n' "$name"
    printf '      "name": "creation-os/%s",\n' "$name"
    printf '      "version": "%s",\n' "$name"
    printf '      "hashes": [{"alg": "SHA-256", "content": "%s"}],\n' "$hash"
    printf '      "licenses": [{"license": {"id": "AGPL-3.0-or-later"}}],\n'
    printf '      "description": "Creation OS %s subsystem"\n' "$name"
    printf '    }'
done
printf '\n  ]\n'
printf '}\n'
