#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_continual
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"       >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true"  >&2; exit 4; }

# Canonical demo: grad = [1.0, 0.0, 0.5, 0.0], threshold 0.005, decay 0.9.
# importance = 0.1·(1, 0, 0.25, 0) → (0.1, 0, 0.025, 0).
# Params 0 and 2 cross threshold → zeros_written = 2, frozen_count = 2.
grep -q '"grad_masked":2'        <<<"$OUT" || { echo "grad_masked"    >&2; exit 5; }
grep -q '"frozen_count":2'       <<<"$OUT" || { echo "frozen_count"   >&2; exit 6; }
grep -q '"n_steps":1'            <<<"$OUT" || { echo "n_steps"        >&2; exit 7; }
grep -q '"freeze_threshold":0.0050' <<<"$OUT" || { echo "threshold"   >&2; exit 8; }
grep -q '"importance":\[0.1000,0.0000,0.0250,0.0000\]' <<<"$OUT" || { echo "importance vector" >&2; exit 9; }

echo "check-sigma-continual: PASS (Fisher importance + freeze mask: 2/4 params frozen on step 1)"
