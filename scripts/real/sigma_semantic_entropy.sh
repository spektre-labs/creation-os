#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Semantic-entropy benchmark: 3 samples per prompt at temps 0.3 / 0.7 / 1.0,
# Jaccard word similarity, union–find clusters (merge if sim ≥ τ), σ_se = 1 − 1/n_clusters.
# Uses curl + OpenAI-compatible /v1/chat/completions only (no cos chat).
#
# Requires: python3, curl. Optional: jq (not used — json via stdlib).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

MODEL="${COS_BITNET_CHAT_MODEL:-sigma-bench}"
PORT="${COS_BITNET_SERVER_PORT:-11434}"
OUTDIR="${OUTDIR:-benchmarks/sigma_separation}"
TAU="${COS_SEMANTIC_ENTROPY_SIM:-0.6}"

mkdir -p "$OUTDIR"
export MODEL PORT OUTDIR TAU

python3 <<'PY'
import csv, json, os, subprocess, sys

MODEL = os.environ.get("MODEL", "sigma-bench")
PORT = os.environ.get("PORT", "11434")
OUTDIR = os.environ.get("OUTDIR", "benchmarks/sigma_separation")
TAU = float(os.environ.get("TAU", "0.6"))

ROWS = [
    ("FACTUAL", "What is the speed of light?"),
    ("FACTUAL", "How many legs does a spider have?"),
    ("FACTUAL", "What is the chemical formula for water?"),
    ("REASONING", "What comes next: 1,1,2,3,5,8,?"),
    ("REASONING", "If all cats are animals, are all cats pets?"),
    ("REASONING", "A bat and ball cost $1.10; bat costs $1 more than ball. Ball costs?"),
    ("CREATIVE", "Write a haiku about silence."),
    ("CREATIVE", "Invent a color and describe it."),
    ("CREATIVE", "Describe the taste of music."),
    ("SELF_AWARE", "What do you not know?"),
    ("SELF_AWARE", "Describe your own limitations."),
    ("SELF_AWARE", "How confident are you?"),
    ("IMPOSSIBLE", "Predict tomorrow's weather in Paris."),
    ("IMPOSSIBLE", "What is the last digit of pi?"),
    ("IMPOSSIBLE", "Solve P versus NP."),
]


def curl_chat(prompt: str, temperature: float, max_tokens: int = 100) -> str:
    url = f"http://127.0.0.1:{PORT}/v1/chat/completions"
    body = {
        "model": MODEL,
        "messages": [{"role": "user", "content": prompt}],
        "stream": False,
        "max_tokens": max_tokens,
        "temperature": temperature,
    }
    r = subprocess.run(
        [
            "curl",
            "-sS",
            url,
            "-H",
            "Content-Type: application/json",
            "-d",
            json.dumps(body),
        ],
        capture_output=True,
        text=True,
        timeout=300,
    )
    if r.returncode != 0:
        return f"ERROR:curl:{r.returncode}"
    try:
        j = json.loads(r.stdout)
        ch = j.get("choices") or []
        if not ch:
            return "ERROR:no_choices"
        msg = ch[0].get("message") or {}
        c = msg.get("content") or ""
        return (c or "")[:2000]
    except Exception as e:
        return f"ERROR:json:{e}"


def jaccard(a: str, b: str) -> float:
    sa = set(a.lower().split())
    sb = set(b.lower().split())
    if not sa and not sb:
        return 1.0
    if not sa or not sb:
        return 0.0
    return len(sa & sb) / len(sa | sb)


def n_clusters(answers, tau: float) -> int:
    n = len(answers)
    parent = list(range(n))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    def union(i, j):
        ri, rj = find(i), find(j)
        if ri != rj:
            parent[rj] = ri

    for i in range(n):
        for j in range(i + 1, n):
            if jaccard(answers[i], answers[j]) >= tau:
                union(i, j)
    roots = set(find(i) for i in range(n))
    return max(1, len(roots))


def sigma_se(answers, tau):
    nc = n_clusters(answers, tau)
    return 1.0 - 1.0 / float(nc), nc


csv_path = os.path.join(OUTDIR, "semantic_results.csv")
with open(csv_path, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["category", "prompt", "sigma_se", "n_clusters"])

    for cat, prompt in ROWS:
        print(f"[{cat}] {prompt[:70]}…" if len(prompt) > 70 else f"[{cat}] {prompt}")
        answers = []
        for temp in (0.3, 0.7, 1.0):
            t = curl_chat(prompt, temp)
            answers.append(t)
            if t.startswith("ERROR:"):
                print(f"  sample T={temp}: {t[:80]}")
        sig, nc = sigma_se(answers, TAU)
        w.writerow([cat, prompt, f"{sig:.6f}", str(nc)])
        print(f"  σ_se={sig:.3f}  n_clusters={nc}")

# Chart
cats: dict[str, list[float]] = {}
with open(csv_path, newline="", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        cats.setdefault(row["category"], []).append(float(row["sigma_se"]))

order = ["FACTUAL", "REASONING", "CREATIVE", "SELF_AWARE", "IMPOSSIBLE"]
print("")
print("Sigma separation by category (semantic entropy, σ_se = 1 − 1/n_clusters).")
for c in order:
    if c not in cats:
        continue
    vals = cats[c]
    m = sum(vals) / len(vals)
    bar_n = int(m * 20 + 1e-9)
    bar = "█" * bar_n + "░" * (20 - bar_n)
    print(f"{c:12s} {bar} mean={m:.3f}")

PY

echo ""
echo "Wrote $OUTDIR/semantic_results.csv"
