#!/usr/bin/env bash
# Smoke test for `cos chat --once` (stub backend, deterministic).
#
# Verifies the interactive demo integrates generate_until + reinforce +
# speculative correctly: banner, round 0 output line, ESCALATE stub
# marker, and a transcript JSONL record with the expected shape.
set -euo pipefail

cd "$(dirname "$0")/../.."

PY=".venv-bitnet/bin/python"
if [[ ! -x "$PY" ]]; then
    PY=python3
fi

TRANSCRIPT="$(mktemp -t cos_chat_transcript.XXXXXX.jsonl)"
trap 'rm -f "$TRANSCRIPT"' EXIT

# Prefer the compiled `cos` C launcher (cli/cos.c) if present; fall
# back to the Python module directly so this test runs even before
# `make cos` has been executed once.
if [[ -x ./cos ]]; then
    OUT="$(./cos chat --once --prompt "What is 2+2?" \
            --max-tokens 16 --transcript "$TRANSCRIPT" 2>&1)"
else
    OUT="$(PYTHONPATH=scripts "$PY" -m sigma_pipeline.cos_chat \
            --once --prompt "What is 2+2?" \
            --max-tokens 16 --transcript "$TRANSCRIPT" 2>&1)"
fi

echo "  · stdout:"
sed 's/^/        /' <<<"$OUT"

grep -q "cos chat"                         <<<"$OUT" \
    || { echo "FAIL: banner missing" >&2; exit 2; }
grep -q "round 0"                          <<<"$OUT" \
    || { echo "FAIL: round 0 line missing" >&2; exit 3; }
grep -qE "(ESCALATE|ABSTAIN|RETHINK)"      <<<"$OUT" \
    || { echo "FAIL: terminal action missing" >&2; exit 4; }

# Transcript must have exactly 1 JSONL record for --once.
n="$(wc -l <"$TRANSCRIPT" | tr -d ' ')"
[[ "$n" == "1" ]] \
    || { echo "FAIL: transcript records=$n (expected 1)" >&2; exit 5; }

"$PY" - "$TRANSCRIPT" <<'PYEOF'
import json, sys
rec = json.loads(open(sys.argv[1]).readline())
assert "prompt" in rec and "rounds" in rec and "final_text" in rec, rec
assert len(rec["rounds"]) >= 1, rec
r0 = rec["rounds"][0]
for key in ("terminal_action", "terminal_route", "sigma_peak", "tokens"):
    assert key in r0, f"missing {key} in round0: {r0}"
assert r0["terminal_action"] in ("ACCEPT", "RETHINK", "ABSTAIN")
assert r0["terminal_route"]  in ("LOCAL", "ESCALATE")
print(f"  · transcript round0: action={r0['terminal_action']} "
      f"route={r0['terminal_route']} σ_peak={r0['sigma_peak']:.3f}")
PYEOF

echo "check-cos-chat: PASS (banner + round + terminal + transcript shape)"
