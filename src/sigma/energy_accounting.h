/*
 * Process-scoped energy accounting (CPU time × configured draw → J, CO₂, €).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_ENERGY_ACCOUNTING_H
#define COS_SIGMA_ENERGY_ACCOUNTING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_energy_measurement {
    float    cpu_time_ms;
    float    wall_time_ms;
    float    estimated_joules;
    float    estimated_watts_avg;
    float    co2_grams;
    float    cost_eur;
    int      tokens_generated;
    float    joules_per_token;
    float    co2_per_token;

    float    reasoning_per_joule;
    float    sigma_per_joule;
    float    cache_savings_joules;
    float    abstain_savings_joules;
    float    spike_savings_joules;

    int64_t  timestamp_ms;
};

struct cos_energy_config {
    float grid_co2_per_kwh;
    float electricity_eur_per_kwh;
    float tdp_watts;
};

typedef struct cos_energy_timer {
    uint64_t cpu_start_ns;
    uint64_t wall_start_ns;
    int      started;
} cos_energy_timer_t;

int cos_energy_init(const struct cos_energy_config *config);

void cos_energy_reset_session(void);

const struct cos_energy_measurement *cos_energy_session_total(void);

const struct cos_energy_measurement *cos_energy_lifetime_total(void);

void cos_energy_context(float sigma_next, float reasoning_units);

struct cos_energy_measurement cos_energy_measure(float cpu_time_ms,
                                                 int              tokens_generated);

struct cos_energy_measurement cos_energy_measure_wall(
    float cpu_time_ms, float wall_time_ms, int tokens_generated);

void cos_energy_accumulate(struct cos_energy_measurement       *total,
                           const struct cos_energy_measurement *current);

char *cos_energy_to_json(const struct cos_energy_measurement *m);

void cos_energy_print_receipt(const struct cos_energy_measurement *m);

void cos_energy_print_savings_line(const struct cos_energy_measurement *m);

int cos_energy_persist(const char *path);

int cos_energy_load(const char *path);

int cos_energy_lifetime_report(char *report, int max_len);

int64_t cos_energy_lifetime_query_count(void);

void cos_energy_timer_start(cos_energy_timer_t *t);

struct cos_energy_measurement cos_energy_timer_stop_and_measure(
    cos_energy_timer_t *t,
    int                 tokens_generated);

int cos_energy_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_ENERGY_ACCOUNTING_H */
