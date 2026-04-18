# v135 σ-Symbolic — Neurosymbolic reasoning

Source: `src/v135/symbolic.[ch]` · `src/v135/main.c`
Gate:   `benchmarks/v135/check_v135_symbolic_prolog_roundtrip.sh`
Build:  `make creation_os_v135_symbolic` · `make check-v135`

## Problem

LLMs reason *statistically*. Symbolic engines reason *logically*.
They're typically combined as pipelines — the symbolic system is
invoked on every query regardless of the LLM's confidence, which
(a) bottlenecks latency and (b) ignores that most answers don't
need verification.

## σ-innovation

σ is the **router** between the two lanes:

```
            σ < τ_symbolic            → EMIT     (fast BitNet path)
            σ ≥ τ_symbolic, logical   → VERIFY   (consult KB; may
                                                  CONFIRM or OVERRIDE)
            σ ≥ τ_symbolic, ¬logical  → ABSTAIN  (neither lane owns it)
```

v135 is the first σ-aware neurosymbolic layer: the symbolic engine
pays its cost only when the statistical leg isn't confident.

## Prolog micro-engine

~400 lines of pure C. Supports **ground-fact Horn bases** plus
**unification-based queries** with any number of variables
(including repeated variables that must unify with the same atom):

| Surface | Role |
|---------|------|
| `cos_v135_kb_assert("works_at(lauri, spektre_labs)")` | append a ground fact |
| `cos_v135_kb_load_file(path)` | batch-load from a Prolog-syntax file |
| `cos_v135_parse_query("?- works_at(X, spektre_labs).", &q)` | template with free variables |
| `cos_v135_solve(kb, &q, sols, cap)` | enumerate all satisfying bindings |
| `cos_v135_kb_mark_functional("works_at")` | tag predicate many-to-one on arg 0 |
| `cos_v135_check_triple(s, p, o)` | CONFIRM / CONTRADICT / UNKNOWN for a candidate claim |
| `cos_v135_route(σ, cfg)` | the σ-router above |
| `cos_v135_extract_triples(text, σ, τ_accept)` | σ-gated KG ingestion |

Rules (`head :- body1, body2, …`) are a **v135.1** follow-up and
are **refused** by the parser with a clear error message so callers
never silently degrade.

## Contract

| Claim | Check |
|-------|-------|
| Query `?- works_at(X, spektre_labs).` returns exactly one binding `X = lauri` on the canonical KB | `check-v135` |
| Functional `works_at`: claim "lauri works_at openai" → **CONTRADICT** | `check-v135` |
| Unrelated claim "alice likes pizza" → **UNKNOWN** | `check-v135` |
| σ-router: 0.10 → EMIT, 0.50 + logical → VERIFY, 0.50 + else → ABSTAIN | `check-v135` |
| σ-gated extraction: σ=0.10,τ=0.20 ingests; σ=0.90 rejects | `check-v135` |
| Rule input (`:-`) is refused | `check-v135` |

## v135.0 vs v135.1

| | Shipped (v135.0) | Follow-up (v135.1) |
|---|---|---|
| Ground-fact KB + queries | ✅ | — |
| Functional-predicate consistency | ✅ | — |
| σ-router | ✅ | — |
| σ-gated extraction stub | ✅ | Real NER via v115 memory |
| Horn rules (`:-` + body resolution) | — | Head-body SLD resolution |
| v106 `/v1/verify` endpoint | — | HTTP hook on BitNet answers |
| v115 memory → automatic KB ingest | — | Streaming extraction from v115 |

## Honest claims

v135.0 is **Horn-base-only**. The engine cannot evaluate rules or
do backward chaining. That's deliberate: the smallest useful
σ-symbolic surface is *consistency checking over a fact base*,
which is all the BitNet verification path actually needs at
inference time. Full SLD resolution changes the complexity
story and is scoped out of v135.0.
