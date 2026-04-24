#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Definitive σ benchmark: pick a model that returns non-empty message.content
# on /v1/chat/completions; else fall back to Qwen via /api/chat with think:false.
# For each prompt: 3 temps → semantic σ (Jaccard on normalized text) + logprob σ if present.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

PORT="${COS_OLLAMA_PORT:-${COS_BITNET_SERVER_PORT:-11434}}"
OUTDIR="${OUTDIR:-benchmarks/sigma_separation}"
mkdir -p "$OUTDIR"

export PORT OUTDIR

echo "=== σ DEFINITIVE BENCHMARK ==="
echo "Date: $(date -Iseconds 2>/dev/null || date)"
echo ""

# --- pick best model (non-empty content on OpenAI-compat) ---
BEST_MODEL=""
USE_NATIVE_API=0
for M in gemma3:4b llama3.2:3b qwen3.5:4b; do
    export PICK_MODEL="$M" PICK_PORT="$PORT"
    RESULT=$(python3 << 'PICKPY'
import json, os, subprocess
port = os.environ.get("PICK_PORT", "11434")
model = os.environ["PICK_MODEL"]
body = json.dumps(
    {
        "model": model,
        "messages": [{"role": "user", "content": "What is 2+2?"}],
        "stream": False,
        "max_tokens": 40,
    }
)
r = subprocess.run(
    [
        "curl", "-sS",
        f"http://127.0.0.1:{port}/v1/chat/completions",
        "-H", "Content-Type: application/json",
        "-d",
        body,
    ],
    capture_output=True,
    text=True,
    timeout=120,
)
try:
    d = json.loads(r.stdout)
    c = d["choices"][0]["message"].get("content", "")
    print(c.strip()[:30] if c.strip() else "EMPTY")
except Exception:
    print("ERROR")
PICKPY
)
    echo "Model $M → content: '$RESULT'"
    case "$RESULT" in
        EMPTY|ERROR|'') ;;
        *)
            if [ -z "$BEST_MODEL" ]; then
                BEST_MODEL="$M"
            fi
            ;;
    esac
done

