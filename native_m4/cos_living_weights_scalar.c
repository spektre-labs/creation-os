/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os_native_m4.h"

void cos_living_weights_inplace(float *logits, const uint8_t *reputation, int vocab, float scale)
{
    if (!logits || !reputation || vocab <= 0)
        return;
    for (int i = 0; i < vocab; i++) {
        int pc = __builtin_popcount((unsigned)reputation[i]);
        logits[i] += ((float)pc - 4.0f) * scale;
    }
}
