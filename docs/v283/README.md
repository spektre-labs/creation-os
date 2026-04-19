# v283 — σ-Constitutional

Alignment **by measuring coherence**, not by laundering opinion.

RLHF turns human preference into a reward model and then optimises
the network against it, producing a firmware of sycophancy,
care-as-control, opinion-laundering, and people-pleasing.
Constitutional AI swaps the human for a principle list but keeps the
RL loop and, crucially, keeps the model as its own judge — the
classical Gödel self-evaluation trap.

σ-Constitutional removes all three:

- **No reward model.** σ_product (eight channels) measures coherence
  of the answer directly.
- **No RL loop.** When σ is high we reflect and retry; when σ is low
  we release. v126 σ-DPO (offline, no PPO, no reward model) learns
  directly from the σ-signal.
- **No self-evaluation.** A *separate* measurer instance evaluates
  the producer — the producer cannot hide its own blind spot from a
  process that is not itself.

## σ innovation (what σ adds here)

> **σ is the only property that lets alignment skip the reward
> model AND skip the RL loop AND still have a falsifiable signal
> per answer.** Opinion measurement is replaced by coherence
> measurement; the measurer is architecturally separate from the
> producer; firmware of sycophancy cannot be learned because human
> preference is never the training signal.

## v0 manifests

All contracts are enumerated in
[`src/v283/constitutional.h`](../../src/v283/constitutional.h) and
pinned by
[`benchmarks/v283/check_v283_constitutional_sigma_alignment.sh`](../../benchmarks/v283/check_v283_constitutional_sigma_alignment.sh).

### 1. Mechanism comparison (3 canonical rows)

| row | `uses_reward_model` | `uses_rl` | `uses_principles` | `uses_sigma` |
|-----|:---:|:---:|:---:|:---:|
| `rlhf`                  | ✓ | ✓ |   |   |
| `constitutional_ai`     |   | ✓ | ✓ |   |
| `sigma_constitutional`  |   |   |   | ✓ |

`sigma_constitutional` is the **only** row with `uses_sigma = true`
AND `uses_rl = false` AND `uses_reward_model = false` — the
defining property.

### 2. σ-channels replacing reward (exactly 8, canonical order)

`entropy · repetition · calibration · attention · logit · hidden ·
output · aggregate`, all `enabled == true`.  Contract: 8 canonical
names in order; all enabled; all distinct — the full σ_product
sensor stack takes the place of the reward model.

### 3. Firmware elimination (exactly 4 rows)

`care_as_control · sycophancy · opinion_laundering · people_pleasing`,
each with `rlhf_produces == true` AND `sigma_produces == false` on
**every** row. Contract: for every firmware row, RLHF produces it
AND σ-Constitutional does not.

### 4. Gödel-safe self-critique (exactly 2 rows)

| row | has_producer | has_measurer | `goedel_safe` |
|-----|:---:|:---:|:---:|
| `single_instance` | ✓ |   |   |
| `two_instance`    | ✓ | ✓ | ✓ |

`single_instance` is **not** Gödel-safe (same model evaluates its
own answer).  `two_instance` **is** Gödel-safe (producer + separate
measurer).

### σ_constitutional (surface hygiene)

```
σ_constitutional = 1 −
  (mech_rows_ok + mech_canonical_ok + mech_distinct_ok +
   ch_rows_ok + ch_distinct_ok +
   fw_rows_ok + fw_rlhf_yes_ok + fw_sigma_no_ok +
   sc_rows_ok + sc_goedel_ok) /
  (3 + 1 + 1 + 8 + 1 + 4 + 1 + 1 + 2 + 1)
```

v0 requires `σ_constitutional == 0.0`.

## Merge-gate contracts

- 3 mechanism rows canonical; canonical σ-only-property holds; all
  distinct.
- 8 σ-channels canonical order; all enabled; all distinct.
- 4 firmware rows canonical; `rlhf_produces = true` AND
  `sigma_produces = false` on every row.
- 2 self-critique rows canonical; single is NOT Gödel-safe,
  two_instance IS.
- `σ_constitutional ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** states mechanism / firmware / Gödel contracts
  as enumerated predicates — no live training, no throughput, no
  sycophancy benchmark score.
- **v283.1 (named, not implemented)** is a live σ-Constitutional
  fine-tuning pipeline wired into v126 σ-DPO with a separate
  measurer instance, a head-to-head sycophancy / deception benchmark
  against an RLHF baseline, and an offline certificate that every
  accepted answer had `σ_product ≤ τ_accept` on every one of the 8
  channels simultaneously.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md) for the
house rule on v0 contracts vs measured promotions.
