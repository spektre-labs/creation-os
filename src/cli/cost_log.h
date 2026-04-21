/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *
 *  src/cli/cost_log.h — POLISH-4 per-inference usage log.
 *
 *  Every pipeline call (LOCAL or ESCALATED) appends one JSON line to
 *  ~/.cos/cost_log.jsonl:
 *
 *    {"ts":<epoch_s>,"tokens_in":N,"tokens_out":N,"route":"LOCAL|API",
 *     "provider":"bitnet|claude|openai|deepseek","cost_eur":<num>,
 *     "sigma":<num>}
 *
 *  `cos cost --from-log` aggregates the file into
 *  today / this-week / this-month totals and reports savings against
 *  a constant cloud-only baseline.
 *
 *  File location can be overridden with CREATION_OS_COST_LOG=<path>.
 *  All failures are silent: cost logging must never tear a session
 *  down.
 */

#ifndef COS_COST_LOG_H
#define COS_COST_LOG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Append a single usage record.  `provider` and `route` are not
 * escaped — pass short literal strings (no quotes, no backslashes). */
void cos_cost_log_append(const char *provider,
                         const char *route,
                         int         tokens_in,
                         int         tokens_out,
                         float       sigma,
                         double      cost_eur);

/* Aggregated totals read back by `cos cost --from-log`. */
typedef struct {
    int    queries_today;
    int    queries_week;
    int    queries_month;
    int    local_today;
    int    local_week;
    int    local_month;
    double eur_today;
    double eur_week;
    double eur_month;
    int    tokens_in_month;
    int    tokens_out_month;
} cos_cost_log_agg_t;

/* Reads ~/.cos/cost_log.jsonl and fills `out` with today / 7-day /
 * 30-day totals.  Returns 0 on success, -1 if the file cannot be
 * opened (in which case `out` is zeroed).  Never faults. */
int cos_cost_log_aggregate(cos_cost_log_agg_t *out);

#ifdef __cplusplus
}
#endif

#endif /* COS_COST_LOG_H */
