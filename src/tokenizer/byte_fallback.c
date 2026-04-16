/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include <string.h>

#include "cos_tokenizer.h"

static uint8_t pad_at(const uint8_t *data, int len, int idx)
{
    if (len <= 0)
        return 0;
    if (idx < len)
        return data[idx];
    return data[len - 1];
}

static uint64_t g_byte_table[COS_TOK_BYTE_TABLE][COS_W];

void byte_codebook_build(uint64_t seed)
{
    for (int b = 0; b < COS_TOK_BYTE_TABLE; b++) {
        uint64_t st = seed ^ ((uint64_t)b * 0xC6BC279692B5C323ULL);
        for (int i = 0; i < COS_W; i++) {
            st = st * 6364136223846793005ULL + 1442695040888963407ULL;
            g_byte_table[b][i] = st;
        }
    }
}

void byte_symbol_hypervector(uint8_t b, uint64_t *hv)
{
    memcpy(hv, g_byte_table[b], sizeof(uint64_t) * COS_W);
}

void byte_bundle(const uint8_t *data, int len, uint64_t *out)
{
    /* Tier-2 composition: XOR-bind chain over per-byte codebook rows (MAJ-window bundle is roadmap). */
    memset(out, 0, sizeof(uint64_t) * COS_W);
    for (int k = 0; k < len; k++) {
        uint64_t row[COS_W];
        byte_symbol_hypervector(data[k], row);
        for (int i = 0; i < COS_W; i++)
            out[i] ^= row[i];
    }
}

void byte_bundle_maj_sliding(const uint8_t *data, int len, uint64_t *out)
{
    memset(out, 0, sizeof(uint64_t) * COS_W);
    if (len <= 0)
        return;
    for (int k = 0; k < len; k++) {
        uint64_t a[COS_W], b[COS_W], c[COS_W], m[COS_W];
        byte_symbol_hypervector(pad_at(data, len, k), a);
        byte_symbol_hypervector(pad_at(data, len, k + 1), b);
        byte_symbol_hypervector(pad_at(data, len, k + 2), c);
        cos_hv_maj3(m, a, b, c);
        for (int i = 0; i < COS_W; i++)
            out[i] ^= m[i];
    }
}
