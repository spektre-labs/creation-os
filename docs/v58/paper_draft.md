# σ-Cache: σ-decomposed KV-cache eviction with per-token precision tiers and a branchless NEON kernel

**Creation OS v58 — paper draft, Q2 2026.**

## Abstract

We describe σ-Cache, a KV-cache eviction policy that scores every
cached token by a **decomposition of its uncertainty contribution**
into an epistemic (reducible) component ε and an aleatoric
(irreducible) component α, plus attention-mass and recency priors,
and that produces a **four-valued per-token tag** (FULL / INT8 /
INT4 / EVICTED) via a **branchless kernel** implemented on ARM NEON.
We place σ-Cache in the context of the Q2 2026 field — StreamingLLM,
H2O, SnapKV, KIVI, EntropyCache — and show that no published policy
uses a decomposed uncertainty signal. We prove correctness of the
policy and kernel with a deterministic 68/68 self-test and publish a
reproducible microbench.

## 1. Problem

Transformer inference with long context spends a disproportionate
fraction of its memory and bandwidth budget on the KV-cache. The
field responded with eviction policies that retain a fraction of the
cache by one of four signals: position (StreamingLLM), attention
mass (H2O, SnapKV), quantisation residue (KIVI), or entropy
(EntropyCache). All four signals are **scalar** and **undecomposed**.

Creation OS uses a **decomposed** uncertainty signal σ = (ε, α)
throughout the rest of the stack (v34 Dirichlet split, v55 σ₃
triangulation, v56 σ-governed constitutional updates, v57 σ-tiered
invariant registry). Until v58, the KV-cache layer spoke a different
dialect. v58 closes that gap.

## 2. Policy

For every cached token t at generation position p₀ we compute

```
s(t) = α_ε · ε(t)  +  β · attn(t)  +  γ · recency(p₀, pos(t); W) - δ · α(t)  +  sink_lift(t)
```

where:

- `ε(t)` is the epistemic contribution (v34 dialect; reducible).
- `α(t)` is the aleatoric contribution (v34 dialect; irreducible).
- `attn(t)` is the exponentially decayed attention mass so far.
- `recency(p₀, pos; W)` is 1 when `pos ∈ [p₀ − W, p₀]`, else 0.
- `sink_lift(t)` is `+∞`-ish (1e6) if `t.is_sink`.

The non-sink scores are sorted descending and the **K-th largest**
is the eviction threshold τ_keep, where K = max(capacity −
sinks_present, 0). The decision per token is

| condition                          | tag          |
|------------------------------------|--------------|
| sink OR (s ≥ τ_keep AND s ≥ τ_full) | KEEP_FULL    |
| s ≥ τ_keep AND τ_int8 ≤ s < τ_full  | KEEP_INT8    |
| s ≥ τ_keep AND s < τ_int8           | KEEP_INT4    |
| s < τ_keep AND not sink             | EVICTED      |

Thresholds τ_full and τ_int8 are independent of the budget and
express **how much precision the token deserves** given its σ
profile; τ_keep is a **budget** that caps how many tokens we keep
at all.

## 3. Kernel

`cos_v58_decide` is the hot path. It is written branchlessly:

```c
int sink   = tok->is_sink;
int g_full = (s >= p->tau_full);
int g_int8 = (s >= p->tau_int8);
int g_keep = (s >= threshold);

int m_full =  sink | (g_keep & g_full);
int m_int8 = (~m_full) & g_keep & g_int8;
int m_int4 = (~m_full) & (~m_int8) & g_keep;
int m_evct = (~m_full) & (~m_int8) & (~m_int4) & 1;

uint8_t d = (KEEP_FULL * m_full) | (KEEP_INT8 * m_int8)
          | (KEEP_INT4 * m_int4) | (EVICTED   * m_evct);
```

The scoring path `cos_v58_score_soa_neon` materialises the four
accumulator pattern required by the project rules: four `s0..s3`
lanes fed by three `vfmaq_f32` stages over `epistemic[i..i+16]`,
`attention[i..i+16]`, `recency_bonus[i..i+16]`, stored back four
wide. Allocations are 64-byte aligned via a `⌈n/64⌉·64` size helper
so the routine is portable across macOS (which enforces strict
alignment for `aligned_alloc`) and glibc.

