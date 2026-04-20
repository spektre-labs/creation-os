#!/usr/bin/env bash
# Verify the σ pipeline builds under portable (non-native) flags —
# the contract for Raspberry Pi 5 / iPhone / ARM cross builds.
set -euo pipefail
cd "$(dirname "$0")/../.."

[[ -x scripts/edge/build_portable.sh ]] || {
  chmod +x scripts/edge/build_portable.sh
}

mkdir -p build
bash scripts/edge/build_portable.sh > build/edge.log 2>&1 || {
  echo "edge build failed; log:" >&2
  cat build/edge.log            >&2
  exit 2
}

# Every σ primitive translation unit must be present.
for name in reinforce speculative ttt engram moe multimodal tinyml swarm live continual unlearn; do
  [[ -s build/edge/"$name".o ]] || {
    echo "missing build/edge/$name.o" >&2
    exit 3
  }
done

# The tinyml demo must link + run successfully under portable flags.
[[ -x build/edge/creation_os_sigma_tinyml ]] || {
  echo "missing portable tinyml binary" >&2
  exit 4
}

# Confirm the log advertises -Os, NOT -march=native.
grep -q '\-Os'           build/edge.log || { echo "no -Os in build log"         >&2; exit 5; }
# '-march=native' must not appear as a compiler flag.  The build script
# mentions the phrase in its human-readable banner, so only fail on the
# leading dash+space form.
if grep -E '(^| )-march=native( |$)' build/edge.log >/dev/null; then
  echo "-march=native leaked into edge build" >&2
  exit 6
fi

# Report the σ library footprint (for humans; sanity-cap at 256 KiB).
tiny_sz=$(wc -c < build/edge/creation_os_sigma_tinyml | tr -d ' ')
[[ "$tiny_sz" -lt $((256 * 1024)) ]] || {
  echo "portable tinyml binary too large: $tiny_sz B" >&2
  exit 7
}

echo "check-edge-portability: PASS (11 σ primitives + tinyml demo build with -Os, no -march=native; tinyml binary ${tiny_sz} B)"
