/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-4 — σ routes fast System-1 vs slow System-2 symbolic check. */

#ifndef COS_ULTRA_NEURO_SYM_H
#define COS_ULTRA_NEURO_SYM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sigma_system1;
    float tau_symbolic;
    int   use_system2; /* 1 when symbolic path should engage */
} cos_ultra_neuro_sym_t;

void cos_ultra_neuro_sym_route(cos_ultra_neuro_sym_t *out,
                               float sigma_system1, float tau_symbolic);

int cos_ultra_neuro_sym_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
