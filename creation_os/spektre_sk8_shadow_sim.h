/* Spektre — SK8 hypothetical token σ (shadow kernel copy). Does not modify superkernel_v8.c. */
#ifndef SPEKTRE_SK8_SHADOW_SIM_H
#define SPEKTRE_SK8_SHADOW_SIM_H

#include "superkernel_v8.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Clone kernel, commit one UTF-8 piece, return resulting sigma (for top-k screening).
 * Returns 0 on success, -1 on alloc failure.
 */
int spektre_sk8_shadow_commit_sigma(const sk8_kernel_t *baseline, const char *piece_utf8, int *sigma_after);

#ifdef __cplusplus
}
#endif

#endif
