/* SPDX-License-Identifier: AGPL-3.0-or-later
 * K-agent hypervector consensus — per-bit majority vote (odd K only).
 * For K==3, bit-majority is identical to per-lane MAJ3 → cos_hv_maj3_hw (AArch64) / cos_hv_maj3.
 * For K>3, distributed vote (scalar bit loop).
 */
#ifndef COS_PARLIAMENT_H
#define COS_PARLIAMENT_H

#include "cos_neon_hamming.h"
#include <stddef.h>

/* agents layout: row-major, agent k occupies words [k*COS_W .. (k+1)*COS_W). */
static inline void cos_hv_bit_parliament(uint64_t *out, const uint64_t *agents, int K)
{
    if (K < 1 || (K % 2) == 0)
        return;
    if (K == 3) {
#if defined(__aarch64__)
        cos_hv_maj3_hw(out, agents, agents + (size_t)COS_W, agents + (size_t)2 * (size_t)COS_W);
#else
        cos_hv_maj3(out, agents, agents + (size_t)COS_W, agents + (size_t)2 * (size_t)COS_W);
#endif
        return;
    }
    for (int w = 0; w < COS_W; w++) {
        uint64_t ow = 0;
        for (int bit = 0; bit < 64; bit++) {
            int cnt = 0;
            for (int k = 0; k < K; k++)
                cnt += (int)((agents[(size_t)k * (size_t)COS_W + (size_t)w] >> bit) & 1);
            if (cnt + cnt > K)
                ow |= (1ULL << bit);
        }
        out[w] = ow;
    }
}

#endif /* COS_PARLIAMENT_H */
