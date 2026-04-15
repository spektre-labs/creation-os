/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Hypercube mind + SOC seed (subset of creation_os_v2.c §2).
 */
#include "creation_os.h"
#include <stdio.h>

#define N_FACES 10
#define SOC_WINDOW 8

static const char *FACE_NAMES[] = {
    "perception", "physics", "codebook", "robotics", "soul",
    "worldmodel", "geometry", "consciousness", "temporal", "evolution",
};

typedef struct {
    uint64_t faces[N_FACES][COS_W];
    float coupling[N_FACES][N_FACES];
    float sigma_history[SOC_WINDOW];
    int tick;
    float phi;
} hypercube_mind_t;

static void mind_init(uint64_t *rng, hypercube_mind_t *m)
{
    memset(m, 0, sizeof(*m));
    for (int i = 0; i < N_FACES; i++)
        cos_hv_random(rng, m->faces[i]);
    for (int i = 0; i < N_FACES; i++)
        for (int j = 0; j < N_FACES; j++)
            m->coupling[i][j] = (i == j) ? 0.f : cos_hv_sigma(m->faces[i], m->faces[j]);
}

static float mind_tick(uint64_t *rng, hypercube_mind_t *m)
{
    (void)rng;
    for (int i = 0; i < N_FACES; i++) {
        int nearest = -1;
        float best = 999.f;
        for (int j = 0; j < N_FACES; j++) {
            if (i == j)
                continue;
            if (m->coupling[i][j] < best) {
                best = m->coupling[i][j];
                nearest = j;
            }
        }
        if (nearest >= 0) {
            uint64_t inter[COS_W];
            for (int w = 0; w < COS_W; w++)
                inter[w] = m->faces[i][w] ^ m->faces[nearest][w];
            for (int w = 0; w < COS_W; w++) {
                m->faces[i][w] ^= inter[w];
                m->faces[nearest][w] ^= inter[w];
            }
        }
    }
    float total = 0;
    int cnt = 0;
    for (int i = 0; i < N_FACES; i++)
        for (int j = i + 1; j < N_FACES; j++) {
            m->coupling[i][j] = cos_hv_sigma(m->faces[i], m->faces[j]);
            m->coupling[j][i] = m->coupling[i][j];
            total += m->coupling[i][j];
            cnt++;
        }
    float avg = total / (float)cnt;
    m->sigma_history[m->tick % SOC_WINDOW] = avg;
    float var = 0;
    for (int i = 0; i < SOC_WINDOW; i++) {
        float d = m->sigma_history[i] - avg;
        var += d * d;
    }
    m->phi = 1.0f - var / (float)SOC_WINDOW;
    m->tick++;
    return avg;
}

void cos_the_mind_demo(uint64_t seed)
{
    uint64_t rng = seed;
    hypercube_mind_t m;
    mind_init(&rng, &m);
    printf("THE_MIND: %d faces, first 5 coupling columns\n", N_FACES);
    for (int i = 0; i < 5; i++)
        printf(" %s", FACE_NAMES[i]);
    printf("\n");
    for (int i = 0; i < 5; i++) {
        printf("%-12s", FACE_NAMES[i]);
        for (int j = 0; j < 5; j++)
            printf("%.4f ", m.coupling[i][j]);
        printf("\n");
    }
    for (int t = 0; t < 5; t++)
        mind_tick(&rng, &m);
    printf("THE_MIND: tick 5 avg sigma ~ %.4f phi ~ %.4f\n", m.sigma_history[(m.tick - 1) % SOC_WINDOW], m.phi);
}
