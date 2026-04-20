<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
Copyright (c) 2026 Spektre Labs / Lauri Rainio
-->
# v259 — `sigma_measurement_t` canonical primitive

_The 12-byte struct every later kernel already referenced by string
(see v290 dougong coupling rows) made concrete, size-asserted,
round-trip-tested, and benchmarked with a nanosecond-precision
clock on the host._

## Why this exists

Before v259, `sigma_measurement_t` was a **string** in the v290
coupling manifest and an **implicit convention** scattered across
the σ-gated kernels.  v259 turns that convention into:

- a public C header (`src/v259/sigma_measurement.h`) that the whole
  stack can include,
- a 12-byte struct with asserted layout and alignment,
- a deterministic, pure gate predicate with a 3-regime total order,
- a microbench that reports ns/call per platform.

No new architectural layer is added; this fills the `v258 → v260`
gap with the primitive that was always assumed.

## v0 manifest contracts

| # | surface | contract |
|---|---------|----------|
| 1 | layout     | `sizeof == 12`, `alignof == 4`, fields `{header, sigma, tau}` at offsets `{0, 4, 8}` size `{4, 4, 4}` |
| 2 | roundtrip  | 4 canonical `(σ, τ)` pairs roundtrip bit-identically via `memcpy` AND produce the expected gate verdict |
| 3 | gate       | 3 regimes — `signal_dominant < critical < noise_dominant` (total order on verdict) AND 256 identical calls produce identical outputs (purity) |
| 4 | microbench | 3 workloads (`encode_decode`, `gate_allow`, `gate_abstain`), each `≥ 1e6 iters`, `mean_ns < 1 ms`, `min_ns > 0` — distributional shape (min/median/mean/max) is emitted to JSON |
| 5 | σ-hygiene  | `σ_measurement == 0.0` over a 21-cell denominator AND FNV-1a chain replays byte-identically |

## Layout

```c
typedef struct {
    uint32_t header;   // version:8 | flags:8 | gate:8 | reserved:8
    float    sigma;    // σ ∈ [0, 1]
    float    tau;      // abstain threshold
} cos_sigma_measurement_t;  // 12 bytes, alignof 4
```

`header` is deliberately opaque: bits 0-7 carry a protocol version so
that readers from v1 and v1000 can coexist (the old-reader-ignores-new-
fields contract used in v297 clock); bits 8-15 carry gate flags; the
rest is reserved.

## Gate predicate

```c
static inline cos_sigma_gate_t
cos_sigma_measurement_gate(const cos_sigma_measurement_t *m) {
    if (m->sigma <  m->tau) return COS_SIGMA_GATE_ALLOW;       // 0
    if (m->sigma == m->tau) return COS_SIGMA_GATE_BOUNDARY;    // 1
    return COS_SIGMA_GATE_ABSTAIN;                             // 2
}
```

Total order: `ALLOW < BOUNDARY < ABSTAIN`.  `critical` (σ == τ
exactly) is classified as BOUNDARY, not ALLOW, so that the
half-operator fence in v306 Ω is honoured at this primitive's level
too.

## Measured (from `make check-v259` on the merge-gate host)

Nanosecond figures vary by machine; they are reported in JSON, not
asserted in the v0 contract.  The numbers below are representative
of an Apple M3 with `-O2 -march=native`:

| workload       | iters     | mean ns | median ns | min ns | max ns |
|----------------|----------:|--------:|----------:|-------:|-------:|
| encode_decode  | 1 000 000 | ≈ 0.5   | ≈ 0.5     | ≈ 0.5  | ≈ 0.6  |
| gate_allow     | 1 000 000 | ≈ 0.6   | ≈ 0.6     | ≈ 0.5  | ≈ 0.6  |
| gate_abstain   | 1 000 000 | ≈ 0.6   | ≈ 0.6     | ≈ 0.5  | ≈ 0.6  |

The encode/decode path is a pure `memcpy` on 12 bytes; the gate path
is a single IEEE-754 float comparison.  Both are effectively
inlined to a couple of instructions on this target, which the
sub-nanosecond result confirms.

## Reproduce

```bash
make check-v259
./creation_os_v259_sigma_measurement                  # JSON dump
./creation_os_v259_sigma_measurement --self-test      # returns 0 on success
```

## v259.1 — evidence ledger (honest, per-theorem status)

The proof obligations for the v259 primitive are written down in a
machine-checkable form across three layers of evidence.  Each layer
is strictly weaker than the next; every theorem below lists which
layers have actually discharged it.

