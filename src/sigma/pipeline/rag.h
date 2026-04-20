/*
 * σ-RAG — local retrieval-augmented generation with σ-filtering.
 *
 * 2026-era RAG lives in the cloud (Pinecone, Weaviate, Qdrant
 * SaaS).  Creation OS runs it on the user's own disk: the user's
 * corpus never leaves the machine, and every retrieval carries a
 * σ that governs whether the chunk is allowed into the prompt.
 *
 * Design
 * ------
 *   * Chunker: sliding window over words of size chunk_words with
 *     chunk_overlap overlap (defaults 128 / 16).  Chunks are
 *     normalised (ASCII-folded, lowered, whitespace collapsed)
 *     before embedding.
 *   * Embedder: deterministic bag-of-hashed-ngrams into a
 *     128-dimensional unit vector.  No GPU, no network, no model
 *     weights — we trade semantic depth for offline reproducibility
 *     and zero dependencies.  Callers who want a real MiniLM /
 *     BGE / Jina model plug in via σ-Plugin.
 *   * Index: caller-owned array of chunks; cosine similarity via
 *     a single-pass scan (N ≤ 100 k is fine, the σ-pipeline does
 *     < 1 M chunks per laptop anyway).  If a persistence backend
 *     is wanted, σ-Persist (PROD-3) is the companion kernel.
 *   * σ-filter:
 *         σ_retrieval = clip(1 - cosine_similarity, [0, 1])
 *     so low σ means "highly relevant" and a chunk is admitted to
 *     the prompt iff σ_retrieval ≤ τ_rag.  Default τ_rag = 0.50
 *     matches the rest of the pipeline.
 *
 * Contracts (v0)
 * --------------
 *   1. Deterministic: given the same chunks and query, search
 *      returns a byte-identical ranking on any tier-1 host.
 *   2. Pure: embedding is a pure function of the text; the index
 *      adds, removes, and queries but never mutates on read.
 *   3. Bounded memory: COS_RAG_MAX_CHUNKS chunks and
 *      COS_RAG_MAX_TEXT bytes per chunk; overflow flips a
 *      truncated flag, never silently dropped.
 *   4. σ bounds: every emitted σ_retrieval is clip01'd.
 *   5. No I/O in the hot path: loading from disk is a separate
 *      append call the caller orchestrates.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_RAG_H
#define COS_SIGMA_RAG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COS_RAG_DIM
#define COS_RAG_DIM          128
#endif
#ifndef COS_RAG_MAX_CHUNKS
#define COS_RAG_MAX_CHUNKS   4096
#endif
#ifndef COS_RAG_MAX_TEXT
#define COS_RAG_MAX_TEXT     (16 * 1024)
#endif
#ifndef COS_RAG_TAU_DEFAULT
#define COS_RAG_TAU_DEFAULT  0.50f
#endif
#ifndef COS_RAG_CHUNK_WORDS_DEFAULT
#define COS_RAG_CHUNK_WORDS_DEFAULT 128
#endif
#ifndef COS_RAG_CHUNK_OVERLAP_DEFAULT
#define COS_RAG_CHUNK_OVERLAP_DEFAULT 16
#endif

enum cos_rag_status {
    COS_RAG_OK       =  0,
    COS_RAG_ERR_ARG  = -1,
    COS_RAG_ERR_OOM  = -2,
    COS_RAG_ERR_FULL = -3,
};

typedef struct {
    char    *text;                 /* normalised chunk text, owned */
    float    embedding[COS_RAG_DIM];
    float    sigma_at_index;       /* σ observed when indexed      */
    char    *source_file;
    int      chunk_id;
    uint64_t insertion_order;
} cos_rag_chunk_t;

typedef struct {
    int                 chunk_id;
    float               score;      /* cosine ∈ [-1, 1]             */
    float               sigma_retrieval; /* 1 - cosine, clip01      */
    const cos_rag_chunk_t *chunk;   /* aliases the index; read-only */
} cos_rag_hit_t;

typedef struct {
    cos_rag_chunk_t *chunks;        /* dense array, size n_chunks   */
    int      n_chunks;
    int      cap;                   /* allocated capacity           */
    int      chunk_words;
    int      chunk_overlap;
    float    tau_rag;               /* σ-filter threshold           */
    uint64_t insertion_counter;
} cos_rag_index_t;

/* -------- Lifecycle ------------------------------------------------ */
int  cos_rag_index_init(cos_rag_index_t *idx,
                        int chunk_words, int chunk_overlap,
                        float tau_rag);
void cos_rag_index_free(cos_rag_index_t *idx);

/* -------- Pure helpers -------------------------------------------- */

/* Hash-based deterministic embedding.  `text` is normalised
 * (ASCII-fold, tolower, whitespace collapse) and hashed into a
 * COS_RAG_DIM-dimensional unit vector.  `out` must be
 * COS_RAG_DIM floats wide.  Returns 0 on success. */
int  cos_rag_embed(const char *text, float *out);

/* Cosine similarity of two unit-norm embeddings. */
float cos_rag_cosine(const float *a, const float *b);

/* σ_retrieval = clip01(1 - cosine). */
float cos_rag_sigma_retrieval(float cosine);

/* -------- Ingest / chunking --------------------------------------- */

/* Append a *single* pre-formed chunk; returns COS_RAG_OK or
 * COS_RAG_ERR_FULL. */
int cos_rag_append_chunk(cos_rag_index_t *idx,
                         const char *text,
                         const char *source_file,
                         float sigma_at_index);

/* Chunk `text` under (chunk_words, chunk_overlap) and append every
 * resulting chunk under `source_file`.  Returns the number of
 * chunks appended (≥ 0) or a negative status. */
int cos_rag_append_text(cos_rag_index_t *idx,
                        const char *text,
                        const char *source_file);

/* -------- Search -------------------------------------------------- */

/* Rank chunks by cosine similarity to `query`, σ-filter against
 * `idx->tau_rag` (filtered_out hits are still reported if
 * `include_filtered != 0` with sigma_retrieval > τ).  Writes
 * min(k, n_chunks) hits into `out` ranked by descending score.
 * `*out_n` receives the number written. */
int cos_rag_search(const cos_rag_index_t *idx,
                   const char *query,
                   int k,
                   int include_filtered,
                   cos_rag_hit_t *out,
                   int *out_n);

/* -------- Stats --------------------------------------------------- */
typedef struct {
    int   n_chunks;
    int   n_retained;   /* under current τ_rag                     */
    float mean_sigma_at_index;
    float mean_text_bytes;
} cos_rag_stats_t;

void cos_rag_stats(const cos_rag_index_t *idx, cos_rag_stats_t *out);

/* -------- Self-test ----------------------------------------------- */
int cos_rag_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_RAG_H */
