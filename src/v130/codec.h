/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v130 σ-Codec — compression layer for peer communication.
 *
 * Purpose: v128 mesh + v129 federated move data between nodes.
 * v130 compresses everything on the wire with σ-adaptive ratios.
 *
 * Three sub-codecs, all pure C / deterministic:
 *
 *   A.  FP4 LoRA-delta packer.  Signed magnitude, 4 bits/value
 *       (1 sign + 3 exponent/mantissa), shared per-tensor scale.
 *       8× compression vs fp32; round-trip RMSE < 0.1·scale on
 *       ~Gaussian deltas.  See `cos_v130_quantize_fp4`.
 *
 *   B.  σ-profile packer.  8 floats in [0,1] ↔ 8 uint8 (×255).
 *       4× compression, round-trip error ≤ 1/255.
 *
 *   C.  Product Quantization (PQ) for 2568-d embeddings
 *       (v126 σ-embed).  M=8 subspaces × 321 dims, K=128 codes
 *       per subspace → 8 bytes/vector (vs 10 272 B fp32 = 1284×),
 *       trained by k-means-style Lloyd iterations with fixed seeds.
 *
 *   D.  σ-aware context compression.  Rank chunks by σ_product and
 *       allocate character budget: low-σ chunks (confident) get
 *       short summaries, high-σ chunks (uncertain) keep more raw
 *       context because the downstream reasoner needs the detail.
 *
 * v130.0 is framework-free: no zlib, no lz4.  v130.1 can layer an
 * entropy coder (ANS/zstd) on top without breaking the contract.
 */
#ifndef COS_V130_CODEC_H
#define COS_V130_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V130_SIGMA_DIM             8
#define COS_V130_FP4_BINS             16

#define COS_V130_EMBED_DIM          2568    /* must match v126 */
#define COS_V130_PQ_SUBSPACES          8
#define COS_V130_PQ_CODES            128
/* 2568 / 8 = 321 dims per subspace */
#define COS_V130_PQ_SUB_DIM \
    (COS_V130_EMBED_DIM / COS_V130_PQ_SUBSPACES)

/* ---------- FP4 LoRA-delta codec ---------------------------------- */

/* Quantize `n` floats to FP4 (4 bits / value, packed 2-per-byte)
 * with a shared signed-magnitude scale.  Returns the per-tensor
 * absolute scale (max |x|) so the decoder can reconstruct.
 * `packed` must be at least ((n + 1) / 2) bytes. */
float cos_v130_quantize_fp4(const float *in, int n, uint8_t *packed);

void  cos_v130_dequantize_fp4(const uint8_t *packed, int n,
                              float scale, float *out);

/* ---------- σ-profile packer -------------------------------------- */

void  cos_v130_pack_sigma  (const float *channels, uint8_t *out);
void  cos_v130_unpack_sigma(const uint8_t *codes, float *channels);

/* ---------- Product Quantization for 2568-d embeddings ------------ */

typedef struct cos_v130_pq_codebook {
    float centroids[COS_V130_PQ_SUBSPACES][COS_V130_PQ_CODES][COS_V130_PQ_SUB_DIM];
    int   trained;
} cos_v130_pq_codebook_t;

/* Train the codebook by mini-Lloyd iterations on the stacked training
 * vectors.  Deterministic — `seed` feeds the SplitMix64 init.
 * Returns 0 on ok, -1 on bad args.  `iters` small (e.g. 8) — this is
 * a stand-in until v130.1 adds a proper PQ trainer. */
int   cos_v130_pq_train(cos_v130_pq_codebook_t *cb,
                        const float *vectors, int n_vectors,
                        int iters, uint64_t seed);

/* Encode a single 2568-d vector into 8 uint8 codes (one per
 * subspace). */
int   cos_v130_pq_encode(const cos_v130_pq_codebook_t *cb,
                         const float *vec,
                         uint8_t *out_codes /* COS_V130_PQ_SUBSPACES */);

int   cos_v130_pq_decode(const cos_v130_pq_codebook_t *cb,
                         const uint8_t *codes, float *out_vec);

/* ---------- σ-aware context compression --------------------------- */

typedef struct cos_v130_chunk {
    char  text[256];
    float sigma_product;   /* 0 = confident, 1 = very uncertain */
} cos_v130_chunk_t;

/* Compress an array of chunks into `out` under total byte budget.
 * Allocation rule (deterministic):
 *   base = budget / n
 *   w_i  = (1 − 0.5) + σ_i           // range [0.5, 1.5]
 *   alloc_i = round(base · w_i / mean(w))
 * Each chunk's text is truncated to its allocation (with an ellipsis
 * if cut).  Returns bytes written (excluding NUL), or -1 on error. */
int   cos_v130_compress_context(const cos_v130_chunk_t *chunks, int n,
                                size_t total_budget,
                                char *out, size_t cap);

/* ---------- Stats / JSON ------------------------------------------- */

typedef struct cos_v130_fp4_stats {
    int   n;
    float scale;
    float rmse;          /* on the round-trip */
    float rel_rmse;      /* rmse / scale */
} cos_v130_fp4_stats_t;

int   cos_v130_fp4_stats_to_json(const cos_v130_fp4_stats_t *s,
                                 char *out, size_t cap);

int   cos_v130_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
