#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v61 — build the example LSM-BPF policy.  Linux only.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [ "$(uname -s)" != "Linux" ]; then
  echo "ebpf-policy: SKIP (requires Linux + libbpf headers; host is $(uname -s))"
  exit 0
fi

if ! command -v clang >/dev/null 2>&1; then
  echo "ebpf-policy: SKIP (no clang on PATH)"
  exit 0
fi
if ! [ -e /usr/include/bpf/bpf_helpers.h ] && \
   ! [ -e /usr/local/include/bpf/bpf_helpers.h ]; then
  echo "ebpf-policy: SKIP (libbpf headers not found)"
  exit 0
fi

OUT=".build/ebpf/sigma_shield.bpf.o"
mkdir -p "$(dirname "$OUT")"
clang -O2 -Wall -target bpf -c ebpf/sigma_shield.bpf.c -o "$OUT"
echo "ebpf-policy: OK ($OUT; load via sudo bpftool prog loadall)"
