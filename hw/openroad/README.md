# OpenROAD / PDK (physical stack, host-side)

This directory is the **integration anchor** for the path from elaborated RTL to placed & routed GDSII. Nothing here pins a proprietary PDK; use an open flow on your machine or CI runner.

## Recommended open stack

1. **Yosys** — synthesis from `rtl/*.sv` (see `../yosys/elaborate_rtl.ys` and `prove_agency_comb.ys`).
2. **OpenROAD** — floorplan, placement, CTS, routing ([OpenROAD](https://theopenroadproject.org/)).
3. **PDK** — e.g. **SkyWater sky130** with open PDK + open libs, or your foundry kit under NDA.

## Flow sketch

- Export a single top (Chisel `GenVerilog`, hand RTL, or Yosys-generated netlist).
- Liberty + LEF + tech LEF → OpenROAD Tcl flow (`init_floorplan`, `global_placement`, `detailed_route`, …).
- Signoff: Magic/KLayout + OpenSTA (timing) + IR drop tools as needed.

The Creation OS repo gates **RTL lint** (Verilator `-Wall --timing`), **Yosys elaboration**, **Yosys assert proof** for agency, **Verilator sim**, **Rust iron parity**, and **Chisel compile** via `make stack-ultimate` on apt tools, plus a parallel **[YosysHQ/setup-oss-cad-suite](https://github.com/YosysHQ/setup-oss-cad-suite)** job for **SymbiYosys** (`hw/formal/agency_ctrl_prove.sby`) and a **clang ASan+UBSan** `make check` job (see `.github/workflows/ci.yml`).
