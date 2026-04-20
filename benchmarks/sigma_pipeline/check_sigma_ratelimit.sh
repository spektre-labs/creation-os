#!/usr/bin/env bash
#
# PROD-2 smoke test: σ-RateLimit 4-scenario pinning.
#
# Canonical config: 5 qpm / 60 qph, r_ban=0.10, r_good=0.80,
# give_min=0.10, σ_reject_peer=0.70.  Four peers exercise the
# full decision matrix (ALLOWED / THROTTLED_RATE / THROTTLED_GIVE
# / BLOCKED).
#
# Pinned facts:
#   - self_test_rc == 0
#   - alice  : reputation 1.000, 8/8 ALLOWED, 0 throttles, 0 blocks
#   - spammer: allowed=5, rate=3, blocked=2 (burst → rate-throttle → ban)
#   - griefer: reputation 0.000, blocked=3 (high-σ answers banned peer)
#   - leech  : give=2, blocked=0 (allowed 10, then give-ratio throttle)
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_ratelimit"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$($BIN)"

for fact in \
    '"self_test_rc":0' \
    '"pass":true' \
    '"max_per_minute":5' \
    '"max_per_hour":60' \
    '"r_ban":0.10' \
    '"r_good":0.80' \
    '"give_min":0.10' \
    '"sigma_reject_peer":0.70'
do
    grep -q -F "$fact" <<<"$OUT" \
        || { echo "FAIL: missing '$fact'" >&2; echo "$OUT"; exit 3; }
done

python3 - "$OUT" <<'PY'
import json, sys, re
j = json.loads(sys.argv[1])
assert j["self_test_rc"] == 0, j
peers = {p["id"].strip(): p for p in j["peers"]}

# alice: trusted, everything allowed
a = peers["alice"]
assert a["allowed"] == 8, a
assert a["blocked"] == 0, a
assert a["rate"] == 0, a
assert a["give"] == 0, a
assert a["reputation"] >= 0.80, a

# spammer: burst → rate throttle then ban
s = peers["spammer"]
assert s["allowed"] == 5, s
assert s["rate"] >= 1, s
assert s["blocked"] >= 1, s

# griefer: all queries blocked
g = peers["griefer"]
assert g["blocked"] == 3, g
assert g["reputation"] == 0.0, g

# leech: at least one give-ratio throttle, zero blocks
l = peers["leech"]
assert l["give"] >= 1, l
assert l["blocked"] == 0, l
assert l["allowed"] >= 10, l

print("check-sigma-ratelimit: PASS (ALLOWED/THROTTLED_RATE/THROTTLED_GIVE/BLOCKED all reproduced)")
PY
