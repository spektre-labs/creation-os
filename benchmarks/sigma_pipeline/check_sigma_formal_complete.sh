#!/usr/bin/env bash
#
# OMEGA-3 smoke test: σ-formal-complete ledger coherence.
#
# Honest invariants (no toolchain runs here — we parse the
# artefacts and cross-check the banner):
#   self_test == 0
#   lean.n_theorems >= 12      (v259 Measurement.lean is scaffolded)
#   lean.discharged_concrete >= 2
#   lean.discharged_abstract >= 3   (T2α / T3α / T4α / T5α family)
#   lean.pending >= 3          (Float-specific theorems still carry sorry
#                              until a Lean4 + Mathlib Float story or Frama-C
#                              Wp discharges them — this is the honest state)
#   acsl.requires >= 3
#   acsl.ensures  >= 5
#   ledger.header_proofs_total == 6
#   ledger.ledger_matches_truth == true
#   ledger.header_proofs <= ledger.effective_discharged
#   Two invocations produce byte-identical output.
#
# The point of this check is that ANY change to the Lean file that
# removes a `sorry` lets us legitimately bump COS_FORMAL_PROOFS in
# include/cos_version.h; ANY change that adds a `sorry` forces the
# banner to retreat.  The ledger cannot drift from the artefacts.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_formal_complete"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_formal_complete: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_formal_complete", doc
assert doc["self_test"] == 0, doc
assert doc["scan_rc"] == 0, doc

lean = doc["lean"]
assert lean["n_theorems"] >= 12, lean
assert lean["discharged_concrete"] >= 2, lean
assert lean["discharged_abstract"] >= 3, lean
assert lean["pending"] >= 3, lean
assert lean["discharged_concrete"] + lean["discharged_abstract"] + lean["pending"] == lean["n_theorems"], lean

# Make sure the α-family (LinearOrder lifts) is present.
alpha_names = [t["name"] for t in lean["entries"] if t["abstract"]]
assert any("monotone_in_sigma" in n for n in alpha_names), alpha_names
assert any("anti_monotone_in_tau" in n for n in alpha_names), alpha_names
assert any("boundary_tiebreak" in n for n in alpha_names), alpha_names

# The Float-specific (non-α) theorems still carry `sorry` — that
# is the honest current state.  If any of these flip to
# discharged_concrete, COS_FORMAL_PROOFS must bump accordingly.
pending_names = [t["name"] for t in lean["entries"] if t["kind"] == "pending"]
for expected in ("gate_totality", "gate_monotone_in_sigma",
                 "gate_anti_monotone_in_tau", "gate_boundary_tiebreak"):
    assert expected in pending_names, (expected, pending_names)

acsl = doc["acsl"]
assert acsl["requires"] >= 3, acsl
assert acsl["ensures"]  >= 5, acsl
assert acsl["file_bytes"] > 1000, acsl

ledger = doc["ledger"]
assert ledger["header_proofs_total"] == 6, ledger
assert ledger["ledger_matches_truth"] is True, ledger
assert 0 <= ledger["header_proofs"] <= ledger["effective_discharged"], ledger

print("check_sigma_formal_complete: PASS", {
    "theorems":            lean["n_theorems"],
    "concrete":            lean["discharged_concrete"],
    "abstract":            lean["discharged_abstract"],
    "pending_float_sorry": lean["pending"],
    "banner":              f'{ledger["header_proofs"]}/{ledger["header_proofs_total"]}',
})
PY
