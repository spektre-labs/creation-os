# σ-gate RISC-V custom ISA sketch — **Xsigma** (lab / not ratified)

**SPDX-License-Identifier:** LicenseRef-SCSL-1.0 OR AGPL-3.0-only  

**Language:** English (repository policy).  

**Non-goals:** This document is a **design sketch** for integrators. It is **not** a RISC-V International ratified extension, **not** tape-out guidance, and **does not** change [`python/cos/sigma_gate.h`](../../python/cos/sigma_gate.h) (packaging hook only). Behavioral ground truth for σ arithmetic and verdicts is the Q16 mirror in [`src/inference/cos_sigma_mirror.h`](../../src/inference/cos_sigma_mirror.h) and [`python/cos/sigma_gate_core.py`](../../python/cos/sigma_gate_core.py).

**Evidence class:** *Lab demo / roadmap* — opcode plumbing and cycle counts are **targets**, not measured silicon (see [benchmark.md](benchmark.md)).

---

## 1. Overview

**Xsigma** names a vendor-specific **custom function unit (CFU)** pattern (conceptually compatible with RoT CFU / PULP-style custom opcodes) that accelerates:

1. **σ-state update** — same algebra as `cos_sigma_q16_update` (not a generic EMA).
2. **σ verdict** — same predicate order as `cos_sigma_mirror_verdict` (`k_eff`, then `d_sigma`, else ACCEPT).
3. **Ternary dot contributions** — one packed weight code per issue (`{-1,0,+1}`), matching [`ternary_engine` inference packing](../../src/inference/ternary_engine.h).
4. **Cascade / spike** — **placeholders** for multi-level policy and LIF-style stepping (see §4).

All scalar σ values below are **unsigned Q16** in the range **[0, 65535]** representing **[0, 1)** unless noted.

---

## 2. Architectural state (CFU CSRs, not GPR)

The hot path keeps a persistent **σ cognitive state** (mirrors `cos_sigma_q16_state_t`):

| CSR (conceptual) | Width | Meaning |
|------------------|-------|---------|
| `xsigma.sigma`   | 32    | Current σ (Q16 lane; uses low 17 bits operationally) |
| `xsigma.d_sigma` | 32    | Last step delta `new_sigma - sigma_prev` |
| `xsigma.k_eff`   | 32    | Effective gain after update |
| `xsigma.lif_vm`  | 32    | Optional membrane for `SIGMA.SPIKE` lab opcode |

GPR operands are used only for **per-instruction** data (e.g. `new_sigma`, `k_raw`, activations). A full core would **scoreboard** CFU latency like any multi-cycle custom instruction.

---

## 3. Instruction mnemonics (R-type skeleton)

**Encoding skeleton:** treat as **custom-0 / custom-1** R-type space reserved by the **SoC vendor**; `funct3 = 0` selects the Xsigma bundle; `funct7` selects the operation.

| `funct7` | Mnemonic | Operands | Summary |
|----------|----------|----------|---------|
| `0x01` | `SIGMA.UPDATE` | `rd, rs1, rs2` | `rs1` = `new_sigma` (Q16); `rs2` = `k_raw` (Q16). Updates CSRs per `cos_sigma_q16_update`. `rd` = new `k_eff`. |
| `0x02` | `SIGMA.GATE` | `rd, rs1` | `rs1` **ignored** in minimal CFU (reads CSRs). `rd` = verdict: `0` ACCEPT, `1` RETHINK, `2` ABSTAIN. |
| `0x03` | `TERN.MATVEC` | `rd, rs1, rs2` | **One weight:** `rs1[1:0]` = ternary code (`00` skip, `01` +1, `10` −1); `rs2[7:0]` = int8 activation (sign-extended). `rd` = contribution to accumulate (caller adds into row accumulator). |
| `0x04` | `SIGMA.CASCADE` | `rd, rs1, rs2` | **Lab placeholder:** five-level cascade; see RTL note. |
| `0x05` | `SIGMA.SPIKE` | `rd, rs1, rs2` | **Lab LIF step:** `rs1` = prior membrane; `rs2` = input current. Updates `xsigma.lif_vm`; `rd` = `1` if spike fired else `0`. |

**Important:** `SIGMA.UPDATE` is **not** an exponential moving average. It implements the **creation-os σ interrupt recurrence** (δσ and residual-weighted `k_eff`).

**TERN.MATVEC row:** A full matrix row is a **loop** of `TERN.MATVEC` (or a future **widened** packed intrinsic) over packed bytes (4 weights per byte), identical in semantics to the scalar packed path in `ternary_matmul_packed`.

---

## 4. Placeholder opcodes (cascade / spike)

- **`SIGMA.CASCADE`:** A production definition would consume five σ scalars and five thresholds (memory or wide register block). The shipped **soft-CFU** returns a constant **level 5** stub so decode/simulation has a defined behavior; replace in a SoC fork with real comparators.
- **`SIGMA.SPIKE`:** Simplified integrate-and-fire **without** full σ-gate coupling; optional coupling belongs in a neuromorphic chapter, not in the cognitive `sigma_gate_core` path.

---

## 5. Reference RTL and tooling

- Verilog soft-CFU: [`sigma_core.v`](sigma_core.v) (`sigma_rv_cfu`).
- Cycle **targets** vs software baselines: [`benchmark.md`](benchmark.md).
- CLI lab: `python -m cos silicon --help` (generate / simulate / synth / benchmark stubs).

---

*Spektre Labs · Creation OS · v159 σ-silicon lab sketch*
