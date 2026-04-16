#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# Synthesize σ-pipeline on xc7 (Yosys) + optional SymbiYosys formal. Writes logs under hdl/v37/.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
mkdir -p hdl/v37

if ! command -v yosys >/dev/null 2>&1; then
  echo "synth_and_measure.sh: SKIP (yosys not installed)" >&2
  exit 0
fi

yosys -p "
read_verilog -sv hdl/v37/sigma_pipeline.sv
chparam -set HV_BITS 64 -set N_HEADS 4 -set ABSTAIN_THRESHOLD 16'h0200 sigma_pipeline
hierarchy -top sigma_pipeline
proc; opt
synth_xilinx -top sigma_pipeline -family xc7
stat
write_json hdl/v37/synth_stats.json
" 2>&1 | tee hdl/v37/yosys.log

grep "Number of cells" hdl/v37/yosys.log || true
grep "Number of wires" hdl/v37/yosys.log || true

if command -v sby >/dev/null 2>&1; then
  (cd hdl/v37 && sby -f sigma_pipeline.sby) 2>&1 | tee hdl/v37/formal.log
else
  echo "synth_and_measure.sh: formal SKIP (sby not installed)" | tee hdl/v37/formal.log
fi

echo "Results:"
echo "  Synthesis: hdl/v37/yosys.log"
echo "  Formal:    hdl/v37/formal.log"
echo "  Stats:     hdl/v37/synth_stats.json"
