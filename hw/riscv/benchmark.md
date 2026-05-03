# σ-silicon — hardware vs software **targets** (lab)

**SPDX-License-Identifier:** LicenseRef-SCSL-1.0 OR AGPL-3.0-only  

**Evidence class:** These figures are **design targets / ROM estimates** for a small CFU next to RV32IM(C)(V). They are **not** merge-gate measurements on FPGA or ASIC. For claim hygiene see [docs/CLAIM_DISCIPLINE.md](../../docs/CLAIM_DISCIPLINE.md).

---

## 1. Software baseline (scalar C on RV32IM)

Rough order-of-magnitude for a **naive** compiler output **without** vector or custom ops:

| Operation | Target cycles (order of magnitude) | Notes |
|-----------|------------------------------------|--------|
| σ update (`cos_sigma_q16_update`) | ~8–15 | 32-bit mul + shift; exact count is compiler/core dependent |
| σ verdict (`cos_sigma_mirror_verdict`) | ~4–10 | Two compares + branches |
| One ternary weight (unpack + add/sub) | ~5–8 | Load byte, shift/mask, branch |
| Update + verdict sequence | ~20–30 | **Not** a single indivisible atomic region in software |

---

## 2. Hardware target (Xsigma CFU)

Assumptions: **single-issue** CFU combinatorial path registered once, σ state in CFU CSRs, no external memory for σ hot path.

| Operation | Target cycles | Notes |
|-----------|---------------|--------|
| `SIGMA.UPDATE` | 1 | After pipeline fill |
| `SIGMA.GATE` | 1 | Combinational on registered state |
| `TERN.MATVEC` (one weight) | 1 | Contribution only; row **sum** still needs a GPR add tree or loop |
| Update + verdict | 2–3 | Issued back-to-back |

**Speedup:** versus the **scalar C** column above, expect roughly **~6–8×** on the **update+verdict micro-sequence** when the CFU is on the critical path; **row matvec** speedup depends on issue width and memory bandwidth (dominated by loads), not the CFU alone.

---

## 3. FPGA **floorplan** targets (illustrative only)

| Device | Sketch | Notes |
|--------|--------|--------|
| Xilinx Artix-7 (e.g. XC7A35T) | σ CFU + modest unroll | Needs placement + timing closure |
| Lattice ECP5 (e.g. LFE5U-45F) | σ CFU + 2-wide dot helper | Yosys + nextpnr flows vary by license |

**Area / power:** “&lt; 500 LUT” and “&lt; 50 mW @ 100 MHz” are **aspirational budgets** for the **CFU slice alone**, excluding core, buses, and BRAM — not verified in this repository.

---

## 4. How to run local tooling (optional)

- **Lint / elaborate:** `make check-sigma-riscv-verilator` (requires `verilator`; skips if absent).
- **Synthesis stats:** `python3 -m cos silicon --synth` (runs **Yosys** if installed; otherwise reports `skipped`).
- **Semantic smoke:** `python3 -m cos silicon --simulate --test 'sigma_update 0x1000 0xE000'` — compares against [`sigma_gate_core`](../../python/cos/sigma_gate_core.py) Q16 semantics.

---

*Spektre Labs · Creation OS · v159*
