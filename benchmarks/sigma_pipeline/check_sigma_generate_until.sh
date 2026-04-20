#!/usr/bin/env bash
# Smoke test for v103.1 generate_until with σ-logging.
#
# Uses the stub backend (deterministic; no BitNet/PyTorch required) so
# this runs on any host.  Verifies:
#   - JSONL log produced (start + token records + summary)
#   - σ-peak reported and > 0
#   - At the default thresholds the stub trace triggers either RETHINK
#     or ABSTAIN on the fixed high-σ token "forty" (step 3)
#   - Python mirror agrees with the C primitives (parity)
set -euo pipefail

cd "$(dirname "$0")/../.."

PY=".venv-bitnet/bin/python"
if [[ ! -x "$PY" ]]; then
    PY=python3
fi

LOG="$(mktemp -t sigma_gen_until.XXXXXX.jsonl)"
trap 'rm -f "$LOG"' EXIT

OUT="$(PYTHONPATH=scripts "$PY" -m sigma_pipeline.generate_until \
        --backend stub \
        --prompt "What is 2+2?" \
        --tau-accept 0.30 --tau-rethink 0.70 --tau-escalate 0.75 \
        --max-tokens 16 \
        --log "$LOG")"

echo "  · summary: $OUT"

python3 - "$LOG" <<'PYEOF'
import json, sys
log = [json.loads(l) for l in open(sys.argv[1], encoding="utf-8")]
kinds = [r["kind"] for r in log]
assert kinds[0]  == "start",   f"first record not 'start': {kinds[0]}"
assert kinds[-1] == "summary", f"last record not 'summary': {kinds[-1]}"
tokens = [r for r in log if r["kind"] == "token"]
assert len(tokens) >= 1, "no token records emitted"
peaks = [r["sigma_peak"] for r in tokens]
assert max(peaks) > 0.0, "σ_peak never rose above 0"
# Stub trace hits σ=0.78 on token "forty" (step 3) → ABSTAIN or RETHINK.
terminal = log[-1]["terminal_action"]
assert terminal in ("RETHINK", "ABSTAIN"), \
    f"unexpected terminal action: {terminal}"
print(f"  · tokens={len(tokens)} peak={max(peaks):.3f} terminal={terminal}")
PYEOF

# Run the parity check too — must PASS.
PYTHONPATH=scripts "$PY" benchmarks/sigma_pipeline/check_sigma_parity.py

echo "check-sigma-generate-until: PASS (JSONL shape + σ_peak + policy terminal)"
