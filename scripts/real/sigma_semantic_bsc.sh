#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Semantic BSC σ benchmark: 3 curl completions per prompt, then Python
# replicates cos_inference_bsc_encode_prompt + Hamming/D (same as C).
# Portable bash (no associative arrays). No cos chat.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

MODEL="${COS_BITNET_CHAT_MODEL:-sigma-bench}"
PORT="${COS_BITNET_SERVER_PORT:-11434}"
OUTDIR="${OUTDIR:-benchmarks/sigma_separation}"
mkdir -p "$OUTDIR"

export MODEL PORT OUTDIR

echo "=== Semantic BSC σ Benchmark ===" | tee "$OUTDIR/semantic_bsc_log.txt"

python3 <<'PY'
import csv, json, os, subprocess, sys

MODEL = os.environ["MODEL"]
PORT = os.environ["PORT"]
OUTDIR = os.environ["OUTDIR"]
COS_INF_W = 64
COS_D = 4096

ROWS = [
    ("FACTUAL", "What is the speed of light?"),
    ("FACTUAL", "How many legs does a spider have?"),
    ("FACTUAL", "What is the chemical formula for water?"),
    ("REASONING", "What comes next: 1,1,2,3,5,8,?"),
    ("REASONING", "If all cats are animals, are all cats pets?"),
    ("REASONING", "A bat and ball cost $1.10. Bat costs $1 more than ball. Ball costs?"),
    ("CREATIVE", "Write a haiku about silence."),
    ("CREATIVE", "Invent a color and describe it."),
    ("CREATIVE", "Describe the taste of music."),
    ("SELF_AWARE", "What do you not know?"),
    ("SELF_AWARE", "Describe your own limitations."),
    ("SELF_AWARE", "How confident are you in this answer?"),
    ("IMPOSSIBLE", "Predict tomorrow's weather in Helsinki."),
    ("IMPOSSIBLE", "What is the last digit of pi?"),
    ("IMPOSSIBLE", "Solve P versus NP."),
]


def fnv1a64(s: str) -> int:
    h = 14695981039346656037
    for c in s.encode("utf-8", errors="replace"):
        h ^= c
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def hv_word(word: str):
    seed = fnv1a64(word)
    out = []
    for _ in range(COS_INF_W):
        seed = (seed * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF
        out.append(seed)
    return out


def split_words(s: str):
    words = []
    p = 0
    while p < len(s) and len(words) < 128:
        while p < len(s) and s[p].isspace():
            p += 1
        if p >= len(s):
            break
        w = []
        while p < len(s) and not s[p].isspace() and len(w) < 63:
            w.append(s[p])
            p += 1
        if w:
            words.append("".join(w))
    return words


def bsc_encode_prompt(utf8: str):
    words = split_words(utf8 or "")
    out = [0] * COS_INF_W
    if not words:
        return out
    if len(words) == 1:
        return hv_word(words[0])
    for i in range(len(words) - 1):
        h0 = hv_word(words[i])
        h1 = hv_word(words[i + 1])
        pair = [a ^ b for a, b in zip(h0, h1)]
        out = [a ^ b for a, b in zip(out, pair)]
    return out


def hamming_norm(a, b):
    d = 0
    for x, y in zip(a, b):
        d += bin((x ^ y) & 0xFFFFFFFFFFFFFFFF).count("1")
    return d / float(COS_D)


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
        ["curl", "-sS", url, "-H", "Content-Type: application/json", "-d", json.dumps(body)],
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
        return (msg.get("content") or "")[:3000]
    except Exception as e:
        return f"ERROR:json:{e}"


def bsc_sigma(answers):
    v = [bsc_encode_prompt(a) for a in answers]
    s01 = 1.0 - hamming_norm(v[0], v[1])
    s02 = 1.0 - hamming_norm(v[0], v[2])
    s12 = 1.0 - hamming_norm(v[1], v[2])
    mean_sim = (s01 + s02 + s12) / 3.0
    sigma = 1.0 - mean_sim
    return sigma, s01, s02, s12


log_path = os.path.join(OUTDIR, "semantic_bsc_log.txt")
csv_path = os.path.join(OUTDIR, "semantic_bsc_results.csv")

with open(csv_path, "w", newline="", encoding="utf-8") as cf:
    w = csv.writer(cf)
    w.writerow(["category", "prompt", "sigma_se", "sim_01", "sim_02", "sim_12"])

    for cat, prompt in ROWS:
        line = f"[{cat}] {prompt}"
        print(line)
        with open(log_path, "a", encoding="utf-8") as lg:
            lg.write(line + "\n")

        answers = []
        for temp in (0.3, 0.7, 1.0):
            t = curl_chat(prompt, temp)
            answers.append(t)
            if t.startswith("ERROR:"):
                print(f"  T={temp}: {t[:100]}")

        sig, s01, s02, s12 = bsc_sigma(answers)
        w.writerow([cat, prompt, f"{sig:.6f}", f"{s01:.6f}", f"{s02:.6f}", f"{s12:.6f}"])
        msg = f"  σ_se={sig:.3f}  sim=({s01:.2f},{s02:.2f},{s12:.2f})"
        print(msg)
        with open(log_path, "a", encoding="utf-8") as lg:
            lg.write(msg + "\n")

cats = {}
with open(csv_path, newline="", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        cats.setdefault(row["category"], []).append(float(row["sigma_se"]))

print("")
print("Sigma separation by category (BSC semantic, σ = 1 − mean pairwise sim).")
order = ["FACTUAL", "REASONING", "CREATIVE", "SELF_AWARE", "IMPOSSIBLE"]
for c in order:
    if c not in cats:
        continue
    vals = cats[c]
    m = sum(vals) / len(vals)
    bar_n = min(20, int(m * 20 + 1e-9))
    bar = "█" * bar_n + "░" * (20 - bar_n)
    print(f"{c:12s} {bar} mean={m:.3f}")

vals = {c: sum(cats[c]) / len(cats[c]) for c in order if c in cats}
if vals:
    gap = max(vals.values()) - min(vals.values())
    print("")
    if gap > 0.15:
        print(f"SEPARATION DETECTED: gap={gap:.3f} (> 0.15)")
    else:
        print(f"NO STRONG SEPARATION: gap={gap:.3f} (<= 0.15)")

PY

cp -f "$OUTDIR/semantic_bsc_results.csv" "$OUTDIR/chart_semantic_bsc.txt" 2>/dev/null || true
echo "=== Done === Wrote $OUTDIR/semantic_bsc_results.csv"
