#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
if ! command -v yosys >/dev/null 2>&1; then
  echo "synth_yosys.sh: SKIP (yosys not installed)" >&2
  exit 0
fi
mkdir -p hdl/synthesis
yosys -q -p "read_verilog -sv hdl/synthesis/xnor_binding_4096.sv; hierarchy -check; stat" \
  >hdl/synthesis/yosys_xnor_binding_stats.txt
echo "hdl/synthesis/yosys_xnor_binding_stats.txt"
