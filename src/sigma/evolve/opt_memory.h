/*
 * σ-Evolve optimisation memory.
 *
 * Every mutation attempt — accepted or reverted — is recorded here.
 * The next generation's mutator can query top-N accepted mutations
 * for a given kernel and feed them as few-shot guidance to whichever
 * mutator backend the caller is using (random, LLM, search, etc.).
 *
 * Schema (opt_mutations v1):
 *   id          INTEGER PRIMARY KEY
 *   ts          INTEGER  — unix seconds, wall-clock
 *   kernel      TEXT     — target kernel id (e.g. "sigma_router")
 *   mutation    TEXT     — human description ("perturb w_logprob +0.03")
 *   brier_before REAL
 *   brier_after  REAL
 *   delta        REAL    — brier_after - brier_before  (negative = good)
 *   accepted     INTEGER  — 0 or 1
 *   code_diff    TEXT     — opaque blob (unified diff, JSON patch, …)
 *
 * Pure-C wrapper around sqlite3; no global state, no thread hazard
 * beyond sqlite3's own.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_OPT_MEMORY_H
#define COS_SIGMA_OPT_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_opt_memory_s cos_opt_memory_t;

typedef struct {
    int64_t id;
    int64_t ts;
    char    kernel[64];
    char    mutation[256];
    double  brier_before;
    double  brier_after;
    double  delta;
    int     accepted;
    /* code_diff not copied into row on list(); fetch explicitly. */
} cos_opt_memory_row_t;

typedef struct {
    int    n_total;
    int    n_accepted;
    double best_delta;          /* most negative delta seen             */
    double mean_delta_accepted; /* mean over accepted only              */
} cos_opt_memory_stats_t;

/* Open / create memory at `path`; schema is migrated idempotently.
 * Caller owns the handle; close with cos_opt_memory_close. */
int  cos_opt_memory_open (const char *path, cos_opt_memory_t **out);
void cos_opt_memory_close(cos_opt_memory_t *m);

int  cos_opt_memory_record(cos_opt_memory_t *m,
                           const char *kernel,
                           const char *mutation,
                           double brier_before,
                           double brier_after,
                           int    accepted,
                           const char *code_diff);

/* Fill `rows[0 .. cap-1]` with the top-N accepted mutations for a
 * kernel (sorted by delta ascending — most-negative first).  Returns
 * number written; negative on error.  kernel==NULL matches all. */
int  cos_opt_memory_top_accepted(cos_opt_memory_t *m,
                                 const char *kernel,
                                 cos_opt_memory_row_t *rows, int cap);

int  cos_opt_memory_stats(cos_opt_memory_t *m,
                          const char *kernel,
                          cos_opt_memory_stats_t *out);

int  cos_sigma_opt_memory_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_OPT_MEMORY_H */
