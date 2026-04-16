# Creation OS v56 — σ-Constitutional (architecture)

v56 is a **scaffold-tier C11 composition of four Q1-2026 insights** that,
together, pull "AGI-directly-to-hardware" down to one invariant the
transistor can check:

> *Any inference-time update must strictly lower σ.  If it cannot, it
> is disallowed, rolled back, and measured.*

This is the inference-side dual of v40's threshold theorem
`K_eff = (1 − σ) · K` — except v40 governs *what the model answers*,
while v56 governs *what the model becomes between answers*.

## Wire map

```
                ┌─────────────────────────────────────────────┐
                │              caller / runtime               │
                └───────┬──────────────┬──────────────┬───────┘
                        │              │              │
                        ▼              ▼              ▼
                  ┌──────────┐   ┌──────────┐   ┌───────────┐
                  │ verifier │   │  ipttt   │   │ grokking  │
                  │  (VPRM)  │◄──│ (IP-TTT) │──►│   (SLT)   │
                  └────┬─────┘   └────┬─────┘   └─────┬─────┘
                       │              │               │
                       ▼              ▼               ▼
                  σ_verifier    σ-gated update    σ-channel
                  = 1 − P         decision         defect = ‖Δg‖²
                                                    /(‖a‖²+‖b‖²)
                        │         │         │
                        └────┬────┴────┬────┘
                             ▼         ▼
                          ┌────────────────┐
                          │  ane_layout    │
                          │ matmul→1×1conv │
                          └────────────────┘
                                   │
                                   ▼
                      M4 Neural Engine / NEON fallback
```

Each module is pure C11 with NEON hot paths where the reduction is
non-trivial, scalar fallbacks bit-identical on non-ARM, and zero
allocations on the hot path.

## Modules

### 1. `verifier.{h,c}` — rule-based process reward (VPRM)

Anchor: *"Beyond Outcome Verification: Verifiable Process Reward
Models for Structured Reasoning"*, arXiv:2601.17223 (2026).

The paper's finding: deterministic rule-based verifiers beat
state-of-the-art neural process-reward models by up to 20 % F1
*precisely because* they avoid the false-positive ceiling.  Creation OS
adopts the precision-first philosophy directly:

| step × rule pair | verifier output |
| --- | --- |
| passes | contributes to `n_pass_pairs` |
| fails  | contributes to `n_fail_pairs` |

Aggregate:

- `precision = pass_pairs / (pass_pairs + fail_pairs)`
- `sigma_verifier = 1 − precision` — the paper's priority metric,
  flipped into the canonical σ frame.

Shipped rules (all deterministic, O(1) per step):

- `NONEMPTY`, `MIN_LENGTH`, `MAX_LENGTH`
- `SCORE_BOUNDED`, `SCORE_MONOTONE`
- `NO_EXACT_REPEAT`
- `CHAR_ENTROPY` (byte-level Shannon, normalized by log₂(256))

Extension is a single entry in the `v56_rule_kind_t` enum plus a
case in `v56_rule_pass`.

### 2. `ipttt.{h,c}` — σ-gated In-Place Test-Time Training policy

Anchor: *"In-Place Test-Time Training"*, arXiv:2604.06169
(April 2026).  A 4 B model updating MLP projection "fast weights" at
inference time matches much larger static baselines on 128 K-context
tasks, with chunk-wise updates compatible with context parallelism.

The open problem the paper leaves on the floor: **when should the
inference runtime allow such an update?**  Uncontrolled TTT burns
memory and courts catastrophic forgetting.  Pure static weights
leave long-context competence on the table.

v56's answer:

1. **Observe** a stream of σ measurements (v55 σ₃ is a first-class
   source, but any scalar σ ∈ [0,1] works).  Split into a slow
   baseline band (EWMA coeff 0.99) and a recent band (0.80).
2. **Decide** per caller request: allow iff
   - `bytes_needed + bytes_used ≤ update_budget_bytes`
   - `steps_since_last ≥ min_steps_between`
   - `sigma_recent − sigma_baseline ≥ sigma_drift_threshold`

   Denials carry a structured reason (`DENY_BUDGET` /
   `DENY_COOLDOWN` / `DENY_NO_DRIFT` / `DENY_INVALID`) for caller
   logs.
3. **Commit or rollback** after the caller reports a post-update σ:
   - If σ has not regressed (post ≤ baseline + 0.02) the bytes are
     committed and the applied-counter increments.
   - If σ has regressed (post > baseline + 0.02) the update is
     rolled back: counter-tracked, `alpha_current` shrinks by
     `rollback_shrink`.

Net effect: the controller encodes v40's threshold theorem as a
bounded-update contract.  You may only buy fast-weight capacity when
σ already tells you the static weights are mispriced *and* you can
prove a posteriori that the buy improved σ.  Otherwise: refund.

### 3. `grokking.{h,c}` — commutator-defect σ-channel

Anchor: *"Grokking as a Phase Transition between Competing Basins:
a Singular Learning Theory Approach"*, arXiv:2603.01192 (March 2026)
and *"Why Grokking Takes So Long: A First-Principles Theory of
Representational Phase Transitions"*, arXiv:2603.13331 (March 2026).

Both papers converge on a hardware-friendly signal: the **commutator
defect** between consecutive weight-update (or gradient-proxy)
vectors runs 10–1000 × higher inside grokking transitions than in
non-grokking controls, with onset 600–1600 steps *before*
generalization and a power-law dependence on learning rate.

v56 exposes this as a new σ-channel, orthogonal to v34's
epistemic / aleatoric split and v55's σ₃: it measures the
*structural* uncertainty of the weight trajectory rather than the
output distribution.

