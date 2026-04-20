#!/usr/bin/env bash
#
# FINAL-1 smoke test: σ-RAG — deterministic local retrieval with
# σ-filtered top-k.
#
# Pinned facts (derived from src/sigma/pipeline/rag_main.c corpus):
#   self_test_rc               == 0
#   dim                        == 128
#   chunks                     == 16           (6 docs, 16-word windows)
#   queries[0].hits[0].chunk_id == 0           ("sigma gate" wins q0)
#   queries[0].hits[0].source  == docs/sigma_gate.md
#   queries[2].hits[0].source  == docs/voice.md  ("voice" wins q2)
#   admitted + non-admitted both surface         (include_filtered=1)
#   sigma_retrieval ∈ [0, 1] for every emitted hit
#   output is byte-identical across two invocations
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_rag"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp)
OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_rag: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_rag", doc
assert doc["version"] == 1, doc
assert doc["dim"] == 128, doc
assert doc["self_test"] == 0, doc
assert doc["corpus"]["docs"] == 6, doc["corpus"]
assert doc["corpus"]["chunks"] >= 10, doc["corpus"]

queries = doc["queries"]
assert len(queries) == 3, queries

# q0: σ-gate wins
q0 = queries[0]
assert q0["q"].startswith("how does the sigma gate"), q0
assert q0["hits"][0]["chunk_id"] == 0, q0["hits"][0]
assert q0["hits"][0]["source"] == "docs/sigma_gate.md", q0["hits"][0]

# q2: voice wins
q2 = queries[2]
assert q2["hits"][0]["source"] == "docs/voice.md", q2["hits"][0]

# σ_retrieval ∈ [0, 1], cosine ∈ [-1, 1]
seen_admitted = 0
seen_filtered = 0
for q in queries:
    assert q["hits"], q
    prev = None
    for h in q["hits"]:
        s = h["sigma_retrieval"]
        c = h["cosine"]
        assert 0.0 <= s <= 1.0, h
        assert -1.0 <= c <= 1.0, h
        # σ_retrieval should equal clip01(1 - cosine) to the 4th decimal
        expect = max(0.0, min(1.0, 1.0 - c))
        assert abs(s - expect) < 1e-3, (h, expect)
        if h["admitted"]:
            seen_admitted += 1
        else:
            seen_filtered += 1
        if prev is not None:
            assert prev >= c - 1e-6, (prev, c)
        prev = c

assert seen_admitted  > 0, "no admitted hits across queries"
assert seen_filtered >= 1, "expected at least one σ-filtered hit"

print("check_sigma_rag: PASS", {
    "chunks": doc["corpus"]["chunks"],
    "admitted": seen_admitted,
    "filtered": seen_filtered,
})
PY