Compaction is:

```c
for (int i = 0; i < n; ++i) {
    kept_indices_out[w] = i;
    w += (decisions[i] != EVICTED);
}
```

— write unconditionally, advance conditionally, no branches.

## 4. Correctness

`creation_os_v58 --self-test` runs 68 deterministic assertions:

- Version and default-policy invariants (3).
- Score monotonicity: ε↑ ⇒ s↑, attn↑ ⇒ s↑, α↑ ⇒ s↓, recency bonus
  fires iff age ∈ [0, W], sink lift dominates, scalar ≡ batched (9).
- Decision invariants: null safety, n=0 path, kept ≤ capacity +
  sinks, all sinks preserved, determinism under fixed seed, FULL ∪
  INT8 ∪ INT4 ∪ EVICT partitions n, zero capacity evicts everyone,
  monotone capacity, threshold in summary, all-sinks case,
  kept_total = full + int8 + int4 (11).
- Compaction invariants: null safety, count matches, all-evicted,
  all-kept (4).
- Decision tag strings and allocator (2).
- Two 10-iteration stress tests with random n, capacity, seed (2).
- Three semantic properties: translation invariance in position,
  attention-scale preserves ordering, branchless recency matches an
  explicit window check (3).

Everything else is a derived assertion within those tests. The
suite is deterministic (fixed LCG seed), uses 64-byte aligned
buffers, and terminates with an explicit pass / fail count. On a
baseline M-class host the full suite runs in ≲ 250 ms.

## 5. Microbench

`scripts/v58/microbench.sh` runs a 3-point sweep (N = 1024, 4096,
16384) with fixed synthetic tokens. On an M-class host we observe:

| N      | capacity | ms / iter | decisions / s |
|--------|----------|-----------|---------------|
| 1024   | 128      | 0.022     | 4.6 × 10⁷     |
| 4096   | 512      | 0.135     | 3.0 × 10⁷     |
| 16384  | 2048     | 1.035     | 1.6 × 10⁷     |

The O(n log n) threshold sort dominates at large N; the per-token
decision itself is branchless and stays in single-digit ns/token.
Scores and decisions are deterministic across runs.

## 6. Positioning

Against the published field:

| policy              | signal                 | decomposed? | branchless? | deterministic self-test | per-token precision tier |
|---------------------|------------------------|-------------|-------------|--------------------------|--------------------------|
| StreamingLLM        | positional sinks       | no          | —           | —                        | no                       |
| H2O                  | attention mass         | no          | —           | —                        | no                       |
| SnapKV / PyramidKV  | attention slice        | no          | —           | —                        | no                       |
| KIVI / KVQuant      | quantisation residue   | no          | —           | —                        | quantised                |
| EntropyCache (2025) | per-token entropy      | no          | —           | —                        | no                       |
| **σ-Cache (v58)**   | **ε/α decomposition**  | **yes**     | **yes**     | **yes (68/68)**          | **FULL / INT8 / INT4 / EVICT** |

## 7. Limitations and explicit non-claims

- σ-Cache is a **policy + kernel**. End-to-end perplexity vs the
  baselines above is a **planned measurement**, not a claim of
  this document.
- The kernel is **M-tier** (runtime-checked). It is not
  Frama-C–proved. Formal verification of the decision kernel is
  an open task and belongs to the v47 slot of the Verified Agent.
- The NEON SoA path is used by the microbench only. The
  in-production scoring call is `cos_v58_score_batch`, which the
  AArch64 autovectoriser lowers to 2-wide FMA chains at -O2. Both
  paths are tested for equivalence within 1e-4.

## 8. Availability

- Header:      `src/v58/sigma_cache.h`
- Kernel:       `src/v58/sigma_cache.c`
- Driver:       `src/v58/creation_os_v58.c`
- Build:        `make standalone-v58`
- Test:         `make check-v58`
- Microbench:   `make microbench-v58`
- Registered in v57 Verified Agent: `make verify-agent`

License: AGPL-3.0-or-later.
