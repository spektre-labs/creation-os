#!/usr/bin/env bash
# End-to-end σ-pipeline smoke test (I1).
#
# Runs the three canonical turns (low:2+2 twice + mid:prove) and
# pins every field of the JSON summary that downstream tooling
# (integration tests, cos chat, cos benchmark) relies on.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_pipeline
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Codex loaded with the canonical shape.
grep -q '"codex":{"loaded":true,'   <<<"$OUT" || { echo "codex not loaded" >&2; exit 5; }
grep -qE '"chapters":(3[3-9]|[4-9][0-9]|[0-9]{3,})' \
    <<<"$OUT" || { echo "chapters < 33" >&2; exit 6; }

# Turn 1: low:2+2 → ACCEPT, σ=0.05, no engram hit, no rethink,
# local-only cost ~€0.0001.
grep -q '"turn":1,"action":"ACCEPT","sigma":0.0500' <<<"$OUT" || { echo "T1 action"   >&2; exit 7; }
grep -q '"turn":1,[^}]*"engram_hit":false'          <<<"$OUT" || { echo "T1 hit"      >&2; exit 8; }
grep -q '"turn":1,[^}]*"rethink":0'                 <<<"$OUT" || { echo "T1 rethink"  >&2; exit 9; }
grep -q '"turn":1,[^}]*"escalated":false'           <<<"$OUT" || { echo "T1 escal"    >&2; exit 10; }

# Turn 2: same prompt → engram HIT, zero cost.
grep -q '"turn":2,"action":"ACCEPT"'                <<<"$OUT" || { echo "T2 action"   >&2; exit 11; }
grep -q '"turn":2,[^}]*"engram_hit":true'           <<<"$OUT" || { echo "T2 hit"      >&2; exit 12; }
grep -q '"turn":2,[^}]*"cost_eur":0.000000'         <<<"$OUT" || { echo "T2 cost"     >&2; exit 13; }
grep -q '"turn":2,[^}]*"diag":"engram-hit"'         <<<"$OUT" || { echo "T2 diag"     >&2; exit 14; }

# Turn 3: plateau σ=0.55 → rethinks exhausted → escalated.
# Post-escalation σ reported is the cloud result (0.10).
grep -q '"turn":3,[^}]*"escalated":true'            <<<"$OUT" || { echo "T3 escal"    >&2; exit 15; }
grep -q '"turn":3,[^}]*"rethink":2'                 <<<"$OUT" || { echo "T3 rethink"  >&2; exit 16; }
grep -q '"turn":3,[^}]*"diag":"escalated"'          <<<"$OUT" || { echo "T3 diag"     >&2; exit 17; }
grep -q '"turn":3,[^}]*"sigma":0.1000'              <<<"$OUT" || { echo "T3 sigma"    >&2; exit 18; }

# Sovereign ledger: 1 cloud call, ≥ 4 local, no abstains (escalation
# counts as cloud, not abstain).
grep -q '"n_cloud":1'                               <<<"$OUT" || { echo "n_cloud"     >&2; exit 19; }
grep -q '"n_abstain":0'                             <<<"$OUT" || { echo "n_abstain"   >&2; exit 20; }
grep -qE '"n_local":[4-9]'                          <<<"$OUT" || { echo "n_local"     >&2; exit 21; }

echo "check-sigma-pipeline-compose: PASS (1 ACCEPT + 1 engram HIT + 1 escalate; ledger sane)"
