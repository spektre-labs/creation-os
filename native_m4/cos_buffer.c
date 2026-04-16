/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os_native_m4.h"

#include <stddef.h>

size_t cos_aligned_size_up(size_t n, size_t align)
{
    if (align == 0u)
        return n;
    return (n + align - 1u) / align * align;
}

void cos_lw_buffer_sizes(int vocab, size_t *reputation_bytes, size_t *logits_bytes)
{
    if (!reputation_bytes || !logits_bytes)
        return;
    if (vocab <= 0) {
        *reputation_bytes = 0u;
        *logits_bytes = 0u;
        return;
    }
    *reputation_bytes = cos_aligned_size_up((size_t)vocab, 64u);
    *logits_bytes = cos_aligned_size_up((size_t)vocab * sizeof(float), 64u);
}
