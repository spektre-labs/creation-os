#!/usr/bin/env bash
#
# v275 σ-TTT — merge-gate check (σ-gated updates + dual-track drift + σ-evict window).
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v275_ttt"
[ -x "$BIN" ] || { echo "v275: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v275", d
assert abs(d["tau_update"] - 0.30) < 1e-5, d
assert abs(d["tau_sync"] - 0.15) < 1e-5, d
assert abs(d["tau_reset"] - 0.50) < 1e-5, d

C = d["citations"]
assert C == ["v124_sigma_continual", "ttt_e2e_2025"], C

U = d["updates"]
assert len(U) == 4
n_learn = sum(1 for u in U if u["decision"] == "LEARN")
n_skip = sum(1 for u in U if u["decision"] == "SKIP")
assert n_learn == 2 and n_skip == 2, U
assert all(u["decision_ok"] for u in U), U
assert d["update_branches_ok"] is True, d

Dr = d["drift"]
assert len(Dr) == 3
states = {x["state"] for x in Dr}
assert states == {"SYNCED", "DIVERGING", "RESET"}, Dr
assert all(x["state_ok"] for x in Dr), Dr
assert d["drift_branches_ok"] is True, d

W = d["window"]
assert len(W) == 6
ranks = sorted(t["evict_rank"] for t in W)
assert ranks == [1, 2, 3, 4, 5, 6], W
assert all(t["rank_ok"] for t in W), W
assert d["window_permutation_ok"] is True, d

assert d["passing"] == d["denominator"] == 19, d
assert d["manifest_closed"] is True, d
assert abs(d["sigma_ttt"]) < 1e-5, d
assert d["chain_hash"].startswith("0x"), d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v275: non-deterministic JSON" >&2; exit 1; }
