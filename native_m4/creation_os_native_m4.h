/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_NATIVE_M4_H
#define CREATION_OS_NATIVE_M4_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Living weights: popcount(reputation[id]) -> logit bias (inplace). */
void cos_living_weights_inplace(float *logits, const uint8_t *reputation, int vocab, float scale);

/* NEON path (AArch64). */
void cos_living_weights_neon(float *logits, const uint8_t *reputation, int vocab, float scale);

/* NEON path over a half-open index range [begin, end). */
void cos_living_weights_neon_range(float *logits, const uint8_t *reputation, int begin, int end, float scale);

/* NEON + GCD parallelization for large vocabs (best-effort). */
void cos_living_weights_neon_parallel(float *logits, const uint8_t *reputation, int vocab, float scale);

/* Metal path (Apple). Returns false if Metal is unavailable or metallib missing. */
bool cos_living_weights_metal(float *logits, const uint8_t *reputation, int vocab, float scale);

/* Best-effort runtime probe (macOS only). */
bool cos_runtime_has_sme(void);

#ifdef __cplusplus
}
#endif

#endif

