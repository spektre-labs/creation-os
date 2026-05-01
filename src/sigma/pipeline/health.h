/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Health — runtime health snapshot of the σ-pipeline.
 *
 * A single-shot query that collapses the live state of the
 * σ-pipeline into one JSON object, suitable for (a) a human
 * operator running `cos health`, (b) a Prometheus scraper reading
 * the same object as key/value labels, (c) a paging system that
 * wants a binary HEALTHY / DEGRADED / UNHEALTHY verdict.
 *
 * Inputs (in priority order):
 *   1. An optional σ-Persist SQLite file at state.db — when
 *      present, engram count, τ calibration, and cost ledger are
 *      read from the file.
 *   2. Compile-time baselines for the static facets (number of
 *      σ-primitives, number of check targets, formal proofs
 *      discharged, substrate count).
 *
 * Output fields (stable):
 *   mode                HYBRID | LOCAL | SOVEREIGN
 *   status              HEALTHY | DEGRADED | UNHEALTHY
 *   pipeline_ok         int            (checks_green / checks_total)
 *   checks_total        int
 *   checks_green        int
 *   engrams             int
 *   tau_accept          float
 *   tau_rethink         float
 *   sigma_mean_last_100 float
 *   uptime_hours        float
 *   cost_today_eur      float
 *   rethink_rate_pct    int
 *   escalation_rate_pct int
 *   weakest_domain      string
 *   formal_proofs_discharged  int
 *   sigma_primitives          int
 *   substrates                int
 *
 * The module is a thin synthesiser: it does not decide WHAT the
 * values are, only HOW they are assembled.  Callers feed in the
 * parts that are dynamic and we wrap them in a single struct.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_HEALTH_H
#define COS_SIGMA_HEALTH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum cos_health_status {
    COS_HEALTH_HEALTHY   = 0,
    COS_HEALTH_DEGRADED  = 1,
    COS_HEALTH_UNHEALTHY = 2,
};

enum cos_health_mode {
    COS_HEALTH_MODE_HYBRID    = 0,
    COS_HEALTH_MODE_LOCAL     = 1,
    COS_HEALTH_MODE_SOVEREIGN = 2,
};

typedef struct {
    /* Static facets (populated from compile-time knowledge). */
    int   sigma_primitives;
    int   checks_total;
    int   checks_green;
    int   substrates;
    int   formal_proofs_discharged;
    int   formal_proofs_total;

    /* Dynamic facets (populated from state.db or defaults). */
    int   engrams;
    float tau_accept;
    float tau_rethink;
    float sigma_mean_last_100;
    float uptime_hours;
    float cost_today_eur;
    int   rethink_rate_pct;
    int   escalation_rate_pct;
    char  weakest_domain[32];

    /* DEV-7: runtime facets populated by cos_sigma_health_load_live().
     * When the live loader is skipped these stay zero / empty and the
     * banner prints them as "unknown". */
    int   llama_server_up;        /* 1 if GET /health returned "ok" */
    int   llama_server_port;      /* resolved port (COS_BITNET_SERVER_PORT) */
    char  llama_server_host[64];  /* resolved host */
    char  model_path[256];        /* COS_BITNET_SERVER_MODEL */
    int   distill_pairs_today;    /* count of escalations today */

    /* Roll-up. */
    int   mode;
    int   status;
} cos_health_report_t;

int  cos_sigma_health_init_defaults(cos_health_report_t *r);

/* Populate r->engrams / τ / cost from a σ-Persist SQLite file;
 * returns 0 on success, <0 if the file cannot be opened / read.
 * Fields not present in the DB are left at their default values. */
int  cos_sigma_health_load_state(cos_health_report_t *r,
                                 const char *state_db_path);

/* DEV-7: enrich a report with live runtime metrics collected from
 * this machine:
 *   - engrams           ← row count of ~/.cos/engram.db (DEV-3)
 *   - sigma_mean_last_100 ← avg σ of most-recent 100 engram rows
 *   - cost_today_eur    ← sum of cost_eur in ~/.cos/distill_pairs.jsonl
 *                          with ts ≥ midnight today (local)
 *   - distill_pairs_today, llama_server_up, llama_server_host/port,
 *     model_path
 *
 * Paths are resolved from COS_ENGRAM_DB, CREATION_OS_DISTILL_LOG,
 * COS_BITNET_SERVER_HOST / _PORT, COS_BITNET_SERVER_MODEL with the
 * same defaults as the runtime.  Returns 0 on success (partial OK
 * if a source is missing; the field simply stays at its current
 * value). */
int  cos_sigma_health_load_live(cos_health_report_t *r);

/* Decide the HEALTHY / DEGRADED / UNHEALTHY verdict from the
 * report's quantitative fields.  Mutates r->status in place.
 * Thresholds (v0):
 *   UNHEALTHY  if pipeline_ok < 90%   OR cost_today > €10.00
 *   DEGRADED   if pipeline_ok < 100%  OR sigma_mean_last_100 > 0.50
 *                                      OR escalation_rate_pct > 20
 *   HEALTHY    otherwise.
 */
int  cos_sigma_health_grade(cos_health_report_t *r);

/* Serialise as pretty text (fd 0) or stable JSON (fd 1). */
int  cos_sigma_health_emit(const cos_health_report_t *r, int emit_json);

const char *cos_sigma_health_status_name(int s);
const char *cos_sigma_health_mode_name  (int m);

int cos_sigma_health_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_HEALTH_H */
