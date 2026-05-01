/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-3 — event-driven decode: recompute only when σ moves enough. */

#ifndef COS_ULTRA_SELECTIVE_H
#define COS_ULTRA_SELECTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 if downstream should run a full decode step. */
int cos_ultra_selective_should_decode(float sigma_t, float sigma_prev,
                                      float epsilon);

int cos_ultra_selective_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
