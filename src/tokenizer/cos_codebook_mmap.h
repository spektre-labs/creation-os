/* SPDX-License-Identifier: AGPL-3.0-or-later
 * mmap-backed 4096-bit hypervector codebook (Tier-1 table path).
 * On-disk layout is little-endian u32 header + row_bytes * n_vocab rows.
 * See docs/VOCAB_PIPELINE_V27.md for scope vs training artifacts.
 */
#ifndef COS_CODEBOOK_MMAP_H
#define COS_CODEBOOK_MMAP_H

#include <stddef.h>
#include <stdint.h>

#include "core/cos_bsc.h"

#define COS_CB_MAGIC 0x434f5342u /* "COSB" when stored LE on little-endian hosts */
#define COS_CB_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t n_vocab;
    uint32_t row_bytes; /* must be COS_W * sizeof(uint64_t) */
} CosCodebookHeader;

typedef struct CosCodebook CosCodebook;

/** Deterministic row fill (same bits as written by tools/gen_cos_codebook.c). */
void cos_codebook_row_fill(uint32_t id, uint64_t *hv /* COS_W words */);

/** Write a valid codebook file (header + rows). Returns 0 on success. */
int cos_codebook_write_file(const char *path, uint32_t n_vocab);

CosCodebook *cos_codebook_mmap_open(const char *path);
void cos_codebook_mmap_close(CosCodebook *cb);

uint32_t cos_codebook_n_vocab(const CosCodebook *cb);

/** Copy row id%n_vocab into hv (full COS_W words). Returns 0 on success. */
int cos_codebook_lookup(const CosCodebook *cb, uint32_t id, uint64_t *hv);

#endif /* COS_CODEBOOK_MMAP_H */
