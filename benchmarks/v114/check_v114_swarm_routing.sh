#!/usr/bin/env bash
# v114 σ-Swarm — routing smoke check.
#
# Verifies that:
#   1. The pure-C arbitrator self-test passes.
#   2. The TOML [swarm] section parses into 3 specialists.
#   3. A code-leaning prompt routes to the `code` specialist.
#   4. A math-leaning prompt routes to the `reasoning` specialist.
#   5. Response headers include X-COS-Specialist, -Sigma, and
#      -Consensus (values unchecked — just the presence/contract).
set -u
cd "$(dirname "$0")/../.."

BIN=./creation_os_v114_swarm
if [ ! -x "$BIN" ]; then
    echo "[check-v114] building creation_os_v114_swarm"
    if ! make -s creation_os_v114_swarm 2>/dev/null; then
        echo "[check-v114] SKIP: toolchain missing"
        exit 0
    fi
fi

CFG=benchmarks/v114/swarm.toml
[ -f "$CFG" ] || { echo "[check-v114] missing $CFG"; exit 1; }

echo "[check-v114] 1/5 self-test"
"$BIN" --self-test || { echo "[check-v114] self-test failed"; exit 1; }

echo "[check-v114] 2/5 parse config"
"$BIN" --parse-config "$CFG" | grep -q 'specialists=3' \
    || { echo "[check-v114] config parse missing 3 specialists"; exit 1; }

echo "[check-v114] 3/5 code prompt → code specialist wins"
OUT="$("$BIN" --run "$CFG" 'write a python program to print the first 10 primes')"
echo "$OUT" | grep -q '^X-COS-Specialist: code' \
    || { echo "[check-v114] code prompt did not route to code"; echo "$OUT"; exit 1; }

echo "[check-v114] 4/5 math prompt → reasoning specialist wins"
OUT="$("$BIN" --run "$CFG" 'use logic to reason about this math puzzle: two trains…')"
echo "$OUT" | grep -q '^X-COS-Specialist: reasoning' \
    || { echo "[check-v114] math prompt did not route to reasoning"; echo "$OUT"; exit 1; }

echo "[check-v114] 5/5 header contract (Consensus + Sigma fields)"
echo "$OUT" | grep -q '^X-COS-Specialist-Sigma:'    || { echo "missing sigma header";   exit 1; }
echo "$OUT" | grep -q '^X-COS-Runner-Up:'           || { echo "missing runner-up";      exit 1; }
echo "$OUT" | grep -q '^X-COS-Consensus: '          || { echo "missing consensus";      exit 1; }
echo "$OUT" | grep -q '^X-COS-Parallel-Mode: sequential' \
    || { echo "parallel-mode honesty missing"; exit 1; }
echo "$OUT" | grep -q '"parallel_mode":"sequential"' \
    || { echo "body parallel_mode missing"; exit 1; }

echo "[check-v114] OK"
