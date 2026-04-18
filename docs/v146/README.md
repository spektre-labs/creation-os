# v146 — σ-Genesis · Automated Kernel Generation

**Kernel:** [`src/v146/genesis.h`](../../src/v146/genesis.h), [`src/v146/genesis.c`](../../src/v146/genesis.c) · **CLI:** [`src/v146/main.c`](../../src/v146/main.c) · **Gate:** [`benchmarks/v146/check_v146_genesis_template_gen.sh`](../../benchmarks/v146/check_v146_genesis_template_gen.sh) · **Make:** `check-v146`

## Problem

Every v-layer in Creation OS has the same six-file shape:

```
src/vN/kernel.h         API + contracts
src/vN/kernel.c         implementation
src/vN/main.c           CLI (--self-test / --demo)
tests/vN/test.c         ≥ 5 deterministic tests
docs/vN/README.md       honest-claims page
Makefile entries        check-vN-* → check-vN
```

Until now, humans wrote those files. v146 is the first Creation
OS kernel that *generates other kernels* from a **gap descriptor**
produced by v133 meta (weakness axis) + v140 causal attribution
(which σ-channels lack coverage).

## σ-innovation

The σ-gate is on the **generated code**, not on a live model:

| State | Meaning |
|---|---|
| `PENDING`   | σ_code < τ_merge — offered for human review |
| `APPROVED`  | User clicked "merge" after review |
| `REJECTED`  | User clicked "no"; counter toward the 3-strike pause |
| `GATED_OUT` | σ_code ≥ τ_merge at generation time — recorded, never offered |

The loop has the same **3-consecutive-reject pause latch** as
v144, so if the template keeps missing the real architectural
gap the system stops spamming the reviewer.

σ_code in v146.0 is produced deterministically from `(seed,
completeness)`; all four files are always emitted cleanly, so
the live axis is the seeded jitter that represents the downstream
reviewer's view of novelty + internal consistency. On seed
`0xDEEDF00D` this maps to σ_code ≈ 0.10 — a clean merge-candidate
window.

## Merge-gate

`make check-v146`:

1. Self-test covers PENDING / GATED_OUT / APPROVED / REJECTED /
   paused branches and JSON emit.
2. `--demo --write TMPDIR` writes the canonical 4 files
   (`kernel.h`, `kernel.c`, `tests/test.c`, `README.md`) to disk
   **and** the benchmark script compiles the generated
   `kernel.c` with `$(CC) -c -Wall -std=c11` — the generator is
   honest only if its output compiles.
3. `--pause-demo` latches `paused=true` on the 3rd consecutive
   reject.
4. Byte-identical state JSON across two runs at the same seed.

## v146.0 vs v146.1

* **v146.0** — deterministic template, offline, zero model
  calls. Four-file skeleton that compiles cleanly.
* **v146.1** — replaces the template with **v114 swarm (8B
  code specialist)** as the actual generator; v135 Prolog
  consistency checker on the emitted symbol graph; v108 UI
  webhook for HITL review: *"v149 σ-TemporalGrounding generated,
  5/5 tests pass, σ_code=0.18. Merge?"*

## Honest claims

* **Skeletons, not kernels.** A v146.0 candidate is a
  compiling stub with a non-trivial self-test — the *real*
  kernel logic is human-authored on merge. We do not claim v146
  writes working kernels; we claim it writes scaffolds that
  reliably accept a human edit.
* **σ_code is synthetic.** Like σ_avg in v145.0, σ_code is a
  deterministic seed-controlled number. The contract v146.0
  proves is the *state machine* around σ_code, not its
  calibration against a real reviewer.
* **3-strike pause is non-negotiable.** Even the most confident
  auto-generator must pause after three consecutive rejects from
  the human — that is the genesis loop's safety floor, shared
  verbatim with v144.
