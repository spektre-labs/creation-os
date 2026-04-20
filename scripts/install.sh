#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Creation OS — one-liner installer v2.
#
# Usage:
#
#    curl -sSL https://creation.os/install.sh | sh
#
# or locally:
#
#    bash scripts/install.sh
#
# What it does:
#   1. Clone (or update) the creation-os repo into ~/.creation-os.
#   2. Check build prereqs (cc, make, python3).  Refuses if missing.
#   3. `make cos sigma-pipeline` → builds the σ-pipeline primitives +
#      the `cos` launcher.
#   4. Runs `cos chat --once --prompt "What is 2+2?"` as a smoke
#      test so the user sees the pipeline actually fire before the
#      install returns.
#   5. Drops a ~/.creation-os/bin symlink into /usr/local/bin if
#      writable (else prints the PATH snippet to add).
#
# Network/model note: this installer does NOT download any LLM
# weights.  BitNet gguf is opt-in; the σ-pipeline self-tests and
# `cos benchmark --fixture demo` run offline over the deterministic
# StubBackend so the user sees a working product on first run.

set -euo pipefail

# -------------------- config --------------------
REPO_URL="${COS_REPO_URL:-https://github.com/spektre-labs/creation-os.git}"
INSTALL_DIR="${COS_HOME:-$HOME/.creation-os}"
BRANCH="${COS_BRANCH:-main}"
BIN_DIR="${COS_BIN_DIR:-/usr/local/bin}"
RUN_SMOKE="${COS_RUN_SMOKE:-1}"

# -------------------- tiny io helpers --------------------
log()  { printf "\033[1;36m[creation-os]\033[0m %s\n" "$*"; }
warn() { printf "\033[1;33m[creation-os]\033[0m %s\n" "$*" >&2; }
fail() { printf "\033[1;31m[creation-os]\033[0m %s\n" "$*" >&2; exit 1; }

# -------------------- prereqs --------------------
need() {
    command -v "$1" >/dev/null 2>&1 \
        || fail "missing prereq: $1 — please install and retry"
}
need git
need make
need cc || need gcc
need python3

# -------------------- clone / update --------------------
if [[ -d "$INSTALL_DIR/.git" ]]; then
    log "updating existing checkout: $INSTALL_DIR"
    git -C "$INSTALL_DIR" fetch --quiet origin "$BRANCH" || true
    git -C "$INSTALL_DIR" checkout --quiet "$BRANCH"     || true
    git -C "$INSTALL_DIR" pull    --quiet --ff-only      || \
        warn "git pull failed (non-fast-forward?); continuing with local state"
else
    log "cloning $REPO_URL → $INSTALL_DIR"
    git clone --depth 1 --branch "$BRANCH" --quiet "$REPO_URL" "$INSTALL_DIR"
fi

cd "$INSTALL_DIR"

# -------------------- build --------------------
log "building: cos launcher + σ-pipeline primitives"
make cos                          >/dev/null
make creation_os_sigma_reinforce  >/dev/null
make creation_os_sigma_speculative >/dev/null
make creation_os_sigma_ttt        >/dev/null
make creation_os_sigma_engram     >/dev/null

# -------------------- smoke test --------------------
if [[ "$RUN_SMOKE" == "1" ]]; then
    log "smoke test: pipeline primitives"
    make check-sigma-pipeline     >/dev/null \
        || fail "σ-pipeline primitives failed self-test"
    log "smoke test: end-to-end pipeline benchmark (offline, stub backend)"
    ./cos benchmark 2>/dev/null | tail -6 | sed 's/^/    /'
    log "smoke test: cost measurement"
    ./cos cost --fixture demo 2>/dev/null | head -15 | sed 's/^/    /'
fi

# -------------------- PATH wiring --------------------
COS_BIN="$INSTALL_DIR/cos"
if [[ -w "$BIN_DIR" ]]; then
    ln -sf "$COS_BIN" "$BIN_DIR/cos"
    log "symlinked cos → $BIN_DIR/cos"
elif sudo -n true 2>/dev/null && [[ -d "$BIN_DIR" ]]; then
    sudo ln -sf "$COS_BIN" "$BIN_DIR/cos"
    log "symlinked cos → $BIN_DIR/cos (via sudo)"
else
    warn "$BIN_DIR not writable; add to PATH manually:"
    warn "    export PATH=\"$INSTALL_DIR:\$PATH\""
fi

cat <<EOF

  Creation OS installed at $INSTALL_DIR

  Try:
      cos              # status board
      cos chat         # σ-gated REPL (uses stub backend unless gguf present)
      cos benchmark    # end-to-end pipeline: engram + BitNet + σ + TTT + API
      cos cost --fixture demo    # headline: X% of API cost saved, Y% accuracy retained

EOF
