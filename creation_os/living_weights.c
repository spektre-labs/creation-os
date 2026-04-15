/* CREATION OS — LIVING WEIGHTS (σ-guided token reputation, bit shift registers) */
#include "living_weights.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define HISTORY_BITS  8
#define NEUTRAL_POINT (HISTORY_BITS / 2)
#define BOOST_SCALE   0.5f

#define LW_MAGIC   0x4C573031u /* "LW01" */
#define DECAY_EVERY  500u
#define SAVE_EVERY   2000u

#if defined(__GNUC__) || defined(__clang__)
#define POPCNT8(x) (__builtin_popcount((unsigned)(x) & 0xFF))
#else
static int lw_popcnt8(uint8_t v) {
    int c = 0;
    while (v) {
        c += (int)(v & 1u);
        v >>= 1;
    }
    return c;
}
#define POPCNT8(x) lw_popcnt8((uint8_t)(x))
#endif

static uint8_t * g_rep       = NULL;
static int32_t   g_n_vocab   = 0;
static uint64_t  g_total_tok = 0;
static uint64_t  g_coherent  = 0;
static uint64_t  g_incoh     = 0;
static bool      g_dirty     = false;
static bool      g_inited    = false;
static char      g_path[512];

static void lw_try_load(void) {
    if (g_path[0] == '\0' || !g_rep || g_n_vocab <= 0) {
        return;
    }
    FILE * f = fopen(g_path, "rb");
    if (!f) {
        return;
    }
    uint32_t magic = 0;
    uint32_t nv    = 0;
    if (fread(&magic, 4, 1, f) != 1 || magic != LW_MAGIC) {
        fclose(f);
        return;
    }
    if (fread(&nv, 4, 1, f) != 1 || nv != (uint32_t) g_n_vocab) {
        fclose(f);
        return;
    }
    if (fread(g_rep, 1, (size_t) g_n_vocab, f) != (size_t) g_n_vocab) {
        fclose(f);
        return;
    }
    (void) fread(&g_total_tok, sizeof g_total_tok, 1, f);
    (void) fread(&g_coherent, sizeof g_coherent, 1, f);
    (void) fread(&g_incoh, sizeof g_incoh, 1, f);
    fclose(f);
    g_dirty = false;
}

static bool lw_save(void) {
    if (g_path[0] == '\0' || !g_rep || g_n_vocab <= 0) {
        return false;
    }
    FILE * f = fopen(g_path, "wb");
    if (!f) {
        return false;
    }
    uint32_t magic = LW_MAGIC;
    uint32_t nv    = (uint32_t) g_n_vocab;
    fwrite(&magic, 4, 1, f);
    fwrite(&nv, 4, 1, f);
    fwrite(g_rep, 1, (size_t) g_n_vocab, f);
    fwrite(&g_total_tok, sizeof g_total_tok, 1, f);
    fwrite(&g_coherent, sizeof g_coherent, 1, f);
    fwrite(&g_incoh, sizeof g_incoh, 1, f);
    fclose(f);
    g_dirty = false;
    return true;
}

static void lw_decay(void) {
    if (!g_rep || g_n_vocab <= 0) {
        return;
    }
    for (int32_t i = 0; i < g_n_vocab; i++) {
        uint8_t r    = g_rep[i];
        int     score = POPCNT8(r);
        if (score >= 7) {
            for (int b = 7; b >= 0; b--) {
                if (r & (1u << b)) {
                    r &= (uint8_t) ~(1u << b);
                    break;
                }
            }
        } else if (score <= 1) {
            for (int b = 7; b >= 0; b--) {
                if (!(r & (1u << b))) {
                    r |= (uint8_t)(1u << b);
                    break;
                }
            }
        }
        g_rep[i] = r;
    }
    g_dirty = true;
}

void living_weights_init(int32_t n_vocab, const char * save_path) {
    living_weights_teardown();
    if (n_vocab <= 0) {
        return;
    }
    g_rep = (uint8_t *) calloc((size_t) n_vocab, 1);
    if (!g_rep) {
        return;
    }
    g_n_vocab = n_vocab;
    memset(g_rep, 0x55, (size_t) n_vocab);
    g_path[0] = '\0';
    if (save_path && save_path[0]) {
        strncpy(g_path, save_path, sizeof g_path - 1);
        g_path[sizeof g_path - 1] = '\0';
        lw_try_load();
    }
    g_total_tok = 0;
    g_coherent  = 0;
    g_incoh     = 0;
    g_dirty     = false;
    g_inited    = true;
}

void living_weights_teardown(void) {
    if (!g_inited && !g_rep) {
        return;
    }
    if (g_inited && g_dirty && g_path[0]) {
        (void) lw_save();
    }
    free(g_rep);
    g_rep     = NULL;
    g_n_vocab = 0;
    g_inited  = false;
    g_path[0] = '\0';
}

void living_weights_apply_rows(lw_token_row_t * rows, size_t n_rows) {
    if (!g_inited || !g_rep || !rows || n_rows == 0) {
        return;
    }
    for (size_t i = 0; i < n_rows; i++) {
        int32_t tid = rows[i].id;
        if (tid < 0 || tid >= g_n_vocab) {
            continue;
        }
        int score = POPCNT8(g_rep[tid]);
        int delta = score - NEUTRAL_POINT;
        if (delta != 0) {
            rows[i].logit += (float) delta * BOOST_SCALE;
        }
    }
}

void living_weights_on_token_committed(int32_t token_id, int sigma_before, int sigma_after) {
    if (!g_inited || !g_rep || token_id < 0 || token_id >= g_n_vocab) {
        return;
    }
    uint8_t history = (uint8_t)(g_rep[token_id] << 1);
    if (sigma_after <= sigma_before) {
        history |= 1u;
        g_coherent++;
    } else {
        g_incoh++;
    }
    g_rep[token_id] = history;
    g_total_tok++;
    g_dirty = true;

    if (g_total_tok % DECAY_EVERY == 0u) {
        lw_decay();
    }
    if (g_total_tok % SAVE_EVERY == 0u && g_path[0]) {
        (void) lw_save();
    }
}