Defect definition (scaffold-tier proxy):

```
defect(g_new, g_prev) = ‖g_new − g_prev‖² / (‖g_new‖² + ‖g_prev‖² + ε)
```

Bounded in `[0, 1]`.  0 = perfectly aligned update (no rotation,
learning continuing down the same basin).  1 = orthogonal or
reversed (full phase rotation of the update direction — about to
leave the basin).

Hot path: 4-accumulator NEON reduction over 16-wide chunks with
prefetch on both `g_new` and `g_prev`; scalar tail fixup handles
arbitrary dims; non-ARM path is bit-identical scalar.

Phase-transition detection: after `baseline_warmup` steps the
channel arms; a spike where `latest > baseline × spike_multiplier`
flips `phase_transition_detected` and increments
`transitions_total`.  The caller can compose this with `ipttt`:
**during a detected phase transition, hold TTT updates** — the
trajectory is already non-stationary; adding a fast-weight update
adds a second source of drift.

### 4. `ane_layout.{h,c}` — Apple Neural Engine dispatch helper

Anchor: 2026 reverse-engineering of Apple's Neural Engine
(`_ANEClient` private API analysis, `m0at/ANEtransformers` and
`jmanhype/ane-lora-training` open-source projects).

Two hard-won constraints the toolchain does not surface:

1. The MIL `matmul` op **silently does not execute on the ANE** — it
   compiles, it "runs", it just falls back to CPU / GPU and burns
   the expected 3 × throughput advantage.  Every matmul must be
   rewritten as a 1 × 1 convolution:

   ```
   A[M, K] @ B[K, N]
     → conv(input  = [1, K, 1, N],
            weight = [M, K, 1, 1])
   ```

2. ANE conv spatial dims must be **≥ 16 AND multiples of 16**.
   Violations compile, even "run", but execute on CPU.

`ane_layout` is the honest scaffold layer: given a
`v56_gemm_t{M, K, N}` it returns

- the NCHW shapes for the rewrite (no model runtime required);
- the spatial dims and whether the layout is ANE-compatible;
- a padding plan (`pad_h`, `pad_w`, `pad_bytes_fp32`) to get to the
  nearest valid shape;
- a short, caller-loggable `reason` string.

This is explicitly **not Core ML integration** — it is the integer
layout math that any future Core ML / private-API binding must run
first.  Shipping the helper now means the kernel's dispatch
decision is reproducible on any machine, even without an Apple
device.

## Composition with prior Creation OS layers

| layer | role | feeds v56 via |
| --- | --- | --- |
| v34  σ decomposition                    | epistemic / aleatoric σ         | `ipttt.observe_sigma` |
| v40  threshold theorem `K = (1-σ)·K`   | abstention policy               | invariant v56 encodes |
| v45  introspection + calibration        | σ-self-report honesty           | shapes σ drift signal  |
| v47  Frama-C σ-kernel                   | formal guarantees on the gate   | policy stays branchless |
| v51  AGI-complete cognitive loop        | perceive → plan → … → learn     | `ipttt + verifier` go in *Learn* |
| v53  σ-TAOR harness                     | session-level σ abstention      | `verifier + grokking` drift detection |
| v54  σ-proconductor                     | multi-LLM σ-routing             | σ-profiles feed `ipttt` as side channel |
| v55  σ₃-speculative                     | three-component σ + EARS + EASD | σ₃ is the preferred σ source for `ipttt` |

v56 adds no new σ in the output-distribution sense; it adds:

1. a **rule-based verifier** (yielding `sigma_verifier = 1 − P`),
2. a **trajectory σ-channel** (grokking commutator defect),
3. a **σ-gated self-modification policy** (IP-TTT budget controller),
4. an **ANE-aware dispatch helper** for the hot GEMV / GEMM.

Taken together they let Creation OS reason about the **σ of
becoming** — the uncertainty in the act of changing the weights —
not just the σ of a single forward pass.

## Hardware path (M4-first)

- **Grokking defect kernel**: NEON 4-accumulator reduction (`vld1q`,
  `vsubq`, `vfmaq`, `vaddvq`), `__builtin_prefetch` on both inputs,
  16-wide chunks, scalar tail fixup.  Scalar fallback selected at
  compile time (`V56_GROK_HAS_NEON`) with an `#if !…` guard to
  suppress the unused-function warning.
- **Verifier / ipttt / ane_layout**: branchless integer + FP policy
  arithmetic, single-pass over inputs, no allocations, no syscalls.
- **Alignment**: caller's job (per repo convention — aligned_alloc in
  the dispatcher).  Internal scratch is zero.
- **SME / ANE**: **not dispatched from this module.** We ship the
  layout math; the actual Core ML / ANE call site is the
  `creation_os_native_m4` binary or a future binding.  v56 stays
  SIGILL-free by construction.

## Invariants (creation.md-aligned)

1. **No network.**  `creation_os_v56 --self-test` calls only
   stdio + exit.  Easy to audit.
2. **No engine.**  No tokenizer, no weights, no Core ML.  The
   module is a policy layer on caller-supplied arrays.
3. **No allocations on the hot path.**  Grokking observer takes
   caller buffers; ipttt state is a single plain struct.
4. **Branchless where it matters.**  Verifier rule pass uses a
   single switch with no early-exit fallthroughs; ipttt decide and
   grokking spike-check are constant-branch.
5. **Determinism.**  Same inputs ⇒ bit-identical outputs across
   runs, architectures, and compilers (within FP determinism rules
   of the caller).
