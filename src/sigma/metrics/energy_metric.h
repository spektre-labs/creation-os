/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-7 — reasoning-per-joule and σ-efficiency (dimensionless helpers). */

#ifndef COS_ULTRA_ENERGY_METRIC_H
#define COS_ULTRA_ENERGY_METRIC_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

float cos_ultra_reasoning_per_joule(float correct_answers, float total_joules);

float cos_ultra_sigma_efficiency(float sigma_mean, float joules_per_query);

void cos_ultra_energy_print_demo_table(FILE *fp);

int cos_ultra_energy_metric_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
