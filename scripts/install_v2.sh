#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Creation OS — installer v2 (for I3+I4 CLI products)
#
#   curl -sSL https://creation.os/install_v2.sh | sh
#   bash scripts/install_v2.sh
#
# What it does (target: < 60 s on a modern laptop):
#   1. Verify prereqs (cc/gcc, make, git).
#   2. Clone or update the canonical repo into ~/.creation-os
#      (set COS_HOME to override).
#   3. Build the three reference CLIs:
#          cos-chat
#          cos-benchmark
#          cos-cost
#      and the end-to-end self-test binary creation_os_sigma_pipeline.
#   4. Verify the Atlantean Codex is on disk
#      (data/codex/atlantean_codex.txt + data/codex/codex_seed.txt).
#   5. Run check-sigma-pipeline (primitives + integration + CLI smoke).
#   6. Print a one-page "what now?" message with the three
#      canonical demos the user can run offline, using the stub
#      backend, in under three seconds each.
#
# Zero-cloud by default:
#   - no model weights are downloaded here
#   - no network calls except the git clone in step (2)
#   - the σ-pipeline runs fully offline over the deterministic
#     stub generator the tests use
#
# Exit codes:
#   0  — install + smoke test OK
#   2  — prereq missing
#   3  — clone / update failed
#   4  — build failed
#   5  — codex missing
#   6  — smoke test failed

set -euo pipefail

REPO_URL="${COS_REPO_URL:-https://github.com/spektre-labs/creation-os.git}"
INSTALL_DIR="${COS_HOME:-$HOME/.creation-os}"
BRANCH="${COS_BRANCH:-main}"
RUN_SMOKE="${COS_RUN_SMOKE:-1}"

log()  { printf '\033[1;36m[creation-os]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[creation-os]\033[0m %s\n' "$*" >&2; }
fail() { printf '\033[1;31m[creation-os]\033[0m %s\n' "$*" >&2; exit "${2:-1}"; }

# ---------------------------------------------------------------- prereqs
log "step 1/6: checking prereqs"
command -v git   >/dev/null 2>&1 || fail "missing: git"   2
command -v make  >/dev/null 2>&1 || fail "missing: make"  2
command -v cc    >/dev/null 2>&1 || \
    command -v gcc >/dev/null 2>&1 || fail "missing: cc/gcc" 2

# ---------------------------------------------------------------- clone
START_TS=$(date +%s)
if [[ -d "$INSTALL_DIR/.git" ]]; then
    log "step 2/6: updating $INSTALL_DIR"
    git -C "$INSTALL_DIR" fetch   --quiet origin "$BRANCH" || \
        fail "fetch failed" 3
    git -C "$INSTALL_DIR" checkout --quiet "$BRANCH" || \
        fail "checkout failed" 3
    git -C "$INSTALL_DIR" pull    --quiet --ff-only || \
        warn "pull non-ff; continuing with local state"
elif [[ -f "$(pwd)/creation_os_v2.c" ]]; then
    log "step 2/6: running in-tree (no clone); using $(pwd)"
    INSTALL_DIR="$(pwd)"
else
    log "step 2/6: cloning $REPO_URL → $INSTALL_DIR"
    git clone --depth 1 --branch "$BRANCH" --quiet \
        "$REPO_URL" "$INSTALL_DIR" || fail "clone failed" 3
fi

cd "$INSTALL_DIR"

# ---------------------------------------------------------------- build
log "step 3/6: building CLIs + pipeline binary"
make -s cos-chat cos-benchmark cos-cost \
        creation_os_sigma_pipeline \
        >/dev/null 2>/tmp/cos_install_v2.log || {
    tail -30 /tmp/cos_install_v2.log >&2
    fail "build failed — see /tmp/cos_install_v2.log" 4
}

# ---------------------------------------------------------------- codex check
log "step 4/6: verifying Atlantean Codex on disk"
[[ -s data/codex/atlantean_codex.txt ]] || \
    fail "missing: data/codex/atlantean_codex.txt" 5
[[ -s data/codex/codex_seed.txt      ]] || \
    fail "missing: data/codex/codex_seed.txt"      5
CODEX_BYTES=$(wc -c < data/codex/atlantean_codex.txt | tr -d ' ')
SEED_BYTES=$(wc -c < data/codex/codex_seed.txt      | tr -d ' ')
log "       full codex : ${CODEX_BYTES} bytes"
log "       seed codex : ${SEED_BYTES} bytes"

# ---------------------------------------------------------------- smoke test
if [[ "$RUN_SMOKE" == "1" ]]; then
    log "step 5/6: running check-sigma-pipeline (primitives + integration + CLIs)"
    make -s check-sigma-pipeline >/tmp/cos_install_v2.log 2>&1 || {
        tail -40 /tmp/cos_install_v2.log >&2
        fail "σ-pipeline self-tests failed" 6
    }
else
    log "step 5/6: skipping smoke test (COS_RUN_SMOKE=0)"
fi

END_TS=$(date +%s)
ELAPSED=$(( END_TS - START_TS ))

# ---------------------------------------------------------------- wrap up
log "step 6/6: installed in ${ELAPSED}s — welcome."
cat <<EOF

  Creation OS is ready at: $INSTALL_DIR

  Try these three commands (each runs offline, < 3s, uses the stub backend):

      $INSTALL_DIR/cos-chat         # σ-gated REPL, Codex loaded as system prompt
      $INSTALL_DIR/cos-benchmark    # 4-config comparison table (bitnet / ± codex / api)
      $INSTALL_DIR/cos-cost         # zero-cloud sovereignty ledger

  The σ-pipeline composes all 15 σ-primitives (P1–P20 + I0 Codex) behind
  a single function; the stub generator proves the control plane works
  end-to-end. To swap in a real BitNet backend, replace the generate
  callback — see docs/DOC_INDEX.md for the pipeline entry.

  assert(declared == realized);  1 = 1.

EOF
