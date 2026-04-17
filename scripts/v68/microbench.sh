#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v68 σ-Mnemos microbench — builds creation_os_v68 if needed, then runs
# the in-binary microbench (HV Hamming, recall, Hebb, consolidate, MML).
set -euo pipefail
cd "$(dirname "$0")/../.."
if [[ ! -x ./creation_os_v68 ]]; then
    make -s standalone-v68
fi
echo "v68 σ-Mnemos microbench:"
./creation_os_v68 --bench | sed 's/^/  /'
