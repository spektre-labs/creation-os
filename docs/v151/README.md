# v151 — σ-Code-Agent · TDD Loop + Sandbox Compile + σ-Gated PR

**Kernel:** [`src/v151/codeagent.h`](../../src/v151/codeagent.h), [`src/v151/codeagent.c`](../../src/v151/codeagent.c) · **CLI:** [`src/v151/main.c`](../../src/v151/main.c) · **Gate:** [`benchmarks/v151/check_v151_code_agent_tdd_cycle.sh`](../../benchmarks/v151/check_v151_code_agent_tdd_cycle.sh) · **Make:** `check-v151`

## Problem

v146 σ-genesis emits kernel *skeletons* (header + source + test
+ README). That is not enough for a self-writing code agent:

* the emit is never compiled,
* the generated test is never executed,
* σ_code is a jitter constant, not a signal tied to real
  compile or run outcomes.

v151 closes that loop. The code-agent **actually invokes the
system compiler** on the emitted C, **runs the test binary**,
and composes σ_code from real compile + run signals before
proposing a σ-gated PR candidate.

## σ-innovation

A three-phase sandbox cycle:

| Phase | Command | Expected | σ contribution |
|---|---|---|---|
| **A** | `cc -o preimpl tests/test.c` | FAIL (link errors; kernel.c absent) | σ_A = 0.10 on expected-fail; 0.90 if it somehow passes (trivial test) |
| **B** | `cc -o full kernel.c tests/test.c` | PASS (skeleton is known-good) | σ_B = 0.10 on compile-ok; 0.90 on any warning/error |
| **C** | `./full` | PASS (exit 0) | σ_C = 0.10 on run-ok; 0.90 on non-zero exit |

`σ_code_composite = geomean(σ_A, σ_B, σ_C)`. Three OK legs
⇒ σ_code ≈ 0.10. One broken leg pushes σ_code over 0.40 which
trips the σ-gate.

σ-gate: `σ_code_composite ≤ τ_code` ⇒ PENDING (PR candidate);
else GATED_OUT (agent refuses to open a PR).

Human-in-the-loop: `cos_v151_approve` / `cos_v151_reject`
route through v146. Three consecutive rejects latch
`paused = true`; `cos_v151_resume()` clears the latch.

## Merge-gate

`make check-v151` runs:

1. Self-test (structural + pause-latch + JSON).
2. Full TDD cycle: Phase A fails as expected, Phase B
   compiles, Phase C runs, `status:"pending"`.
3. `σ_code_composite ≤ 0.20` on three OK legs.
4. Disk layout: `kernel.h`, `kernel.c`, `tests/test.c`,
   `README.md`, `tdd.log`, and the Phase-B binary all exist
   in `<workroot>/v<version>/`.
5. Forced `gated_out` when τ = 0.05 (below the OK geomean
   floor).
6. `--pause-demo` latches `paused:true` after three rejects.
7. Same seed ⇒ byte-identical JSON (modulo tmp paths).

## v151.0 vs v151.1

* **v151.0** — pure C11 + libc `system()` / `popen()`. Uses
  `$CC` (default `cc`) with `-O0 -Wall -std=c11`. Every file it
  writes is emitted by v146; v151 only orchestrates compile +
  run + σ composition.
* **v151.1** —
  * `-fsanitize=address,undefined` on Phase B/C; ASAN / UBSAN
    failures push σ_B or σ_C into the 0.90 bucket.
  * LCOV coverage ≥ 80 % required or σ_C is penalised.
  * Real git branch emission: the PR candidate is pushed to
    `feature/v<N>-σ-code-agent` and opened against `main`.
  * v114 swarm (code specialist) replaces v146 as the emit
    source for the kernel body — v146 stays the template
    emitter, v151 decides what body goes in it.
  * `/v1/code-agent/propose` on v106 HTTP exposes the cycle.

## Honest claims

* **v151.0 does not write original code.** The body of the
  generated kernel is v146's canonical skeleton. What v151 adds
  is *verification* — compile, run, σ-compose — so when v151.1
  swaps in a real code generator (v114 swarm), the σ-gate math
  is already honest.
* **A passing merge-gate does not mean the *kernel* is
  correct.** It means the emitted code compiles, the emitted
  test passes, and the sandbox σ machinery ran without
  structural error. Kernel correctness under actual use is the
  responsibility of every downstream v<N> merge-gate.
* **HITL is not optional.** A PENDING PR is not a merged PR.
  v151 always goes through v146 approve / reject, and the
  3-strike latch stops auto-proposal if the reviewer is
  rejecting everything.
