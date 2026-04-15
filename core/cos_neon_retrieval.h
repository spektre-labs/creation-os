/* SPDX-License-Identifier: AGPL-3.0-or-later
 * NEON-accelerated row scan: argmin Hamming distance vs a bank of hypervectors.
 * Memory layout: row-major, n_rows × COS_W uint64_t.
 * Hot loop: 2× row unroll + deep prefetch (memory-bound scan).
 */
#ifndef COS_NEON_RETRIEVAL_H
#define COS_NEON_RETRIEVAL_H

#include "cos_neon_hamming.h"
#include <stddef.h>
#include <stdint.h>

static inline int cos_hv_argmin_hamming_rows_hw(const uint64_t *query, const uint64_t *mem, int n_rows)
{
    if (n_rows < 1)
        return -1;
    uint32_t best_h = UINT32_MAX;
    int      best_i = 0;
    int      r        = 0;
    for (; r + 1 < n_rows; r += 2) {
        if (r + 8 < n_rows) {
            __builtin_prefetch((const void *)(mem + (size_t)(r + 8) * (size_t)COS_W), 0, 3);
            __builtin_prefetch((const void *)(mem + (size_t)(r + 9) * (size_t)COS_W), 0, 3);
        }
        uint32_t h0 = cos_hv_hamming_hw(query, mem + (size_t)r * (size_t)COS_W);
        uint32_t h1 = cos_hv_hamming_hw(query, mem + (size_t)(r + 1) * (size_t)COS_W);
        if (h0 < best_h) {
            best_h = h0;
            best_i = r;
        }
        if (h1 < best_h) {
            best_h = h1;
            best_i = r + 1;
        }
    }
    if (r < n_rows) {
        uint32_t h = cos_hv_hamming_hw(query, mem + (size_t)r * (size_t)COS_W);
        if (h < best_h) {
            best_h = h;
            best_i = r;
        }
    }
    return best_i;
}

#endif /* COS_NEON_RETRIEVAL_H */
