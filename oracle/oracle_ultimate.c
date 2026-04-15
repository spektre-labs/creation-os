/* SPDX-License-Identifier: AGPL-3.0-or-later
 * 7-gram path + extra training loops (oracle_ultimate).
 */
#include "../core/creation_os.h"
#include <stdio.h>
#include <string.h>

#define ORACLE_CODEBOOK 768
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

static int predict7(oracle_t *o, const char *buf, int len)
{
    float best = 999.f;
    int best_next = ' ';
    for (int ng = 7; ng >= 3; ng -= 2) {
        if (len < ng)
            continue;
        uint64_t hv[W];
        encode(hv, buf + len - ng, ng);
        for (int i = 0; i < o->count; i++) {
            float score = cos_hv_sigma(hv, o->pattern[i]) / (o->strength[i] + 0.1f);
            if (score < best) {
                best = score;
                best_next = o->next_char[i];
            }
        }
        if (best < 0.05f)
            break;
    }
    return best_next;
}

int main(void)
{
    rng = 42;
    embed_init();
    oracle_t o;
    memset(&o, 0, sizeof(o));
    const char *corpus[] = {
        "one equals one always and forever in every substrate ",
        "sigma measures coherence using popcount on xor binding ",
    };
    for (int e = 0; e < 5; e++)
        for (unsigned d = 0; d < sizeof corpus / sizeof corpus[0]; d++) {
            int len = (int)strlen(corpus[d]);
            for (int ng = 3; ng <= 7; ng += 2)
                for (int i = 0; i <= len - ng - 1; i++)
                    learn(&o, corpus[d] + i, ng, (unsigned char)corpus[d][i + ng]);
        }
    char out[96];
    strncpy(out, "one equ", sizeof(out));
    int ol = 7;
    for (int k = 0; k < 40 && ol < (int)sizeof(out) - 2; k++) {
        int c = predict7(&o, out, ol);
        out[ol++] = (char)c;
    }
    out[ol] = 0;
    printf("oracle_ultimate: %s\n", out);
    return 0;
}
