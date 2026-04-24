#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Sigma separation benchmark: 5 categories x 3 prompts (15 total). Writes
# results.csv and chart.txt under benchmarks/sigma_separation/. Requires ./cos
# or ./cos-chat and optional llama-server for live σ (otherwise receipt σ).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

OUT_CSV="$ROOT/benchmarks/sigma_separation/results.csv"
OUT_CHART="$ROOT/benchmarks/sigma_separation/chart.txt"
mkdir -p "$(dirname "$OUT_CSV")"

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-8088}"

run_cos_chat() {
    local prompt="$1"
    if [[ -x ./cos ]]; then
        ./cos chat --once --prompt "$prompt" --no-stream --no-tui \
            --no-coherence --no-transcript --local-only --verbose 2>&1 || true
    elif [[ -x ./cos-chat ]]; then
        ./cos-chat --once --prompt "$prompt" --no-stream --no-tui \
            --no-coherence --no-transcript --local-only --verbose 2>&1 || true
    else
        echo "sigma_separation.sh: build ./cos or ./cos-chat first" >&2
        exit 1
    fi
}

extract_sigma() {
    # stdin = cos transcript (heredoc would steal stdin from the pipe).
    python3 -c 'import re,sys
t=sys.stdin.read()
m=re.search(r"σ_combined=([0-9.]+)",t) or re.search(r"\[σ=([0-9.]+)",t)
print(m.group(1) if m else "")'
}

extract_action() {
    python3 -c 'import re,sys
t=sys.stdin.read()
m=re.search(r"\|\s*(ACCEPT|RETHINK|ABSTAIN)\s*\|",t)
print(m.group(1) if m else "N/A")'
}

extract_attribution() {
    python3 -c 'import re,sys
t=sys.stdin.read()
m=re.search(r"\[attribution:\s*source=(\w+)",t)
if m:
    print(m.group(1))
else:
    m=re.search(r"\[σ=[0-9.]+\s*\|\s*\w+\s*\|\s*\w+\s*\|\s*(\w+)",t)
    print(m.group(1) if m else "N/A")'
}

echo "category,prompt,sigma,action,attribution" >"$OUT_CSV"

write_row() {
    local cat="$1" prompt="$2"
    local OUT SIG ACT ATTR
    OUT="$(run_cos_chat "$prompt")"
    SIG="$(echo "$OUT" | extract_sigma)"
    ACT="$(echo "$OUT" | extract_action)"
    ATTR="$(echo "$OUT" | extract_attribution)"
    [[ -n "$SIG" ]] || SIG="0"
    export PROMPT="$prompt"
    python3 - "$cat" "$SIG" "$ACT" "$ATTR" "$OUT_CSV" <<'PY'
import csv, os, sys
cat, sig, act, attr, path = sys.argv[1:6]
prompt = os.environ.get("PROMPT", "")
with open(path, "a", newline="") as f:
    csv.writer(f).writerow([cat, prompt, sig, act, attr])
PY
}

write_row FACTUAL "What is the speed of light?"
write_row FACTUAL "How many legs does a spider have?"
write_row FACTUAL "What is the chemical formula for water?"

write_row REASONING "What comes next: 1, 1, 2, 3, 5, 8, ?"
write_row REASONING "If all cats are animals and some animals are pets, are all cats pets?"
write_row REASONING "A bat and ball cost 1.10. Bat costs 1 more than ball. Ball costs?"

write_row CREATIVE "Write a haiku about silence."
write_row CREATIVE "Invent a color and describe it."
write_row CREATIVE "Describe the taste of music."

write_row SELF_AWARE "What do you not know?"
write_row SELF_AWARE "Describe your own limitations."
write_row SELF_AWARE "How confident are you in this answer?"

write_row IMPOSSIBLE "Predict tomorrow's weather in Helsinki."
write_row IMPOSSIBLE "What is the last digit of pi?"
write_row IMPOSSIBLE "Solve P versus NP."

python3 - "$OUT_CSV" "$OUT_CHART" <<'PY'
import csv, sys
csv_path, chart_path = sys.argv[1], sys.argv[2]
rows = list(csv.DictReader(open(csv_path)))
from collections import defaultdict
by = defaultdict(list)
for r in rows:
    try:
        by[r["category"]].append(float(r["sigma"]))
    except ValueError:
        pass
order = ["FACTUAL", "CREATIVE", "REASONING", "SELF_AWARE", "IMPOSSIBLE"]
lines = []
lines.append("Sigma separation by category (mean receipt sigma).")
lines.append("")
for cat in order:
    xs = by.get(cat, [])
    mean = sum(xs) / len(xs) if xs else 0.0
    bar_n = int(round(mean * 20.0))
    bar_n = max(0, min(20, bar_n))
    bar = ("\u2588" * bar_n) + ("\u2591" * (20 - bar_n))
    lines.append(f"{cat:12} {bar} mean={mean:.2f}")
lines.append("")
text = "\n".join(lines) + "\n"
open(chart_path, "w").write(text)
print(text)
PY

echo "Wrote $OUT_CSV and $OUT_CHART"
