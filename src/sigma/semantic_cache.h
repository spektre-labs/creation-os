/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * In-memory exact-match cache on normalized prompt (FNV-1a + 256-slot
 * recency).  Optional; disable with COS_SEMANTIC_LRU_CACHE=0.
 */
#ifndef COS_SEMANTIC_CACHE_H
#define COS_SEMANTIC_CACHE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t cos_semantic_cache_prompt_hash(const char *prompt);

/** On hit returns σ in [0,1] and copies response; returns -1.f on miss. */
float cos_semantic_cache_lookup(uint64_t hash, char *response_out,
                                size_t response_cap);

void cos_semantic_cache_store(uint64_t hash, float sigma,
                              const char *response);

#ifdef __cplusplus
}
#endif

#endif /* COS_SEMANTIC_CACHE_H */
