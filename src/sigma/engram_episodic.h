/*
 * Persistent episodic → semantic memory layer (SQLite).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_ENGRAM_EPISODIC_H
#define COS_SIGMA_ENGRAM_EPISODIC_H

#include "error_attribution.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_engram_episode {
    int64_t timestamp_ms;
    uint64_t prompt_hash;
    float sigma_combined;
    int action;
    int was_correct;
    enum cos_error_source attribution;
    int ttt_applied;
    int turn_timeout; /* 1 if Ω-loop wall-clock turn budget exceeded */
};

struct cos_engram_semantic {
    uint64_t pattern_hash;
    float sigma_mean;
    int encounter_count;
    float reliability;
    float tau_local;
};

uint64_t cos_engram_prompt_hash(const char *prompt_utf8);

int cos_engram_episode_store(const struct cos_engram_episode *ep);

int cos_engram_consolidate(int n_recent);

struct cos_engram_semantic *cos_engram_query_semantic(uint64_t prompt_hash);

float cos_engram_get_local_tau(uint64_t domain_hash);

int cos_engram_forget(int64_t older_than_ms, float reliability_below);

void cos_engram_sqlite_shutdown(void);

int cos_engram_episodic_self_test(void);

/** Top semantic rows by descending σ_mean (highest uncertainty first). */
int cos_engram_semantic_weakest(uint64_t *pattern_hashes, float *sigma_means,
                                int max, int *n_out);

/** Recent episodes, newest first (prompt text not stored; hash only). */
int cos_engram_episode_fetch_recent(struct cos_engram_episode *eps, int max,
                                    int *n_out);

/** Random semantic pattern hashes (for anti-forgetting probes). */
int cos_engram_semantic_sample_hashes(uint64_t *pattern_hashes, int max,
                                      int *n_out);

/**
 * Merge web-learned evidence into semantic row. verified=1 applies a
 * stronger σ pull and reliability bump.
 */
int cos_engram_semantic_merge_learn(uint64_t pattern_hash, float evidence_sigma,
                                    int verified);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_ENGRAM_EPISODIC_H */
