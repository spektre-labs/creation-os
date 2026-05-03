#!/usr/bin/env bash
#
# OMEGA-3 / CLOSE-1 smoke test: σ-formal-complete ledger coherence.
#
# Honest invariants (no toolchain runs here — we parse the
# artefacts and cross-check the banner):
#   self_test == 0
#   lean.n_theorems >= 14      (v259 + formal/lean/CreationOS/V133.lean)
#   lean.discharged_concrete >= 14
#   lean.pending == 0          (zero `sorry` in scanned Lean files)
#   acsl.requires + acsl.ensures >= 30 (v259 companion + v133 stack stub)
#   ledger.header_proofs == 14
#   ledger.header_proofs_total == 14
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
assert lean["n_theorems"] >= 14, lean
assert lean["discharged_concrete"] >= 14, lean
assert lean["pending"] == 0, lean
assert lean["discharged_concrete"] + lean["discharged_abstract"] + lean["pending"] == lean["n_theorems"], lean

# Post-CLOSE-1: the canonical T1..T6 theorems must be present and
# discharged (non-pending).  Any regression that re-introduces a
# `sorry` on these would flip the kind to "pending" and fail the
# pending == 0 invariant above.
expected_theorems = {
    "gate_totality",
    "gate_monotone_in_sigma",
    "gate_anti_monotone_in_tau_a",
    "gate_anti_monotone_in_tau_r",
    "gate_boundary_at_tau_a",
    "gate_boundary_at_tau_r",
}
present = {t["name"] for t in lean["entries"]}
missing = expected_theorems - present
assert not missing, f"missing T1..T6 theorems: {missing}"

expected_v133 = {
    "engram_stores_only_accept",
    "abstain_does_not_propagate",
    "cascade_monotone",
    "circuit_breaker_trips",
    "proconductor_overrides_all",
    "kv_eviction_removes_highest_sigma",
    "speculative_skip_safe",
    "staleness_increases_sigma",
}
missing133 = expected_v133 - present
assert not missing133, f"missing v133 stack theorems: {missing133}"

acsl = doc["acsl"]
assert acsl["requires"] + acsl["ensures"] >= 30, acsl
assert acsl["file_bytes"] > 1000, acsl

ledger = doc["ledger"]
assert ledger["header_proofs"] == 14, ledger
assert ledger["header_proofs_total"] == 14, ledger
assert ledger["ledger_matches_truth"] is True, ledger
assert 0 <= ledger["header_proofs"] <= ledger["effective_discharged"], ledger

print("check_sigma_formal_complete: PASS", {
    "theorems":            lean["n_theorems"],
    "concrete":            lean["discharged_concrete"],
    "abstract":            lean["discharged_abstract"],
    "pending_sorry":       lean["pending"],
    "banner":              f'{ledger["header_proofs"]}/{ledger["header_proofs_total"]}',
})
PY
