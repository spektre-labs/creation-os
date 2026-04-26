/* In-memory prompt → σ cache for cos-chat (optional semantic shortcut).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_RESPONSE_CACHE_H
#define COS_RESPONSE_CACHE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t cos_response_cache_prompt_hash(const char *prompt);

/** Return cached σ in [0,1] on hit; -1.f on miss. response_out optional. */
float cos_response_cache_lookup(uint64_t hash, char *response_out, int max_len);

void cos_response_cache_store(uint64_t hash, float sigma, const char *response);

#ifdef __cplusplus
}
#endif
#endif /* COS_RESPONSE_CACHE_H */
