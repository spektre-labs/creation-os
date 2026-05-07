# RISC-V Xσ — specification sketch (not implemented)

**Status:** SPECIFICATION ONLY. There is **no** toolchain plugin, RTL, or silicon for this extension in this repository.

**Language:** This document is informative architecture copy for integrators. It is **not** a product commitment or an ISA standard proposal.

## Vision

A RISC-V **vendor-defined custom-executable** (or equivalent custom-op) space could expose a single **σ-score** primitive that:

- Takes **addresses** of prompt bytes, response bytes, and a small **result struct** (σ float + discrete verdict).
- Runs a **deterministic** entropy-style proxy comparable to Python L1 (character / token bucket histogram), or a **frozen** C kernel policy agreed for that core.
- Avoids **dynamic allocation** in the fast path and targets **fixed latency** for embedded / per-packet use cases.

Illustrative assembly mnemonic (pseudocode **only**):

```asm
/* Pseudocode — not a ratified opcode */
xsigma.score  a0, a1, a2   /* a0=prompt*, a1=response*, a2=result* */
/* Conceptual results: σ in memory at result+0, verdict enum at result+4 */
```

## Why (use cases)

- Per-packet or per-flow scoring in network ingress (policy hint only; not a security proof).
- Per-token or per-step gating beside an accelerator (still needs software policy).
- Per-sample sensor pipelines where a deterministic doubt scalar is useful (lab).

## Non-goals

- Replacing `python/cos/sigma_gate.py` or `sigma_gate.h` packaging hooks.
- Claiming one-cycle implementations, formal verification, or cryptographic strength.
- **NOT AGI ACHIEVED** — see `docs/CLAIM_DISCIPLINE.md`.

## Implementation requirements (external)

- RISC-V toolchain with custom-instruction or coprocessor support.
- RTL / FPGA / ASIC workflow outside this repo.
- Traceability to whatever evidence tier you assign (lab vs silicon vs harness).
