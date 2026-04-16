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

/* Best-effort runtime probe (macOS only). */
bool cos_runtime_has_sme(void);

#ifdef __cplusplus
}
#endif

#endif

