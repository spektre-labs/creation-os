# SPDX-License-Identifier: AGPL-3.0-or-later
# cos(1) — Fish completion for Creation OS unified front door.
#
# Install:
#   cp scripts/completion/cos.fish ~/.config/fish/completions/
# or
#   make completion-install

set -l cos_subs status doctor health verify chace sigma think seal unseal \
    mcts hv si nx noesis mn mnemos cn constellation hs hyperscale \
    wh wormhole ch chain om omnimodal ux experience sf surface mobile \
    rv reversible landauer \
    gd godel gödel attest meta \
    license lic scsl decide version help

complete -c cos -f

# No subcommand yet → complete the top level set with rich descriptions.
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "status"    -d "status board (default)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "doctor"    -d "full repo-health rollup (license + verify + hardening + receipts)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "health"    -d "alias of doctor"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "verify"    -d "Verified-Agent (v57) rollup across all eighteen kernels"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "chace"     -d "CHACE-class 12-layer capability-hardening gate"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "sigma"     -d "run every kernel self-test v60 → v78"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "think"     -d "latent-CoT + EBT + HRM demo (v62)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "seal"      -d "attestation-bound E2E seal a file (v63 σ-Cipher)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "unseal"    -d "verify and open a sealed envelope (v63 σ-Cipher)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "mcts"      -d "MCTS-σ + skill + tool-authz self-test (v64)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "hv"        -d "HDC + VSA + HVL bytecode (v65)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "si"        -d "INT8/ternary GEMV + HSL bytecode (v66)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "nx"        -d "BM25 + dense + graph-walk + NBL (v67)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "noesis"    -d "long form of nx (v67)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "mn"        -d "HV + ACT-R + MML (v68)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "mnemos"    -d "long form of mn (v68)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "cn"        -d "tree-spec + debate + MoE (v69)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "constellation" -d "long form of cn (v69)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "hs"        -d "ShiftAddLLM + Mamba-2 + RWKV-7 + HSL (v70)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "hyperscale" -d "long form of hs (v70)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "wh"        -d "ER-portal + teleport + WHL (v71)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "wormhole"  -d "long form of wh (v71)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "ch"        -d "Merkle ledger + WOTS+ + SCL (v72)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "chain"     -d "long form of ch (v72)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "om"        -d "universal tokenizer + OML (v73)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "omnimodal" -d "long form of om (v73)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "ux"        -d "Fitts-V2P + adaptive layout + XPL (v74)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "experience" -d "long form of ux (v74)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "sf"        -d "touch + messenger + legacy + SBL (v76)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "surface"   -d "long form of sf (v76)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "mobile"    -d "long form of sf (v76)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "rv"        -d "reversible logic — Landauer / Bennett plane (v77)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "reversible" -d "long form of rv (v77)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "landauer"  -d "alias of rv (v77)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "gd"        -d "σ-Gödel-Attestor — meta-cognitive plane (v78)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "godel"     -d "long form of gd (v78)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "gödel"     -d "UTF-8 form of godel (v78)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "attest"    -d "alias of gd (v78)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "meta"      -d "alias of gd (v78)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "license"   -d "SCSL-1.0 licence kernel front door (v75)"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "lic"       -d "alias of license"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "scsl"      -d "alias of license"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "decide"    -d "16-bit composed decision as JSON"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "version"   -d "one-line version string"
complete -c cos -n "not __fish_seen_subcommand_from $cos_subs" -a "help"      -d "full command table"

# Second-level completions.
complete -c cos -n "__fish_seen_subcommand_from seal unseal" -F
complete -c cos -n "__fish_seen_subcommand_from license lic scsl" -a "status self-test verify bundle receipt guard print"
complete -c cos -n "__fish_seen_subcommand_from decide" -a "0 1"
