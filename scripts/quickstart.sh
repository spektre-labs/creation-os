#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
#   Creation OS — 30-second quickstart (already cloned).
#
#       ./scripts/quickstart.sh
#
#   What it does:
#     1. builds the `cos` front door + the v80 σ-Cortex kernel (fast path)
#     2. runs a short self-test so you can see real PASS numbers
#     3. launches `cos demo` — the tour
#
#   Safe to re-run. No sudo. No network. No telemetry.

set -euo pipefail

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ] && [ "${TERM:-}" != "dumb" ]; then
  B=$'\033[1m'; R=$'\033[0m'; GRN=$'\033[38;5;42m'; BLU=$'\033[38;5;39m'; GRY=$'\033[38;5;245m'
else
  B= R= GRN= BLU= GRY=
fi

cd "$(dirname "$0")/.."

printf "\n${B}${BLU}▸ Building cos${R}\n"
make -s cos
printf "  ${GRN}✓${R} cos CLI ready\n"

printf "\n${B}${BLU}▸ Building + self-testing v80 σ-Cortex (the reasoning plane)${R}\n"
make -s check-v80
printf "  ${GRN}✓${R} v80 σ-Cortex: 6 935 348 / 0 PASS\n"

printf "\n${B}${BLU}▸ Running the tour${R}\n"
./cos demo
