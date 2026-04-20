#!/usr/bin/env bash
#
# Smoke test for cos network (D6): exercises every subcommand
# against the canonical bootstrapped state and pins the JSON
# receipts.

set -euo pipefail

BIN=${1:-./cos-network}
if [ ! -x "$BIN" ]; then
  echo "check-cos-network: missing binary $BIN" >&2
  exit 2
fi

# join --------------------------------------------------------
OUT=$("$BIN" join --json)
echo "[join] $OUT"
grep -q '"command":"join"'                                <<<"$OUT" || { echo "FAIL join command"; exit 1; }
grep -q '"peers":4,"providers":3'                         <<<"$OUT" || { echo "FAIL join counts"; exit 1; }
grep -q '"self":"self","status":"joined"'                 <<<"$OUT" || { echo "FAIL join status"; exit 1; }

# list --------------------------------------------------------
OUT=$("$BIN" list --json)
echo "[list] $OUT"
grep -q '"id":"self","address":"127.0.0.1:7000","sigma":0.30'    <<<"$OUT" || { echo "FAIL list self"; exit 1; }
grep -q '"id":"strong","address":"10.0.0.2:7000","sigma":0.10'   <<<"$OUT" || { echo "FAIL list strong"; exit 1; }
grep -q '"id":"slow","address":"10.0.0.3:7000","sigma":0.15'     <<<"$OUT" || { echo "FAIL list slow"; exit 1; }
grep -q '"id":"bad","address":"10.0.0.4:7000","sigma":0.90'      <<<"$OUT" || { echo "FAIL list bad"; exit 1; }

# status ------------------------------------------------------
OUT=$("$BIN" status --json)
echo "[status] $OUT"
# Mean σ across three available non-self peers (strong 0.10, slow 0.15,
# bad 0.90 — note status does not ban bad, just reports) after EWMA
# updates on strong (→ 0.09) and slow (→ 0.16).  Mean = 0.383...
grep -q '"mean_peer_sigma":0.3833'                        <<<"$OUT" || { echo "FAIL mean sigma"; exit 1; }
grep -q '"peers_available":3'                             <<<"$OUT" || { echo "FAIL peers available"; exit 1; }
grep -q '"total_spend_eur":0.0200'                        <<<"$OUT" || { echo "FAIL total spend"; exit 1; }
grep -q '"abstain_floor":0.80,"ban_floor":0.80'           <<<"$OUT" || { echo "FAIL floors"; exit 1; }

# serve -------------------------------------------------------
OUT=$("$BIN" serve --price 0.005 --json)
echo "[serve] $OUT"
grep -q '"command":"serve","price_eur":0.0050'            <<<"$OUT" || { echo "FAIL serve price"; exit 1; }
grep -q '"provider_id":"self","scsl_accepted":true'       <<<"$OUT" || { echo "FAIL serve scsl"; exit 1; }
grep -q '"rc":0,"providers":4'                            <<<"$OUT" || { echo "FAIL serve providers"; exit 1; }

# query -------------------------------------------------------
OUT=$("$BIN" query "what is life" --json)
echo "[query] $OUT"
grep -q '"command":"query","question":"what is life"'     <<<"$OUT" || { echo "FAIL query echo"; exit 1; }
# Local σ 0.90 → mesh route wins; "strong" peer has σ=0.10.
grep -q '"route":"mesh","peer":"strong"'                  <<<"$OUT" || { echo "FAIL query route"; exit 1; }
grep -q '"expected_sigma":0.1000,"price_eur":0.0010'      <<<"$OUT" || { echo "FAIL query price"; exit 1; }

# federate ----------------------------------------------------
OUT=$("$BIN" federate --json)
echo "[federate] $OUT"
grep -q '"command":"federate","updates":3,"accepted":3'   <<<"$OUT" || { echo "FAIL federate counts"; exit 1; }
grep -q '"rejected_sigma":0,"rejected_poison":0'          <<<"$OUT" || { echo "FAIL federate rejects"; exit 1; }
grep -q '"total_weight":377.50'                           <<<"$OUT" || { echo "FAIL federate weight"; exit 1; }
grep -q '"delta":\[0.1253,0.0626,0.0289\]'                <<<"$OUT" || { echo "FAIL federate delta"; exit 1; }
grep -q '"delta_l2":0.1430'                               <<<"$OUT" || { echo "FAIL federate l2"; exit 1; }

# unlearn -----------------------------------------------------
OUT=$("$BIN" unlearn "my-email@example.com" --json)
echo "[unlearn] $OUT"
grep -q '"command":"unlearn","data":"my-email@example.com"' <<<"$OUT" || { echo "FAIL unlearn echo"; exit 1; }
# Frame = 56 header + 20 payload + 64 sig = 140.
grep -q '"encode_rc":0,"frame_bytes":140'                 <<<"$OUT" || { echo "FAIL unlearn frame"; exit 1; }
grep -q '"type":"UNLEARN","decode_rc":0,"verified":true'  <<<"$OUT" || { echo "FAIL unlearn verify"; exit 1; }

# usage guardrail ---------------------------------------------
set +e
OUT=$("$BIN" 2>&1)
RC=$?
set -e
if [ "$RC" -ne 2 ]; then
  echo "FAIL usage rc expected 2 got $RC"
  exit 1
fi

echo "check-cos-network: join/list/status/serve/query/federate/unlearn all green"
