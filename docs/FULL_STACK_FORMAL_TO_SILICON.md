# Creation OS — full stack: formalism to silicon (open-source spine)

This document is an **engineering orientation map**. It does **not** assert tape-out counts, shuttle economics, or foundry SLAs — those change with PDK, MPW schedule, and tooling versions. Verify externally before procurement.

---

## Layer 0 — Truth (paper + invariants)

- **LaTeX / Zenodo / corpus** — publication-grade statements live in the **corpus** repo and papers; the portable kernel cites them under [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).
- **In-repo scaffold:** \(K_{\mathrm{eff}} = (1-\sigma)\cdot K\), \(L = 1 - 2\sigma\), **1 = 1** — exercised in `creation_os_v*.c` self-tests and README; not a substitute for peer-reviewed proofs of a physical chip.

---

## Layer 1 — Thought languages (this repository today)

| Lane | Role in Creation OS |
|------|---------------------|
| **C11** | `creation_os_v2.c` … `creation_os_v10.c`, `make check*`, evidence-class discipline |
| **SystemVerilog** | `rtl/cos_*_iron_combo.sv`, `cos_agency_iron_formal.sv`; `make formal-rtl-lint`; **`make stack-ultimate`** in CI |
| **Chisel 3.6 (Scala)** | `hw/chisel/` — **SpektreCore** emits Verilog for the same gates; `sbt compile` / `runMain spektre.core.GenVerilog` |
| **Rust (iron gate)** | `hw/rust/spektre-iron-gate` — `agency_ctrl` parity + exhaustive tests vs SV assign chain; `make rust-iron-test` |
| **Python / …** | Named in the project narrative as **forward paths** (GDA, Amaranth, hls4ml, etc.); land in separate repos or future PRs — do not block shipping the C + SV + Chisel spine |

---

## Layer 2 — Instruction set reality (bare metal)

- **NEON / SME / RISC-V V** — project rules (`.cursorrules`) describe M4-oriented paths; assembly kernels are **targets**, not all shipped in this tree.

---

## Layer 3 — HDL (industry + generator)

- **SystemVerilog** — shipped; CI runs Verilator lint, cos_silicon_chip_tb sim, and Yosys assert proof on the agency formal wrapper (`make stack-ultimate`).
- **Chisel** — shipped as **generator** for `SpektreCore` (parametric Verilog producer).
- **VHDL / SpinalHDL / Amaranth / Clash** — optional parallel fronts; same semantics must **match** CLAIM_DISCIPLINE when cited.

---

## Layer 4 — Model → netlist compilers (open tools)

- **Yosys** — open synthesis + **in-CI combinational proof** (`hw/yosys/prove_agency_comb.ys`, `sat -prove-asserts`); elaboration gate `hw/yosys/elaborate_rtl.ys`. Consume `rtl/*.sv` and/or Chisel-generated `*.v`.
- **SymbiYosys (SBY)** — second CI lane with **[YosysHQ OSS CAD Suite](https://github.com/YosysHQ/oss-cad-suite-build)** + [`setup-oss-cad-suite`](https://github.com/YosysHQ/setup-oss-cad-suite): `hw/formal/agency_ctrl_prove.sby` (`mode prove`, `smtbmc`) and **`agency_ctrl_cover.sby`** (`mode cover`).
- **EQY** — `hw/formal/agency_self.eqy` proves **gold == gate** for `cos_agency_iron_combo` (sanity for tooling + future Chisel-vs-SV flows); see [EQY quickstart](https://yosyshq.readthedocs.io/projects/eqy/en/latest/quickstart.html).
- **Supply chain / static analysis** — **GitHub CodeQL** on the C build, **OpenSSF Scorecard** (scheduled), **ShellCheck** on `tools/*.sh`.
- **hls4ml / FINN** — ML-quantized flows; **evidence class** is *tooling demo* until archived bitstreams and lab notes exist.

---

## Layer 5 — Physical implementation (open PDK narrative)

- **OpenROAD / OpenLane / SKY130** — public-flow story; turnaround and cost are **not** guaranteed here. In-repo anchor: [hw/openroad/README.md](../hw/openroad/README.md).
- **Shuttle / MPW** — treat as **business + yield** problem, not a lemma from σ.

---

## Layer 6 — Substrates

FPGA, ASIC shuttle, neuromorphic, memristive, photonic — each has a different **receipt class**. The kernel repo stays honest: **Verilog in hand** is lab demo; **GDSII in repo** is layout artifact; **silicon in socket** is measured hardware.

---

## Canonical path (conceptual)

```
paper / LaTeX / corpus DOI
        ↓
creation_os_v*.c   (gcc, self-test, merge discipline)
        ↓
rtl/*.sv           (SystemVerilog, Verilator lint)
        ↓
hw/chisel          (Chisel → Verilog via GenVerilog)
        ↓
Yosys + OpenROAD + PDK   (your chosen open ASIC flow)
        ↓
GDSII → fab → packaged die
```

**Chisel is the parametric generator link** between typed Scala structure and the same combinational policy already frozen in SV.

---

## Where to start in this repo

1. `make merge-gate` or `make check` + `make formal-rtl-lint` (per CI).  
2. `cd hw/chisel && sbt compile && sbt "runMain spektre.core.GenVerilog"`.  
3. Point Yosys at `rtl/` **or** `hw/chisel/generated/` — **equivalence check** between SV and generated Verilog is a recommended next receipt (not automated here yet).

---

*Spektre Labs · Creation OS · 2026*
