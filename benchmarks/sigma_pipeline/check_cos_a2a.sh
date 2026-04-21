#!/usr/bin/env bash
# NEXT-5 smoke: cos-a2a (product-level A2A CLI).
#
# Verifies:
#   * card prints JSON with our self_id + capabilities
#   * register + list round-trip a peer
#   * request updates σ_trust via EMA (5 high-σ exchanges → blocklist)
#   * state persists across invocations
#   * demo envelope is deterministic (alice trusted, bob blocklisted)
set -euo pipefail
cd "$(dirname "$0")/../.."

[[ -x ./cos-a2a ]] || { echo "missing ./cos-a2a (run 'make cos-a2a')" >&2; exit 2; }

STATE="$(mktemp -t cos_a2a_state.XXXXXX.json)"
rm -f "$STATE"
trap 'rm -f "$STATE"' EXIT
export COS_A2A_STATE="$STATE"

echo "  · cos-a2a card"
C="$(./cos-a2a --self creation-os card)"
grep -q '"creation-os"'    <<<"$C" || { echo "FAIL: self_id missing"          >&2; exit 3; }
grep -q '"qa"'             <<<"$C" || { echo "FAIL: qa capability missing"    >&2; exit 4; }
grep -q '"sigma_gate"'     <<<"$C" || { echo "FAIL: sigma_gate cap missing"   >&2; exit 5; }

echo "  · cos-a2a register alice @ https://alice.example/"
./cos-a2a register alice https://alice.example qa code >/dev/null
./cos-a2a register bob   https://bob.example   forecast news >/dev/null

echo "  · cos-a2a list (after register)"
L1="$(./cos-a2a list)"
grep -q 'peers: 2'         <<<"$L1" || { echo "FAIL: peers count"             >&2; exit 6; }
grep -q 'alice'            <<<"$L1" || { echo "FAIL: alice missing from list" >&2; exit 7; }
grep -q 'bob'              <<<"$L1" || { echo "FAIL: bob missing from list"   >&2; exit 8; }
[[ -s "$STATE" ]]                || { echo "FAIL: state not persisted to $STATE" >&2; exit 9; }

echo "  · cos-a2a request (persist EMA across invocations → blocklist bob)"
# EMA lr=0.20, τ_block=0.90 → ≈10 bob@σ=0.95 exchanges trip the block.
for _ in 1 2 3 4 5 6 7 8 9 10 11; do
    ./cos-a2a request bob forecast 0.95 >/dev/null || true
done

L2="$(./cos-a2a list)"
grep -qE 'bob.*YES' <<<"$L2" || { echo "FAIL: bob should be blocklisted after 5× high-σ"; echo "$L2"; exit 10; }

echo "  · cos-a2a request (alice stays trusted)"
./cos-a2a request alice qa 0.10 >/dev/null
./cos-a2a request alice qa 0.08 >/dev/null
L3="$(./cos-a2a list)"
alice_trust="$(awk '/^ +alice/ { print $(NF-2) }' <<<"$L3")"
awk -v t="$alice_trust" 'BEGIN { exit !(t > 0 && t < 0.5) }' \
    || { echo "FAIL: alice σ_trust=$alice_trust should be in (0, 0.5)"; echo "$L3"; exit 11; }

echo "  · cos-a2a demo (deterministic 5-exchange envelope)"
D="$(./cos-a2a demo)"
grep -q 'alice'                         <<<"$D" || { echo "FAIL: demo alice"       >&2; exit 12; }
grep -q 'bob'                           <<<"$D" || { echo "FAIL: demo bob"         >&2; exit 13; }
grep -qE 'bob.*\[blocklisted\]'         <<<"$D" || { echo "FAIL: demo bob not blocked"; echo "$D"; exit 14; }

echo "check-cos-a2a: PASS (card + register + request + EMA + blocklist + persist)"
