#!/usr/bin/env bash
# Creation OS — reproducible-build sanity.
#
# Builds creation_os_v60 twice with SOURCE_DATE_EPOCH pinned and
# compares SHA-256 digests.  If both digests match the build is
# reproducible under the current toolchain + host clock normalisation.
#
# This is a local smoke, not SLSA L3 provenance.  Full provenance
# generation (sigstore + Rekor) is slated under the P-tier roadmap.

set -euo pipefail
cd "$(dirname "$0")/../.."

SDE=${SOURCE_DATE_EPOCH:-1767225600}   # 2026-01-01T00:00:00Z
export SOURCE_DATE_EPOCH=$SDE

echo "=== Creation OS reproducible-build (SOURCE_DATE_EPOCH=$SDE) ==="

# Build #1
rm -f creation_os_v60
make -s standalone-v60 >/dev/null
h1=$(shasum -a 256 creation_os_v60 | awk '{print $1}')
mv creation_os_v60 creation_os_v60.rep1

# Build #2
make -s standalone-v60 >/dev/null
h2=$(shasum -a 256 creation_os_v60 | awk '{print $1}')
mv creation_os_v60 creation_os_v60.rep2

echo "  build-1 sha256: $h1"
echo "  build-2 sha256: $h2"

rm -f creation_os_v60.rep1 creation_os_v60.rep2

if [ "$h1" = "$h2" ]; then
    echo "reproducible-build: PASS (digests identical)"
    exit 0
else
    echo "reproducible-build: FAIL (digest drift — toolchain carries non-deterministic metadata)"
    exit 1
fi
