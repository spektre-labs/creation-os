# v148 — σ-Sovereign · Full Autonomy Loop

**Kernel:** [`src/v148/sovereign.h`](../../src/v148/sovereign.h), [`src/v148/sovereign.c`](../../src/v148/sovereign.c) · **CLI:** [`src/v148/main.c`](../../src/v148/main.c) · **Gate:** [`benchmarks/v148/check_v148_sovereign_supervised_smoke.sh`](../../benchmarks/v148/check_v148_sovereign_supervised_smoke.sh) · **Make:** `check-v148`

## Problem

Creation OS already had every piece of a self-improving AGI
separately:

| Function | Kernel |
|---|---|
| Measure itself | v133 meta + v143 benchmark |
| Identify its weaknesses | v141 curriculum |
| Generate training data | v120 distill |
| Train itself | v124 continual + v125 σ-DPO |
| Generate new kernels | **v146 genesis** |
| Reflect on its reasoning | **v147 reflect** |
| Share what it has learned | v128 mesh + v129 federated |
| Roll back on error | v124 rollback + **v144 RSI** |
| Report to the user | v108 UI + v133 dashboard |

v148 is the **orchestrator** that runs the whole cycle.

## σ-innovation

One cycle, six ordered stages:

```
1. MEASURE   → synthesise a score, submit to v144
2. IDENTIFY  → rotate the weakness roster (v141-style)
3. IMPROVE   → v145 acquire on the current weakness topic
4. EVOLVE    → v146 genesis proposes a new kernel (every 2nd cycle)
5. REFLECT   → v147 on a canonical 4-step reasoning trace
6. SHARE     → v145 share-sweep at τ_share
```

Two σ-gates pace the loop:

| Gate | Condition | Consequence |
|---|---|---|
| **G1 Stability** | σ_rsi(v144) > τ_sovereign | `unstable_halt` — cycle counted, downstream stages skipped; user's "system unstable, manual review" clause |
| **G2 Supervision** | `cfg.mode == SUPERVISED` | Every structural change accumulates in `pending_approvals`; user must call `cos_v148_approve_all()` or the per-kernel approve routines |

The **emergency stop** (`cos stop-sovereign` →
`cos_v148_emergency_stop()`) is a separate hot latch —
deliberately *not* cleared by `cos_v144_resume()`, so a
paused-for-σ RSI loop cannot accidentally restart an
emergency-stopped sovereign loop. Only
`cos_v148_resume_from_emergency()` unblocks it.

## Merge-gate

`make check-v148` runs five assertions:

1. Self-test covers supervised + autonomous + emergency +
   unstable halts.
2. `--run --mode supervised --cycles 6`: ≥ 3 skills installed,
   ≥ 1 kernel PENDING, 0 kernels APPROVED, reflect_weakest_step
   = 2 on every cycle.
3. `--run --mode autonomous --cycles 6`: ≥ 1 kernel APPROVED.
4. `--emergency-stop-after 3` short-circuits every remaining
   cycle (`emergency_stopped:true` lines ≥ 3, final_state
   reports `emergency_stopped:true`).
5. Same seed ⇒ byte-identical JSON.

## v148.0 vs v148.1

* **v148.0** — the orchestrator wires the four sibling kernels
  (v144 / v145 / v146 / v147) directly in C. Measurement is a
  synthetic PRNG (SplitMix64), identify is a 5-topic rotation,
  reflect is a canonical trace. Everything is deterministic.
* **v148.1** —
  * MEASURE calls `cos_v143_run()` for a real benchmark score.
  * IDENTIFY calls `cos_v141_weakest()` on a live curriculum
    state that v133 meta populates.
  * IMPROVE runs `cos_v120_distill()` + `cos_v125_dpo_train()`
    + `cos_v124_hot_swap()` on the weakness topic.
  * EVOLVE's genesis uses v114 swarm (8B code specialist) as
    the real template generator.
  * REFLECT ingests the v111.2 reasoning trace instead of the
    canonical stub.
  * SHARE crosses a node boundary via v128 mesh gossip.
  * `/sovereign-dashboard` is exposed on v108 UI so the user
    sees every cycle in real time.
  * Configuration lives in `config.toml`:
    `sovereign_mode = "supervised" | "autonomous"` and
    `sovereign_interval = 24h`.

## Honest claims

* **This is not a running AGI.** v148.0 is the **control
  plane** of one. Every stage compiles and enforces its
  contracts; none of the stages is yet wired to a live model.
  v148.1 is where the plumbing meets the weights.
* **σ is still the subject, not the noise.** Every stage either
  produces or consumes a σ value. The loop's *only* authority
  to mutate the system comes from σ-gates — unstable_halt (G1),
  σ-install gate (v145 S1), σ_code gate (v146 σ-gate),
  pending_approvals (G2), and the 3-strike latches (v144 C2,
  v146 pause_after).
* **Supervised ≠ nothing.** In SUPERVISED mode the loop still
  *acquires* skills via v145 (the σ-gate there is enough to
  refuse bad skills outright), but every genesis candidate sits
  PENDING until the reviewer speaks. In AUTONOMOUS mode only
  σ-gates guard the system; the merge-gate proves the two modes
  produce observably different effects on the genesis state.
