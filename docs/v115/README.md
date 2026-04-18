# v115 — σ-Memory: Persistent Cross-Session State

Creation OS historically had **no memory across sessions**. The Oracle
(v67 Noesis + v68 Mnemos) is an integer-kernel abstraction, not a
persistent store. v115 adds a real file-backed memory layer with a
σ-governance twist that no other RAG system ships today.

## The problem v115 solves

Retrieval-Augmented Generation pipelines rank candidate memories by
cosine similarity to the query embedding. When one of the retrieved
memories was produced by an **uncertain** generation, that doubt is
silently mixed into the next prompt — the LLM cannot distinguish a
confident recollection from a tentative guess and treats them as
equal evidence.

v115 fixes this at the storage layer: every episodic row carries the
σ_product observed at write time, and recall ranks by

    score(q, m) = cosine(q, m) / (1 + λ · σ_product(m))

Uncertain memories are down-weighted automatically; confident ones
dominate the context.

## What ships in v115.0

| Piece                         | Status |
|-------------------------------|--------|
| SQLite schema (3 tables)      | ✅ implemented |
| 384-d hash-shingled embedder  | ✅ implemented (deterministic, zero-dep) |
| σ-weighted cosine recall      | ✅ implemented + self-tested |
| `/v1/memory/search` endpoint  | ⏳ scheduled for v115.1 (library + CLI shipped) |
| Write-back on successful emit | ⏳ scheduled for v115.1 (library API available) |
| MiniLM-L6-v2 ONNX swap-in     | ⏳ scheduled for v115.1 (function-pointer seam in place) |

The embedder is the **dependency-free placeholder** (FNV-1a shingling
over unigrams/bigrams + character 3-grams, L2-normalised). This is
honest: we ship a working σ-weighted retrieval contract today, with a
function-pointer seam (`cos_v115_embed`) so the MiniLM drop-in replaces
only that single symbol.

## Storage model

    ~/.creation-os/memory.sqlite

    episodic_memory (id, content, embedding 384×f32, sigma_product,
                     session_id, tags, created_at)
    knowledge_base  (id, source_file, chunk, embedding, doc_type,
                     metadata_json)
    chat_history    (id, session_id, role, content, sigma_profile_json,
                     created_at)

Indices on `created_at` and `(session_id, created_at)`. WAL journal.

## Source

- [`src/v115/memory.h`](../../src/v115/memory.h) — API
- [`src/v115/memory.c`](../../src/v115/memory.c) — SQLite + embedder
  + σ-weighted top-k
- [`src/v115/main.c`](../../src/v115/main.c) — CLI (`--write-episodic`,
  `--search`, `--counts`, `--self-test`)
- [`benchmarks/v115/`](../../benchmarks/v115/) — roundtrip +
  σ-weighted recall merge-gate checks

## Self-test

```bash
make check-v115
```

Runs:

1. `creation_os_v115_memory --self-test` — embedder round-trip +
   in-memory SQLite schema + σ-weighted ranking on duplicate content
   with different σ.
2. `check_v115_memory_roundtrip.sh` — schema, JSON shape, top-k ordering.
3. `check_v115_sigma_weighted_recall.sh` — low-σ memory must outrank
   a high-σ memory with identical content under the default λ=1.

If `libsqlite3-dev` is absent the build falls back to
`COS_V115_NO_SQLITE`; the self-test still runs (embedder only) and the
merge-gate scripts **SKIP cleanly** rather than fail.

## Claim discipline

- The **storage + σ-weighted recall** contract is measured and tested.
- The **embedder quality** is NOT measured against MiniLM-L6-v2; the
  current hash-shingled embedder is a placeholder that gives shape-
  correct similarity for the API contract. Real semantic retrieval
  quality numbers arrive in v115.1 behind the same API.
- No cross-framework RAG benchmark is claimed. The claim is that
  **σ-weighted recall is a net-new retrieval signal** no other
  framework exposes.

## σ-stack layer

v115 sits at the **Training / Persistence** layer of the Creation OS
six-layer stack, feeding the v111.2 σ-Reason loop with retrieved
context and accepting write-back from successful low-σ emits.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_
