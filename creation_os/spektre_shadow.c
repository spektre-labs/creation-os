#include "spektre_shadow.h"

#include <stdlib.h>

static uint32_t * g_shadow = NULL;
static int32_t    g_nv     = 0;

void spektre_shadow_init(int32_t n_vocab) {
    spektre_shadow_teardown();
    if (n_vocab <= 0) {
        return;
    }
    g_shadow = (uint32_t *) calloc((size_t) n_vocab, sizeof(uint32_t));
    if (!g_shadow) {
        return;
    }
    g_nv = n_vocab;
}

void spektre_shadow_teardown(void) {
    free(g_shadow);
    g_shadow = NULL;
    g_nv     = 0;
}

void spektre_shadow_note_scorch(int32_t token_id) {
    if (!g_shadow || token_id < 0 || token_id >= g_nv) {
        return;
    }
    if (g_shadow[token_id] != 0xFFFFFFFFu) {
        g_shadow[token_id]++;
    }
}

const uint32_t * spektre_shadow_get_counts(int32_t * out_n_vocab) {
    if (out_n_vocab) {
        *out_n_vocab = g_nv;
    }
    return g_shadow;
}
