# σ-Pipeline Tapeout Checklist (Creation OS v38)

## Roadmap — post-Efabless “still alive” paths (2026)

Efabless shut down; ChipIgnite-style SkyWater shuttles through that channel are **not** the current default story. Three **live** classes of routes remain:

1. **IHP SG13G2 (130 nm)** — Tiny Tapeout moved here; public shuttles continue (check Tiny Tapeout site for the next run calendar). Open PDK. The `tt_um_sigma_tile` here targets **digital-only** σ logic that is meant to fit a Tiny Tapeout-class tile budget (see `hdl/asic/config.json` die box).
2. **GlobalFoundries GF180MCU** — open PDK at 180 nm; larger features / larger die budget; good fit for pure digital tiles. Tiny Tapeout and other aggregators experiment with this node.
3. **Direct IHP shuttle** — if aggregator tile limits block the full macro, direct MPW slots remain a paid researcher/small-team route (order-of-magnitude **USD 5k–15k** per slot is a planning bracket, not a quote).

Flow stack: **LibreLane** is the maintained successor to OpenLane 1.x (FOSSi Foundation). This repo carries a **template** `hdl/asic/config.json` plus `hdl/asic/librelane_run.sh`; a green GDSII run is **host- and PDK-dependent**, not a merge-gate promise.

## Schedule (planning; not “M” until artifacts exist)

| Month (2026) | Goal |
|--------------|------|
| April | v37 σ-pipeline Yosys synthesis path exercised (`make synth-v37`) |
| May | v38 Tiny Tapeout tile (`tt_um_sigma_tile`) + Verilator smoke green (`make check-asic-tile`) |
| June | Submit to an open Tiny Tapeout shuttle (IHP/GF180 per that run’s rules) |
| Fall | Packaged parts back; lab measurements; promote silicon claims only with archived repro |

## Pre-submission

- [ ] Verilator simulation passes (`make check-asic-tile`)
- [ ] SymbiYosys formal verification passes for the v37 core harness (`make formal-sby-v37`)
- [ ] LibreLane flow completes without DRC/LVS errors on a pinned tool + PDK revision (`hdl/asic/librelane_run.sh`)
- [ ] GDSII exists under the run directory LibreLane emits (path varies by tool version)
- [ ] Timing met at the chosen clock (re-run with post-layout STA; Yosys-only numbers are not silicon)
- [ ] Area fits the shuttle tile budget for that run
- [ ] Power estimate reviewed (signoff tool / vendor flow; not a Verilator number)

## Submission (when a Tiny Tapeout window opens)

- [ ] Fork / use the Tiny Tapeout template repository for the target shuttle
- [ ] Copy `hdl/asic/sigma_tile.sv` to the template’s `src/tt_um_sigma_tile.sv` (or wire it as a submodule) and set `info.yaml` metadata
- [ ] CI green on the Tiny Tapeout GitHub Actions pipeline (LibreLane/OpenLane class flows)
- [ ] Submit before the run deadline

## Post-fabrication

- [ ] Receive packaged parts + breakout / carrier
- [ ] Exercise SPI-ish pin plan against an FPGA or MCU host
- [ ] Compare measured popcount + abstain against a golden software model on canned vectors
- [ ] Measure power with bench supply + ammeter if you are promoting **M-tier** power claims
- [ ] Update `docs/WHAT_IS_REAL.md`: only then move specific silicon claims from **P** to **M** with a repro bundle pointer
