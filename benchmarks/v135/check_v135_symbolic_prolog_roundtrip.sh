#!/usr/bin/env bash
# check-v135: σ-symbolic — Prolog-style KB + hybrid reasoner
#
# Verifies:
#   1. self-test passes (KB assert + variable query + consistency)
#   2. CLI --demo returns X=lauri and CONTRADICT for the openai claim
#   3. CLI --check refuses a contradiction on a functional predicate
#   4. Rule input (":-") is rejected (v135.0 does ground facts only)
set -euo pipefail
BIN="./creation_os_v135_symbolic"
[ -x "$BIN" ] || { echo "check-v135: $BIN not built"; exit 1; }

echo "check-v135: self-test"
"$BIN" --self-test

echo "check-v135: demo query round-trip"
out=$("$BIN" --demo)
echo "$out" | grep -q "X = lauri" || { echo "missing X=lauri"; exit 1; }
echo "$out" | grep -q "CONTRADICT" || { echo "missing CONTRADICT"; exit 1; }
echo "  demo OK"

echo "check-v135: --check CONTRADICT on functional predicate"
out=$("$BIN" --check lauri works_at openai \
    --fact 'works_at(lauri, spektre_labs)' \
    --functional works_at)
echo "$out" | grep -q "verdict=CONTRADICT" || { echo "expected CONTRADICT"; exit 1; }

echo "check-v135: --check UNKNOWN on unrelated predicate"
out=$("$BIN" --check alice likes pizza \
    --fact 'works_at(lauri, spektre_labs)')
echo "$out" | grep -q "verdict=UNKNOWN" || { echo "expected UNKNOWN"; exit 1; }

echo "check-v135: rule rejection (v135.0 is ground-fact only)"
out=$("$BIN" --query '?- foo(X).' --fact 'bar(X,Y) :- baz(X,Y)' 2>&1 || true)
echo "$out" | grep -q "rules (:-)" || { echo "rule rejection message missing"; exit 1; }

echo "check-v135: all OK"
