#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
#
# scripts/distclean.sh — deep-clean the repo root.
#
# Removes all generated binaries matching the canonical artifact globs
# (creation_os_v*, cos-*, creation_os_sigma_*, .dSYM bundles), plus
# runtime byproducts (session logs, SBOM stub). Source files (.c/.h/.md)
# are never touched; the helper uses an explicit file-list for named
# binaries and a safe numeric glob ("creation_os_v<N>") for versioned
# ones. Idempotent; safe to run on a clean checkout.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

shopt -s nullglob extglob

# 1. Named binaries (keep everything on a single line per entry so that a
#    source file with the same stem + .c / .h cannot be caught by mistake).
named_binaries=(
  creation_os
  creation_os_openai_stub
  creation_os_suite_stub
  creation_os_native_m4
  creation_os_proxy
  creation_os_mcp
  creation_os_server
  creation_os_iron_gate
  cos
  cos_lm
  license_attest
  license_attest_hardened
  test_bsc
  test_sigma_pipeline_integration
  tokenizer_throughput
  binding_fidelity
  vocab_scaling
  vs_transformer
  oracle_speaks
  oracle_ultimate
  gemm_vs_bsc
  coherence_gate_batch
  hv_agi_gate_neon
  genesis
  qhdc
  SBOM.json
  cos_chat_transcript.jsonl
  repro_bundle.txt
  repro_bundle.json
)

for f in "${named_binaries[@]}"; do
  if [[ -f "$f" && ! -L "$f" ]]; then
    rm -f -- "$f"
  fi
done

# 2. Versioned binaries: match "creation_os_v<digits>" optionally followed
#    by "_<anything>" (asan, ubsan, hardened, reason, tools, ...).
#    The extglob pattern "+([0-9])" ensures the stem is at least one digit
#    and cannot match "creation_os_v10.c" (which has "." before "c").
for f in creation_os_v+([0-9]) creation_os_v+([0-9])_*; do
  if [[ -f "$f" && ! -L "$f" ]]; then
    rm -f -- "$f"
  fi
done

# 3. Other binary families (all plain-file globs; never matches .c/.h).
for f in creation_os_server_* creation_os_sigma_* cos-* libcos_plugin_demo.*; do
  if [[ -f "$f" && ! -L "$f" ]]; then
    rm -f -- "$f"
  fi
done

# 4. .dSYM debug bundles at root.
for d in *.dSYM; do
  if [[ -d "$d" ]]; then
    rm -rf -- "$d"
  fi
done

# 5. Runtime session logs.
rm -f metrics/session_*.jsonl 2>/dev/null || true

# 6. Finder duplicates (whole-tree).
bash scripts/prune_finder_duplicates.sh

echo "distclean: OK"
