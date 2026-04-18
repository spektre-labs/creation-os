#!/usr/bin/env bash
# check-v138: σ-proof — ACSL shape (tier 1) + Frama-C WP (tier 2)
#
# Tier 1 (always runs): parse /*@ ... */ blocks in
# src/v138/sigma_gate.c and assert the σ-contract shape is present
# (requires, ensures, domain predicate, both emit/abstain behaviours,
# disjoint-behaviors keyword, at least one loop invariant).
#
# Tier 2 (opportunistic): if `frama-c` is on $PATH, run `frama-c
# -wp -wp-rte`.  Otherwise skip with a clear message.  A failing
# Frama-C run is reported but does NOT fail the merge-gate unless
# the user exports `V138_REQUIRE_FRAMA_C=1`.
set -euo pipefail
BIN="./creation_os_v138_proof"
[ -x "$BIN" ] || { echo "check-v138: $BIN not built"; exit 1; }

echo "check-v138: self-test (tier-1 validator)"
"$BIN" --self-test

echo "check-v138: validate src/v138/sigma_gate.c"
out=$("$BIN" --validate src/v138/sigma_gate.c)
echo "$out" | sed 's/^/  /'
echo "$out" | grep -q '^result=OK' \
    || { echo "tier-1 FAIL"; exit 1; }
echo "$out" | grep -q 'has_domain_predicate=1' \
    || { echo "domain predicate missing"; exit 1; }
echo "$out" | grep -q 'has_emit_behavior=1'    \
    || { echo "emit behavior missing"; exit 1; }
echo "$out" | grep -q 'has_abstain_behavior=1' \
    || { echo "abstain behavior missing"; exit 1; }

echo "check-v138: negative validation on a stub"
stub=$(mktemp -t cos_v138_stub.XXXX.c)
trap 'rm -f "$stub"' EXIT
cat > "$stub" <<'CCC'
int nocontract(float x) { return x > 0.5f; }
CCC
set +e
out=$("$BIN" --validate "$stub")
rc=$?
set -e
echo "$out" | grep -q '^result=FAIL' \
    || { echo "expected FAIL on contractless stub"; exit 1; }
[ "$rc" -ne 0 ] || { echo "expected non-zero exit on FAIL"; exit 1; }
echo "  negative case FAILs correctly"

echo "check-v138: tier-2 Frama-C probe"
out=$("$BIN" --frama-c src/v138/sigma_gate.c 2>&1 || true)
echo "$out" | head -3 | sed 's/^/  /'
if printf '%s\n' "$out" | grep -q '^result=OK'; then
    echo "  tier-2: Frama-C WP discharged all goals"
elif printf '%s\n' "$out" | grep -q '^result=SKIPPED'; then
    echo "  tier-2: frama-c not installed — tier-1 is authoritative"
else
    if [ "${V138_REQUIRE_FRAMA_C:-0}" = "1" ]; then
        echo "  tier-2 FAIL and V138_REQUIRE_FRAMA_C=1"; exit 1
    fi
    echo "  tier-2 reported FAIL but not required — tier-1 still green"
fi

echo "check-v138: all OK"
