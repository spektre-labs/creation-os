/*
 * σ-measured skill distillation — SQLite persistence + BSC trigger match.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SKILL_DISTILL_H
#define COS_SIGMA_SKILL_DISTILL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_think_result;

#define COS_SKILL_MAX_STEPS 8

struct cos_skill {
    uint64_t skill_hash;
    char     name[128];
    char     trigger_pattern[256];
    char     steps[COS_SKILL_MAX_STEPS][1024];
    int      n_steps;
    float    sigma_mean;
    float    reliability;
    int      times_used;
    int      times_succeeded;
    int64_t  created_ms;
    int64_t  last_used_ms;
};

/** Build a skill record from a completed think run (does not write DB). */
int cos_skill_distill(const struct cos_think_result *result,
                      struct cos_skill *skill_out);

/** INSERT OR REPLACE skill + steps (requires prior cos_skill_open). */
int cos_skill_save(const struct cos_skill *skill);

/**
 * Best Hamming-neighbour among non-archived rows.
 * Threshold is normalised Hamming distance in [0,1] (same as inference_cache).
 * Returns heap-allocated row or NULL — free with cos_skill_free().
 */
struct cos_skill *cos_skill_lookup(const uint64_t *bsc_goal,
                                   float threshold_norm);

void cos_skill_free(struct cos_skill *p);

/** Optional: reset active-skill bookkeeping before a think run. */
void cos_skill_begin_session(void);

/** Hash from last successful cos_skill_lookup in this process (0 if none). */
uint64_t cos_skill_last_lookup_hash(void);

void cos_skill_clear_lookup_hash(void);

int cos_skill_update_reliability(uint64_t skill_hash, int succeeded);

/** Mark archived=1 (soft retire). */
int cos_skill_retire(uint64_t skill_hash);

/**
 * List non-archived skills into caller buffer; writes *n_found on success.
 */
int cos_skill_list(struct cos_skill *skills, int max_skills, int *n_found);

int cos_skill_fetch(uint64_t skill_hash, struct cos_skill *out);

/** Archive low-reliability skills (reliability < 0.5 after ≥5 uses). */
int cos_skill_retire_low_reliability(void);

/** Archive unused for ~30 days (wall-clock ms). */
int cos_skill_archive_stale(int64_t now_ms, int64_t max_age_ms);

/** Flag rows where sigma_mean > tau_ref for operator review. */
int cos_skill_flag_sigma_drift(float tau_ref);

int cos_skill_maintain(float tau_ref, int64_t now_ms);

int cos_skill_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_SKILL_DISTILL_H */