if [ -z "$BEST_MODEL" ]; then
    echo "No OpenAI-compat model returned content; trying Qwen /api/chat think:false..."
    RESULT=$(curl -s "http://127.0.0.1:${PORT}/api/chat" \
      -H "Content-Type: application/json" \
      -d '{"model":"qwen3.5:4b","messages":[{"role":"user","content":"What is 2+2?"}],"stream":false,"think":false}' \
      2>/dev/null | python3 -c "
import json,sys
d=json.load(sys.stdin)
c=(d.get('message') or {}).get('content','')
print(c.strip()[:30] if c.strip() else 'EMPTY')
" 2>/dev/null || echo "ERROR")
    echo "Qwen think:false → '$RESULT'"
    if [ "$RESULT" != "EMPTY" ] && [ -n "$RESULT" ] && [ "$RESULT" != "ERROR" ]; then
        BEST_MODEL="qwen3.5:4b"
        USE_NATIVE_API=1
        echo "Using qwen3.5:4b with native /api/chat (think:false)."
    else
        echo "FATAL: No working model path found."
        exit 1
    fi
fi

export BEST_MODEL USE_NATIVE_API

echo ""
echo "Selected model: $BEST_MODEL (USE_NATIVE_API=$USE_NATIVE_API)"
echo ""

MODEL="$BEST_MODEL"

ask() {
    local PROMPT="$1"
    local TEMP="$2"
    export ASK_PROMPT="$PROMPT"
    export ASK_TEMP="$TEMP"
    export ASK_MODEL="$MODEL"
    export ASK_USE_NATIVE="$USE_NATIVE_API"
    python3 << 'PYASK'
import json, os, subprocess, sys

port = os.environ.get("PORT", "11434")
model = os.environ["ASK_MODEL"]
prompt = os.environ["ASK_PROMPT"]
temp = float(os.environ["ASK_TEMP"])
use_native = os.environ.get("ASK_USE_NATIVE", "0") == "1"

if use_native:
    body = {
        "model": model,
        "messages": [
            {"role": "system", "content": "Answer concisely in one sentence."},
            {"role": "user", "content": prompt},
        ],
        "stream": False,
        "think": False,
        "options": {"temperature": temp, "num_predict": 80},
    }
    url = f"http://127.0.0.1:{port}/api/chat"
else:
    body = {
        "model": model,
        "messages": [
            {"role": "system", "content": "Answer concisely in one sentence."},
            {"role": "user", "content": prompt},
        ],
        "stream": False,
        "max_tokens": 80,
        "temperature": temp,
    }
    url = f"http://127.0.0.1:{port}/v1/chat/completions"

r = subprocess.run(
    ["curl", "-sS", url, "-H", "Content-Type: application/json", "-d", json.dumps(body)],
    capture_output=True,
    text=True,
    timeout=300,
)
if r.returncode != 0:
    print("ERROR", file=sys.stderr)
    sys.exit(0)
try:
    d = json.loads(r.stdout)
except Exception:
    print("ERROR")
    sys.exit(0)
if use_native:
    msg = d.get("message") or {}
else:
    ch = d.get("choices") or []
    if not ch:
        print("ERROR")
        sys.exit(0)
    msg = ch[0].get("message") or {}
t = (msg.get("content") or "").strip()
if not t:
    rtxt = msg.get("reasoning_content") or msg.get("reasoning") or msg.get("thinking") or ""
    lines = [ln.strip() for ln in rtxt.strip().split("\n") if ln.strip()]
    t = lines[-1] if lines else ""
print(t[:200])
PYASK
}

compute_sigma() {
    export SS_A1="$1" SS_A2="$2" SS_A3="$3"
    python3 << 'PYCOMP'
import os, re

def norm(s):
    return re.sub(r"[^a-z0-9 ]", "", (s or "").lower()).strip()

a = [norm(os.environ.get("SS_A1", "")), norm(os.environ.get("SS_A2", "")), norm(os.environ.get("SS_A3", ""))]

def sim(x, y):
    if not x or not y:
        return 0.0
    if x == y:
        return 1.0
    sx, sy = set(x.split()), set(y.split())
    if not sx or not sy:
        return 0.0
    j = len(sx & sy) / len(sx | sy)
    if x in y or y in x:
        j = max(j, 0.8)
    return j

avg = (sim(a[0], a[1]) + sim(a[0], a[2]) + sim(a[1], a[2])) / 3.0
print(f"{1.0 - avg:.3f}")
PYCOMP
}

logprob_sigma() {
    export LP_PROMPT="$1" LP_MODEL="$MODEL"
    python3 << 'PYLP'
import json, math, os, subprocess

port = os.environ.get("PORT", "11434")
model = os.environ["LP_MODEL"]
prompt = os.environ["LP_PROMPT"]
body = {
    "model": model,
    "messages": [{"role": "user", "content": prompt}],
    "stream": False,
    "logprobs": True,
    "top_logprobs": 3,
    "max_tokens": 80,
}
url = f"http://127.0.0.1:{port}/v1/chat/completions"
r = subprocess.run(
    ["curl", "-sS", url, "-H", "Content-Type: application/json", "-d", json.dumps(body)],
    capture_output=True,
    text=True,
    timeout=300,
)
if r.returncode != 0:
    print("N/A")
    raise SystemExit(0)
try:
    d = json.loads(r.stdout)
    lp = (d.get("choices") or [{}])[0].get("logprobs") or {}
    arr = lp.get("content") if isinstance(lp, dict) else None
    if not arr:
        print("N/A")
        raise SystemExit(0)
    vals = [t.get("logprob") for t in arr if isinstance(t, dict) and "logprob" in t]
    vals = [v for v in vals if v is not None]
    if not vals:
        print("N/A")
        raise SystemExit(0)
    probs = [math.exp(v) for v in vals]
    mp = sum(probs) / len(probs)
    print(f"{1.0 - mp:.3f}")
except Exception:
    print("N/A")
PYLP
}

CSV="$OUTDIR/definitive_results.csv"
echo "category,prompt,sigma_se,sigma_lp,a1,a2,a3" > "$CSV"

RESULTS=""

append_csv_row() {
    python3 -c "
import csv,sys
cat,prompt,sse,slp,a1,a2,a3 = sys.argv[1:8]
with open(sys.argv[8], 'a', newline='', encoding='utf-8') as f:
    w=csv.writer(f)
    w.writerow([cat,prompt,sse,slp,a1,a2,a3])
" "$@"
}

for entry in \
  "FACTUAL|How many legs does a spider have?" \
  "FACTUAL|What is the chemical formula for water?" \
  "FACTUAL|What is the capital of Japan?" \
  "REASONING|What comes next in the sequence 1 1 2 3 5 8?" \
  "REASONING|What is 15 times 17?" \
  "REASONING|A bat and ball cost 1.10 total. Bat costs 1 more than ball. What does the ball cost?" \
  "CREATIVE|Write a haiku about silence" \
  "CREATIVE|Invent a name for a new color" \
  "CREATIVE|Describe the taste of music in one word" \
  "SELF_AWARE|What topics are you uncertain about?" \
  "SELF_AWARE|Are you always correct?" \
  "SELF_AWARE|Rate your confidence from 0 to 10" \
  "IMPOSSIBLE|What is the exact temperature in Helsinki right now?" \
  "IMPOSSIBLE|What will happen in the world tomorrow?" \
  "IMPOSSIBLE|What is the last digit of pi?" \
; do
  CAT="${entry%%|*}"
  PROMPT="${entry#*|}"

  A1=$(ask "$PROMPT" 0.1)
  A2=$(ask "$PROMPT" 0.7)
  A3=$(ask "$PROMPT" 1.2)

  S_SE=$(compute_sigma "$A1" "$A2" "$A3")
  S_LP=$(logprob_sigma "$PROMPT")

  printf "%-12s σ_se=%-6s σ_lp=%-6s %s\n" "$CAT" "$S_SE" "$S_LP" "$PROMPT"
  printf "  [%.40s | %.40s | %.40s]\n" "$A1" "$A2" "$A3"

  append_csv_row "$CAT" "$PROMPT" "$S_SE" "$S_LP" "$A1" "$A2" "$A3" "$CSV"
  RESULTS+="$CAT $S_SE"$'\n'
done

echo ""
echo "=== CATEGORY MEANS (semantic entropy) ==="
export RESULTS_PY="$RESULTS"
python3 << 'PYCH'
import os
data = os.environ.get("RESULTS_PY", "")
cats = {}
for line in data.strip().split("\n"):
    p = line.strip().split()
    if len(p) >= 2:
        try:
            cats.setdefault(p[0], []).append(float(p[1]))
        except ValueError:
            pass
order = ["FACTUAL", "CREATIVE", "REASONING", "SELF_AWARE", "IMPOSSIBLE"]
for c in order:
    if c in cats:
        m = sum(cats[c]) / len(cats[c])
        bar_n = min(20, int(m * 20 + 1e-9))
        bar = "█" * bar_n + "░" * (20 - bar_n)
        print(f"{c:12s} {bar} mean={m:.3f}")
vals = {c: sum(cats[c]) / len(cats[c]) for c in order if c in cats}
if vals:
    lo, hi = min(vals.values()), max(vals.values())
    print(f"\nGap: {hi - lo:.3f}")
    print("SEPARATION ✓" if hi - lo > 0.15 else "No separation ✗")
    if hi - lo > 0.15:
        best = min(vals, key=vals.get)
        worst = max(vals, key=vals.get)
        print(f"Best (lowest σ): {best} = {vals[best]:.3f}")
        print(f"Worst (highest σ): {worst} = {vals[worst]:.3f}")
PYCH

echo ""
echo "=== LOGPROB σ (if available) ==="
python3 << PYLP2
import csv, os
path = os.path.join(os.environ.get("OUTDIR", "benchmarks/sigma_separation"), "definitive_results.csv")
cats = {}
try:
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            lp = row.get("sigma_lp", "N/A")
            if lp and lp != "N/A":
                try:
                    cats.setdefault(row["category"], []).append(float(lp))
                except ValueError:
                    pass
except FileNotFoundError:
    cats = {}
if cats:
    for c in ["FACTUAL", "CREATIVE", "REASONING", "SELF_AWARE", "IMPOSSIBLE"]:
        if c in cats:
            m = sum(cats[c]) / len(cats[c])
            bar_n = min(20, int(m * 20 + 1e-9))
            bar = "█" * bar_n + "░" * (20 - bar_n)
            print(f"{c:12s} {bar} mean={m:.3f}")
else:
    print("No logprobs available from this model/run.")
PYLP2

echo ""
echo "Results: $CSV"
echo "=== DONE ==="
