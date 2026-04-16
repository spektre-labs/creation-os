/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os_native_m4.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

size_t cos_aligned_size_up(size_t n, size_t align)
{
    if (align == 0u)
        return n;
    if (align > SIZE_MAX / 2u + 1u)
        return 0u;
    if (n > SIZE_MAX - (align - 1u))
        return 0u;
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
    if ((size_t)vocab > SIZE_MAX / sizeof(float)) {
        *reputation_bytes = 0u;
        *logits_bytes = 0u;
        return;
    }
    {
        const size_t logits_raw = (size_t)vocab * sizeof(float);
        const size_t rep = cos_aligned_size_up((size_t)vocab, 64u);
        const size_t flt = cos_aligned_size_up(logits_raw, 64u);
        if (rep == 0u || flt == 0u) {
            *reputation_bytes = 0u;
            *logits_bytes = 0u;
            return;
        }
        *reputation_bytes = rep;
        *logits_bytes = flt;
    }
}

int cos_lw_buffers_alloc(int vocab, cos_lw_owned_buffers_t *out)
{
    if (!out)
        return -1;
    memset(out, 0, sizeof(*out));
    if (vocab <= 0)
        return -1;

    size_t rb, lb;
    cos_lw_buffer_sizes(vocab, &rb, &lb);
    if (rb == 0u || lb == 0u)
        return -1;

    out->reputation = (uint8_t *)aligned_alloc(64, rb);
    out->logits = (float *)aligned_alloc(64, lb);
    if (!out->reputation || !out->logits) {
        cos_lw_buffers_free(out);
        return -1;
    }
    out->reputation_bytes = rb;
    out->logits_bytes = lb;
    memset(out->logits, 0, lb);
    return 0;
}

void cos_lw_buffers_free(cos_lw_owned_buffers_t *b)
{
    if (!b)
        return;
    free(b->reputation);
    free(b->logits);
    memset(b, 0, sizeof(*b));
}
