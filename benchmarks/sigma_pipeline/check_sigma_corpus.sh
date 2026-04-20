#!/usr/bin/env bash
#
# FINAL-4 smoke test: σ-Corpus — Creation OS's own prose as RAG
# knowledge base.
#
# Pinned facts (per data/corpus/manifest.txt, 12 hand-curated docs):
#   self_test.rag              == 0
#   self_test.corpus           == 0
#   ingest.total               == 12    (manifest lines)
#   ingest.ok                  == 12    (every file readable)
#   ingest.skipped             == 0
#   ingest.total_chunks        >= 200   (corpus is ≥ ~100 KiB text)
#   every query returns ≥ 1 hit
#   every hit's source is a basename found in manifest.txt
#   output is byte-identical across two invocations
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_corpus"
MANIFEST="data/corpus/manifest.txt"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }
[[ -f "$MANIFEST" ]] || { echo "missing $MANIFEST" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_corpus: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" "$MANIFEST" <<'PY'
import json, os, sys

with open(sys.argv[1]) as f:
    doc = json.load(f)

# Collect allowed source basenames from the manifest.
allowed = set()
man_dir = os.path.dirname(sys.argv[2])
with open(sys.argv[2]) as f:
    for raw in f:
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        allowed.add(os.path.basename(line))

assert doc["kernel"] == "sigma_corpus", doc
assert doc["self_test"]["rag"] == 0, doc
assert doc["self_test"]["corpus"] == 0, doc

ing = doc["ingest"]
assert ing["total"] == len(allowed), (ing["total"], len(allowed))
assert ing["ok"] == ing["total"], ing
assert ing["skipped"] == 0, ing
assert ing["total_chunks"] >= 200, ing["total_chunks"]

ingested_names = {f["name"] for f in ing["files"]}
assert allowed == ingested_names, (allowed, ingested_names)

# The hallmark paper must have contributed ≥ 10 chunks.
paper = next(f for f in ing["files"] if f["name"] == "creation_os_v1.md")
assert paper["ok"] is True and paper["chunks"] >= 10, paper

queries = doc["queries"]
assert len(queries) == 3, queries
for q in queries:
    assert q["hits"], q
    for h in q["hits"]:
        assert h["source"] in allowed, h
        assert 0.0 <= h["sigma_retrieval"] <= 1.0, h

# First query ("what is the sigma gate...") should surface the
# paper or the codex — both are pinned to be in the corpus.
top_q0 = queries[0]["hits"][0]["source"]
assert top_q0 in {"creation_os_v1.md", "WHAT_IS_REAL_v31.md",
                  "atlantean_codex.txt", "SIGMA_FULL_STACK.md",
                  "SIGMA_GUIDED_SPEC.md",
                  "RESEARCH_AND_THESIS_ARCHITECTURE.md",
                  "GLOSSARY.md"}, top_q0

print("check_sigma_corpus: PASS", {
    "files": ing["total"],
    "chunks": ing["total_chunks"],
    "top_q0": top_q0,
})
PY
