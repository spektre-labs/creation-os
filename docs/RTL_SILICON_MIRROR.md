# RTL silicon mirror (SystemVerilog)

Synthesizable **combinational and sequential** building blocks that mirror the same architectural commitments as the portable C kernel and consortium docs: **state before act**, **formal limits**, **control triad**, **non-delegable commit**, **clock-domain hygiene**.

| RTL module | Idea mirrored | Notes |
|------------|---------------|--------|
| `rtl/cos_formal_iron_combo.sv` | Spektre **formal stack** step + **limits** bitplane + **no semantics** flag + **simulation ≠ commit** | `stack_step_ok`, `limits_bad`, `sim_commit_forbidden`, ordinal `level_tag` |
| `rtl/cos_agency_iron_combo.sv` | **Control matrix** (EXECUTE / HOLD / STABILIZE) | `$countones` on trap mask; anchor-override and amplification force STABILIZE |
| `rtl/cos_agency_iron_formal.sv` | **Agency properties** | Immediate `assert`: valid `ctrl` encoding; STABILIZE only when `stab` |
| `rtl/cos_agency_iron_cover.sv` | **Agency cover** | SymbiYosys `cover` for each `ctrl` encoding (not in default Verilator lint bundle) |
| `rtl/cos_commit_iron_combo.sv` | **C–I–L** + falsifier bit + **model lane may not own commit** | `commit_allowed` only in EXECUTE with owner + context + ignored |
| `rtl/cos_boundary_sync.sv` | **Async boundary** / tool-domain signal into kernel clock | Classic 2-flop synchronizer |
| `rtl/cos_silicon_chip_tb.sv` | Self-checking bench | Verilator `--binary` entry |

## Chisel generator (Scala → Verilog)

Same semantics as the SV blocks, parametric description: **`hw/chisel/`** (`SpektreCore`, `GenVerilog`).  
Build: `make chisel-compile` / `make chisel-verilog` (requires JDK + sbt). See [hw/chisel/README.md](../hw/chisel/README.md).

**Rust assign-chain mirror** of `cos_agency_iron_combo` (memory-safe parity tests): **`hw/rust/spektre-iron-gate`**. Run `make rust-iron-test` (requires `cargo`).

**Yosys:** elaboration `hw/yosys/elaborate_rtl.ys` (`make yosys-elab`); combinational assert proof `hw/yosys/prove_agency_comb.ys` (`make yosys-prove-agency`). See [hw/formal/README.md](../hw/formal/README.md).

**Toward tape-out:** [hw/openroad/README.md](../hw/openroad/README.md) — OpenROAD / PDK anchor (host-side flow; no PDK committed here).

## Build

```bash
make formal-rtl-lint    # Verilator --lint-only; required in CI when Verilator is installed
make formal-rtl-sim     # compile + run cos_silicon_chip_tb (optional local)
make yosys-elab         # Yosys hierarchy/proc/opt on iron RTL
make yosys-prove-agency # Yosys sat -prove-asserts on agency formal wrapper
make rust-iron-test     # Rust parity + exhaustive agency vs SV assign chain
make chisel-verilog     # optional: emit SpektreCore.v into hw/chisel/generated/
make stack-ultimate     # CI: lint + elab + prove + sim + chisel verilog + rust (see Makefile)
make stack-nucleon      # + SymbiYosys agency_ctrl_prove.sby (local, if sby installed)
```

CI also runs **SymbiYosys** on **`hw/formal/agency_ctrl_prove.sby`** via the **YosysHQ OSS CAD Suite** setup action (see `.github/workflows/ci.yml` job `oss-cad-formal`).

## Relation to other docs

- Consortium / push hygiene: [REPOS_AND_ROLES.md](REPOS_AND_ROLES.md)  
- Evidence classes (RTL does not replace harness claims): [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md)

---

*Spektre Labs · Creation OS · 2026*
