/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-5 — four-channel meta-σ; max drives diagnostic policy. */

#ifndef COS_ULTRA_METACOG_H
#define COS_ULTRA_METACOG_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sigma_perception;
    float sigma_self;
    float sigma_social;
    float sigma_situational;
} cos_ultra_metacog_levels_t;

float cos_ultra_metacog_max_sigma(const cos_ultra_metacog_levels_t *lv);

/* Writes a short English recommendation into buf. */
void cos_ultra_metacog_recommend(const cos_ultra_metacog_levels_t *lv,
                                 char *buf, size_t cap);

void cos_ultra_metacog_emit_report(FILE *fp,
                                   const cos_ultra_metacog_levels_t *lv);

int cos_ultra_metacog_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
