/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Trigram oracle (subset of creation_os_v2.c §3).
 */
#include "../core/creation_os.h"
#include <stdio.h>
#include <string.h>

#define ORACLE_CODEBOOK 512
#define W COS_W
#define B COS_B

static uint64_t CHAR_EMBED[128][W];
static uint64_t rng;

static uint64_t next_rng(void)
{
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return rng;
}

static void embed_init(void)
{
    for (int i = 0; i < 128; i++)
        for (int w = 0; w < W; w++)
            CHAR_EMBED[i][w] = next_rng();
}

static void encode(uint64_t *out, const char *text, int len)
{
    memset(out, 0, B);
    for (int i = 0; i < len; i++) {
        uint64_t rotated[W];
        cos_hv_rotl(rotated, CHAR_EMBED[(unsigned char)text[i] % 128], i);
        for (int w = 0; w < W; w++)
            out[w] ^= rotated[w];
    }
}

typedef struct {
    uint64_t pattern[ORACLE_CODEBOOK][W];
    int next_char[ORACLE_CODEBOOK];
    float strength[ORACLE_CODEBOOK];
    int count;
} oracle_t;

static void learn(oracle_t *o, const char *ctx, int n, int next)
{
    uint64_t hv[W];
    encode(hv, ctx, n);
    if (o->count < ORACLE_CODEBOOK) {
        memcpy(o->pattern[o->count], hv, B);
        o->next_char[o->count] = next;
        o->strength[o->count] = 1.0f;
        o->count++;
    }
}

static int predict(oracle_t *o, const char *buf, int len)
{
    float best = 999.f;
    int best_next = ' ';
    if (len < 3)
        return best_next;
    uint64_t hv[W];
    encode(hv, buf + len - 3, 3);
    for (int i = 0; i < o->count; i++) {
        float score = cos_hv_sigma(hv, o->pattern[i]) / (o->strength[i] + 0.1f);
        if (score < best) {
            best = score;
            best_next = o->next_char[i];
        }
    }
    return best_next;
}

int main(void)
{
    rng = 1;
    embed_init();
    oracle_t o;
    memset(&o, 0, sizeof(o));
    const char *t = "hello world ";
    int L = (int)strlen(t);
    for (int i = 0; i <= L - 4; i++)
        learn(&o, t + i, 3, (unsigned char)t[i + 3]);
    char out[64];
    strncpy(out, "hel", sizeof(out));
    int ol = 3;
    for (int k = 0; k < 20 && ol < (int)sizeof(out) - 2; k++) {
        int c = predict(&o, out, ol);
        out[ol++] = (char)c;
    }
    out[ol] = 0;
    printf("oracle_speaks: %s\n", out);
    return 0;
}
