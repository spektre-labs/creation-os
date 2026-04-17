#compdef cos
# SPDX-License-Identifier: AGPL-3.0-or-later
# cos(1) — Zsh completion for Creation OS unified front door.
#
# Install:
#   fpath=(/path/to/creation-os-kernel/scripts/completion $fpath)
#   autoload -U compinit && compinit
# or
#   make completion-install

_cos() {
    local -a subcmds
    subcmds=(
        'status:status board (default)'
        'doctor:full repo-health rollup (license + verify + hardening + receipts)'
        'health:alias for doctor'
        'verify:Verified-Agent (v57) rollup across all eighteen kernels'
        'chace:CHACE-class 12-layer capability-hardening gate'
        'sigma:run every kernel self-test v60 → v78 and print verdicts'
        'think:latent-CoT + EBT verify + HRM converge demo (v62)'
        'seal:attestation-bound E2E seal a file (v63 σ-Cipher)'
        'unseal:verify and open a sealed envelope (v63 σ-Cipher)'
        'mcts:MCTS-σ + skill library + tool-authz self-test (v64)'
        'hv:HDC + VSA + cleanup + HVL bytecode self-test (v65)'
        'si:INT8/ternary GEMV + NTW + CFC + HSL bytecode (v66)'
        'nx:BM25 + dense + graph-walk + beam + NBL bytecode (v67)'
        'noesis:long form of nx (v67 σ-Noesis)'
        'mn:bipolar HV + ACT-R + MML bytecode (v68)'
        'mnemos:long form of mn (v68 σ-Mnemos)'
        'cn:tree-spec + debate + Byzantine vote + MoE (v69)'
        'constellation:long form of cn (v69 σ-Constellation)'
        'hs:ShiftAddLLM + Mamba-2 + RWKV-7 + MoE-10k + HSL (v70)'
        'hyperscale:long form of hs (v70 σ-Hyperscale)'
        'wh:ER-portal + teleport + Kleinberg routing + WHL (v71)'
        'wormhole:long form of wh (v71 σ-Wormhole)'
        'ch:Merkle ledger + WOTS+ + t-of-n + VRF + SCL (v72)'
        'chain:long form of ch (v72 σ-Chain)'
        'om:universal tokenizer + rectified-flow + OML (v73)'
        'omnimodal:long form of om (v73 σ-Omnimodal)'
        'ux:Fitts-V2P + adaptive layout + XPL (v74)'
        'experience:long form of ux (v74 σ-Experience)'
        'sf:touch + gesture + 10-messenger + legacy + SBL (v76)'
        'surface:long form of sf (v76 σ-Surface)'
        'mobile:long form of sf (v76 σ-Surface)'
        'rv:reversible logic — Landauer / Bennett plane (v77)'
        'reversible:long form of rv (v77 σ-Reversible)'
        'landauer:alias for rv (v77 σ-Reversible)'
        'gd:σ-Gödel-Attestor — meta-cognitive plane (v78)'
        'godel:long form of gd (v78 σ-Gödel-Attestor)'
        'gödel:UTF-8 form of godel (v78 σ-Gödel-Attestor)'
        'attest:alias for gd (v78 σ-Gödel-Attestor)'
        'meta:alias for gd (v78 σ-Gödel-Attestor)'
        'license:SCSL-1.0 licence kernel front door (v75)'
        'lic:alias for license (v75 σ-License)'
        'scsl:alias for license (v75 σ-License)'
        'decide:16-bit composed decision — emits one JSON verdict (v77 + v78 ride lateral)'
        'version:one-line version string'
        'help:this message'
    )

    if (( CURRENT == 2 )); then
        _describe 'command' subcmds
        return
    fi

    case "$words[2]" in
        seal|unseal)       _files ;;
        license|lic|scsl)
            local -a subs
            subs=(status self-test verify bundle receipt guard print)
            (( CURRENT == 3 )) && _describe 'license subcommand' subs
            ;;
        decide)
            _values '0 | 1' 0 1
            ;;
    esac
}

compdef _cos cos
