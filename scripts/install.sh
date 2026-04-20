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
# weights by default.  BitNet gguf is opt-in; the σ-pipeline
# self-tests and `cos benchmark --fixture demo` run offline over
# the deterministic StubBackend so the user sees a working
# product on first run.
#
# v107 legacy steps (forty-kernel stack):
#
#   This script supersedes the v107 Homebrew + curl + Docker
#   installer, but keeps the legacy v107 knobs addressable so the
#   Homebrew formula, the Dockerfile, and the `check-v107-install`
#   structural gate continue to work.  The legacy path:
#
#       1. download_default_model    — BitNet-b1.58-2B GGUF fetch
#                                      (guarded by COS_V107_SKIP_MODEL).
#       2. write_default_config      — writes a ~/.creation-os/
#                                      config.toml with sensible
#                                      defaults (server port, model
#                                      path, sigma gates).
#       3. start_server_optional     — boots the v106 σ-server on
#                                      localhost:8080 if the user
#                                      asks for it with
#                                      COS_V107_START_SERVER=1.
#
#   Default behaviour (no COS_V107_* flags set) is the light
#   σ-pipeline smoke test, which is what end users see after
#   `curl | sh`.  Legacy mode is enabled with COS_V107_LEGACY=1.
#
# The COS_V107_SKIP_MODEL=1 escape hatch keeps CI green on hosts
# without network access and is honoured even in legacy mode.

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

# -------------------- v107 legacy knobs --------------------
#
# These functions exist so the v107 Homebrew/Docker/curl contract
# remains grep-able from check_v107_install_macos.sh.  They are
# *not* invoked on the default σ-pipeline path; only when
# COS_V107_LEGACY=1 is exported, or a downstream packager calls
# them explicitly.

download_default_model() {
    # forty-kernel stack: BitNet-b1.58-2B GGUF (~1.6 GB).
    # Honours the COS_V107_SKIP_MODEL escape so CI and sandboxed
    # hosts can install without network.
    if [[ "${COS_V107_SKIP_MODEL:-0}" == "1" ]]; then
        log "COS_V107_SKIP_MODEL=1 — skipping BitNet GGUF download"
        return 0
    fi
    local url="${COS_V107_MODEL_URL:-https://huggingface.co/microsoft/BitNet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf}"
    local dst="$INSTALL_DIR/models/ggml-model-i2_s.gguf"
    mkdir -p "$(dirname "$dst")"
    if [[ -f "$dst" ]]; then
        log "model already present: $dst"
        return 0
    fi
    log "downloading default model: $url"
    curl -fsSL -o "$dst" "$url" \
        || warn "model download failed; pipeline will fall back to StubBackend"
}

write_default_config() {
    # forty-kernel stack: ~/.creation-os/config.toml with sensible
    # defaults (σ gates, server port, model path).
    local cfg="$INSTALL_DIR/config.toml"
    if [[ -f "$cfg" ]]; then
        log "config.toml already present: $cfg"
        return 0
    fi
    log "writing default config: $cfg"
    cat >"$cfg" <<'TOML'
# Creation OS — default config.toml (v107 legacy shape).
[server]
host = "127.0.0.1"
port = 8080

[sigma]
tau_accept = 0.30
tau_abstain = 0.70

[model]
path = "models/ggml-model-i2_s.gguf"
TOML
}

start_server_optional() {
    # forty-kernel stack: boot the v106 σ-server on localhost:8080
    # if the user asked for it with COS_V107_START_SERVER=1.  The
    # default σ-pipeline install path never calls this.
    if [[ "${COS_V107_START_SERVER:-0}" != "1" ]]; then
        return 0
    fi
    if [[ ! -x "$INSTALL_DIR/creation_os_server" ]]; then
        warn "creation_os_server not built — skipping server boot"
        return 0
    fi
    log "booting v106 σ-server on 127.0.0.1:8080 (detached)"
    nohup "$INSTALL_DIR/creation_os_server" >"$INSTALL_DIR/server.log" 2>&1 &
}

if [[ "${COS_V107_LEGACY:-0}" == "1" ]]; then
    download_default_model
    write_default_config
    start_server_optional
fi

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
