/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-2 — latent next-state error as a σ-style world signal. */

#include "jepa.h"

#include <math.h>

float cos_ultra_jepa_sigma_world(const float *z_hat, const float *z_next,
                                 int dim, float eps) {
    if (!z_hat || !z_next || dim < 1) return 1.0f;
    if (!(eps > 0.0f)) eps = 1e-6f;
    double sumsq = 0.0, nrm = 0.0;
    for (int i = 0; i < dim; ++i) {
        double d = (double)z_hat[i] - (double)z_next[i];
        sumsq += d * d;
        double t = (double)z_next[i];
        nrm += t * t;
    }
    float zn = (float)sqrt(nrm) + eps;
    return (float)(sqrt(sumsq) / (double)zn);
}

int cos_ultra_jepa_self_test(void) {
    float zhat[] = { 1.0f, 0.1f, -0.9f, 0.5f };
    float zn[]   = { 1.0f, 0.0f, -1.0f, 0.5f };
    float s = cos_ultra_jepa_sigma_world(zhat, zn, 4, 1e-6f);
    if (s < 0.0f || s > 10.0f) return 1;
    float s2 = cos_ultra_jepa_sigma_world(zn, zn, 4, 1e-6f);
    if (s2 > 1e-5f) return 2;
    return 0;
}
