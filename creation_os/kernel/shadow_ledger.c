#include "shadow_ledger.h"

#include <string.h>

#define SHADOW_CAP 4096u
static uint32_t g_shadow[SHADOW_CAP];
static int g_inited;

void shadow_ledger_init(void) {
    if (!g_inited) {
        memset(g_shadow, 0, sizeof(g_shadow));
        g_inited = 1;
    }
}

void shadow_ledger_mark(uint32_t token_id, uint32_t flags) {
    shadow_ledger_init();
    uint32_t i = token_id % SHADOW_CAP;
    g_shadow[i] |= flags;
}

uint32_t shadow_ledger_get(uint32_t token_id) {
    shadow_ledger_init();
    return g_shadow[token_id % SHADOW_CAP];
}
