#!/usr/bin/env bash
#
# SCI-5 smoke test: multi-σ ensemble.
#
# Pinned facts:
#   1. --self-test PASSes.
#   2. --demo is byte-deterministic across runs.
#   3. --demo prints all four component labels + σ_combined.
#   4. σ_combined parses as a float in [0, 1].
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_multi"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

# 1. Self-test.
"$BIN" --self-test >/dev/null

# 2. + 3. + 4. Demo determinism + structure.
OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT
"$BIN" --demo >"$OUT1"
"$BIN" --demo >"$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
    echo "check_sigma_multi: --demo non-deterministic" >&2; exit 3;
}

python3 - "$OUT1" <<'PY'
import re, sys
with open(sys.argv[1]) as f:
    text = f.read()

for label in ("σ_logprob", "σ_entropy", "σ_perplexity",
              "σ_consistency", "σ_combined"):
    assert label in text, (label, text)

m = re.search(r"σ_combined\s+([0-9.]+)", text)
assert m, text
v = float(m.group(1))
assert 0.0 <= v <= 1.0, v

print("check_sigma_multi: PASS", {"sigma_combined": v})
PY
