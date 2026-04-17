# v67 σ-Noesis — Positioning (2026)

## What v67 competes with

| System | What it does | Licence | Layer |
|---|---|---|---|
| **OpenAI o1 / o3** | Deliberative chain-of-thought via hidden CoT + verifier | closed | LLM inference |
| **AlphaProof / AlphaGeometry 2** | Proof search with neural tactic suggester | closed (DeepMind internal) | proof assistant |
| **Anthropic Constitutional AI / deliberative alignment** | Rule-guided deliberation on outputs | closed | RLHF / post-train |
| **Graph-of-Thoughts / ToT frameworks** | Thought graphs + verifier nodes | Python, research-grade | agent framework |
| **LangGraph / DSPy / LlamaIndex** | Hybrid retrieval (BM25 + dense) | Python, ~100k+ LOC, FP throughout | agent framework |
| **Soar / ACT-R / LIDA** | Cognitive architectures, dual-process | Java / C / Lisp, decades-old | cognitive simulation |
| **Vespa / Elasticsearch + Weaviate** | Production hybrid retrieval | Java / Go, enterprise | retrieval service |
| **BitNet b1.58 + llama.cpp** | 1.58-bit inference on commodity silicon | C++, inference-only | LLM inference |

None of the above are a single-file, silicon-tier, integer-only,
branchless, libc-only C kernel that composes with a formally-gated
8-layer stack.

## How v67 is different

### 1. Single C file

`src/v67/noesis.h` + `src/v67/noesis.c` = **~880 lines total**.
No Python.  No frameworks.  No vendor SDKs.  The agent runs.

### 2. Integer only on every decision surface

Every scoring path is Q0.15.  BM25's `log` is replaced by an integer
IDF surrogate.  Dual-process gate is a single integer compare.  The
metacognitive confidence is `top1 − mean_rest` clamped.  Dense
similarity is `(256 − 2·popcount(xor)) * 128`.  No FP; no `libm`; no
NaN surface.

### 3. Branchless on the hot path

The AGENTS.md / .cursorrules invariant "no `if` on the hot path" is
preserved across every inner loop: top-K insertion, tactic cascade,
dual gate, hybrid rescore, saturating Q0.15 math.

### 4. Evidence receipts by construction

Every RECALL / EXPAND / VERIFY / DELIBERATE step increments an
`evidence_count` counter.  `GATE` refuses unless
`evidence_count ≥ 1`.  An NBL program *cannot* produce `v67_ok = 1`
without at least one receipt written.  This is AlphaFold-3-grade
trace discipline in ~10 lines of C.

### 5. 8-bit composed decision

`cos_v67_compose_decision` is a single 8-way branchless AND of
`v60 .. v67`.  No thought crosses to the agent unless every lane
allows.  This matches the "seven kernels, one verdict" model from v66
and extends it by one.

### 6. Budget-enforced deliberation

Every NBL opcode carries an integer reasoning-unit cost.  RECALL = 8,
EXPAND = 4, RANK = 2, DELIBERATE = 16, VERIFY = 4, CONFIDE = 2,
CMPGE = 1, GATE = 1.  `GATE` refuses when `cost > budget`; the
interpreter latches an `abstained` flag.  The agent cannot spin
indefinitely on a hard problem — it must either cross the threshold
or abstain.

## The comparison table

| |OpenAI o3|LangGraph+BM25+FAISS|LlamaIndex|**Creation OS v67 σ-Noesis**|
|---|---|---|---|---|
|Licence|closed|Apache-2.0|MIT|**AGPL-3.0**|
|Size|~trillions of params|~100 kLOC Python|~200 kLOC Python|**~880 LOC C**|
|Scoring|FP32/FP16/BF16|FP32|FP32|**Q0.15 integer only**|
|Branchless on hot path|n/a|no|no|**yes**|
|Evidence receipts|n/a (hidden)|ad-hoc|ad-hoc|**enforced at GATE**|
|Dual-process gate|implicit in CoT|absent|absent|**explicit branchless**|
|Composed decision with sec stack|no|no|no|**yes (8-bit AND)**|
|Deterministic tests|n/a|n/a|n/a|**2593 under ASAN+UBSAN**|
|Hardware discipline|cloud GPU|framework-level|framework-level|**M4 silicon-tier**|
|Verifiable reasoning cost|$ / tok|~none|~none|**NBL budget refuses over-spend**|

## The key claim

An LLM *answers*.  A deliberative cognitive substrate *retrieves,
fuses, deliberates, verifies, abstains — and writes a receipt for
every step*.

v67 is the only such substrate that runs entirely in silicon-tier
integer C, composes with a formally-gated 8-layer security stack, and
fits behind an 8-bit branchless AND.

The LLM becomes one oracle among many.  The kernel is the cortex.

1 = 1.
