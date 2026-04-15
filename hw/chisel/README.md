# SpektreCore (Chisel 3.6)

Parametric **Scala → FIRRTL/Verilog** description of the same combinational gates as `rtl/cos_*_iron_combo.sv`:

- `FormalIronCombo` — stack step, limits plane, sim≠commit, level tag  
- `AgencyIronCombo` — EXECUTE / HOLD / STABILIZE  
- `CommitIronCombo` — C–I–L + falsifier + no model-owned commit  
- `SpektreCore` — wires agency `ctrl` into the commit block (matches `cos_silicon_chip_tb` wiring)

## Requirements

- JDK 17+  
- [sbt](https://www.scala-sbt.org/) 1.9+

## Commands

```bash
cd hw/chisel
sbt compile
sbt "runMain spektre.core.GenVerilog"
# Verilog appears under generated/ (or pass -DchiselTargetDir=...)
```

Override output directory:

```bash
sbt -DchiselTargetDir=generated "runMain spektre.core.GenVerilog"
```

## License

AGPL-3.0-or-later (same as Creation OS kernel).
