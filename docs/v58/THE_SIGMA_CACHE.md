# The σ-Cache

**Creation OS v58** — one eviction policy that knows the difference
between uncertainty you can reduce and uncertainty you can't, and
acts on it branchlessly.

## What this is

One C11 compilation unit, one NEON kernel, one branchless decision
loop. AGPL-3.0. No framework dependency. Runs on any AArch64 host
with a C11 toolchain.

Every cached token gets a **four-valued** verdict:

- **KEEP_FULL** — sinks, and non-sinks whose σ-score clears both
  the budget threshold and the full-precision threshold.
- **KEEP_INT8** — clears budget, high enough to retain at 8-bit.
- **KEEP_INT4** — clears budget, but low enough to compress to
  4-bit.
- **EVICTED** — below budget, and not a sink.

No heuristic fraction, no random sampling, no per-layer magic.

## What this is not

- A new attention sink policy.
- A new attention-mass heavy-hitter policy.
- A new quantisation scheme.
- A new entropy eviction policy (EntropyCache already exists).

σ-Cache is the **first** policy to use the **epistemic / aleatoric
decomposition** of per-token uncertainty as the retention signal.

## Why it is different

Every other eviction policy says: *"keep the tokens with the
highest X,"* where X is one scalar — position, attention, entropy,
or a learned gate. σ-Cache says: *"keep the tokens whose
uncertainty is **reducible** (ε); discount the tokens whose
uncertainty is **irreducible** (α)."*

Two signals, decomposed, layered:

```
s(t) = α_ε · ε(t)  +  β · attn(t)  +  γ · recency(t)  -  δ · α(t)  +  sink_lift(t)
```

Then a **budgeted** τ_keep (K-th largest non-sink score) and **two
precision** thresholds τ_full, τ_int8 turn that into a four-valued
tag via **bitmask arithmetic** — no `if` on the hot path.

## Run it

```
$ make check-v58
./creation_os_v58 --self-test
v58 self-test: 68 pass, 0 fail
check-v58: OK (v58 σ-Cache eviction + branchless kernel self-test)

$ make microbench-v58
...
----- N=16384 iters=50 -----
  ms / iter (avg)       : 1.0354
  decisions / s (avg)   : 1.58e+07
  keep_threshold        : 1.062360
microbench-v58: OK (3-point sweep completed; deterministic)
```

## How it composes

v58 is a **v57 composition slot**. `make verify-agent` reports:

```
  [M] kv_cache_eviction                    (owner: v58          target: make check-v58     )  ... PASS
```

If v58 breaks, the aggregate report fails — it does not silently
downgrade.

## Read more

- `docs/v58/ARCHITECTURE.md` — wire map + tiering
- `docs/v58/POSITIONING.md` — vs StreamingLLM, H2O, SnapKV, KIVI, EntropyCache
- `docs/v58/paper_draft.md` — full write-up with microbench numbers
- `src/v58/sigma_cache.h` — API contract
- `src/v58/sigma_cache.c` — kernel (branchless, NEON, aligned)
- `src/v58/creation_os_v58.c` — 68-test deterministic driver
