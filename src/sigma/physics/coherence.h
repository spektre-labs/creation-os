/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-9 — coherence / safety scalars from σ and nominal capacity K. */

#ifndef COS_ULTRA_COHERENCE_H
#define COS_ULTRA_COHERENCE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

float cos_ultra_coherence_k_eff(float sigma, float K);

void cos_ultra_coherence_emit_report(FILE *fp, float sigma_mean_1h,
                                     float dsigma_dt_per_h, float K,
                                     float K_crit);

int cos_ultra_coherence_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
