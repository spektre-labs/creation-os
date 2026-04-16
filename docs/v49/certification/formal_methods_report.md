# Formal Methods Report — DO-333-style supplement (v49 lab)

## Purpose

Document how **formal methods** are intended to be used for the σ-critical path, aligned with the *intent* of DO-178C’s **Formal Methods Supplement (DO-333)** — without claiming tool qualification or certification credit.

## Tools

| Tool | Role | Qualification claim |
|---|---|---|
| Frama-C / WP + RTE | C annotations + proof obligations | **None** (not DO-330 qualified in this repo) |
| SymbiYosys | bounded proofs on `sigma_pipeline` harness | **None** |
| GCC + sanitizers | dynamic fault detection | dev tool, not a proof |

## Properties (targets vs shipped status)

### Memory safety (HLR-006 / HLR-008)

- **Bounds / null:** ACSL `\valid` targets on hot reads; `malloc` absent in `sigma_kernel_verified.c` (manifest check).
- **Discharge status:** **T** until a pinned Frama-C report is archived under `benchmarks/` or CI publishes “all goals closed”.

### Functional shape (HLR-001..003)

- Entropy non-negative (after clamp), margin in `[0,1]`, decomposition sum identity: **unit-tested** today; **Frama-C: target**.

### Fail-closed gate (HLR-004 / HLR-009)

- `v49_sigma_gate_calibrated_epistemic`: **unit-tested**; **Frama-C: target** (small TU).

### Floating-point

IEEE-754 assumptions are standard for C float; proofs may require explicit models / axioms in WP. Treat float proofs as **P→M** work with tool pinning.

## Residual assumptions (honest)

1. **Compiler correctness** for the verified TU (non-CompCert toolchain by default).
2. **Hardware** executes instructions as expected.
3. **Integration** correctness (proxy, HTTP, Python) is outside this σ-critical path pack.

## Command

```bash
make certify-formal
```

This runs the same formal entrypoints as `make verify-c` / `formal-sby-v47`, with explicit SKIPs when tools are absent.
