#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v57 Verified Agent — live aggregate driver.
#
# Walks the composition slots that v57 declares (see
# src/v57/verified_agent.c) and dispatches the make target that
# OWNS each slot.  Reports honestly:
#
#   PASS   — make target returned 0
#   FAIL   — make target returned non-zero
#   SKIP   — make target ran but emitted SKIP markers because the
#            backing tool (Frama-C, sby, pytest, garak, ...) is not
#            installed on this machine
#
# The script never silently downgrades.  A missing tool is a SKIP,
# not a PASS.  The exit code is 0 only if every slot is PASS or
# (deliberately accepted) SKIP and no slot is FAIL.
#
# Use this script from the repo root:
#
#   make verify-agent
#   # or
#   bash scripts/v57/verify_agent.sh
#
# Optional flags:
#   --strict        treat SKIP as FAIL (CI mode with all tools present)
#   --json PATH     also write a JSON report to PATH
#   --quiet         only print the final summary line
#
# Exit codes:
#   0 — all slots PASS (or SKIP and not --strict)
#   1 — at least one slot FAIL
#   2 — at least one slot SKIP (only under --strict)

set -u

STRICT=0
QUIET=0
JSON_PATH=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --strict) STRICT=1 ;;
        --quiet)  QUIET=1 ;;
        --json)
            shift
            JSON_PATH="${1:-}"
            if [[ -z "$JSON_PATH" ]]; then
                echo "verify-agent: --json requires a path" >&2
                exit 64
            fi
            ;;
        -h|--help)
            sed -n '1,40p' "$0"
            exit 0
            ;;
        *)
            echo "verify-agent: unknown flag '$1'" >&2
            exit 64
            ;;
    esac
    shift
done

# ----- composition slots (kept in sync with src/v57/verified_agent.c) -----

# slot_name|tier|owner_versions|make target
SLOTS=(
    "execution_sandbox|M|v48|check-v48"
    "sigma_kernel_surface|F|v47|verify-c"
    "harness_loop|M|v53|check-v53"
    "multi_llm_routing|M|v54|check-v54"
    "speculative_decode|M|v55|check-v55"
    "constitutional_self_modification|M|v56|check-v56"
    "do178c_assurance_pack|I|v49|certify"
    "red_team_suite|I|v48+v49|red-team"
    "convergence_self_test|M|v57|check-v57"
    "kv_cache_eviction|M|v58|check-v58"
    "adaptive_compute_budget|M|v59|check-v59"
    "runtime_security_kernel|M|v60|check-v60"
    "defense_in_depth_stack|M|v61|check-v61"
    "reasoning_fabric|M|v62|check-v62"
    "e2e_encrypted_fabric|M|v63|check-v63"
    "agentic_intellect|M|v64|check-v64"
    "hyperdimensional_cortex|M|v65|check-v65"
    "matrix_substrate|M|v66|check-v66"
    "deliberative_cognition|M|v67|check-v67"
    "continual_learning_memory|M|v68|check-v68"
    "distributed_orchestration|M|v69|check-v69"
)

PASS_LIST=()
FAIL_LIST=()
SKIP_LIST=()

run_slot() {
    local entry="$1"
    local slot tier owner target
    slot="${entry%%|*}";          entry="${entry#*|}"
    tier="${entry%%|*}";          entry="${entry#*|}"
    owner="${entry%%|*}";         entry="${entry#*|}"
    target="${entry%%|*}"

    if [[ "$QUIET" -eq 0 ]]; then
        printf "  [%s] %-36s (owner: %-12s target: make %-14s)  ... " \
            "$tier" "$slot" "$owner" "$target"
    fi

    local out rc
    out="$(make -s "$target" 2>&1)"
    rc=$?

    if [[ $rc -ne 0 ]]; then
        if [[ "$QUIET" -eq 0 ]]; then echo "FAIL"; fi
        FAIL_LIST+=("$slot")
        if [[ "$QUIET" -eq 0 ]]; then
            echo "      ---- output ----"
            printf '      %s\n' "$out" | tail -n 10
            echo "      ----------------"
        fi
        return
    fi

    if echo "$out" | grep -qiE 'SKIP\b'; then
        if [[ "$QUIET" -eq 0 ]]; then echo "SKIP"; fi
        SKIP_LIST+=("$slot")
        return
    fi

    if [[ "$QUIET" -eq 0 ]]; then echo "PASS"; fi
    PASS_LIST+=("$slot")
}

if [[ "$QUIET" -eq 0 ]]; then
    cat <<'EOF'

Creation OS v57 — verify-agent
==============================

Walking the composition slots declared in src/v57/verified_agent.c.
Each slot dispatches its owning make target and reports:

  PASS  the deterministic check returned 0
  SKIP  the target ran but the backing tool was absent
  FAIL  the target returned non-zero

Verify-agent never silently downgrades.  A missing tool is a SKIP.

EOF
fi

for entry in "${SLOTS[@]}"; do
    run_slot "$entry"
done

# ----- summary -----

n_pass=${#PASS_LIST[@]}
n_fail=${#FAIL_LIST[@]}
n_skip=${#SKIP_LIST[@]}
n_total=$(( n_pass + n_fail + n_skip ))

if [[ "$QUIET" -eq 0 ]]; then
    echo
    echo "  PASS: $n_pass / $n_total"
    echo "  SKIP: $n_skip / $n_total"
    echo "  FAIL: $n_fail / $n_total"
    echo
fi

if [[ -n "$JSON_PATH" ]]; then
    {
        printf '{\n'
        printf '  "pass": %d,\n' "$n_pass"
        printf '  "skip": %d,\n' "$n_skip"
        printf '  "fail": %d,\n' "$n_fail"
        printf '  "total": %d,\n' "$n_total"
        printf '  "strict": %s,\n' "$( [[ $STRICT -eq 1 ]] && echo true || echo false )"
        printf '  "passed_slots": ['
        for i in "${!PASS_LIST[@]}"; do
            [[ $i -gt 0 ]] && printf ', '
            printf '"%s"' "${PASS_LIST[$i]}"
        done
        printf '],\n'
        printf '  "skipped_slots": ['
        for i in "${!SKIP_LIST[@]}"; do
            [[ $i -gt 0 ]] && printf ', '
            printf '"%s"' "${SKIP_LIST[$i]}"
        done
        printf '],\n'
        printf '  "failed_slots": ['
        for i in "${!FAIL_LIST[@]}"; do
            [[ $i -gt 0 ]] && printf ', '
            printf '"%s"' "${FAIL_LIST[$i]}"
        done
        printf ']\n'
        printf '}\n'
    } > "$JSON_PATH"
    if [[ "$QUIET" -eq 0 ]]; then
        echo "  json report: $JSON_PATH"
    fi
fi

if [[ $n_fail -gt 0 ]]; then
    echo "verify-agent: FAIL ($n_fail slot(s) failed)"
    exit 1
fi
if [[ $STRICT -eq 1 && $n_skip -gt 0 ]]; then
    echo "verify-agent: STRICT-FAIL ($n_skip slot(s) SKIPped; install missing tools)"
    exit 2
fi
echo "verify-agent: OK ($n_pass PASS, $n_skip SKIP, $n_fail FAIL)"
exit 0
