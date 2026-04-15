/* Standalone σ reputation strip for creation_os binary (not sk9 file-backed table). */
#include "living_weights.h"

#include <stdlib.h>
#include <string.h>

static uint8_t *g_rep;
static int g_nv;

void lw_soc_init(int vocab_cap) {
    free(g_rep);
    g_rep = NULL;
    g_nv = 0;
    if (vocab_cap <= 0)
        return;
    g_rep = (uint8_t *)calloc((size_t)vocab_cap, 1);
    if (g_rep)
        g_nv = vocab_cap;
}

uint8_t lw_soc_reputation(uint32_t token_id) {
    if (!g_rep || g_nv <= 0)
        return 8u;
    return g_rep[(size_t)(token_id % (uint32_t)g_nv)];
}

void lw_soc_touch(uint32_t token_id, int coherent) {
    if (!g_rep || g_nv <= 0)
        return;
    size_t i = (size_t)(token_id % (uint32_t)g_nv);
    uint8_t r = g_rep[i];
    int bit = coherent ? 1 : 0;
    r = (uint8_t)(((r << 1) | (uint8_t)bit) & 0xffu);
    g_rep[i] = r;
}
