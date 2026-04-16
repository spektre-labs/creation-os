/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v33: session-level agent metrics (JSONL; lab harness).
 */
#ifndef CREATION_OS_V33_METRICS_H
#define CREATION_OS_V33_METRICS_H

#include <stdint.h>
#include <stdio.h>

#define COS_METRICS_SIGMA_BINS 16
#define COS_METRICS_SIGMA_CHANNELS 8

typedef struct {
    unsigned long task_started;
    unsigned long task_completed;
    unsigned long task_failed;
    unsigned long task_abstained;
    unsigned long schema_valid;
    unsigned long schema_invalid;
    unsigned long tool_exec_ok;
    unsigned long tool_exec_fail;
    double session_cost_eur;
    uint64_t e2e_ns_sum;
    unsigned e2e_count;
    uint64_t tok_ns_sum;
    unsigned tok_count;
    unsigned sigma_hist[COS_METRICS_SIGMA_CHANNELS][COS_METRICS_SIGMA_BINS];
    FILE *fp;
    char path[512];
} cos_metrics_session_t;

int cos_metrics_session_open(cos_metrics_session_t *m, const char *metrics_dir);
void cos_metrics_session_close(cos_metrics_session_t *m);

void cos_metrics_event_json(cos_metrics_session_t *m, const char *json_line);

void cos_metrics_task_started(cos_metrics_session_t *m, int task_id);
void cos_metrics_task_outcome(cos_metrics_session_t *m, int task_id, const char *outcome);
void cos_metrics_schema(cos_metrics_session_t *m, int valid);
void cos_metrics_tool_exec(cos_metrics_session_t *m, int ok);
void cos_metrics_latency_e2e_ns(cos_metrics_session_t *m, uint64_t ns);
void cos_metrics_latency_token_ns(cos_metrics_session_t *m, uint64_t ns);
void cos_metrics_sigma_observe(cos_metrics_session_t *m, int channel_index, float sigma_0_1);

#endif /* CREATION_OS_V33_METRICS_H */
