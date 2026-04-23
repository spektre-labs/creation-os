/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * σ-gated multi-teacher distillation: cloud teachers propose text;
 * local Creation OS measures σ on every string (never trust cloud σ).
 */

#ifndef COS_DISTILL_H
#define COS_DISTILL_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_distill_teacher {
    char name[64];
    char endpoint[256];
    int  available;
    float trust_ema;
    int  total_queries;
    int  best_count;
};

struct cos_distill_example {
    char    prompt[2048];
    char    best_response[4096];
    char    teacher[64];
    float   sigma_local;
    float   sigma_teacher;
    float   improvement;
    int64_t timestamp;
};

enum cos_distill_export_format {
    COS_DISTILL_EXPORT_JSONL     = 0,
    COS_DISTILL_EXPORT_ALPACA    = 1,
    COS_DISTILL_EXPORT_SHAREGPT  = 2,
};

int cos_distill_init(void);
void cos_distill_shutdown(void);

int cos_distill_add_teacher(const struct cos_distill_teacher *teacher);

/** Run single prompt. `only_teachers` is a comma-separated allow-list, or NULL for all. */
int cos_distill_run(const char *prompt, const char *only_teachers,
                    struct cos_distill_example *result);

int cos_distill_batch(const char *prompts_path, const char *only_teachers,
                      int *n_distilled, int *n_skipped);

int cos_distill_export_training_data(const char *output_path,
                                     int format);

float cos_distill_teacher_ranking(struct cos_distill_teacher *teachers,
                                  int max, int *n_found);

int cos_distill_stats(long *n_rows, double *avg_improvement,
                      double *avg_sigma_teacher);

/** Print last N stored examples (from examples.db) to a stream. */
int cos_distill_fprint_history(FILE *f, int max_rows);

/** Internal: fetch one teacher response (used by proconductor). */
int cos_distill_fetch_teacher_text(const struct cos_distill_teacher *t,
                                   const char *prompt,
                                   char *out, size_t out_cap);

/** Read-only view of registered teachers after init (0 .. count-1). */
int cos_distill_teacher_count(void);
const struct cos_distill_teacher *cos_distill_teacher_get(int idx);

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(CREATION_OS_DISTILL_TEST)
int cos_distill_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* COS_DISTILL_H */
