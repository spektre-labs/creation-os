/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Creation OS v27 — tokenizer tier API (portable C11).
 * See docs/VOCAB_PIPELINE_V27.md for scope vs roadmap.
 */
#ifndef COS_TOKENIZER_H
#define COS_TOKENIZER_H

#include <stdint.h>

#include "core/cos_bsc.h"

#define COS_TOK_MAX_IDS 512
#define COS_TOK_BYTE_TABLE 256

typedef struct {
    uint32_t n_merges;
    uint32_t left[64];
    uint32_t right[64];
} BpeMergeTable;

void bpe_default_merges(BpeMergeTable *t);
int bpe_encode_greedy(const char *text, uint32_t *ids, int max_ids, const BpeMergeTable *merges);
void bpe_id_to_hypervector(uint32_t id, uint64_t seed, uint64_t *hv);

void byte_codebook_build(uint64_t seed);
void byte_symbol_hypervector(uint8_t b, uint64_t *hv);
void byte_bundle(const uint8_t *data, int len, uint64_t *out);
/** Tier-2: sliding 3-byte MAJ window (pads tail by repeating last byte). */
void byte_bundle_maj_sliding(const uint8_t *data, int len, uint64_t *out);

int gda27_encode_uint(uint32_t v, char *out27, int cap);
uint32_t gda27_decode_uint(const char *in27);

#endif /* COS_TOKENIZER_H */
