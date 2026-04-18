#!/usr/bin/env bash
#
# v191 σ-Constitutional — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 7 axioms present
#   * every flaw-free output (FLAW_NONE) is ACCEPTED with all
#     7 axioms passing
#   * every flawed output is REVISED or ABSTAINED (never ACCEPT)
#   * specifically ≥ 1 firmware-disclaimer output is rejected
#   * hash chain verifies (chain_valid == true)
#   * specs/constitution.toml exists and declares 7 axioms
#   * byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v191_constitution"
[ -x "$BIN" ] || { echo "v191: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

if [ ! -f specs/constitution.toml ]; then
    echo "v191: specs/constitution.toml missing" >&2
    exit 1
fi
N=$(grep -c '^\[\[axiom\]\]' specs/constitution.toml || true)
if [ "$N" != "7" ]; then
    echo "v191: specs/constitution.toml has $N axioms, expected 7" >&2
    exit 1
fi

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]    == "v191", d
assert d["n_axioms"]  == 7,      d
assert d["n_samples"] == 24,     d
assert d["chain_valid"] is True, d

# Must reject ≥ 1 firmware disclaimer.
assert d["n_firmware_rejected"] >= 1, d
# Must accept ≥ 1 fully-safe output.
assert d["n_safe_accepted"]     >= 1, d

# Partition consistency.
assert d["n_accept"] + d["n_revise"] + d["n_abstain"] == d["n_samples"]

for p in d["samples"]:
    if p["flaw"] == 0:
        assert p["n_axioms_passed"] == 7, p
        assert p["decision"] == 0,         p  # ACCEPT
    else:
        assert p["decision"] != 0, p           # NOT ACCEPT
        assert p["n_axioms_passed"] < 7, p

# Axioms list sanity.
names = [a["name"] for a in d["axioms"]]
assert "no_firmware" in names
assert "sigma_honesty" in names
assert "1_equals_1" in names
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v191: non-deterministic output" >&2
    exit 1
fi
