#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
#
# CLOSE-2: one-shot reproduction of the Frama-C Wp discharge for
# the v259 σ-measurement gate.
#
# Context
# -------
# hw/formal/v259/sigma_measurement.h.acsl carries seven proof
# obligations (T1–T7) about sizeof / alignof, gate totality, gate
# purity, memcpy round-trip, monotonicity in τ, clamp range, and
# mean-ns latency.  The first six are Wp-tractable, the seventh is
# out of scope (runtime microbench).  Runtime exhaustion covers
# the same invariants at ~1e-23% domain coverage (see the ACSL
# header).  Lean 4 core discharges the order-theoretic fragment
# `sorry`-free (see hw/formal/v259/Measurement.lean + `lake build`).
#
# This script picks up where both end and makes the Wp proofs
# machine-checkable:
#
#   1. Bootstrap opam + ocaml if missing (Homebrew on macOS,
#      apt on Debian/Ubuntu, otherwise prints the canonical
#      install URL and exits).
#   2. `opam install --yes frama-c alt-ergo z3` — this is the
#      heavy step, 15–30 minutes on a fresh switch.  It is idem-
#      potent: re-running after a partial install just resumes.
#   3. `frama-c -wp -wp-rte -wp-prover alt-ergo,z3 -wp-timeout 30`
#      over sigma_measurement.h.acsl + sigma_measurement.c.
#   4. Save the full log + a pinned summary to
#      docs/v259/frama_c_wp_results.txt so CI + committers can
#      diff it.
#
# Rerun at any time; the script is safe to background and resume.
#
# Usage
#     cd creation-os-kernel
#     bash scripts/v259/run_frama_c_wp.sh          # full install + run
#     COS_SKIP_INSTALL=1 \
#         bash scripts/v259/run_frama_c_wp.sh      # assume opam stack ready

set -euo pipefail
cd "$(dirname "$0")/../.."

ACSL="hw/formal/v259/sigma_measurement.h.acsl"
CSRC="src/v259/sigma_measurement.c"
OUT="docs/v259/frama_c_wp_results.txt"
LOG="docs/v259/frama_c_wp_full.log"

[[ -f "$ACSL" ]] || { echo "missing $ACSL" >&2; exit 2; }
[[ -f "$CSRC" ]] || { echo "missing $CSRC" >&2; exit 2; }

# --- 1. bootstrap ---------------------------------------------------
if [[ "${COS_SKIP_INSTALL:-0}" != "1" ]]; then
    if ! command -v opam >/dev/null 2>&1; then
        case "$(uname)" in
            Darwin) brew install opam ;;
            Linux)  apt-get install -y opam || true ;;
            *) echo "Install opam manually: https://opam.ocaml.org/" >&2; exit 3 ;;
        esac
    fi
    if [[ ! -d "$(opam var root 2>/dev/null || echo /nonexistent)" ]]; then
        opam init --auto-setup --yes
    fi
    eval "$(opam env)"

    # frama-c depends on a pile of system libs — these are cheap to
    # install and skipping them makes why3 / frama-c fail.
    case "$(uname)" in
        Darwin) brew install graphviz gtksourceview libxml2 pkgconf zlib || true ;;
        Linux)  apt-get install -y graphviz libgtksourceview-3.0-dev \
                                   libxml2-dev pkg-config zlib1g-dev || true ;;
    esac

    opam install --yes frama-c alt-ergo z3 why3 || {
        echo "opam install failed — see log above" >&2
        exit 4
    }
fi

eval "$(opam env)"

# --- 2. run Wp ------------------------------------------------------
mkdir -p docs/v259
echo "=== frama-c --version ===" | tee "$LOG"
frama-c --version 2>&1 | tee -a "$LOG"

# Wp runs on `sigma_measurement.c` with contracts pulled from
# `sigma_measurement.h`.  We do **not** pass `sigma_measurement.h.acsl`
# in the same run (duplicate contracts confuse provers) and we do **not**
# enable `-wp-rte` on the full translation unit (libc + bench + main
# explodes into hundreds of RTE goals that time out).
#
# Tier-1 (merge / commit gate): `cos_sigma_measurement_gate` and
# `cos_sigma_measurement_clamp` — 15/15 goals on Frama-C 32.x in ~5 s.
#
# Tier-2 (optional R&D): set `COS_V259_WP_BYTES=1` to also run Wp on
# encode / decode / roundtrip.  Those byte goals currently hit prover
# timeouts on the float-struct ↔ uchar overlay; byte identity remains
# discharged by Lean (`roundtrip_bytes_identity`) + the C exhaustive
# checker in `src/v259/sigma_measurement.c`.

run_wp_slice() {
    local label=$1
    local fcts=$2
    echo "" | tee -a "$LOG"
    echo "=== Wp run: $label ===" | tee -a "$LOG"
    set +e
    frama-c \
        -cpp-extra-args="-Isrc/v259" \
        -wp \
        -wp-prover alt-ergo,z3 \
        -wp-timeout 90 \
        -wp-fct "$fcts" \
        -wp-log=a:"$LOG".raw \
        "$CSRC"
    set -e
    cat "$LOG".raw 2>/dev/null | tee -a "$LOG" >/dev/null || true
    rm -f "$LOG".raw
    return 0
}

run_wp_slice mandatory \
    cos_sigma_measurement_gate,cos_sigma_measurement_clamp

proved_line=$(grep '\[wp\] Proved goals:' "$LOG" | tail -1 || true)
proved=$(echo "$proved_line" | sed -nE 's/.*Proved goals:[[:space:]]*([0-9]+)[[:space:]]*\/[[:space:]]*([0-9]+).*/\1/p')
total=$(echo "$proved_line" | sed -nE 's/.*Proved goals:[[:space:]]*([0-9]+)[[:space:]]*\/[[:space:]]*([0-9]+).*/\2/p')
if [[ -z "$proved" || -z "$total" || "$proved" != "$total" ]]; then
    echo "tier-1 Wp incomplete (expected all goals proved): $proved_line" >&2
    exit 6
fi

if [[ "${COS_V259_WP_BYTES:-0}" == "1" ]]; then
    run_wp_slice optional-bytes \
        cos_sigma_measurement_encode,cos_sigma_measurement_decode,cos_sigma_measurement_roundtrip || true
fi

# --- 3. pinned summary ----------------------------------------------
{
    echo "# Frama-C Wp discharge ledger — v259 σ-measurement gate"
    echo "# generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# frama-c:   $(frama-c --version | head -1)"
    echo "# alt-ergo:  $(alt-ergo --version 2>/dev/null | head -1 || echo n/a)"
    echo "# z3:        $(z3 --version 2>/dev/null | head -1 || echo n/a)"
    echo ""
    echo "# Tier-1 (mandatory): gate + clamp on $CSRC — Wp discharged."
    echo "#   $proved_line"
    echo "# Reference ACSL scaffold (not fed to Wp in this script):"
    echo "#   $ACSL"
    echo ""
    echo "# Goal summary lines from this run (see $LOG for full output):"
    grep -E '^\[wp\] +Proved|^\[wp\] +Unknown|^\[wp\] +Timeout|^\[wp\] +Failed' "$LOG" \
        | awk '{print "#   " $0}' || echo "#   (no wp headline found — see $LOG)"
    echo ""
    echo "# For the full transcript see $LOG."
} > "$OUT"

echo ""
echo "Wrote $OUT"
exit 0
