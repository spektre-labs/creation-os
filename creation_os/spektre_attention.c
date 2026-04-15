#include "spektre_attention.h"
#include "spektre_shadow.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int32_t g_n_vocab_tables = 0;
static uint8_t * g_scorch = NULL;
static uint8_t * g_boost = NULL;
/* Stronger than baseline logits but not so large that sampling collapses to stray digits */
static const float g_boost_delta = 2.25f;

void spektre_attention_apply(
        kernel_t * k,
        sk8_logit_row_t * rows,
        size_t n_rows,
        int32_t coherence_token_id,
        int32_t eos_token_id,
        spektre_attention_fracture_fn on_fracture,
        void * fracture_user,
        sk8_manifold_mask_t * out_final_mask) {
    sk8_manifold_mask_t mask;
    sk8_compute_manifold_mask(k, &mask);
    if (mask.fracture && on_fracture) {
        on_fracture(fracture_user);
        sk8_compute_manifold_mask(k, &mask);
    }
    sk8_apply_manifold_mask(&mask, rows, n_rows, coherence_token_id, eos_token_id);
    if (out_final_mask) {
        *out_final_mask = mask;
    }
}

void spektre_logits_tables_init(int32_t n_vocab, const uint8_t * scorch, const uint8_t * boost) {
    spektre_logits_tables_teardown();
    if (n_vocab <= 0) {
        return;
    }
    g_n_vocab_tables = n_vocab;
    g_scorch = (uint8_t *)calloc((size_t)n_vocab, 1);
    g_boost = (uint8_t *)calloc((size_t)n_vocab, 1);
    if (!g_scorch || !g_boost) {
        spektre_logits_tables_teardown();
        return;
    }
    if (scorch) {
        memcpy(g_scorch, scorch, (size_t)n_vocab);
    }
    if (boost) {
        memcpy(g_boost, boost, (size_t)n_vocab);
    }
}

void spektre_logits_tables_teardown(void) {
    free(g_scorch);
    free(g_boost);
    g_scorch = NULL;
    g_boost = NULL;
    g_n_vocab_tables = 0;
}

void spektre_mask_logits(float * logits, int32_t n_vocab) {
    if (!logits || !g_scorch || !g_boost || n_vocab != g_n_vocab_tables) {
        return;
    }
    for (int32_t i = 0; i < n_vocab; i++) {
        if (g_scorch[i]) {
            logits[i] = -INFINITY;
        } else if (g_boost[i]) {
            logits[i] += g_boost_delta;
        }
    }
}

void spektre_mask_logits_rows(sk8_logit_row_t * rows, size_t n_rows) {
    if (!rows || !g_scorch || !g_boost || g_n_vocab_tables <= 0) {
        return;
    }
    for (size_t i = 0; i < n_rows; i++) {
        int32_t id = rows[i].id;
        if (id < 0 || id >= g_n_vocab_tables) {
            continue;
        }
        if (g_scorch[id]) {
            rows[i].logit = -INFINITY;
            spektre_shadow_note_scorch(id);
        } else if (g_boost[id]) {
            rows[i].logit += g_boost_delta;
        }
    }
}

void spektre_merge_shadow_into_scorch(const uint32_t * shadow_counts, int32_t n_vocab, uint32_t min_count) {
    if (!g_scorch || !shadow_counts || n_vocab != g_n_vocab_tables || min_count == 0) {
        return;
    }
    for (int32_t i = 0; i < n_vocab; i++) {
        if (shadow_counts[i] >= min_count) {
            g_scorch[i] = 1;
        }
    }
}
