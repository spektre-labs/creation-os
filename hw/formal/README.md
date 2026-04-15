# Formal verification (SymbiYosys / Yosys)

## What exists in-tree

- **`../../rtl/cos_agency_iron_formal.sv`** — immediate assertions over `cos_agency_iron_combo` (STABILIZE only when `stab`).
- **`../../rtl/cos_agency_iron_cover.sv`** — `cover` targets for EXECUTE / HOLD / STABILIZE (SymbiYosys **cover** mode only; not in default Verilator lint file list).
- **`agency_ctrl_prove.sby`** — SymbiYosys **prove** task (`smtbmc`) over the same RTL; sources are copied via `[files]` → `/src/` per [SBY reference](https://yosyshq.readthedocs.io/projects/sby/en/latest/reference.html).
- **`agency_ctrl_cover.sby`** — SymbiYosys **cover** task (`mode cover`, `smtbmc`) on `cos_agency_iron_cover`.
- **`agency_self.eqy`** — [EQY](https://yosyshq.readthedocs.io/projects/eqy/en/latest/quickstart.html) **self-equivalence** on `cos_agency_iron_combo` (gold == gate, `use sat`).

## Local commands

```bash
make yosys-prove-agency   # Yosys sat -prove-asserts (hw/yosys/prove_agency_comb.ys)
make formal-sby-agency    # sby -f agency_ctrl_prove.sby (needs sby on PATH)
make formal-sby-cover-agency
make eqy-agency-self
make rust-iron-lint
make oss-formal-extreme   # all of the above + yosys prove (OSS CAD / local)
make stack-nucleon        # stack-ultimate + formal-sby-agency (SKIPs if sby missing)
make stack-singularity    # nucleon + cover + EQY + rust lint (max local gate)
```

## CI hard mode (upstream YosysHQ practice)

- **[YosysHQ/setup-oss-cad-suite](https://github.com/YosysHQ/setup-oss-cad-suite)** — prebuilt **yosys + sby + solvers** so SymbiYosys matches what YosysHQ ships (see workflow job `oss-cad-formal`).
- **Distro packages** — the main job still uses **apt Verilator + apt Yosys** for a fast, reproducible `make stack-ultimate` without downloading the full OSS CAD bundle.
- **GitHub security** — **CodeQL** (`cpp`) on `make standalone` + `make test`; **OpenSSF Scorecard** (weekly SARIF upload); **ShellCheck** on `tools/*.sh`.

SymbiYosys install options are summarized in the [SBY installation guide](https://yosyshq.readthedocs.io/projects/sby/en/latest/install.html) (OSS CAD Suite, Tabby CAD, or from source).

## Full hardware gate

- **`make stack-ultimate`** — Verilator (lint + sim), Yosys elab + SAT assert proof, Rust iron gate, Chisel (SKIPs without sbt).
- **`make stack-nucleon`** — adds **`make formal-sby-agency`** when `sby` is available.
