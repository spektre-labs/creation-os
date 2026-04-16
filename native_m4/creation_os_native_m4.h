/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_NATIVE_M4_H
#define CREATION_OS_NATIVE_M4_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Round n up to a positive alignment (e.g. 64 for aligned_alloc). */
size_t cos_aligned_size_up(size_t n, size_t align);

/* Padded buffer sizes for reputation + logits (C11 aligned_alloc-friendly). */
void cos_lw_buffer_sizes(int vocab, size_t *reputation_bytes, size_t *logits_bytes);

/* Living weights: popcount(reputation[id]) -> logit bias (inplace). */
void cos_living_weights_inplace(float *logits, const uint8_t *reputation, int vocab, float scale);

/* NEON path (AArch64). On Apple with NEON, vocab >= 65536 uses parallel + tail merge. */
void cos_living_weights_neon(float *logits, const uint8_t *reputation, int vocab, float scale);

/* NEON path over a half-open index range [begin, end). */
void cos_living_weights_neon_range(float *logits, const uint8_t *reputation, int begin, int end, float scale);

/* NEON + GCD parallelization for large vocabs (Apple AArch64 NEON only).
 * For vocab < 65536, or non-Apple / no NEON, behaves like cos_living_weights_neon_range. */
void cos_living_weights_neon_parallel(float *logits, const uint8_t *reputation, int vocab, float scale);

/* Metal path (Apple). Returns false if Metal is unavailable or metallib missing. */
bool cos_living_weights_metal(float *logits, const uint8_t *reputation, int vocab, float scale);

/* Best-effort runtime probe (macOS only). */
bool cos_runtime_has_sme(void);

#ifdef __cplusplus
}
#endif

#endif

