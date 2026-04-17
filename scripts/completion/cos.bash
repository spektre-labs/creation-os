# SPDX-License-Identifier: AGPL-3.0-or-later
# cos(1) — Bash completion for Creation OS unified front door.
#
# Install:
#   source /path/to/creation-os-kernel/scripts/completion/cos.bash
# or
#   make completion-install
#
# Covers every subcommand in cli/cos.c (v60 → v76 composed stack, v75
# license kernel, v74 experience, v73 omnimodal, v72 chain, v71
# wormhole, v70 hyperscale, v69 constellation, v68 mnemos, v67 noesis,
# v66 silicon, v65 hypercortex, v64 intellect, v63 cipher, v62 fabric,
# v61 citadel, v60 shield, and the doctor / sigma / chace rollups).

_cos_completions() {
    local cur prev words cword
    _init_completion || return

    local top="status doctor health verify chace sigma think seal unseal
               mcts hv si nx noesis mn mnemos cn constellation hs
               hyperscale wh wormhole ch chain om omni omnimodal ux xp
               experience sf surface mobile rv reversible landauer
               gd godel gödel attest meta
               license lic scsl decide
               version --version help -h --help"

    if [[ ${cword} -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "${top}" -- "${cur}") )
        return 0
    fi

    case "${words[1]}" in
        seal|unseal)
            _filedir
            return 0
            ;;
        license|lic|scsl)
            if [[ ${cword} -eq 2 ]]; then
                COMPREPLY=( $(compgen -W "status self-test verify bundle receipt guard print" -- "${cur}") )
            fi
            return 0
            ;;
        decide)
            COMPREPLY=( $(compgen -W "0 1" -- "${cur}") )
            return 0
            ;;
        think)
            return 0
            ;;
    esac

    COMPREPLY=()
    return 0
}

complete -F _cos_completions cos
complete -F _cos_completions ./cos
