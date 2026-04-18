#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v124 σ-Continual merge-gate smoke.
#
# Proves the *policy + buffer + forgetting-smoke* state machine end-
# to-end without MLX or weights: trigger semantics, batch selection
# counts, a 10-epoch healthy run (no rollback), and a 10-epoch
# pathological run that drops 4 % on epoch 10 (rollback expected).
# v124.1 replaces the synthetic baseline with a real 50-question
# v106 eval.

set -euo pipefail
BIN=./creation_os_v124_continual
test -x "$BIN" || { echo "missing $BIN — run 'make creation_os_v124_continual' first"; exit 1; }

echo "check-v124: self-test"
"$BIN" --self-test

echo "check-v124: idle-trigger policy table"
test "$("$BIN" --trigger 30 20 1)"  = "skip"
test "$("$BIN" --trigger 90  2 1)"  = "skip"
test "$("$BIN" --trigger 90 20 1)"  = "train"
test "$("$BIN" --trigger 90 20 10)" = "smoke"
test "$("$BIN" --trigger 90 20 20)" = "smoke"
test "$("$BIN" --trigger 90 20 11)" = "train"

echo "check-v124: 10 healthy epochs — no rollback"
HEALTHY=$("$BIN" --epoch-healthy 10)
echo "  $HEALTHY"
echo "$HEALTHY" | grep -q '"epochs_completed":10'
echo "$HEALTHY" | grep -q '"epochs_rolled_back":0'
echo "$HEALTHY" | grep -q '"epochs_with_smoke":1'
echo "$HEALTHY" | grep -q '"active_adapter_version":10'

echo "check-v124: 10 catastrophic epochs — rollback on epoch 10"
CATA=$("$BIN" --epoch-catastrophic 10)
echo "  $CATA"
echo "$CATA" | grep -q '"epochs_completed":10'
echo "$CATA" | grep -q '"epochs_rolled_back":1'
echo "$CATA" | grep -q '"epochs_with_smoke":1'
# active_adapter stays at 9 after rollback
echo "$CATA" | grep -q '"active_adapter_version":9'
# baseline accuracy was pulled back from 0.92 to 0.88 before the rollback
echo "$CATA" | grep -q '"last_baseline_accuracy":0.8800'

echo "check-v124: OK (σ-buffer + idle-trigger + healthy + catastrophic + rollback)"
