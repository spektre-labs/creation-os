/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>
#include <string.h>

#define GEN_PARTICLES 64
#define GEN_TICKS 200

static uint64_t rng;

static uint64_t next_rng(void)
{
    rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return rng;
}

int main(void)
{
    rng = 99;
    uint64_t particles[GEN_PARTICLES][COS_W], noether[COS_W];
    for (int i = 0; i < GEN_PARTICLES; i++)
        cos_hv_random(&rng, particles[i]);
    memset(noether, 0, COS_B);
    for (int i = 0; i < GEN_PARTICLES; i++)
        for (int w = 0; w < COS_W; w++)
            noether[w] ^= particles[i][w];

    for (int t = 0; t < GEN_TICKS; t++) {
        for (int i = 0; i < GEN_PARTICLES; i++) {
            int j = (int)((i + 1 + (next_rng() % (uint64_t)(GEN_PARTICLES - 1))) % GEN_PARTICLES);
            uint64_t inter[COS_W];
            for (int w = 0; w < COS_W; w++)
                inter[w] = particles[i][w] ^ particles[j][w];
            for (int w = 0; w < COS_W; w++) {
                particles[i][w] ^= inter[w];
                particles[j][w] ^= inter[w];
            }
        }
    }
    uint64_t check[COS_W];
    memset(check, 0, COS_B);
    for (int i = 0; i < GEN_PARTICLES; i++)
        for (int w = 0; w < COS_W; w++)
            check[w] ^= particles[i][w];
    float noether_sigma = cos_hv_sigma(check, noether);
    printf("genesis: particles=%d ticks=%d noether_sigma=%.6f\n", GEN_PARTICLES, GEN_TICKS, noether_sigma);
    return noether_sigma < 0.001f ? 0 : 1;
}
