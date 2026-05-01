/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-2 — JEPA-style latent dynamics; σ_world = prediction error norm. */

#ifndef COS_ULTRA_JEPA_H
#define COS_ULTRA_JEPA_H

#ifdef __cplusplus
extern "C" {
#endif

/* L2 distance ||z_hat - z_next|| / (||z_next|| + eps) in [0, ~sqrt(dim)]. */
float cos_ultra_jepa_sigma_world(const float *z_hat, const float *z_next,
                                 int dim, float eps);

int cos_ultra_jepa_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
