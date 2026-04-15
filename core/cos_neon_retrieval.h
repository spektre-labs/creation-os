/* SPDX-License-Identifier: AGPL-3.0-or-later
 * NEON-accelerated row scan: argmin Hamming distance vs a bank of hypervectors.
 * Memory layout: row-major, n_rows × COS_W uint64_t.
 */
#ifndef COS_NEON_RETRIEVAL_H
#define COS_NEON_RETRIEVAL_H

#include "cos_neon_hamming.h"
#include <stdint.h>
#include <stddef.h>

static inline int cos_hv_argmin_hamming_rows_hw(const uint64_t *query, const uint64_t *mem, int n_rows)
{
    if (n_rows < 1)
        return -1;
    uint32_t best_h = UINT32_MAX;
    int best_i = 0;
    for (int r = 0; r < n_rows; r++) {
        if (r + 4 < n_rows)
            __builtin_prefetch((const void *)(mem + (size_t)(r + 4) * (size_t)COS_W), 0, 3);
        uint32_t h = cos_hv_hamming_hw(query, mem + (size_t)r * (size_t)COS_W);
        if (h < best_h) {
            best_h = h;
            best_i = r;
        }
    }
    return best_i;
}

#endif /* COS_NEON_RETRIEVAL_H */