| # | theorem                          | sampling (runtime) | Lean 4 | Frama-C Wp |
|---|----------------------------------|:---:|:---:|:---:|
| 1 | Layout (`sizeof == 12`, offsets) | **PASS** (compile-time `_Static_assert` in `src/v259/sigma_measurement.c`) | PENDING | PENDING |
| 2 | Gate purity (`assigns \nothing`)   | **PASS** (256 identical inputs → identical outputs, `gate_pure_ok`) | DISCHARGED (`rfl`) | PENDING |
| 3 | Gate totality (3 verdicts only)   | **PASS** (canonical 3-regime table + bench) | PENDING (`sorry`) | PENDING |
| 4 | Gate boundary tiebreak (σ==τ → BOUNDARY) | **PASS** (canonical roundtrip row 3) | PENDING (`sorry`) | PENDING |
| 5 | Gate τ anti-monotone               | — | PENDING (`sorry`) | PENDING |
| 6 | **Roundtrip byte-identical (v259.1-roundtrip):** `memcmp(in, roundtrip(in), 12) == 0` for every bit pattern | **PASS** (`cos_v259_roundtrip_exhaustive_check`: 4 canonical pairs + 121 IEEE-754 special cross products [NaN×NaN, NaN×Inf, etc.] + 10⁶ LCG grid of fully random 12-byte surfaces) | DISCHARGED on the abstract byte-vec model — `roundtrip_bytes_identity` (`decode ∘ encode = id`, `rfl`) and `encode_injective` (case-analysis, no `sorry`); the `__builtin_memcpy` bit-level correctness remains with Frama-C | PENDING |
| 7 | **Clamp range (v259.1-range):** `clamp(x) ∈ [0, 1]` for every IEEE-754 float `x` | **PASS** (`cos_v259_clamp_exhaustive_check`: 14 specials + 10⁶ LCG grid + 5 canonical σ; coverage of float domain ≈ 0.024 %) | DISCHARGED on the `LinearOrder α` abstraction (`clampUnit_range`); NaN-handled IEEE-754 lift still PENDING | PENDING |
| 8 | Bench budget (`mean_ns < 1 ms`, `iters ≥ 10⁶`) | **PASS** (`make check-v259` on M3: ≈ 0.6 ns/call) | out of scope | out of scope |

Layer semantics:

- **Sampling** — deterministic runtime checks that the primitive
  satisfies the contract on a finite set of inputs.  Enforced on
  every `make check-v259`; a regression fails merge-gate immediately.
- **Lean 4** — mechanical proofs in
  [`hw/formal/v259/Measurement.lean`](../../hw/formal/v259/Measurement.lean).
  Discharged without `sorry`: `Gate.rank_injective`, `gate_purity`,
  `clampUnit_range` (over `LinearOrder α`), `roundtrip_bytes_identity`
  (`decode ∘ encode = id` on the abstract byte-vec model), and
  `encode_injective`.  Still `sorry`-ending: `gate_totality`,
  `gate_monotone_in_sigma`, `gate_anti_monotone_in_tau`,
  `gate_boundary_tiebreak` — all four gate-order theorems.  No Lean 4
  toolchain is wired into CI yet; a future `make formal-v259` will
  fail hard on any unresolved goal.
- **Frama-C Wp** — ACSL annotations in
  [`hw/formal/v259/sigma_measurement.h.acsl`](../../hw/formal/v259/sigma_measurement.h.acsl).
  Every contract is tagged `PROOF: PENDING` in the preceding comment.
  Frama-C Wp has not been run on this file yet.

CLAIM_DISCIPLINE: the evidence table above is the only place the
status of each theorem is stated.  README and papers must cite this
table directly; no "formally verified" claim may be made without
first updating the table.  The three-layer structure is deliberate:
a failing upstream layer (sampling) signals a regression immediately,
even when the downstream layers have not been run.

## What v259 does NOT claim

- It does **not** claim a perf-superlative.  The ns/call numbers are
  published but not used as a threshold against any external
  baseline.
- It does **not** replace v290's coupling manifest.  It provides the
  struct the manifest already names; the manifest's own invariants
  (direct_call = false, channel = "sigma_measurement_t") are still
  enforced there.
- It does **not** imply formal verification.  Frama-C + Lean are
  v259.1 and above; those files are absent until added.

`assert(declared == realized);  // 12 bytes. 1 = 1.`
