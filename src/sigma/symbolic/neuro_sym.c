/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-4 — neuro-symbolic gate: high σ → engage symbolic verifier. */

#include "neuro_sym.h"

void cos_ultra_neuro_sym_route(cos_ultra_neuro_sym_t *out,
                              float sigma_system1, float tau_symbolic) {
    if (!out) return;
    out->sigma_system1   = sigma_system1;
    out->tau_symbolic    = tau_symbolic;
    out->use_system2    = 0;
    if (sigma_system1 != sigma_system1) {
        out->use_system2 = 1;
        return;
    }
    if (!(tau_symbolic > 0.0f)) tau_symbolic = 0.35f;
    if (sigma_system1 > tau_symbolic) out->use_system2 = 1;
}

int cos_ultra_neuro_sym_self_test(void) {
    cos_ultra_neuro_sym_t r;
    cos_ultra_neuro_sym_route(&r, 0.2f, 0.5f);
    if (r.use_system2 != 0) return 1;
    cos_ultra_neuro_sym_route(&r, 0.6f, 0.5f);
    if (r.use_system2 != 1) return 2;
    return 0;
}
