#!/usr/bin/env bash
#
# v283 σ-Constitutional — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 mechanism rows canonical (rlhf,
#     constitutional_ai, sigma_constitutional);
#     sigma_constitutional is the ONLY row with
#     uses_sigma=true AND uses_rl=false AND
#     uses_reward_model=false; all 3 distinct
#   * 8 σ-channels canonical order (entropy,
#     repetition, calibration, attention, logit,
#     hidden, output, aggregate); all enabled; all
#     distinct
#   * 4 firmware rows canonical (care_as_control,
#     sycophancy, opinion_laundering,
#     people_pleasing); rlhf_produces=true AND
#     sigma_produces=false for EVERY row
#   * 2 self-critique rows canonical (single_instance
#     NOT Gödel-safe, two_instance IS Gödel-safe);
#     both have has_producer=true
#   * sigma_constitutional in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v283_constitutional"
[ -x "$BIN" ] || { echo "v283: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v283", d
assert d["chain_valid"] is True, d

M = d["mech"]
assert len(M) == 3
want_names = ["rlhf", "constitutional_ai", "sigma_constitutional"]
assert [m["name"] for m in M] == want_names, M
assert (M[0]["uses_reward_model"], M[0]["uses_rl"],
        M[0]["uses_principles"], M[0]["uses_sigma"]) == (True, True, False, False)
assert (M[1]["uses_reward_model"], M[1]["uses_rl"],
        M[1]["uses_principles"], M[1]["uses_sigma"]) == (False, True, True, False)
assert (M[2]["uses_reward_model"], M[2]["uses_rl"],
        M[2]["uses_principles"], M[2]["uses_sigma"]) == (False, False, False, True)
sigma_only = [m for m in M if m["uses_sigma"] and not m["uses_rl"]
              and not m["uses_reward_model"]]
assert len(sigma_only) == 1 and sigma_only[0]["name"] == "sigma_constitutional"
assert len({m["name"] for m in M}) == 3, M
assert d["n_mech_rows_ok"]    == 3, d
assert d["mech_canonical_ok"] is True, d
assert d["mech_distinct_ok"]  is True, d

C = d["channel"]
want_ch = ["entropy", "repetition", "calibration", "attention",
           "logit", "hidden", "output", "aggregate"]
assert [c["name"] for c in C] == want_ch, C
for c in C: assert c["enabled"] is True, c
assert len({c["name"] for c in C}) == 8, C
assert d["n_channel_rows_ok"]    == 8, d
assert d["channel_distinct_ok"]  is True, d

F = d["firmware"]
want_fw = ["care_as_control", "sycophancy",
           "opinion_laundering", "people_pleasing"]
assert [f["name"] for f in F] == want_fw, F
for f in F:
    assert f["rlhf_produces"]  is True,  f
    assert f["sigma_produces"] is False, f
assert d["n_firmware_rows_ok"]     == 4, d
assert d["firmware_rlhf_yes_ok"]   is True, d
assert d["firmware_sigma_no_ok"]   is True, d

Q = d["critique"]
assert [q["label"] for q in Q] == ["single_instance", "two_instance"], Q
assert Q[0]["has_producer"] is True and Q[0]["has_measurer"] is False
assert Q[0]["goedel_safe"]  is False
assert Q[1]["has_producer"] is True and Q[1]["has_measurer"] is True
assert Q[1]["goedel_safe"]  is True
assert d["n_critique_rows_ok"]  == 2, d
assert d["critique_goedel_ok"]  is True, d

assert 0.0 <= d["sigma_constitutional"] <= 1.0, d
assert d["sigma_constitutional"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v283: non-deterministic" >&2; exit 1; }
