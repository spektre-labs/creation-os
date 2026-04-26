/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Graded prompt bank + deterministic curriculum (high-σ domains weighted).
 */
#ifndef COS_OMEGA_PROMPT_BANK_H
#define COS_OMEGA_PROMPT_BANK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_PB_MAX        512
#define COS_PB_PROMPT_LEN 512
#define COS_PB_ANS_LEN    256
#define COS_PB_CAT_LEN    64

typedef struct cos_banked_prompt {
    char  prompt[COS_PB_PROMPT_LEN];
    char  answer[COS_PB_ANS_LEN];
    char  category[COS_PB_CAT_LEN];
    float last_sigma;
    int   times_seen;
    int   times_correct;
} cos_banked_prompt_t;

typedef struct cos_prompt_bank {
    cos_banked_prompt_t rows[COS_PB_MAX];
    int                 n;
} cos_prompt_bank_t;

/** Load from CREATION_OS_ROOT/benchmarks/graded/graded_prompts.csv or cwd. */
int cos_prompt_bank_load(cos_prompt_bank_t *pb);

/** Reset per-episode de-duplication state. */
void cos_prompt_bank_episode_begin(cos_prompt_bank_t *pb);

/**
 * Pick next prompt index (0..pb->n-1). Deterministic from (ep,t,salt).
 * `seen_hashes` / `n_seen` block repeats within the episode.
 * Curriculum: ~70% weight on rows with highest `last_sigma` in category.
 */
int cos_prompt_bank_pick(cos_prompt_bank_t *pb, int ep, int t,
                         const uint64_t *seen_hashes, int n_seen,
                         uint64_t *out_hash);

/** Grade response vs expected key (ANY / IMPOSSIBLE / factual). Returns 0/1. */
int cos_prompt_bank_grade(const char *expected, const char *category,
                          const char *response);

#ifdef __cplusplus
}
#endif
#endif /* COS_OMEGA_PROMPT_BANK_H */
