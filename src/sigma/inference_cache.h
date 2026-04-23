/*
 * Semantic inference cache — BSC Hamming similarity (creation-os edge layer).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_INFERENCE_CACHE_H
#define COS_SIGMA_INFERENCE_CACHE_H

#include <stddef.h>
#include <stdint.h>

#ifndef COS_INF_W
#define COS_INF_W 64
#endif

struct cos_inference_cache_entry {
    uint64_t prompt_hash;
    uint64_t bsc_vector[COS_INF_W];
    char     response[4096];
    float    sigma;
    float    similarity_threshold;
    int      hit_count;
    int64_t  timestamp_ms;
    int64_t  latency_original_ms;
};

/** FNV-1a prompt id (distinct from σ-engram hash). */
uint64_t cos_inference_prompt_fnv(const char *utf8);

/** Encode UTF-8 prompt into a D=4096 bipolar-style packed vector (COS_INF_W words). */
void cos_inference_bsc_encode_prompt(const char *utf8, uint64_t out[COS_INF_W]);

/** Normalised Hamming distance in [0,1]: popcount(a^b) / 4096. */
float cos_inference_hamming_norm(const uint64_t *a, const uint64_t *b, int n_words);

int cos_inference_cache_init(int max_entries);

/** Save ring + stats to path (caller passes NULL → ~/.cos/inference_cache.bin). */
int cos_inference_cache_save(void);

/** Load cache from default path on init (best-effort). */
int cos_inference_cache_load(void);

/** Clear all entries + stats + optional unlink persist file when drop_file!=0. */
void cos_inference_cache_clear(int drop_file);

/**
 * Lookup closest entry with hamming_norm < threshold band:
 *   strong  < tau_strong (default 0.35)
 *   weak    < tau_weak   (default 0.45) → σ bumped more
 */
int cos_inference_cache_lookup(const uint64_t *bsc_prompt, int D_words,
                               struct cos_inference_cache_entry *hit_out);

/**
 * Nearest-neighbour σ for speculative prediction only: does not mutate
 * statistics, hit counts, or ring slots.
 */
int cos_inference_cache_peek_nearest(const uint64_t *bsc_prompt, int D_words,
                                     float *sigma_out, float *hamming_norm_out);

int cos_inference_cache_store(const uint64_t *bsc_prompt,
                              const char *response, float sigma);

/** Extended store with measured generation latency for savings accounting. */
int cos_inference_cache_store_latency(const uint64_t *bsc_prompt,
                                      const char *response, float sigma,
                                      int64_t latency_original_ms);

void cos_inference_cache_stats(int *total, int *hits, int *misses,
                               float *avg_latency_saved_ms);

/** Print up to max_lines nearest neighbours for debugging (Hamming norm). */
void cos_inference_cache_dump_similar(const char *prompt_utf8, int max_lines);

int cos_inference_cache_self_test(void);

#endif /* COS_SIGMA_INFERENCE_CACHE_H */
