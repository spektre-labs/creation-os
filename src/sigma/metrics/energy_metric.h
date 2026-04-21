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
