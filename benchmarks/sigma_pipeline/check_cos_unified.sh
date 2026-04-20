#!/usr/bin/env bash
#
# Smoke test for H6 — the unified `cos` CLI dispatching to every
# I/A/S/D/H product binary plus the σ-meta summary.

set -euo pipefail

if [ ! -x ./cos ]; then
  echo "check-cos-unified: missing ./cos" >&2
  exit 2
fi

# --- cos help / version (meta subcommands never regress). ---
HELP=$(./cos help 2>&1)
for needle in agent network omega formal paper sigma-meta; do
  grep -q "^  \?.*${needle}" <<<"$HELP" \
    || { echo "FAIL help missing '$needle'"; exit 1; }
done

# --- cos sigma-meta: pinned machine-readable σ-meta summary. ---
META=$(./cos sigma-meta)
echo "$META"
grep -q '"tool":"cos","subcommand":"sigma-meta"'          <<<"$META" || { echo "FAIL meta tool"; exit 1; }
grep -q '"invariant":"declared==realized"'                <<<"$META" || { echo "FAIL meta invariant"; exit 1; }
grep -q '"discharged":"4/4"'                              <<<"$META" || { echo "FAIL meta ledger"; exit 1; }
grep -q '"t6_bound_ns":250'                               <<<"$META" || { echo "FAIL meta T6 bound"; exit 1; }
grep -q '"equivalent":true'                               <<<"$META" || { echo "FAIL meta substrate"; exit 1; }
grep -q '"domain":"factual_qa","gate_helps":true'         <<<"$META" || { echo "FAIL meta factual_qa"; exit 1; }
grep -q '"domain":"commonsense","gate_helps":false'       <<<"$META" || { echo "FAIL meta commonsense"; exit 1; }
grep -q '"domain":"adversarial_inputs","gate_helps":false'<<<"$META" || { echo "FAIL meta adversarial"; exit 1; }
grep -q '"target_price_eur_per_month":20.00'              <<<"$META" || { echo "FAIL meta price"; exit 1; }
grep -q '"pass":true'                                     <<<"$META" || { echo "FAIL meta pass"; exit 1; }

# --- cos paper: dispatch to creation_os_sigma_paper (H5). ---
if [ -x ./creation_os_sigma_paper ]; then
  PAPER=$(./cos paper)
  grep -q '"kernel":"sigma_paper"'                        <<<"$PAPER" || { echo "FAIL paper dispatch"; exit 1; }
  grep -q '"fingerprint":"9905916cbff54a86"'              <<<"$PAPER" || { echo "FAIL paper fingerprint"; exit 1; }
else
  echo "check-cos-unified: SKIP cos paper (binary missing)"
fi

# --- cos formal: dispatch to creation_os_sigma_formal (H4). ---
if [ -x ./creation_os_sigma_formal ]; then
  FORMAL=$(./cos formal)
  grep -q '"kernel":"sigma_formal"'                       <<<"$FORMAL" || { echo "FAIL formal dispatch"; exit 1; }
  grep -q '"discharged":"4/4"'                            <<<"$FORMAL" || { echo "FAIL formal 4/4"; exit 1; }
else
  echo "check-cos-unified: SKIP cos formal (binary missing)"
fi

# --- cos network: dispatch to cos-network (D6). ---
if [ -x ./cos-network ]; then
  NET=$(./cos network list --json)
  grep -q '"command":"list"'                              <<<"$NET" || { echo "FAIL network dispatch"; exit 1; }
  grep -q '"id":"strong"'                                 <<<"$NET" || { echo "FAIL network payload"; exit 1; }
else
  echo "check-cos-unified: SKIP cos network (binary missing)"
fi

# --- cos omega: dispatch to creation_os_sigma_omega (S6). ---
if [ -x ./creation_os_sigma_omega ]; then
  OM=$(./cos omega)
  grep -q '"kernel":"sigma_omega"'                        <<<"$OM" || { echo "FAIL omega dispatch"; exit 1; }
else
  echo "check-cos-unified: SKIP cos omega (binary missing)"
fi

# --- cos agent: dispatch to cos-agent (A6) with --help-like flag.
#     cos-agent expects `--task` or similar; we just verify the
#     dispatcher invokes it and captures a non-zero exit cleanly. ---
if [ -x ./cos-agent ]; then
  set +e
  ./cos agent --nonexistent-flag >/dev/null 2>&1
  rc=$?
  set -e
  # cos-agent should exit with some non-zero usage error; the
  # dispatcher itself must not explode.  Accept any value < 126.
  if [ "$rc" -ge 126 ]; then
    echo "FAIL agent dispatch itself broke ($rc)"; exit 1
  fi
fi

# --- Unknown subcommand must exit with 2 from the top-level. ---
set +e
./cos definitely-unknown-subcmd >/dev/null 2>&1
rc=$?
set -e
if [ "$rc" -ne 2 ]; then
  echo "FAIL unknown subcommand rc=$rc (expected 2)"; exit 1
fi

echo "check-cos-unified: cos {sigma-meta,paper,formal,network,omega,agent} all dispatch cleanly"
