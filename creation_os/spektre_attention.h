/* CREATION OS — Manifold + logit scorch (CC BY 4.0) */
#ifndef SPEKTRE_ATTENTION_H
#define SPEKTRE_ATTENTION_H

#include "superkernel_v8.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*spektre_attention_fracture_fn)(void * user);

void spektre_attention_apply(
        kernel_t * k,
        sk8_logit_row_t * rows,
        size_t n_rows,
        int32_t coherence_token_id,
        int32_t eos_token_id,
        spektre_attention_fracture_fn on_fracture,
        void * fracture_user,
        sk8_manifold_mask_t * out_final_mask);

/** Build logit-level scorch (firmware) / boost (manifest) tables; copies arrays. */
void spektre_logits_tables_init(int32_t n_vocab, const uint8_t * scorch, const uint8_t * boost);

void spektre_logits_tables_teardown(void);

/** Dense logits [n_vocab] (writable), e.g. from llama_get_logits_ith. */
void spektre_mask_logits(float * logits, int32_t n_vocab);

/** Sparse candidate rows (same layout as llama_token_data). */
void spektre_mask_logits_rows(sk8_logit_row_t * rows, size_t n_rows);

/** Dream / consolidation: OR shadow counts into permanent scorch table (min_count inclusive). */
void spektre_merge_shadow_into_scorch(const uint32_t * shadow_counts, int32_t n_vocab, uint32_t min_count);

#ifdef __cplusplus
}
#endif

#endif /* SPEKTRE_ATTENTION_H */
